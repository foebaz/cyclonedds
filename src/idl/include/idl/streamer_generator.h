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
#ifndef IDL_STREAMER_GENERATOR_H
#define IDL_STREAMER_GENERATOR_H

#include <stdarg.h>
#include <stdio.h>

#include "idl/tree.h"
#include "idl/retcode.h"
#include "idl/export.h"

struct streamer
{
  FILE* header_file;
  FILE* impl_file;
};

typedef struct streamer streamer_t;

streamer_t* create_streamer(const char* filename_prefix);

void destruct_streamer(streamer_t* str);

typedef struct ostream ostream_t;

typedef struct context context_t;

context_t* create_context(streamer_t *str, const char *ctx);

void close_context(context_t* ctx);

idl_retcode_t process_node(context_t* ctx, idl_node_t *node);

IDL_EXPORT void idl_streamers_generate(char* idl, char* outputname);

idl_retcode_t process_member(context_t* ctx, idl_node_t* node);

idl_retcode_t process_module(context_t* ctx, idl_node_t* node);

idl_retcode_t process_constructed(context_t* ctx, idl_node_t* node);

#endif
