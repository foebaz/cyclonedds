/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "idl/streamer_generator.h"
#include "idl/string.h"
#include "idl/cpp11backend.h"
#include "idl/processor.h"

static const char* struct_write_func_fmt = "size_t write_struct(const %s &obj, void *data, size_t position)";
static const char* primitive_calc_alignment_modulo_fmt = "(%d - position%%%d)%%%d;";
static const char* primitive_calc_alignment_shift_fmt = "(%d - position&%#x)&%#x;";
static const char* primitive_incr_alignment_fmt = "  position += alignmentbytes;";
static const char* primitive_write_func_padding_fmt = "  memset(data+position,0x0,%d);  //setting padding bytes to 0x0\n";
static const char* primitive_write_func_alignment_fmt = "  memset(data+position,0x0,alignmentbytes);  //setting alignment bytes to 0x0\n";
static const char* primitive_write_func_write_fmt = "  memcpy(data+position,&obj.%s(),%d%s);  //bytes for member: %s\n";
static const char* incr_comment = "  //moving position indicator\n";
static const char* align_comment = "  //alignment\n";
static const char* padding_comment = "  //padding bytes\n";
static const char* instance_write_func_fmt = "  position = write_struct(obj.%s(), data, position);\n";
static const char* namespace_declaration_fmt = "namespace %s\n";
static const char* struct_write_size_func_fmt = "size_t write_size(const %s &obj, size_t offset)";
static const char* primitive_incr_pos = "  position += %d;";
static const char* instance_size_func_calc_fmt = "  position += write_size(obj.%s(), position);\n";
static const char* struct_read_func_fmt = "size_t read_struct(%s &obj, void *data, size_t position)";
static const char* primitive_read_func_read_fmt = "  memcpy(&obj.%s(), data+position, %d);  //bytes for member: %s\n";
static const char* primitive_read_func_seq_fmt = "  memcpy(&sequenceentries, data+position, %d);  //number of entries in the sequence\n";
static const char* instance_read_func_fmt = "  position = read_struct(obj.%s(), data, position);\n";
static const char* seq_size_fmt = "%s().size";
static const char* seq_read_reserve_fmt = "  obj.%s().reserve(sequenceentries);\n";
static const char* seq_structured_write_fmt = "  for (const auto &%s:obj.%s()) position = write_struct(%s,data,position);\n";
static const char* seq_structured_write_size_fmt = "  for (const auto &%s:obj.%s()) position += write_size(%s, position);\n";
static const char* seq_structured_read_copy_fmt = "  for (size_t %s = 0; %s < sequenceentries; %s++) position = read_struct(obj.%s()[%s], data, position);\n";
static const char* seq_primitive_write_fmt = "  memcpy(data+position,obj.%s().data(),sequenceentries*%d);  //contents for %s\n";
static const char* seq_primitive_read_fmt = "  memcpy(obj.%s().data(),data+position,sequenceentries*%d);\n";
static const char* seq_incr_fmt = "  position += sequenceentries*%d;\n";

#if 0
static const char* fixed_pt_write_digits = "    long long int digits = ((long double)obj.%s()/pow(10.0,obj.%s().fixed_scale()));\n";
static const char* fixed_pt_write_byte = "    int byte = (obj.%s().fixed_digits())/2;\n";
static const char* fixed_pt_write_fill[] = {
"    if (digits < 0)\n",
"    {\n",
"      digits *= -1;\n",
"      data[position + byte] = (digits % 10 << 4) | 0x0d;\n",
"    }\n",
"    else\n",
"    {\n",
"      data[position + byte] = (digits % 10 << 4) | 0x0c;\n",
"    }\n",
"    while (byte && digits)\n",
"    {\n",
"      byte--;\n",
"      digits /= 10;\n",
"      data[position + byte] = ((unsigned char)digits) % 10;\n",
"      digits /= 10;\n",
"      data[position + byte] |= ((unsigned char)digits) % 10 * 16;\n",
"    }\n",
"    memset(data + position,0x0,byte);\n"
};
static const char* fixed_pt_write_position = "  position += (obj.%s().fixed_digits()/2) + 1;\n";

static const char* fixed_pt_read_byte = "    int byte = obj.%s().fixed_digits()/2;\n";
static const char* fixed_pt_read_fill[] = {
"    long long int digits = ((unsigned_char)data[byte] & 0xf0) >> 4;\n",
"    if (data[byte] & 0x0d == 0x0d)\n",
"      digits *= -1;\n",
"    while (byte >= 0)\n",
"    {\n",
"      unsigned char temp = *((usigned char*)(data + position + byte));\n",
"      digits *= 10;\n",
"      digits *= 10;\n",
"      byte--;\n",
"    }\n"
};
static const char* fixed_pt_read_assign = "    obj.%s() = (pow((long double)0.1, obj.%s().fixed_scale()) * digits);\n";
static const char* fixed_pt_read_position = "    position += (obj.%s().fixed_digits()/2) + 1;\n";
#endif

struct context
{
  streamer_t* str;
  char* context;
  ostream_t* header_stream;
  ostream_t* write_size_stream;
  ostream_t* write_stream;
  ostream_t* read_stream;
  size_t depth;
  int currentalignment;
  int accumulatedalignment;
  int alignmentpresent;
  int sequenceentriespresent;
};

struct ostream {
  char* buffer;
  size_t bufsize;
  size_t* indentlength;
};

#ifndef _WIN32
#define strcpy_s(ptr, len, str) strcpy(ptr, str)
#define sprintf_s(ptr, len, str, ...) sprintf(ptr, str, __VA_ARGS__)
#define strcat_s(ptr, len, str) strcat(ptr, str)
/* //with added debug info
#else
#define strcpy_s(ptr, len, str) printf("strcpy_s: %d\n",__LINE__); strcpy_s(ptr, len, str)
#define sprintf_s(ptr, len, str, ...) printf("sprintf_s: %d\n",__LINE__); sprintf_s(ptr, len, str, __VA_ARGS__)
#define strcat_s(ptr, len, str) printf("strcat_s: %d\n",__LINE__); strcat_s(ptr, len, str)*/
#endif

static idl_retcode_t process_instance(context_t* ctx, idl_node_t* node);
static idl_retcode_t process_base(context_t* ctx, idl_node_t* node);
static idl_retcode_t process_template(context_t* ctx, idl_node_t* node);
static idl_retcode_t process_known_width(context_t* ctx, const char* name, int bytewidth, int sequence, const char *seqappend);
static int determine_byte_width(const idl_node_t* node);
static idl_retcode_t add_alignment(context_t* ctx, int bytewidth);

static char* generatealignment(char* oldstring, int alignto)
{
  char* returnval;
  if (alignto < 2)
  {
    size_t len = strlen("0;") + 1;
    returnval = realloc(oldstring, len);
    strcpy_s(returnval, len, "0;");
  }
  else if (alignto == 2)
  {
    size_t len = strlen("position&0x1;") + 1;
    returnval = realloc(oldstring, len);
    strcpy_s(returnval, len, "position&0x1;");
  }
  else
  {
    int mask = 0xFFFFFF;
    while (mask != 0)
    {
      if (alignto == mask+1)
      {
        size_t len = strlen(primitive_calc_alignment_shift_fmt) - 2 + 30 + 1;
        returnval = realloc(oldstring, len);
        sprintf_s(returnval, len, primitive_calc_alignment_shift_fmt, alignto, mask, mask);
        return returnval;
      }
      mask >>= 1;
    }

    size_t len = strlen(primitive_calc_alignment_modulo_fmt) - 10 + 5 + 1;
    returnval = realloc(oldstring, len);
    sprintf_s(returnval, len, primitive_calc_alignment_modulo_fmt, alignto, alignto, alignto);
  }
  return returnval;
}

int determine_byte_width(const idl_node_t* node)
{
  if ((node->kind & IDL_ENUM_TYPE) == IDL_ENUM_TYPE)
    return 4;

  if (node->kind & IDL_BASE_TYPE)
  {
    switch (node->kind & 0x7f) // << no!
    {
      case IDL_INT8:
      case IDL_UINT8:
      case IDL_CHAR:
      case IDL_WCHAR:
      case IDL_BOOL:
      case IDL_OCTET:
        return 1;
      case IDL_INT16: // is equal to IDL_SHORT
      case IDL_UINT16: // is equal to IDL_USHORT
        return 2;
      case IDL_INT32: //is equal to IDL_LONG
      case IDL_UINT32: //is equal to IDL_ULONG
      case IDL_FLOAT:
        return 4;
      case IDL_INT64: //is equal to IDL_LLONG
      case IDL_UINT64: //is equal to IDL_ULLONG
      case IDL_DOUBLE:
      case IDL_LDOUBLE:
        return 8;
    }
  }
  return -1;
}

streamer_t* create_streamer(const char* filename_prefix)
{
  size_t preflen = strlen(filename_prefix);
  size_t headlen = preflen + +strlen(".h") + 1;
  size_t impllen = preflen + +strlen(".cpp") + 1;
  char* header = malloc(headlen);
  char* impl = malloc(impllen);
  header[0] = 0x0;
  impl[0] = 0x0;
  strcat_s(header, headlen, filename_prefix);
  strcat_s(header, headlen, ".h");
  strcat_s(impl, impllen, filename_prefix);
  strcat_s(impl, impllen, ".cpp");


  //generate file names

  FILE* hfile = fopen(header, "w");
  FILE* ifile = fopen(impl, "w");
  free(header);
  free(impl);
  if (NULL == hfile || NULL == ifile)
  {
    if (hfile)
      fclose(hfile);
    if (ifile)
      fclose(ifile);
    return NULL;
  }

  //printf("files opened\n");

  streamer_t* ptr = malloc(sizeof(streamer_t));
  if (NULL != ptr)
  {
    ptr->header_file = hfile;
    ptr->impl_file = ifile;
  }
  return ptr;
}

void destruct_streamer(streamer_t* str)
{
  if (NULL == str)
    return;
  if (str->header_file != NULL)
    fclose (str->header_file);
  if (str->impl_file != NULL)
    fclose (str->impl_file);
  free (str);
}

static ostream_t* create_ostream(size_t *indent)
{
  ostream_t* returnptr = malloc(sizeof(*returnptr));
  returnptr->buffer = NULL;
  returnptr->bufsize = 0;
  returnptr->indentlength = indent;
  return returnptr;
}

static void destruct_ostream(ostream_t* str)
{
  if (str->buffer)
    free(str->buffer);
  free(str);
}

static void clear_ostream(ostream_t* str)
{
  if (str->buffer)
    free(str->buffer);
  str->buffer = NULL;
  str->bufsize = 0;
}

static void flush_ostream(ostream_t* str, FILE* f)
{
  if (str->buffer != NULL)
  {
    if (f == NULL)
      printf("%s", str->buffer);
    else
      fprintf(f, "%s", str->buffer);
    clear_ostream(str);
  }
}

static void append_ostream(ostream_t* str, const char* toappend, bool indent)
{
  size_t appendlength = strlen(toappend);
  size_t indentlength = indent ? 2 * *(str->indentlength) : 0;
  if (str->buffer == NULL)
  {
    str->bufsize = appendlength + 1 + indentlength;
    str->buffer = malloc(str->bufsize);
    memset(str->buffer, ' ', indentlength);
    memcpy(str->buffer + indentlength, toappend, appendlength + 1);
  }
  else
  {
    size_t newlength = appendlength + str->bufsize + indentlength;
    char* newbuffer = malloc(newlength);
    memcpy(newbuffer, str->buffer, str->bufsize);
    memset(newbuffer + str->bufsize - 1, ' ', indentlength);
    memcpy(newbuffer + str->bufsize - 1 + indentlength, toappend, appendlength + 1);
    free(str->buffer);
    str->buffer = newbuffer;
    str->bufsize = newlength;
  }
}

context_t* create_context(streamer_t* str, const char* ctx)
{
  context_t* ptr = malloc(sizeof(context_t));
  if (NULL != ptr)
  {
    ptr->str = str;
    ptr->depth = 0;
    size_t len = strlen(ctx) + 1;
    ptr->context = malloc(len);
    ptr->context[strlen(ctx)] = 0x0;
    strcpy_s(ptr->context, len, ctx);
    ptr->currentalignment = -1;
    ptr->accumulatedalignment = 0;
    ptr->alignmentpresent = 0;
    ptr->sequenceentriespresent = 0;
    ptr->header_stream = create_ostream(&ptr->depth);
    ptr->write_size_stream = create_ostream(&ptr->depth);
    ptr->write_stream = create_ostream(&ptr->depth);
    ptr->read_stream = create_ostream(&ptr->depth);
  }
  //printf("new context generated: %s\n", ctx);
  return ptr;
}

#if 0
static void clear_context(context_t* ctx)
{
  clear_ostream(ctx->header_stream);
  clear_ostream(ctx->write_stream);
  clear_ostream(ctx->write_size_stream);
  clear_ostream(ctx->read_stream);
}
#endif

static void flush_context(context_t* ctx)
{
  flush_ostream(ctx->header_stream, ctx->str->header_file);
  flush_ostream(ctx->write_stream, ctx->str->impl_file);
  flush_ostream(ctx->write_size_stream, ctx->str->impl_file);
  flush_ostream(ctx->read_stream, ctx->str->impl_file);
}

void close_context(context_t* ctx)
{
  //add closing statements to buffers?

  flush_context(ctx);
  destruct_ostream(ctx->header_stream);
  destruct_ostream(ctx->write_size_stream);
  destruct_ostream(ctx->write_stream);
  destruct_ostream(ctx->read_stream);
  //printf("context closed: %s\n", ctx->context);
  free(ctx->context);
}

idl_retcode_t process_node(context_t* ctx, idl_node_t* node)
{
  if (node->kind & IDL_MEMBER)
    process_member(ctx, node);
  else if (node->kind & IDL_MODULE)
    process_module(ctx, node);
  else if (node->kind & IDL_CONSTR_TYPE)
    process_constructed(ctx, node);

  if (node->next)
    process_node(ctx,node->next);

  return IDL_RETCODE_OK;
}

idl_retcode_t process_member(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;
  //printf("member flags: %x\n", node->kind);
  if (node->kind & IDL_BASE_TYPE)
    process_base(ctx, node);
  else if (node->kind & IDL_SCOPED_NAME)
    process_instance(ctx, node);
  else if (node->kind & IDL_TEMPL_TYPE)
    process_template(ctx, node);

  return IDL_RETCODE_OK;
}

idl_retcode_t process_instance(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;
  idl_member_t *member = (idl_member_t *)node;
  // FIXME: this probably needs to loop?
  char* cpp11name = get_cpp11_name(member->declarators->identifier);
  size_t cpp11len = strlen(cpp11name);
  size_t bufsize = strlen(instance_write_func_fmt) - 2 + cpp11len + 1;
  char* buffer = malloc(bufsize);
  sprintf_s(buffer, bufsize, instance_write_func_fmt, cpp11name);
  append_ostream(ctx->write_stream, buffer,true);

  bufsize = strlen(instance_read_func_fmt) - 2 + cpp11len + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, instance_read_func_fmt, cpp11name);
  append_ostream(ctx->read_stream, buffer, true);

  bufsize = strlen(instance_size_func_calc_fmt) - 2 + cpp11len + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, instance_size_func_calc_fmt, cpp11name);
  append_ostream(ctx->write_size_stream, buffer, true);

  ctx->accumulatedalignment = 0;
  ctx->currentalignment = -1;

  free(buffer);
  free(cpp11name);
  return IDL_RETCODE_OK;
}

idl_retcode_t add_alignment(context_t* ctx, int bytewidth)
{
  if (NULL == ctx)
    return IDL_RETCODE_INVALID_PARSETREE;

  size_t bufsize = 0;
  char* buffer = NULL;

  //printf("current alignment: %d, byte width: %d, acc: %d\n", ctx->currentalignment, bytewidth, ctx->accumulatedalignment);
  if ((0 > ctx->currentalignment || bytewidth > ctx->currentalignment) && bytewidth != 1)
  {
    if (0 == ctx->alignmentpresent)
    {
      append_ostream(ctx->write_stream, "  size_t alignmentbytes = ", true);
      append_ostream(ctx->read_stream, "  size_t alignmentbytes = ", true);
      ctx->alignmentpresent = 1;
    }
    else
    {
      append_ostream(ctx->write_stream, "  alignmentbytes = ", true);
      append_ostream(ctx->read_stream, "  alignmentbytes = ", true);
    }

    buffer = generatealignment(buffer, bytewidth);
    append_ostream(ctx->write_stream, buffer, false);
    append_ostream(ctx->write_stream, align_comment, false);
    append_ostream(ctx->write_stream, primitive_write_func_alignment_fmt, true);
    append_ostream(ctx->write_stream, primitive_incr_alignment_fmt, true);
    append_ostream(ctx->write_stream, incr_comment, false);

    append_ostream(ctx->read_stream, buffer, false);
    append_ostream(ctx->read_stream, align_comment, false);
    append_ostream(ctx->read_stream, primitive_incr_alignment_fmt, true);
    append_ostream(ctx->read_stream, incr_comment, false);

    append_ostream(ctx->write_size_stream, "  position += ", true);
    append_ostream(ctx->write_size_stream, buffer, false);
    append_ostream(ctx->write_size_stream, align_comment, false);

    ctx->accumulatedalignment = 0;
    ctx->currentalignment = bytewidth;
  }
  else
  {
    int missingbytes = (bytewidth - (ctx->accumulatedalignment % bytewidth)) % bytewidth;
    if (0 != missingbytes)
    {
      bufsize = strlen(primitive_write_func_padding_fmt) - 2 + 1 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, primitive_write_func_padding_fmt, missingbytes);
      append_ostream(ctx->write_stream, buffer, true);

      bufsize = strlen(primitive_incr_pos) - 2 + 1 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, primitive_incr_pos, missingbytes);
      append_ostream(ctx->write_size_stream, buffer, true);
      append_ostream(ctx->write_size_stream, padding_comment, false);

      append_ostream(ctx->read_stream, buffer, true);
      append_ostream(ctx->read_stream, padding_comment, false);

      append_ostream(ctx->write_stream, buffer, true);
      append_ostream(ctx->write_stream, incr_comment, false);

      ctx->accumulatedalignment = 0;
    }
  }

  free(buffer);
  return IDL_RETCODE_OK;
}

idl_retcode_t process_known_width(context_t* ctx, const char* name, int bytewidth, int sequence, const char *seqappend)
{
  if (NULL == ctx || NULL == name)
    return IDL_RETCODE_INVALID_PARSETREE;

  size_t bufsize = 0;
  char* buffer = NULL;

  if (ctx->currentalignment != bytewidth)
    add_alignment(ctx, bytewidth);

  bufsize = strlen(primitive_write_func_write_fmt) - 2 + 1 + strlen(name)*2 - 4 + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, primitive_write_func_write_fmt, name, bytewidth, seqappend ? seqappend : "", name);
  append_ostream(ctx->write_stream, buffer, true);

  ctx->accumulatedalignment += bytewidth;

  if (0 == sequence)
  {
    bufsize = strlen(primitive_read_func_read_fmt) - 2 + 1 + strlen(name)*2 - 4 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, primitive_read_func_read_fmt, name, bytewidth, name);
    append_ostream(ctx->read_stream, buffer, true);
  }
  else
  {
    if (0 == ctx->sequenceentriespresent)
    {
      append_ostream(ctx->read_stream, "  uint32_t sequenceentries;", true);
      ctx->sequenceentriespresent = 1;
    }

    bufsize = strlen(primitive_read_func_seq_fmt) - 2 + 1 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, primitive_read_func_seq_fmt, bytewidth);
    append_ostream(ctx->read_stream, buffer, true);
  }

  bufsize = strlen(primitive_incr_pos);
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, primitive_incr_pos, bytewidth);
  append_ostream(ctx->write_size_stream, buffer, true);
  append_ostream(ctx->write_size_stream, "  //bytes for member: ", false);
  append_ostream(ctx->write_size_stream, name, false);
  append_ostream(ctx->write_size_stream, "\n", false);

  append_ostream(ctx->write_stream, buffer, true);
  append_ostream(ctx->write_stream, incr_comment, false);

  append_ostream(ctx->read_stream, buffer, true);
  append_ostream(ctx->read_stream, incr_comment, false);

  free(buffer);

  return IDL_RETCODE_OK;
}

idl_retcode_t process_template(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;

  char* buffer = NULL;
  size_t bufsize = 0;
  char* cpp11name = NULL;
  idl_member_t *member = (idl_member_t *)node;
  idl_type_spec_t *type_spec = member->type_spec;

  if ((type_spec->kind & IDL_SEQUENCE_TYPE) == IDL_SEQUENCE_TYPE)
  {
    // FIXME: loop!?
    cpp11name = get_cpp11_name(member->declarators->identifier);
    size_t cpp11len = strlen(cpp11name);
    bufsize = cpp11len + strlen(seq_size_fmt) - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, seq_size_fmt, cpp11name);
    process_known_width(ctx, buffer, 4, 1, NULL);

    bool is_base_type = false;
    if (is_base_type)
    {
      int bytewidth = determine_byte_width(node);  //determine byte width of base type?

      if (bytewidth > 4)
        add_alignment(ctx, bytewidth);

      bufsize = cpp11len*2 + strlen(seq_primitive_write_fmt) - 6 + 1 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_primitive_write_fmt, cpp11name, bytewidth, cpp11name);
      append_ostream(ctx->write_stream, buffer, true);

      bufsize = cpp11len + strlen(seq_read_reserve_fmt) - 2 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_read_reserve_fmt, cpp11name);
      append_ostream(ctx->read_stream, buffer, true);

      bufsize = cpp11len + strlen(seq_primitive_read_fmt) - 4 + 1 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_primitive_read_fmt, cpp11name, bytewidth);
      append_ostream(ctx->read_stream, buffer, true);
      
      bufsize = cpp11len + strlen(seq_incr_fmt) - 2 + 1 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_incr_fmt, bytewidth);
      append_ostream(ctx->write_stream, buffer, true);
      append_ostream(ctx->write_size_stream, buffer, true);
      append_ostream(ctx->read_stream, buffer, true);
    }
    else
    {
      char* iterated_name = NULL;
      size_t iterated_name_size = 0;

      if (strcmp(cpp11name, "_1"))
      {
        iterated_name_size = strlen("_1") + 1;
        iterated_name = malloc(iterated_name_size);
        strcpy_s(iterated_name, iterated_name_size, "_1");
      }
      else
      {
        iterated_name_size = strlen("_2") + 1;
        iterated_name = malloc(iterated_name_size);
        strcpy_s(iterated_name, iterated_name_size, "_2");
      }

      bufsize = cpp11len + strlen(seq_structured_write_fmt) + iterated_name_size*2 - 6 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_structured_write_fmt, iterated_name, cpp11name, iterated_name);
      append_ostream(ctx->write_stream, buffer, true);

      bufsize = cpp11len + strlen(seq_structured_write_size_fmt) - 6 + iterated_name_size*2 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_structured_write_size_fmt, iterated_name, cpp11name, iterated_name);
      append_ostream(ctx->write_size_stream, buffer, true);

      bufsize = cpp11len + strlen(seq_read_reserve_fmt) - 2 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_read_reserve_fmt, cpp11name);
      append_ostream(ctx->read_stream, buffer, true);

      bufsize = cpp11len + strlen(seq_structured_read_copy_fmt) - 8 + iterated_name_size*3 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, seq_structured_read_copy_fmt, iterated_name, iterated_name, iterated_name, cpp11name, iterated_name);
      append_ostream(ctx->read_stream, buffer, true);

      if (NULL != iterated_name)
        free(iterated_name);
    }

    ctx->accumulatedalignment = 0;
    ctx->currentalignment = -1;
  }
  else if ((type_spec->kind & IDL_STRING_TYPE) == IDL_STRING_TYPE ||
           (type_spec->kind & IDL_WSTRING_TYPE) == IDL_WSTRING_TYPE)
  {
    // FIXME: loop!?
    cpp11name = get_cpp11_name(member->declarators->identifier);
    size_t cpp11len = strlen(cpp11name);
    bufsize = cpp11len + strlen(seq_size_fmt) - 2 + 1;
    sprintf_s(buffer, bufsize, seq_size_fmt, cpp11name);
    process_known_width(ctx, buffer, 4, 1, "+1");

    bufsize = cpp11len*2 + strlen(seq_primitive_write_fmt) - 6 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, seq_primitive_write_fmt, cpp11name, 1, cpp11name);
    append_ostream(ctx->write_stream, buffer, true);

    bufsize = cpp11len + strlen(seq_read_reserve_fmt) - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, seq_read_reserve_fmt, cpp11name);
    append_ostream(ctx->read_stream, buffer, true);

    bufsize = cpp11len + strlen(seq_primitive_read_fmt) - 4 + 1 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, seq_primitive_read_fmt, cpp11name, 1);
    append_ostream(ctx->read_stream, buffer, true);

    bufsize = cpp11len + strlen(seq_incr_fmt) - 2 + 1 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, seq_incr_fmt, 1);
    append_ostream(ctx->write_stream, buffer, true);
    append_ostream(ctx->write_size_stream, buffer, true);
    append_ostream(ctx->read_stream, buffer, true);

    ctx->accumulatedalignment = 0;
    ctx->currentalignment = -1;
  }
#if 0
  else if ((node->flags & IDL_FIXED_PT) == IDL_FIXED_PT)
  {
    //fputs("fixed point type template classes not supported at this time", stderr);

    append_ostream(ctx->write_stream, "  {\n", true);

    bufsize = strlen(fixed_pt_write_digits) + cpp11len * 2 - 4 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_write_digits, cpp11name, cpp11name);
    append_ostream(ctx->write_stream, buffer, true);

    bufsize = strlen(fixed_pt_write_byte) + cpp11len - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_write_byte, cpp11name);
    append_ostream(ctx->write_stream, buffer, true);

    for (size_t i = 0; i < sizeof(fixed_pt_write_fill) / sizeof(const char*); i++)
      append_ostream(ctx->write_stream, fixed_pt_write_fill[i], true);

    bufsize = strlen(fixed_pt_write_position) + cpp11len - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_write_position, cpp11name);
    append_ostream(ctx->write_stream, buffer, true);
    append_ostream(ctx->write_size_stream, "  ", true);
    append_ostream(ctx->write_size_stream, buffer, false);

    append_ostream(ctx->write_stream, "  }\n", true);

    append_ostream(ctx->read_stream, "  {\n", true);

    bufsize = strlen(fixed_pt_read_byte) + cpp11len - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_read_byte, cpp11name);
    append_ostream(ctx->read_stream, buffer, true);

    for (size_t i = 0; i < sizeof(fixed_pt_read_fill) / sizeof(const char*); i++)
      append_ostream(ctx->read_stream, fixed_pt_read_fill[i], true);

    bufsize = strlen(fixed_pt_read_assign) + cpp11len*2 - 4 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_read_assign, cpp11name, cpp11name);
    append_ostream(ctx->read_stream, buffer, true);

    bufsize = strlen(fixed_pt_read_position) + cpp11len - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, fixed_pt_read_position, cpp11name);
    append_ostream(ctx->read_stream, buffer, true);

    append_ostream(ctx->read_stream, "  }\n", true);



    ctx->accumulatedalignment = 0;
    ctx->currentalignment = -1;
  }
#endif

  if (cpp11name)
    free(cpp11name);
  if (NULL != buffer)
    free(buffer);
  return IDL_RETCODE_OK;
}

idl_retcode_t process_module(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;

  idl_module_t *module = (idl_module_t *)node;
  if (module->definitions)
  {
    char* cpp11name = get_cpp11_name(module->identifier);
    size_t bufsize = strlen(namespace_declaration_fmt) + strlen(cpp11name) - 2 + 1;
    char* buffer = malloc(bufsize);
    sprintf_s(buffer, bufsize, namespace_declaration_fmt, cpp11name);
    context_t* newctx = create_context(ctx->str, cpp11name);

    newctx->depth = ctx->depth;
    append_ostream(newctx->header_stream, buffer, true);
    append_ostream(newctx->header_stream, "{\n\n", true);
    append_ostream(newctx->write_stream, buffer, true);
    append_ostream(newctx->write_stream, "{\n\n", true);
    newctx->depth++;

    process_node(newctx, (idl_node_t *)module->definitions);
    newctx->depth--;
    append_ostream(newctx->header_stream, "}\n\n", true);
    append_ostream(newctx->read_stream, "}\n\n", true);
    close_context(newctx);

    free(cpp11name);
    free(buffer);
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t process_constructed(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;

  char* cpp11name = NULL;
  size_t cpp11len = strlen(cpp11name);

  if (idl_is_struct(node))
  {
    idl_struct_type_t *_struct = (idl_struct_type_t *)node;
    if (_struct->members)
    {
      cpp11name = get_cpp11_name(_struct->identifier);
      size_t bufsize = strlen(cpp11name) + strlen(struct_write_func_fmt) - 2 + 1;
      char* buffer = malloc(bufsize);
      sprintf_s(buffer, bufsize, struct_write_func_fmt, cpp11name);
      append_ostream(ctx->header_stream, buffer, true);
      append_ostream(ctx->header_stream, ";\n\n",false);
      append_ostream(ctx->write_stream, buffer, true);
      append_ostream(ctx->write_stream, "\n", false);
      append_ostream(ctx->write_stream, "{\n", true);

      bufsize = strlen(cpp11name) + strlen(struct_write_size_func_fmt) - 2 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, struct_write_size_func_fmt, cpp11name);
      append_ostream(ctx->header_stream, buffer, true);
      append_ostream(ctx->header_stream, ";\n\n", false);
      append_ostream(ctx->write_size_stream, buffer, true);
      append_ostream(ctx->write_size_stream, "\n", false);
      append_ostream(ctx->write_size_stream, "{\n", true);
      append_ostream(ctx->write_size_stream, "  size_t position = offset;\n",true);

      bufsize = cpp11len + strlen(struct_read_func_fmt) - 2 + 1;
      buffer = realloc(buffer, bufsize);
      sprintf_s(buffer, bufsize, struct_read_func_fmt, cpp11name);
      append_ostream(ctx->header_stream, buffer, true);
      append_ostream(ctx->header_stream, ";\n\n", false);
      append_ostream(ctx->read_stream, buffer, true);
      append_ostream(ctx->read_stream, "\n", false);
      append_ostream(ctx->read_stream, "{\n", true);

      ctx->currentalignment = -1;
      ctx->alignmentpresent = 0;
      ctx->sequenceentriespresent = 0;
      ctx->accumulatedalignment = 0;

      free(buffer);
      process_node(ctx, (idl_node_t *)_struct->members);

      append_ostream(ctx->write_size_stream, "  return position-offset;\n", true);
      append_ostream(ctx->write_size_stream, "}\n\n", true);
      append_ostream(ctx->write_stream, "  return position;\n", true);
      append_ostream(ctx->write_stream, "}\n\n", true);
      append_ostream(ctx->read_stream, "  return position;\n", true);
      append_ostream(ctx->read_stream, "}\n\n", true);
      flush_context(ctx);
    }
  }
  else if (idl_is_union(node))
  {
    fputs("union constructed types not supported at this time", stderr);
  }
  else if (idl_is_enum(node))
  {
    fputs("enum constructed types not supported at this time", stderr);
  }

  if (cpp11name)
    free(cpp11name);
  return IDL_RETCODE_OK;
}

idl_retcode_t process_base(context_t* ctx, idl_node_t* node)
{
  if (NULL == ctx || NULL == node)
    return IDL_RETCODE_INVALID_PARSETREE;

  idl_member_t *member = (idl_member_t *)node;
  idl_type_spec_t *type_spec = member->type_spec;
  char *cpp11name = get_cpp11_name(member->declarators->identifier);
  int bytewidth = determine_byte_width((idl_node_t *)type_spec);
  if (1 > bytewidth)
    return IDL_RETCODE_PARSE_ERROR;

  process_known_width(ctx, cpp11name, bytewidth, 0, NULL);

  free(cpp11name);
  return IDL_RETCODE_OK;
}

void idl_streamers_generate(char* idl, char* outputname)
{
  idl_tree_t* tree = 0x0;
  idl_parse_string(idl, 0x0, &tree);

  streamer_t *str = create_streamer(outputname);
  context_t* ctx = create_context(str, "");
  process_node(ctx, tree->root);
  close_context(ctx);
  destruct_streamer(str);
}
