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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "idl/retcode.h"
#include "idl/string.h"
#include "idl/scope.h"

idl_retcode_t
idl_create_name(
  idl_processor_t *proc,
  idl_name_t **namep,
  idl_location_t *location,
  char *identifier)
{
  idl_name_t *name;

  (void)proc;
  if (!(name = malloc(sizeof(*name))))
    return IDL_RETCODE_NO_MEMORY;
  name->location = *location;
  name->identifier = identifier;
  *namep = name;
  return IDL_RETCODE_OK;
}

void
idl_delete_name(idl_name_t *name)
{
  if (name) {
    if (name->identifier)
      free(name->identifier);
    free(name);
  }
}

idl_retcode_t
idl_create_scoped_name(
  idl_processor_t *proc,
  idl_scoped_name_t **scoped_namep,
  idl_location_t *location,
  idl_name_t *name,
  bool absolute)
{
  idl_scoped_name_t *scoped_name;

  (void)proc;
  if (!(scoped_name = malloc(sizeof(*scoped_name)))) {
    return IDL_RETCODE_NO_MEMORY;
  }
  if (!(scoped_name->path.names = calloc(1, sizeof(idl_name_t*)))) {
    free(scoped_name);
    return IDL_RETCODE_NO_MEMORY;
  }
  //assert(location->last > location->first);
  //assert(location->last < name->location.last);
  scoped_name->location.first = location->first;
  scoped_name->location.last = name->location.last;
  scoped_name->path.length = 1;
  scoped_name->path.names[0] = name;
  scoped_name->absolute = absolute;
  *scoped_namep = scoped_name;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_append_to_scoped_name(
  idl_processor_t *proc,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name)
{
  size_t size;
  idl_name_t **names;

  assert(scoped_name);
  assert(scoped_name->path.length >= 1);
  assert(name);

  size = (scoped_name->path.length + 1) * sizeof(idl_name_t*);
  if (!(names = realloc(scoped_name->path.names, size)))
    return IDL_RETCODE_NO_MEMORY;
  //assert(name->location.last > name->location.first);
  //assert(name->location.last > scoped_name->location.last);
  scoped_name->location.last = name->location.last;
  scoped_name->path.names = names;
  scoped_name->path.names[scoped_name->path.length++] = name;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_delete_scoped_name(idl_scoped_name_t *scoped_name)
{
  if (scoped_name) {
    for (size_t i=0; i < scoped_name->path.length; i++)
      idl_delete_name(scoped_name->path.names[i]);
    free(scoped_name);
  }
}

idl_retcode_t
idl_create_scope(
  idl_processor_t *state,
  idl_scope_t **scopep,
  idl_scope_type_t type,
  idl_name_t *name)
{
  idl_scope_t *scope;
  idl_entry_t *entry;

  if (!(entry = malloc(sizeof(*entry))))
    goto err_entry;
  if (!(entry->name = malloc(sizeof(*entry->name))))
    goto err_name;
  if (!(entry->name->identifier = idl_strdup(name->identifier)))
    goto err_identifier;
  entry->next = NULL;
  entry->type = IDL_SCOPE;
  entry->name->location = name->location;
  entry->node = NULL;
  entry->scope = NULL;
  if (!(scope = malloc(sizeof(*scope))))
    goto err_scope;
  scope->type = type;
  scope->parent = (const idl_scope_t *)state->scope;
  scope->name = (const idl_name_t *)&entry->name;
  scope->table.first = scope->table.last = entry;

  *scopep = scope;
  return IDL_RETCODE_OK;
err_scope:
  free(entry->name->identifier);
err_identifier:
  free(entry->name);
err_name:
  free(entry);
err_entry:
  return IDL_RETCODE_NO_MEMORY;
}

/* free scopes, not nodes */
void
idl_delete_scope(
  idl_scope_t *scope)
{
  //
  // .. implement ..
  // x. delete all child scopes
  //    >> do not free nodes, that's the function of delete_tree
  //
}

idl_retcode_t
idl_inherit(
  idl_processor_t *proc,
  idl_scope_t *scope,
  idl_scope_t *inherited_scope)
{
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_declare(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  idl_entry_type_t type,
  const idl_name_t *name,
  const void *node,
  idl_scope_t *scope)
{
  idl_entry_t *entry;
  idl_scope_t *parent;

  assert(proc && proc->scope);
  fprintf(stderr, "entry: %llu for %s\n", type, name->identifier);
  if (node) {
  if (((idl_node_t *)node)->mask & (IDL_MODULE|IDL_STRUCT|IDL_UNION)) {
    assert(scope);
  } else {
    assert(!scope);
  }
  }

  parent = proc->scope;
  /* ensure there is no collision with earlier an declaration */
  for (idl_entry_t *e = parent->table.first; e; e = e->next) {
    /* identifiers that differ only in case collide, and will yield a
       compilation error under certain circumstances */
    if (idl_strcasecmp(name->identifier, e->name->identifier) == 0) {
      idl_error(proc, idl_location(node),
        "Declaration '%s' clashes with declaration '%s'",
        name->identifier, e->name->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  if (!(entry = malloc(sizeof(*entry))))
    return IDL_RETCODE_NO_MEMORY;
  entry->next = NULL;
  entry->name = (const idl_name_t *)name;
  entry->node = node;
  entry->scope = scope;

  if (parent->table.first) {
    assert(parent->table.last);
    parent->table.last->next = entry;
    parent->table.last = entry;
  } else {
    assert(!parent->table.last);
    parent->table.last = parent->table.first = entry;
  }

  if (entryp)
    *entryp = entry;
  return IDL_RETCODE_OK;
}

idl_entry_t *
idl_find(
  const idl_processor_t *proc,
  const idl_scope_t *scope,
  const idl_name_t *name)
{
  idl_entry_t *e;

  if (!scope)
    scope = proc->scope;

  for (e = scope->table.first; e; e = e->next) {
    if (idl_strcasecmp(e->name->identifier, name->identifier) == 0)
      return e;
  }

  return NULL;
}

idl_retcode_t
idl_resolve(
  const idl_processor_t *proc,
  idl_entry_t **entryp,
  const idl_scoped_name_t *scoped_name)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  idl_name_t *name = NULL;
  idl_entry_t *entry = NULL;
  idl_scope_t *scope;

  scope = (scoped_name->absolute) ? proc->global_scope : proc->scope;
  assert(scope);

  for (size_t i=0; i < scoped_name->path.length && scope;) {
    const char *identifier = scoped_name->path.names[i]->identifier;
    entry = idl_find(proc, scope, scoped_name->path.names[i]);
    if (entry) {
      /* identifiers are case insensitive. however, all references to a
         definition must use the same case as the defining occurence */
      if (strcmp(identifier, entry->name->identifier) != 0) {
        idl_error(proc, &scoped_name->path.names[i]->location,
          "Scoped name matched up to '%s', but identifier differs in case from '%s'",
          identifier, entry->name->identifier);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      i++;
    } else if (scoped_name->absolute || i != 0) {
      idl_error(proc, &scoped_name->location,
        "Scoped name '%s' cannot be resolved", "<scoped_name>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else {
      /* a name can be used in an unqualified form within a particular scope;
         it will be resolved by successively searching farther out in
         enclosing scopes, while taking into consideration inheritance
         relationships amount interfaces */
      /* assume inheritance applies to extended structs in the same way */
      scope = scope->parent;
    }
  }

  if (!scoped_name->absolute) {
    /* non-absolute qualified names introduce the identifier of the outermost
       scope of the scoped name into the current scope */
    const char *identifier = scoped_name->path.names[0]->identifier;
    if (!(name = malloc(sizeof(*name))))
      goto err_alloc;
    if (!(name->identifier = strdup(identifier)))
      goto err_strdup;
    name->location = scoped_name->path.names[0]->location;
    if ((ret = idl_declare(proc, NULL, IDL_REFERENCED, name, NULL, NULL)))
      goto err_declare;
  }

  *entryp = entry;
  return IDL_RETCODE_OK;
err_declare:
  free(name->identifier);
err_strdup:
  free(name);
err_alloc:
  return ret;
}

  // the inherited use-case described in the idl 4.0 spec in section 7.5.2
  // is interesting!
  // >> since we're doing idl 4.0, which has struct inheritance, we're
  //    obligated to implement it
  // >> but we should watch the extended_base_types flag etc etc
//IDL identifiers are case insensitive. However, all references to a definition must use the same case as the defining
//occurrence. This allows natural mappings to case-sensitive languages.

void
idl_enter_scope(
  idl_processor_t *proc,
  idl_scope_t *scope)
{
  proc->scope = scope;
}

void
idl_exit_scope(
  idl_processor_t *proc)
{
  assert(proc->scope);
  assert(proc->scope != proc->global_scope);
  proc->scope = proc->scope->parent;
}
