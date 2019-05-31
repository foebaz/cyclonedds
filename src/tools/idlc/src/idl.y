/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
%{
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* yyscan_t is an opaque pointer, a typedef is required here to break a
   circular dependency introduced with bison 2.6 (normally a typedef is
   generated by flex). the define is required to disable the typedef in flex
   generated code */
#define YY_TYPEDEF_YY_SCANNER_T

typedef void *yyscan_t;

#include "idl.parser.h"
#include "yy_decl.h" /* prevent implicit declaration of yylex */

#define YYFPRINTF (unsigned int)fprintf

#define YYPRINT(A,B,C) YYUSE(A) /* to make yytoknum available */

int
yyerror(
  YYLTYPE *yylloc, yyscan_t yyscanner, ddsts_context_t *context, char *text);

int illegal_identifier(const char *token);

%}

%code requires {

#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsts/typetree.h"
#include "tt_create.h"

}


%union {
  ddsts_flags_t base_type_flags;
  ddsts_type_t *type_ptr;
  ddsts_literal_t literal;
  ddsts_identifier_t identifier;
  ddsts_scoped_name_t *scoped_name;
}

%define api.pure full
%define api.prefix {ddsts_parser_}
%define parse.trace

%locations
%param {yyscan_t scanner}
%param {ddsts_context_t *context}

%token-table

%start specification

%token <identifier>
  DDSTS_IDENTIFIER_TOKEN
  DDSTS_NOWS_IDENTIFIER_TOKEN

%token <literal>
  DDSTS_INTEGER_LITERAL_TOKEN

%type <base_type_flags>
  base_type_spec
  floating_pt_type
  integer_type
  signed_int
  signed_tiny_int
  signed_short_int
  signed_long_int
  signed_longlong_int
  unsigned_int
  unsigned_tiny_int
  unsigned_short_int
  unsigned_long_int
  unsigned_longlong_int
  char_type
  wide_char_type
  boolean_type
  octet_type

%type <type_ptr>
  type_spec
  simple_type_spec
  template_type_spec
  sequence_type
  string_type
  wide_string_type
  fixed_pt_type
  map_type
  struct_type
  struct_def

%destructor { ddsts_free_type($$); } <type_ptr>

%type <scoped_name>
  scoped_name
  nows_scoped_name

%destructor { ddsts_free_scoped_name($$); } <scoped_name>

%type <literal>
  positive_int_const
  literal
  const_expr

%destructor { ddsts_free_literal(&($$)); } <literal>


%type <identifier>
  simple_declarator
  identifier
  nows_identifier

/* keywords */
%token DDSTS_MODULE_TOKEN "module"
%token DDSTS_CONST_TOKEN "const"
%token DDSTS_NATIVE_TOKEN "native"
%token DDSTS_STRUCT_TOKEN "struct"
%token DDSTS_TYPEDEF_TOKEN "typedef"
%token DDSTS_UNION_TOKEN "union"
%token DDSTS_SWITCH_TOKEN "switch"
%token DDSTS_CASE_TOKEN "case"
%token DDSTS_DEFAULT_TOKEN "default"
%token DDSTS_ENUM_TOKEN "enum"
%token DDSTS_UNSIGNED_TOKEN "unsigned"
%token DDSTS_FIXED_TOKEN "fixed"
%token DDSTS_SEQUENCE_TOKEN "sequence"
%token DDSTS_STRING_TOKEN "string"
%token DDSTS_WSTRING_TOKEN "wstring"

%token DDSTS_FLOAT_TOKEN "float"
%token DDSTS_DOUBLE_TOKEN "double"
%token DDSTS_SHORT_TOKEN "short"
%token DDSTS_LONG_TOKEN "long"
%token DDSTS_CHAR_TOKEN "char"
%token DDSTS_WCHAR_TOKEN "wchar"
%token DDSTS_BOOLEAN_TOKEN "boolean"
%token DDSTS_OCTET_TOKEN "octet"
%token DDSTS_ANY_TOKEN "any"

%token DDSTS_MAP_TOKEN "map"
%token DDSTS_BITSET_TOKEN "bitset"
%token DDSTS_BITFIELD_TOKEN "bitfield"
%token DDSTS_BITMASK_TOKEN "bitmask"

%token DDSTS_INT8_TOKEN "int8"
%token DDSTS_UINT8_TOKEN "uint8"
%token DDSTS_INT16_TOKEN "int16"
%token DDSTS_INT32_TOKEN "int32"
%token DDSTS_INT64_TOKEN "int64"
%token DDSTS_UINT16_TOKEN "uint16"
%token DDSTS_UINT32_TOKEN "uint32"
%token DDSTS_UINT64_TOKEN "uint64"

%token DDSTS_COLON_COLON_TOKEN
%token DDSTS_NOWS_COLON_COLON_TOKEN
%token DDSTS_AT_TOKEN "@"

%token DDSTS_PRAGMA_TOKEN "#pragma"
%token DDSTS_END_DIRECTIVE_TOKEN

%token DDSTS_END_TOKEN 0 "end of file"


%%


/* Constant Declaration */

specification:
    definitions DDSTS_END_TOKEN
    { ddsts_accept(context); YYACCEPT; }
  ;

definitions:
    definition definitions
  | definition
  ;

definition:
    module_dcl ';'
  | type_dcl ';'
  | pragma
  ;

module_dcl:
    "module" identifier
      {
        if (!ddsts_module_open(context, $2)) {
          YYABORT;
        }
      }
    '{' definitions '}'
      { ddsts_module_close(context); };

scoped_name:
    identifier
      {
        if (!ddsts_new_scoped_name(context, 0, false, $1, &($$))) {
          YYABORT;
        }
      }
  | DDSTS_COLON_COLON_TOKEN nows_identifier
      {
        if (!ddsts_new_scoped_name(context, 0, true, $2, &($$))) {
          YYABORT;
        }
      }
  | scoped_name DDSTS_NOWS_COLON_COLON_TOKEN nows_identifier
      {
        if (!ddsts_new_scoped_name(context, $1, false, $3, &($$))) {
          YYABORT;
        }
      }
  ;

nows_scoped_name:
    nows_identifier
      {
        if (!ddsts_new_scoped_name(context, 0, false, $1, &($$))) {
          YYABORT;
        }
      }
  | DDSTS_NOWS_COLON_COLON_TOKEN nows_identifier
      {
        if (!ddsts_new_scoped_name(context, 0, true, $2, &($$))) {
          YYABORT;
        }
      }
  | nows_scoped_name DDSTS_NOWS_COLON_COLON_TOKEN nows_identifier
      {
        if (!ddsts_new_scoped_name(context, $1, false, $3, &($$))) {
          YYABORT;
        }
      }
  ;

const_expr:
    literal
  | '(' const_expr ')'
      { $$ = $2; };

literal:
    DDSTS_INTEGER_LITERAL_TOKEN
  ;

positive_int_const:
    const_expr;

type_dcl:
    constr_type_dcl
  ;

type_spec:
    simple_type_spec
  ;

simple_type_spec:
    base_type_spec
      {
        if (!ddsts_new_base_type(context, $1, &($$))) {
          YYABORT;
        }
      }
  | scoped_name
      {
        if (!ddsts_get_type_from_scoped_name(context, $1, &($$))) {
          YYABORT;
        }
      }
  ;

base_type_spec:
    integer_type
  | floating_pt_type
  | char_type
  | wide_char_type
  | boolean_type
  | octet_type
  ;

/* Basic Types */
floating_pt_type:
    "float" { $$ = DDSTS_FLOAT; }
  | "double" { $$ = DDSTS_DOUBLE; }
  | "long" "double" { $$ = DDSTS_LONGDOUBLE; };

integer_type:
    signed_int
  | unsigned_int
  ;

signed_int:
    "short" { $$ = DDSTS_SHORT; }
  | "long" { $$ = DDSTS_LONG; }
  | "long" "long" { $$ = DDSTS_LONGLONG; }
  ;

unsigned_int:
    "unsigned" "short" { $$ = DDSTS_USHORT; }
  | "unsigned" "long" { $$ = DDSTS_ULONG; }
  | "unsigned" "long" "long" { $$ = DDSTS_ULONGLONG; }
  ;

char_type:
    "char" { $$ = DDSTS_CHAR; };

wide_char_type:
    "wchar" { $$ = DDSTS_WIDE_CHAR; };

boolean_type:
    "boolean" { $$ = DDSTS_BOOLEAN; };

octet_type:
    "octet" { $$ = DDSTS_OCTET; };

template_type_spec:
    sequence_type
  | string_type
  | wide_string_type
  | fixed_pt_type
  | struct_type
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_sequence(context, $3, &($5), &($$))) {
          YYABORT;
        }
      }
  | "sequence" '<' type_spec '>'
      {
        if (!ddsts_new_sequence_unbound(context, $3, &($$))) {
          YYABORT;
        }
      }
  ;

string_type:
    "string" '<' positive_int_const '>'
      {
        if (!ddsts_new_string(context, &($3), &($$))) {
          YYABORT;
        }
      }
  | "string"
      {
        if (!ddsts_new_string_unbound(context, &($$))) {
          YYABORT;
        }
      }
  ;

wide_string_type:
    "wstring" '<' positive_int_const '>'
      {
        if (!ddsts_new_wide_string(context, &($3), &($$))) {
          YYABORT;
        }
      }
  | "wstring"
      {
        if (!ddsts_new_wide_string_unbound(context, &($$))) {
          YYABORT;
        }
      }
  ;

fixed_pt_type:
    "fixed" '<' positive_int_const ',' positive_int_const '>'
      {
        if (!ddsts_new_fixed_pt(context, &($3), &($5), &($$))) {
          YYABORT;
        }
      }
  ;

/* Annonimous struct extension: */
struct_type:
    "struct" '{'
      {
        if (!ddsts_add_struct_open(context, NULL)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(context, &($$)); }
  ;

constr_type_dcl:
    struct_dcl
  ;

struct_dcl:
    struct_def
  | struct_forward_dcl
  ;

struct_def:
    "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(context, $2)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(context, &($$)); }
  ;
members:
    member members
  | member
  ;

member:
    annotation_appls type_spec
      {
        if (!ddsts_add_struct_member(context, &($2))) {
          YYABORT;
        }
      }
    declarators ';'
      { ddsts_struct_member_close(context); }
  | type_spec
      {
        if (!ddsts_add_struct_member(context, &($1))) {
          YYABORT;
        }
      }
    declarators ';'
      { ddsts_struct_member_close(context); }
/* Embedded struct extension: */
  | struct_def { ddsts_add_struct_member(context, &($1)); }
    declarators ';'

  ;

struct_forward_dcl:
    "struct" identifier
      {
        if (!ddsts_add_struct_forward(context, $2)) {
          YYABORT;
        }
      };

array_declarator:
    identifier fixed_array_sizes
      {
        if (!ddsts_add_declarator(context, $1)) {
          YYABORT;
        }
      }
  ;

fixed_array_sizes:
    fixed_array_size fixed_array_sizes
  | fixed_array_size
  ;

fixed_array_size:
    '[' positive_int_const ']'
      {
        if (!ddsts_add_array_size(context, &($2))) {
          YYABORT;
        }
      }
  ;

simple_declarator: identifier ;

declarators:
    declarator ',' declarators
  | declarator
  ;

declarator:
    simple_declarator
      {
        if (!ddsts_add_declarator(context, $1)) {
          YYABORT;
        }
      };


/* From Building Block Extended Data-Types: */
struct_def:
    "struct" identifier ':' scoped_name '{'
      {
        if (!ddsts_add_struct_extension_open(context, $2, $4)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(context, &($$)); }
  | "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(context, $2)) {
          YYABORT;
        }
      }
    '}'
      { ddsts_struct_empty_close(context, &($$)); }
  ;

template_type_spec:
     map_type
  ;

map_type:
    "map" '<' type_spec ',' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_map(context, $3, $5, &($7), &($$))) {
          YYABORT;
        }
      }
  | "map" '<' type_spec ',' type_spec '>'
      {
        if (!ddsts_new_map_unbound(context, $3, $5, &($$))) {
          YYABORT;
        }
      }
  ;

signed_int:
    signed_tiny_int
  | signed_short_int
  | signed_long_int
  | signed_longlong_int
  ;

unsigned_int:
    unsigned_tiny_int
  | unsigned_short_int
  | unsigned_long_int
  | unsigned_longlong_int
  ;

signed_tiny_int: "int8" { $$ = DDSTS_INT8; };
unsigned_tiny_int: "uint8" { $$ = DDSTS_UINT8; };
signed_short_int: "int16" { $$ = DDSTS_SHORT; };
signed_long_int: "int32" { $$ = DDSTS_LONG; };
signed_longlong_int: "int64" { $$ = DDSTS_LONGLONG; };
unsigned_short_int: "uint16" { $$ = DDSTS_USHORT; };
unsigned_long_int: "uint32" { $$ = DDSTS_ULONG; };
unsigned_longlong_int: "uint64" { $$ = DDSTS_ULONGLONG; };

/* From Building Block Anonymous Types: */
type_spec: template_type_spec ;
declarator: array_declarator ;


/* From Building Block Annotations (minimal for support of @key): */

annotation_appls:
    annotation_appl annotation_appls
  | annotation_appl
  ;

annotation_appl:
    "@" nows_scoped_name
    {
      if (!ddsts_add_annotation(context, $2)) {
        YYABORT;
      }
    }
  ;

/* Backward compatability #pragma */

pragma:
    "#pragma"
      { ddsts_pragma_open(context); }
    pragma_arg_list DDSTS_END_DIRECTIVE_TOKEN
      {
        if (!ddsts_pragma_close(context)) {
          YYABORT;
        }
      }
  ;

pragma_arg_list:
    identifier
      {
        if (!ddsts_pragma_add_identifier(context, $1)) {
          YYABORT;
        }
      }
    pragma_arg_list
  |
  ;

identifier:
    DDSTS_IDENTIFIER_TOKEN
      {
        size_t offset = 0;
        if ($1[0] == '_') {
          offset = 1;
        }
        else if (illegal_identifier($1) != 0) {
          yyerror(&yylloc, scanner, context, "Identifier collides with a keyword");
        }
        if (!ddsts_context_copy_identifier(context, $1 + offset, &($$))) {
          YYABORT;
        }
      };

nows_identifier:
    DDSTS_NOWS_IDENTIFIER_TOKEN
      {
        size_t offset = 0;
        if ($1[0] == '_') {
          offset = 1;
        }
        else if (illegal_identifier($1) != 0) {
          yyerror(&yylloc, scanner, context, "Identifier collides with a keyword");
        }
        if (!ddsts_context_copy_identifier(context, $1 + offset, &($$))) {
          YYABORT;
        }
      };


%%

int
yyerror(
  YYLTYPE *yylloc, yyscan_t yyscanner, ddsts_context_t *context, char *text)
{
  DDSRT_UNUSED_ARG(yyscanner);
  DDSRT_UNUSED_ARG(context);
  DDS_ERROR("%d.%d: %s\n", yylloc->first_line, yylloc->first_column, text);
  return 0;
}

int
illegal_identifier(const char *token)
{
  size_t i, n;

  assert(token != NULL);

  for (i = 0, n = strlen(token); i < YYNTOKENS; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && ddsrt_strncasecmp(yytname[i] + 1, token, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == '\0') {
      return 1;
    }
  }

  return 0;
}

int
ddsts_parser_token_matches_keyword(const char *token, int *token_number)
{
  size_t i, n;

  assert(token != NULL);
  assert(token_number != NULL);

  for (i = 0, n = strlen(token); i < YYNTOKENS; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && strncmp(yytname[i] + 1, token, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == 0) {
      *token_number = yytoknum[i];
      return 1;
    }
  }

  return 0;
}

