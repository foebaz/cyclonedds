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
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl.h"
#include "parser.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/strtol.h"

/* 8 bits reserved for directive parsing state.
   4 highest bits are for global state.
   4 lowest bits are for local state (enums). */
#define INITIAL (IDL_SCAN_DIRECTIVE | 0)
#define DISCARD (IDL_SCAN_DIRECTIVE | 1)
#define DIRECTIVE (IDL_SCAN_DIRECTIVE | (1<<4))
#define LINE (DIRECTIVE | (1<<5))
#define PRAGMA (DIRECTIVE |(1<<6))
#define KEYLIST (PRAGMA | (1<<7))

#define STATEMASK (IDL_SCAN_DIRECTIVE | (IDL_SCAN_DIRECTIVE - 1))

static int32_t
push_line(idl_processor_t *proc, idl_line_t *dir)
{
  if (dir->file) {
    idl_file_t *file;
    for (file = proc->files; file; file = file->next) {
      if (strcmp(dir->file, file->name) == 0)
        break;
    }
    if (!file) {
      if (!(file = ddsrt_malloc(sizeof(*file))))
        return IDL_MEMORY_EXHAUSTED;
      file->name = dir->file;
      file->next = proc->files;
      proc->files = file;
      /* do not free filename on return */
      dir->file = NULL;
    } else {
      ddsrt_free(dir->file);
    }
    proc->scanner.position.file = (const char *)file->name;
  }
  proc->scanner.position.line = dir->line;
  proc->scanner.position.column = 1;
  ddsrt_free(dir);
  proc->directive = NULL;
  return 0;
}

static int32_t
parse_line(idl_processor_t *proc, idl_token_t *tok)
{
  idl_line_t *dir = (idl_line_t *)proc->directive;
  enum { line_number = LINE, filename, extra_tokens, newline } state;

  state = proc->state & STATEMASK;
  switch (state) {
    case line_number: {
      char *end;
      unsigned long long ullng;

      assert(!dir);

      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(proc, &tok->location,
          "no line number in #line directive");
        return IDL_PARSE_ERROR;
      }
      ddsrt_strtoull(tok->value.str, &end, 10, &ullng);
      if (end == tok->value.str || *end != '\0') {
        idl_error(proc, &tok->location,
          "invalid line number in #line directive");
        return IDL_PARSE_ERROR;
      }
      if (!(dir = ddsrt_malloc(sizeof(*dir)))) {
        return IDL_MEMORY_EXHAUSTED;
      }
      dir->directive.type = idl_line;
      dir->line = (uint32_t)ullng;
      dir->file = NULL;
      proc->directive = (idl_directive_t *)dir;
      proc->state++;
    } break;
    case filename:
      assert(dir);
      proc->state++;
      if (tok->code != '\n' && tok->code != 0) {
        if (tok->code != IDL_TOKEN_STRING_LITERAL) {
          idl_error(proc, &tok->location,
            "invalid filename in #line directive");
          return IDL_PARSE_ERROR;
        }
        assert(dir && !dir->file);
        dir->file = tok->value.str;
        /* do not free string on return */
        tok->value.str = NULL;
        return 0;
      }
      /* fall through */
    case extra_tokens:
      if (tok->code != '\n' && tok->code != '\0') {
        idl_warning(proc, &tok->location,
          "extra tokens at end of #line directive");
      }
      proc->state++;
      /* fall through */
    case newline:
      assert(dir);
      if (tok->code == '\n' || tok->code == 0) {
        proc->state &= ~STATEMASK;
        return push_line(proc, dir);
      }
      break;
  }
  return 0;
}

static int32_t
push_keylist(idl_processor_t *proc, idl_keylist_t *dir)
{
  ddsts_pragma_open(proc->context);
  if (!ddsts_pragma_add_identifier(proc->context, dir->data_type))
    return IDL_MEMORY_EXHAUSTED;
  ddsrt_free(dir->data_type);
  dir->data_type = NULL;
  for (char **key = dir->keys; key && *key; key++) {
    if (!ddsts_pragma_add_identifier(proc->context, *key))
      return IDL_MEMORY_EXHAUSTED;
    ddsrt_free(*key);
    *key = NULL;
  }
  ddsrt_free(dir);
  proc->directive = NULL;
  switch (ddsts_pragma_close(proc->context)) {
    case DDS_RETCODE_OUT_OF_RESOURCES:
      return IDL_MEMORY_EXHAUSTED;
    case DDS_RETCODE_OK:
      break;
    default:
      return  IDL_PARSE_ERROR;
  }
  return 0;
}

static int32_t
parse_keylist(idl_processor_t *proc, idl_token_t *tok)
{
  idl_keylist_t *dir = (idl_keylist_t *)proc->directive;
  enum { identifier = KEYLIST, first_key, key } state;

  /* #pragma keylist does not support scoped names */

  state = proc->state & STATEMASK;
  switch (state) {
    case identifier:
      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(proc, &tok->location,
          "no data-type in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid data-type in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      }
      assert(!dir);
      if (!(dir = ddsrt_malloc(sizeof(*dir))))
        return IDL_MEMORY_EXHAUSTED;
      dir->directive.type = idl_keylist;
      dir->data_type = tok->value.str;
      dir->keys = NULL;
      proc->directive = (idl_directive_t *)dir;
      /* do not free identifier on return */
      tok->value.str = NULL;
      proc->state &= ~STATEMASK;
      proc->state |= first_key;
      break;
    case first_key:
    case key: {
      char **keys = dir->keys;
      size_t cnt = 0;

      for (; keys && *keys; keys++, cnt++) /* count keys */ ;

      if (tok->code == '\n' || tok->code == '\0') {
        proc->state &= ~STATEMASK;
        return push_keylist(proc, dir);
      } else if (tok->code == ',' && state == key) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid key in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      } else if (idl_istoken(tok->value.str, 1)) {
        idl_error(proc, &tok->location,
          "invalid key %s in #pragma keylist directive", tok->value.str);
        return IDL_PARSE_ERROR;
      } else if (!(keys = ddsrt_realloc(dir->keys, sizeof(*keys) * (cnt + 2)))) {
        return IDL_MEMORY_EXHAUSTED;
      }

      keys[cnt++] = tok->value.str;
      keys[cnt  ] = NULL;
      dir->keys = keys;
      /* do not free identifier on return */
      tok->value.str = NULL;
      proc->state &= ~STATEMASK;
      proc->state |= key;
    } break;
  }
  return 0;
}

int32_t idl_parse_directive(idl_processor_t *proc, idl_token_t *tok)
{
  unsigned int state = proc->state & STATEMASK;

  if ((state & LINE) == LINE) {
    return parse_line(proc, tok);
  } else if ((state & KEYLIST) == KEYLIST) {
    return parse_keylist(proc, tok);
  } else if ((state & PRAGMA) == PRAGMA) {
    /* expect keylist */
    if (tok->code != IDL_TOKEN_IDENTIFIER) {
      idl_error(proc, &tok->location, "invalid compiler directive");
      return IDL_PARSE_ERROR;
    } else if (strcmp(tok->value.str, "keylist") == 0) {
      proc->state |= KEYLIST;
    } else {
      idl_error(proc, &tok->location,
        "unsupported #pragma directive %s", tok->value.str);
      return IDL_PARSE_ERROR;
    }
  } else if ((state & DIRECTIVE) == DIRECTIVE) {
    /* expect line or pragma */
    if (tok->code != IDL_TOKEN_IDENTIFIER) {
      idl_error(proc, &tok->location, "invalid compiler directive");
      return IDL_PARSE_ERROR;
    } else if (strcmp(tok->value.str, "line") == 0) {
      proc->state |= LINE;
    } else if (strcmp(tok->value.str, "pragma") == 0) {
      /* support #pragma keylist for backwards compatibility */
      proc->state |= PRAGMA;
    } else {
      idl_error(proc, &tok->location,
        "invalid compiler directive %s", tok->value.str);
      return IDL_PARSE_ERROR;
    }
  } else if (state == INITIAL) {
    /* expect # or //@ */
    if (tok->code == '#') {
      proc->state |= DIRECTIVE;
    } else {
      idl_error(proc, &tok->location, "invalid compiler directive");
      return IDL_PARSE_ERROR;
    }
  }
  return 0;
}
