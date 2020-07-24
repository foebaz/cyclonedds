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

#ifndef IDL_BACKEND_H
#define IDL_BACKEND_H

#include <stdio.h>
#include <stdarg.h>
#include "idl/tree.h"
#include "idl/retcode.h"
#include "idl/export.h"

typedef struct idl_file_out_s {
  FILE *file;
} *idl_file_out;

struct idl_backend_ctx_s;
typedef struct idl_backend_ctx_s *idl_backend_ctx;

idl_backend_ctx
IDL_EXPORT idl_backend_context_new(uint32_t indent_size, void *custom_context);

idl_retcode_t
IDL_EXPORT idl_backend_context_free(idl_backend_ctx ctx);

idl_retcode_t
IDL_EXPORT idl_file_out_new(idl_backend_ctx ctx, const char *file_name);

idl_retcode_t
IDL_EXPORT idl_file_out_new_membuf(idl_backend_ctx ctx, char *mem_buf, size_t buf_size);

void
IDL_EXPORT idl_file_out_close(idl_backend_ctx ctx);

idl_file_out
IDL_EXPORT idl_get_output_stream(idl_backend_ctx ctx);

void
IDL_EXPORT idl_indent_incr(idl_backend_ctx ctx);

void
IDL_EXPORT idl_indent_double_incr(idl_backend_ctx ctx);

void
IDL_EXPORT idl_indent_decr(idl_backend_ctx ctx);

void
IDL_EXPORT idl_indent_double_decr(idl_backend_ctx ctx);

void
IDL_EXPORT *idl_get_custom_context(idl_backend_ctx ctx);

void
IDL_EXPORT idl_reset_custom_context(idl_backend_ctx ctx);

idl_retcode_t
IDL_EXPORT idl_set_custom_context(idl_backend_ctx ctx, void *custom_context);

void
IDL_EXPORT idl_file_out_printf (
    idl_backend_ctx ctx,
    const char *format,
    ...);

void
IDL_EXPORT idl_file_out_printf_no_indent (
    idl_backend_ctx ctx,
    const char *format,
    ...);

bool
IDL_EXPORT idl_is_reference(const idl_node_t *node);

#define IDL_MASK_ALL 0xffffffff

typedef uint32_t idl_walkResult;

typedef idl_retcode_t
(idl_walkAction)(idl_backend_ctx ctx, const idl_node_t *node);

idl_retcode_t
IDL_EXPORT idl_walk_children(idl_backend_ctx ctx, const idl_node_t *starting_node, idl_walkAction, uint32_t mask);

idl_retcode_t
IDL_EXPORT idl_walk_current_scope(idl_backend_ctx ctx, const idl_node_t *starting_node, idl_walkAction, uint32_t mask);

idl_retcode_t
IDL_EXPORT idl_backendGenerate(idl_backend_ctx ctx, const idl_tree_t *parse_tree);

#endif /* IDL_BACKEND_H */
