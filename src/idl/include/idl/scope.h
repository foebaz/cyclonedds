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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idl/processor.h"
#include "idl/retcode.h"
#include "idl/tree.h"

typedef struct idl_name idl_name_t;
struct idl_name {
  idl_location_t location;
  char *identifier;
};

typedef struct idl_scoped_name idl_scoped_name_t;
struct idl_scoped_name {
  idl_location_t location;
  bool absolute; /**< wheter scoped name is fully qualified or not */
  struct {
    size_t length; /**< number of identifiers that make up the scoped name */
    idl_name_t **names; /**< identifiers that make up the scoped name */
  } path;
};

typedef struct idl_scope idl_scope_t;

/* IDL_MODULE */
#define IDL_INHERITED (1u)
#define IDL_DECLARATION (IDL_DECL)
#define IDL_INSTANCE (2u)
#define IDL_REFERENCED (3u)
#define IDL_SCOPE (4u)

/**
 * Means through which identifier was introduced into the scope.
 *
 * \li \c IDL_MODULE Module
 * \li \c IDL_INHERITED
 * \li \c IDL_DECLARATION
 * \li \c IDL_INSTANCE
 * \li \c IDL_REFERENCED
 * \li \c IDL_SCOPE
 */
typedef idl_mask_t idl_entry_type_t;

typedef struct idl_entry idl_entry_t;
struct idl_entry {
  idl_entry_type_t type;
  idl_entry_t *next;
  idl_name_t *name;
  const idl_node_t *node; /**< node associated with symbol (if applicable) */
  idl_scope_t *scope; /**< scope introduced by symbol (if applicable) */
};

/**
 * @brief Type of declaration that introduced the scope.
 */
typedef idl_mask_t idl_scope_type_t;

#define IDL_GLOBAL (1u)

struct idl_scope {
  /* IDL_GLOBAL, IDL_MODULE, IDL_STRUCT or IDL_UNION */
  idl_scope_type_t type;
  const idl_scope_t *parent;
  const idl_name_t *name;
  struct {
    idl_entry_t *first, *last;
  } table;
};

idl_retcode_t
idl_create_name(
  idl_processor_t *proc,
  idl_name_t **namep,
  idl_location_t *location,
  char *identifier);

void
idl_delete_name(idl_name_t *name);

idl_retcode_t
idl_create_scoped_name(
  idl_processor_t *state,
  idl_scoped_name_t **scoped_namep,
  idl_location_t *location,
  idl_name_t *name,
  bool absolute);

idl_retcode_t
idl_append_to_scoped_name(
  idl_processor_t *state,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name);

idl_retcode_t
idl_delete_scoped_name(idl_scoped_name_t *scoped_name);

idl_retcode_t
idl_create_scope(
  idl_processor_t *state,
  idl_scope_t **scopep,
  idl_scope_type_t type,
  idl_name_t *name);

void
idl_delete_scope(
  idl_scope_t *scope);

void idl_enter_scope(idl_processor_t *proc, idl_scope_t *scope);
void idl_exit_scope(idl_processor_t *proc);

idl_retcode_t
idl_inherit(
  idl_processor_t *proc,
  idl_scope_t *scope,
  idl_scope_t *inherited_scope);

idl_retcode_t
idl_declare(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  idl_entry_type_t type,
  const idl_name_t *name,
  const void *node,
  idl_scope_t *scope);

idl_retcode_t
idl_resolve(
  const idl_processor_t *proc,
  idl_entry_t **entryp,
  const idl_scoped_name_t *scoped_name);

//#define IDL_FIND_IGNORE_CASE (1u<<0)
//#define IDL_FIND_IN_SCOPE (1u<<1)
//#define IDL_FIND_REQUIRED (1u<<2)

idl_entry_t *
idl_find(
  const idl_processor_t *proc,
  const idl_scope_t *scope,
  const idl_name_t *name);

//
//idl_retcode_t
//idl_find(
//  const idl_processor_t *proc,
//  idl_entry_t **entryp,
//  const idl_scope_t *scope,
//  const idl_name_t *name,
//  uint32_t flags);
//
//idl_retcode_t
//idl_find_scoped_name(
//  const idl_processor_t *proc,
//  idl_entry_t **entryp,
//  const idl_scope_t *scope,
//  const idl_scoped_name_t *scoped_name,
//  uint32_t flags);

#endif /* IDL_SCOPE_H */
