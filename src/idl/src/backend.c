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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "idl/backend.h"

#if defined(_MSC_VER)
#define NULL_DEVICE "nul"
//#define SET_BUFFER(file,buf,size) setvbuf(file, buf, _IOFBF, size)
//#define FOPEN(file,file_name,mode) fopen_s(file, file_name, mode)
#else
#define NULL_DEVICE "/dev/null"
//#define SET_BUFFER(file,buf,size) setbuffer(file, buf, size)
//#define FOPEN(file,file_name,mode) file = fopen(file_name, mode)
#endif

static const char *indent_buffer = { "                                                                " };

struct idl_backend_ctx_s
{
  idl_file_out output;
  uint32_t indent_level;
  uint32_t indent_size;
  void *custom_context;
};

idl_backend_ctx
idl_backend_context_new(uint32_t indent_size, void *custom_context)
{
  idl_backend_ctx ctx = malloc(sizeof(struct idl_backend_ctx_s));
  ctx->output = NULL;
  ctx->indent_level = 0;
  ctx->indent_size = indent_size;
  ctx->custom_context = custom_context;
  return ctx;
}

idl_retcode_t
idl_backend_context_free(idl_backend_ctx ctx)
{
  if (ctx->custom_context) return IDL_RETCODE_PRECONDITION_NOT_MET;
  if (ctx->output) idl_file_out_close(ctx);
  free(ctx);
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_file_out_new(idl_backend_ctx ctx, const char *file_name)
{
  idl_retcode_t result = IDL_RETCODE_OK;

  if (ctx->output)
  {
    result = IDL_RETCODE_CANNOT_OPEN_FILE;
  }
  else
  {
    ctx->output = malloc (sizeof(struct idl_file_out_s));
    ctx->output->file = fopen(file_name, "w");

    if (ctx->output->file == NULL)
    {
        free(ctx->output);
        ctx->output = NULL;
        result = IDL_RETCODE_CANNOT_OPEN_FILE;
    }
  }
  return result;
}

idl_retcode_t
idl_file_out_new_membuf(idl_backend_ctx ctx, char *mem_buf, size_t buf_size)
{
  idl_retcode_t result = IDL_RETCODE_OK;

  if (ctx->output)
  {
    result = IDL_RETCODE_CANNOT_OPEN_FILE;
  }
  else
  {
    ctx->output = malloc (sizeof(struct idl_file_out_s));
    ctx->output->file = fopen(NULL_DEVICE, "w");

    if (ctx->output->file == NULL)
    {
        free(ctx->output);
        ctx->output = NULL;
        result = IDL_RETCODE_CANNOT_OPEN_FILE;
    } else {
//#define SET_BUFFER(file,buf,size) setvbuf(file, buf, _IOFBF, size)
//      SET_BUFFER(ctx->output->file, mem_buf, buf_size);
      setvbuf(ctx->output->file, mem_buf, _IOFBF, buf_size);
    }
  }
  return result;
}

void
idl_file_out_close(idl_backend_ctx ctx)
{
  fclose (ctx->output->file);
  free(ctx->output);
  ctx->output = NULL;
}

idl_file_out
idl_get_output_stream(idl_backend_ctx ctx)
{
  return ctx->output;
}

void
idl_indent_incr(idl_backend_ctx ctx)
{
  ++ctx->indent_level;
}

void
idl_indent_double_incr(idl_backend_ctx ctx)
{
  ctx->indent_level += 2;
}

void
idl_indent_decr(idl_backend_ctx ctx)
{
  --ctx->indent_level;
}

void
idl_indent_double_decr(idl_backend_ctx ctx)
{
  ctx->indent_level -= 2;
}

void *
idl_get_custom_context(idl_backend_ctx ctx)
{
  return ctx->custom_context;
}

void
idl_reset_custom_context(idl_backend_ctx ctx)
{
  ctx->custom_context = NULL;
}

idl_retcode_t
idl_set_custom_context(idl_backend_ctx ctx, void *custom_context)
{
  if (ctx->custom_context) return IDL_RETCODE_PRECONDITION_NOT_MET;
  ctx->custom_context = custom_context;
  return IDL_RETCODE_OK;
}

static void
file_out_indent(idl_backend_ctx ctx)
{
  uint32_t total_indent = ctx->indent_level * ctx->indent_size;
  while (total_indent)
  {
    int32_t nr_spaces = total_indent % strlen(indent_buffer);
    fputs (indent_buffer + (strlen(indent_buffer) - nr_spaces), ctx->output->file);
    total_indent -= nr_spaces;
  }
}

void
idl_file_out_printf (
    idl_backend_ctx ctx,
    const char *format,
    ...)
{
  va_list args;

  if(ctx->output)
  {
    va_start (args, format);
    file_out_indent(ctx);
    vfprintf (ctx->output->file, format, args);
    va_end (args);
  }
}

void
idl_file_out_printf_no_indent (
    idl_backend_ctx ctx,
    const char *format,
    ...)
{
  va_list args;

  if(ctx->output)
  {
    va_start (args, format);
    vfprintf (ctx->output->file, format, args);
    va_end (args);
  }
}

bool
idl_is_reference(const idl_node_t *node)
{
  bool result = false;
  if (node->kind & IDL_TEMPL_TYPE)
  {
    switch(node->kind & IDL_TEMPL_TYPE_MASK)
    {
    case IDL_SEQUENCE_TYPE:
    case IDL_STRING_TYPE:
    case IDL_WSTRING_TYPE:
      result = true;
      break;
    default:
      result = false;
      break;
    }
  }
  return result;
}

idl_retcode_t
idl_walk_children(
    idl_backend_ctx ctx,
    const idl_node_t *target_node,
    idl_walkAction action,
    uint32_t mask)
{
  uint32_t target_category;
  idl_node_t *child = NULL;
  idl_retcode_t result = IDL_RETCODE_OK;

  assert(target_node);

  target_category = target_node->kind & IDL_CATEGORY_MASK;
  if ((target_node->kind & IDL_CATEGORY_MASK) || (target_node->kind & IDL_MEMBER))
  {
    child = (idl_node_t *)((idl_struct_type_t *)target_node)->members;
  }
  else
  {
    result = IDL_RETCODE_INVALID_PARSETREE;
  }
  while (child && result == IDL_RETCODE_OK) {
    if (child->kind & mask)
    {
      result = action(ctx, child);
    }
    child = child->next;
  }
  return result;
}

idl_retcode_t
idl_walk_current_scope(
    idl_backend_ctx ctx,
    const idl_node_t *target_node,
    idl_walkAction action,
    uint32_t mask)
{
  idl_retcode_t result = IDL_RETCODE_OK;
  while (target_node && result == IDL_RETCODE_OK) {
    if (target_node->kind & mask)
    {
      result = action(ctx, target_node);
    }
    target_node = target_node->next;
  }
  return result;
}
