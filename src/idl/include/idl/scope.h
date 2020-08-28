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
#ifndef IDL_SCOPE_H
#define IDL_SCOPE_H

typedef struct idl_entry idl_entry_t;
struct idl_entry {
  char *identifier;
  idl_scope_t *scope;
  idl_node_t *node;
};

typedef struct idl_scope idl_scope_t;
struct idl_scope {
  idl_scope_t *parent;
  // each scope will have it's fully scoped name
  char *scoped_name;
    // >> each declaration will point towards it's scope
  struct {
    idl_entry_t *first, *last;
  } table;
  // >> I guess we have a list of entries?
  //   >> these just contain a pointer to the node
};

idl_entry_t
idl_find(
  const idl_processor_t *proc,
  const idl_scope_t *scope,
  const char *identifier,
  bool ignore_case);

#endif /* IDL_SCOPE_H */
