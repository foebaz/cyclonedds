/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef NN_INVERSE_UINT32_SET_H
#define NN_INVERSE_UINT32_SET_H

#include "util/ut_avl.h"

struct inverse_uint32_set_node {
  ut_avlNode_t avlnode;
  uint32_t min, max;
};
struct inverse_uint32_set {
  ut_avlTree_t ids;
  uint32_t cursor;
  uint32_t min, max;
};

void inverse_uint32_set_init(struct inverse_uint32_set *set, uint32_t min, uint32_t max);
void inverse_uint32_set_fini(struct inverse_uint32_set *set);
int inverse_uint32_set_alloc(uint32_t * const id, struct inverse_uint32_set *set);
void inverse_uint32_set_free(struct inverse_uint32_set *set, uint32_t id);

#endif
