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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "tree.h"
#include "expression.h"

static uint64_t uintmax(idl_type_t type);
static int64_t intmax(idl_type_t type);
static int64_t intmin(idl_type_t type);
static idl_intval_t intval(const idl_const_expr_t *expr);
static idl_floatval_t floatval(const idl_const_expr_t *expr);

idl_operator_t idl_operator(const void *node)
{
  idl_mask_t mask = idl_mask(node);

  mask &= ((((unsigned)IDL_BINARY_OPERATOR)<<1) - 1) |
          ((((unsigned)IDL_UNARY_OPERATOR)<<1) - 1);
  switch ((idl_operator_t)mask) {
    case IDL_MINUS:
    case IDL_PLUS:
    case IDL_NOT:
    case IDL_OR:
    case IDL_XOR:
    case IDL_AND:
    case IDL_LSHIFT:
    case IDL_RSHIFT:
    case IDL_ADD:
    case IDL_SUBTRACT:
    case IDL_MULTIPLY:
    case IDL_DIVIDE:
    case IDL_MODULO:
      return (idl_operator_t)mask;
    default:
      break;
  }

  return IDL_NOP;
}

static idl_retcode_t
eval_int_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp);

static unsigned max(unsigned a, unsigned b)
{
  return a > b ? a : b;
}

#define n(x) negative(x)
#define s(x) x->value.llng
#define t(x) x->type
#define u(x) x->value.ullng

static inline bool negative(idl_intval_t *a)
{
  switch (t(a)) {
    case IDL_LONG:
    case IDL_LLONG:
      return s(a) < 0;
    default:
      return false;
  }
}

static bool int_overflows(idl_intval_t *val, idl_type_t type)
{
  if (((unsigned)type & (unsigned)IDL_UNSIGNED))
    return val->value.ullng > uintmax(type);
  else
    return val->value.llng < intmin(type) || val->value.llng > intmax(type);
}

static idl_retcode_t
int_or(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = max(t(a), t(b));
  u(r) = u(a) | u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_xor(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = max(t(a), t(b));
  u(r) = u(a) ^ u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_and(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = max(t(a), t(b));
  u(r) = u(a) & u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_lshift(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));
  if (u(b) >= (uintmax(gt) == UINT64_MAX ? 64 : 32))
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  t(r) = gt;
  if (n(a))
    s(r) = s(a) << u(b);
  else
    u(r) = u(a) << u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_rshift(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));
  if (u(b) >= ((gt & IDL_LLONG) == IDL_LLONG ? 64 : 32))
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  t(r) = gt;
  if (n(a))
    s(r) = s(a) >> u(b);
  else
    u(r) = u(a) >> u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_add(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(a) > uintmax(gt) - u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) + u(b);
      t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      break;
    case 1:
      if (-u(a) < u(b)) {
        u(r) = u(a) + u(b);
        t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      } else {
        u(r) = u(a) + u(b);
        t(r) = gt;
      }
      break;
    case 2:
      if (-u(b) >= u(a)) {
        u(r) = u(a) + u(b);
        t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      } else {
        u(r) = u(a) + u(b);
        t(r) = gt;
      }
      break;
    case 3:
      if (s(b) < (intmin(gt) + -s(a)))
        return -1;
      s(r) = s(a) + s(b);
      t(r) = gt & ~1u;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_subtract(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(a) < u(b) && u(a) - u(b) > -(uint64_t)intmin(gt))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) - u(b);
      t(r) = gt & ~1u;
      break;
    case 1:
      if (u(b) > -(uint64_t)intmin(gt) || -u(a) > -(uint64_t)intmin(gt) - u(b))
        return -1;
      u(r) = u(a) - u(b);
      t(r) = gt & ~1u;
      break;
    case 2:
      if (-u(b) > uintmax(gt) - u(a))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) - u(b);
      t(r) = gt | (-u(b) > u(a));
      break;
    case 3:
      s(r) = s(a) - s(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_multiply(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(b) && u(a) > uintmax(gt) / u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt;
      break;
    case 1:
      if (u(b) > (uint64_t)(intmin(gt) / s(a)))
        return -1;
      u(r) = u(a) * u(b);
      t(r) = gt & ~1u;
      break;
    case 2:
      if (u(a) && u(a) > (uint64_t)(intmin(gt) / s(b)))
        return -1;
      u(r) = u(a) * u(b);
      t(r) = gt & ~1u;
      break;
    case 3:
      if (-u(a) > uintmax(gt) / -u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt | (u(r) > (uint32_t)intmax(gt));
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_divide(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = max(t(a), t(b));

  if (u(b) == 0)
    return IDL_RETCODE_ILLEGAL_EXPRESSION;

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 1:
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 2:
      if (-(uint64_t)intmin(gt) < u(a) && s(b) == -1)
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 3:
      s(r) = s(a) / s(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_modulo(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_mask_t gt = max(t(a), t(b));

  if (u(b) == 0)
    return IDL_RETCODE_ILLEGAL_EXPRESSION;

  switch ((n(a) ? 1:0) + n(b) ? 2:0) {
    case 0:
      u(r) = u(a) % u(b);
      t(r) = gt;
      break;
    case 1:
      u(r) = -(-u(a) % u(b));
      t(r) = gt;
      break;
    case 2:
      u(r) = u(a) % -u(b);
      t(r) = gt;
      break;
    case 3:
      u(r) = u(a) % u(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_binary_int_expr(
  idl_pstate_t *pstate,
  const idl_binary_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  idl_retcode_t ret;
  idl_intval_t val, lhs, rhs;

  assert((type & IDL_LONG) == IDL_LONG ||
         (type & IDL_LLONG) == IDL_ULLONG);

  if ((ret = eval_int_expr(pstate, expr->left, type, &lhs)))
    return ret;
  if ((ret = eval_int_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_OR:       ret = int_or(&lhs, &rhs, &val);       break;
    case IDL_XOR:      ret = int_xor(&lhs, &rhs, &val);      break;
    case IDL_AND:      ret = int_and(&lhs, &rhs, &val);      break;
    case IDL_LSHIFT:   ret = int_lshift(&lhs, &rhs, &val);   break;
    case IDL_RSHIFT:   ret = int_rshift(&lhs, &rhs, &val);   break;
    case IDL_ADD:      ret = int_add(&lhs, &rhs, &val);      break;
    case IDL_SUBTRACT: ret = int_subtract(&lhs, &rhs, &val); break;
    case IDL_MULTIPLY: ret = int_multiply(&lhs, &rhs, &val); break;
    case IDL_DIVIDE:   ret = int_divide(&lhs, &rhs, &val);   break;
    case IDL_MODULO:   ret = int_modulo(&lhs, &rhs, &val);   break;
    default:
      idl_error(pstate, idl_location(expr),
        "Cannot evaluate '%s' as integer expression", "<foobar>");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (ret == IDL_RETCODE_ILLEGAL_EXPRESSION) {
    idl_error(pstate, idl_location(expr), "Invalid integer expression");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  } else if (ret == IDL_RETCODE_OUT_OF_RANGE || int_overflows(&val, type)) {
    idl_error(pstate, idl_location(expr), "Integer expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static int int_minus(idl_intval_t *a, idl_intval_t *r)
{
  t(r) = t(a);
  u(r) = -u(a);
  return IDL_RETCODE_OK;
}

static int int_plus(idl_intval_t *a, idl_intval_t *r)
{
  *r = *a;
  return IDL_RETCODE_OK;
}

static int int_not(idl_intval_t *a, idl_intval_t *r)
{
  t(r) = t(a);
  u(r) = ~u(a);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_unary_int_expr(
  idl_pstate_t *pstate,
  const idl_unary_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  idl_retcode_t ret;
  idl_intval_t val, rhs;

  assert((type & IDL_LONG) == IDL_LONG ||
         (type & IDL_LLONG) == IDL_LLONG);

  if ((ret = eval_int_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_MINUS: ret = int_minus(&rhs, &val); break;
    case IDL_PLUS:  ret = int_plus(&rhs, &val);  break;
    case IDL_NOT:   ret = int_not(&rhs, &val);   break;
    default:
      idl_error(pstate, idl_location(expr),
        "Cannot evaluate '%s' as integer expression", "<foobar>");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (ret == IDL_RETCODE_ILLEGAL_EXPRESSION) {
    idl_error(pstate, idl_location(expr), "Invalid integer expression");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  } else if (ret == IDL_RETCODE_OUT_OF_RANGE || int_overflows(&val, type)) {
    idl_error(pstate, idl_location(expr), "Integer expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

#undef u
#undef s
#undef t
#undef n

static idl_retcode_t
eval_int_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  if (idl_is_masked(expr, IDL_LITERAL)) {
    if (idl_type(expr) == IDL_ULONG ||
        idl_type(expr) == IDL_ULLONG)
    {
      *valp = intval(expr);
      return IDL_RETCODE_OK;
    }
  } else if (idl_is_masked(expr, IDL_CONST|IDL_DECLARATION)) {
    idl_constval_t *constval = ((idl_const_t *)expr)->const_expr;
    if (idl_is_masked(constval, IDL_OCTET) ||
        idl_is_masked(constval, IDL_INTEGER_TYPE))
    {
      *valp = intval(constval);
      return IDL_RETCODE_OK;
    }
  } else if (idl_is_masked(expr, IDL_BINARY_OPERATOR)) {
    return eval_binary_int_expr(pstate, expr, type, valp);
  } else if (idl_is_masked(expr, IDL_UNARY_OPERATOR)) {
    return eval_unary_int_expr(pstate, expr, type, valp);
  }

  idl_error(pstate, idl_location(expr),
    "Cannot evaluate '%s' as integer expression", "<foobar>");
  return IDL_RETCODE_ILLEGAL_EXPRESSION;
}

static idl_retcode_t
eval_int(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_intval_t val;
  idl_constval_t constval;
  idl_type_t as = IDL_LONG;

  if (((unsigned)type & (unsigned)IDL_LLONG) == IDL_LLONG)
    as = IDL_LLONG;
  if ((ret = eval_int_expr(pstate, expr, as, &val)))
    return ret;
  if (int_overflows(&val, type))
    goto overflow;

  if ((ret = idl_create_constval(pstate, idl_location(expr), type, nodep)))
    return ret;

  switch (type) {
    case IDL_INT8:   constval.value.int8 = (int8_t)val.value.llng;      break;
    case IDL_OCTET:
    case IDL_UINT8:  constval.value.uint8 = (uint8_t)val.value.ullng;   break;
    case IDL_SHORT:
    case IDL_INT16:  constval.value.int16 = (int16_t)val.value.llng;    break;
    case IDL_USHORT:
    case IDL_UINT16: constval.value.uint16 = (uint16_t)val.value.ullng; break;
    case IDL_LONG:
    case IDL_INT32:  constval.value.int32 = (int32_t)val.value.llng;    break;
    case IDL_ULONG:
    case IDL_UINT32: constval.value.uint32 = (uint32_t)val.value.ullng; break;
    case IDL_LLONG:
    case IDL_INT64:  constval.value.int64 = (int64_t)val.value.llng;    break;
    case IDL_ULLONG:
    case IDL_UINT64: constval.value.uint64 = val.value.ullng;           break;
    default:
      break;
  }

  (*((idl_constval_t **)nodep))->value = constval.value;
  return IDL_RETCODE_OK;
overflow:
  idl_error(pstate, idl_location(expr), "Integer expression overflows");
  return IDL_RETCODE_OUT_OF_RANGE;
}

static idl_retcode_t
eval_float_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp);

static bool
float_overflows(long double ldbl, idl_type_t type)
{
  if (type == IDL_FLOAT)
    return isnan((float)ldbl) || isinf((float)ldbl);
  else if (type == IDL_DOUBLE)
    return isnan((double)ldbl) || isinf((double)ldbl);
  else if (type == IDL_LDOUBLE)
    return isnan(ldbl) || isinf(ldbl);
  abort();
}

static idl_retcode_t
eval_binary_float_expr(
  idl_pstate_t *pstate,
  const idl_binary_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  idl_retcode_t ret;
  idl_floatval_t val, lhs, rhs;

  if ((ret = eval_float_expr(pstate, expr->left, type, &lhs)))
    return ret;
  if ((ret = eval_float_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_ADD:      val = lhs+rhs; break;
    case IDL_SUBTRACT: val = lhs-rhs; break;
    case IDL_MULTIPLY: val = lhs*rhs; break;
    case IDL_DIVIDE:
      if (rhs == 0.0l) {
        idl_error(pstate, idl_location(expr),
          "Division by zero in floating point expression");
        return IDL_RETCODE_ILLEGAL_EXPRESSION;
      }
      val = lhs/rhs;
      break;
    default:
      idl_error(pstate, idl_location(expr),
        "Invalid floating point expression");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (float_overflows(val, type)) {
    idl_error(pstate, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_unary_float_expr(
  idl_pstate_t *pstate,
  const idl_unary_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  idl_retcode_t ret;
  idl_floatval_t val, rhs;

  if ((ret = eval_float_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_PLUS:  val = +rhs; break;
    case IDL_MINUS: val = -rhs; break;
    default:
      idl_error(pstate, idl_location(expr),
        "Invalid floating point expression");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (float_overflows(val, type)) {
    idl_error(pstate, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  if (idl_is_masked(expr, IDL_LITERAL)) {
    if (idl_type(expr) == IDL_DOUBLE ||
        idl_type(expr) == IDL_LDOUBLE)
    {
      *valp = floatval(expr);
      return IDL_RETCODE_OK;
    }
  } else if (idl_is_masked(expr, IDL_CONST|IDL_DECLARATION)) {
    idl_constval_t *constval = ((idl_const_t *)expr)->const_expr;
    if (idl_type(constval) == IDL_FLOAT ||
        idl_type(constval) == IDL_DOUBLE ||
        idl_type(constval) == IDL_LDOUBLE)
    {
      *valp = floatval(constval);
      return IDL_RETCODE_OK;
    }
  } else if (idl_is_masked(expr, IDL_BINARY_OPERATOR)) {
    return eval_binary_float_expr(pstate, expr, type, valp);
  } else if (idl_is_masked(expr, IDL_UNARY_OPERATOR)) {
    return eval_unary_float_expr(pstate, expr, type, valp);
  }

  idl_error(pstate, idl_location(expr),
    "Cannot evaluate '%s' as floating point expression", "<foobar>");
  return IDL_RETCODE_ILLEGAL_EXPRESSION;
}

static idl_retcode_t
eval_float(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_mask_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_floatval_t val;
  idl_constval_t *constval = NULL;
  idl_type_t as = (type == IDL_LDOUBLE) ? IDL_LDOUBLE : IDL_DOUBLE;

  if ((ret = eval_float_expr(pstate, expr, as, &val)))
    return ret;
  if (float_overflows(val, type))
    goto overflow;
  if ((ret = idl_create_constval(pstate, idl_location(expr), type, &constval)))
    return ret;

  switch (type) {
    case IDL_FLOAT:   constval->value.flt = (float)val;  break;
    case IDL_DOUBLE:  constval->value.dbl = (double)val; break;
    case IDL_LDOUBLE: constval->value.ldbl = val;        break;
    default:
      break;
  }

  *((idl_constval_t **)nodep) = constval;
  return IDL_RETCODE_OK;
overflow:
  idl_error(pstate, idl_location(expr),
    "Floating point expression overflows");
  return IDL_RETCODE_OUT_OF_RANGE;
}

idl_retcode_t
idl_evaluate(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_constval_t constval;
  static const char fmt[] = "Cannot evaluate '%s' as %s expression";

  /* enumerators are referenced */
  if (type == IDL_ENUM) {
    expr = idl_unalias(expr);
    if (!idl_is_masked(expr, IDL_ENUMERATOR)) {
      idl_error(pstate, idl_location(expr), fmt, "<foobar>", "enumerator");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
    *((idl_enumerator_t **)nodep) = expr;
    return IDL_RETCODE_OK;
  } else if (type == IDL_OCTET ||
             ((unsigned)type & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE)
  {
    if ((ret = eval_int(pstate, expr, type, nodep)))
      return ret;
    goto done;
  } else if (((unsigned)type & IDL_FLOATING_PT_TYPE) == IDL_FLOATING_PT_TYPE) {
    if ((ret = eval_float(pstate, expr, type, nodep)))
      return ret;
    goto done;
  }

  if (type == IDL_CHAR) {
    if (idl_type(expr) == IDL_CHAR && idl_is_literal(expr)) {
      constval.value.chr = ((idl_literal_t *)expr)->value.chr;
    } else if (idl_type(expr) == IDL_CHAR && idl_is_const(expr)) {
      idl_constval_t *val = ((idl_const_t *)expr)->const_expr;
      constval.value.chr = val->value.chr;
    } else {
      idl_error(pstate, idl_location(expr), fmt, "<foobar>", "character");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  } else if (type == IDL_BOOL) {
    if (idl_type(expr) == IDL_BOOL && idl_is_literal(expr)) {
      constval.value.bln = ((idl_literal_t *)expr)->value.bln;
    } else if (idl_type(expr) == IDL_BOOL || idl_is_const(expr)) {
      idl_constval_t *val = ((idl_const_t *)expr)->const_expr;
      constval.value.bln = val->value.bln;
    } else {
      idl_error(pstate, idl_location(expr), fmt, "<foobar>", "boolean");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  } else if (type == IDL_STRING) {
    if (idl_type(expr) == IDL_STRING && idl_is_literal(expr)) {
      if (!(constval.value.str = idl_strdup(((idl_literal_t *)expr)->value.str)))
        return IDL_RETCODE_OUT_OF_MEMORY;
    } else if (idl_type(expr) == IDL_STRING && idl_is_const(expr)) {
      idl_constval_t *val = ((idl_const_t *)expr)->const_expr;
      if (!(constval.value.str = idl_strdup(val->value.str)))
        return IDL_RETCODE_OUT_OF_MEMORY;
    } else {
      idl_error(pstate, idl_location(expr), fmt, "<foobar>", "string");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  }

  if ((ret = idl_create_constval(pstate, idl_location(expr), type, nodep)))
    return ret;
  (*((idl_constval_t **)nodep))->value = constval.value;
done:
  idl_unreference_node(expr);
  return IDL_RETCODE_OK;
}

static uint64_t uintmax(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return UINT8_MAX;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return UINT16_MAX;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return UINT32_MAX;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return UINT64_MAX;
    default:
      break;
  }

  return 0llu;
}

static int64_t intmax(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return INT8_MAX;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return INT16_MAX;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return INT32_MAX;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return INT64_MAX;
    default:
      break;
  }

  return 0ll;
}

static int64_t intmin(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return INT8_MIN;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return INT16_MIN;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return INT32_MIN;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return INT64_MIN;
    default:
      break;
  }

  return 0ll;
}

static idl_intval_t intval(const idl_const_expr_t *expr)
{
  idl_mask_t mask = idl_mask(expr);
  idl_type_t type = idl_type(expr);

#define SIGNED(t,v) (idl_intval_t){ .type = (t), .value = { .llng = (v) } }
#define UNSIGNED(t,v) (idl_intval_t){ .type = (t), .value = { .ullng = (v) } }

  if (mask & IDL_CONST) {
    const idl_constval_t *rhs = (idl_constval_t *)expr;
    assert(type == IDL_OCTET || (type & IDL_INTEGER_TYPE));

    switch (type) {
      case IDL_INT8:   return SIGNED(IDL_LONG, rhs->value.int8);
      case IDL_UINT8:
      case IDL_OCTET:  return UNSIGNED(IDL_ULONG, rhs->value.uint8);
      case IDL_INT16:
      case IDL_SHORT:  return SIGNED(IDL_LONG, rhs->value.int16);
      case IDL_UINT16:
      case IDL_USHORT: return UNSIGNED(IDL_ULONG, rhs->value.uint16);
      case IDL_INT32:
      case IDL_LONG:   return SIGNED(IDL_LONG, rhs->value.int32);
      case IDL_UINT32:
      case IDL_ULONG:  return UNSIGNED(IDL_ULONG, rhs->value.uint32);
      case IDL_INT64:
      case IDL_LLONG:  return SIGNED(IDL_LLONG, rhs->value.int64);
      case IDL_UINT64:
      case IDL_ULLONG: return UNSIGNED(IDL_ULLONG, rhs->value.uint64);
      default:
        break;
    }
  } else {
    const idl_literal_t *rhs = (idl_literal_t *)expr;
    assert(mask & IDL_LITERAL);
    assert(type == IDL_ULONG || type == IDL_ULLONG);

    switch (type) {
      case IDL_ULONG:
        if (rhs->value.ulng > INT32_MAX)
          return UNSIGNED(IDL_ULONG, rhs->value.ulng);
        return SIGNED(IDL_LONG, (int32_t)rhs->value.ulng);
      case IDL_ULLONG:
        if (rhs->value.ullng > INT64_MAX)
          return UNSIGNED(IDL_ULLONG, rhs->value.ullng);
        return SIGNED(IDL_LLONG, (int64_t)rhs->value.ullng);
      default:
        break;
    }
  }

#undef UNSIGNED
#undef SIGNED

  return (idl_intval_t){ .type = IDL_NULL, .value = { .ullng = 0llu } };
}

static idl_floatval_t floatval(const idl_const_expr_t *expr)
{
  idl_mask_t mask = idl_mask(expr);
  idl_type_t type = idl_type(expr);
  assert((unsigned)type & IDL_FLOATING_PT_TYPE);

  if (mask & IDL_CONST) {
    const idl_constval_t *rval = (idl_constval_t *)expr;
    assert(type & IDL_FLOATING_PT_TYPE);

    switch (type) {
      case IDL_FLOAT:   return (idl_floatval_t)rval->value.flt;
      case IDL_DOUBLE:  return (idl_floatval_t)rval->value.dbl;
      case IDL_LDOUBLE: return rval->value.ldbl;
      default:
        break;
    }
  } else {
    const idl_literal_t *rval = (idl_literal_t *)expr;
    assert(mask & IDL_LITERAL);
    assert(type == IDL_DOUBLE || type == IDL_LDOUBLE);

    switch (type) {
      case IDL_DOUBLE:  return (idl_floatval_t)rval->value.ldbl;
      case IDL_LDOUBLE: return (idl_floatval_t)rval->value.ldbl;
      default:
        break;
    }
  }

  return 0.0l;
}

int
idl_compare(idl_pstate_t *pstate, const void *left, const void *right)
{
  idl_type_t ltype, rtype;

  assert(pstate);
  (void)pstate;

  ltype = idl_type(left);
  rtype = idl_type(right);

  if (((unsigned)ltype & (unsigned)IDL_INTEGER_TYPE) &&
      ((unsigned)rtype & (unsigned)IDL_INTEGER_TYPE))
  {
    idl_intval_t lval, rval;
    lval = intval(left);
    rval = intval(right);

    switch ((negative(&lval) ? 1:0) + (negative(&rval) ? 2:0)) {
      case 0:
        return (lval.value.ullng < rval.value.ullng)
          ? -1 : (lval.value.ullng > rval.value.ullng);
      case 1:
        return 1;
      case 2:
        return -1;
      case 3:
        return (lval.value.llng < rval.value.llng)
          ? -1 : (lval.value.llng > rval.value.llng);
        break;
      default:
        return 0;
    }
  } else if (((unsigned)ltype & (unsigned)IDL_FLOATING_PT_TYPE) &&
             ((unsigned)rtype & (unsigned)IDL_FLOATING_PT_TYPE))
  {
    idl_floatval_t lval, rval;
    lval = floatval(left);
    rval = floatval(right);

    return (lval < rval) ? -1 : (lval > rval);
  } else if (ltype != rtype) {
    return -2; /* incompatible types */
  } else if (ltype == IDL_ENUM) {
    const idl_enumerator_t *lval, *rval;
    lval = left;
    rval = right;
    assert(idl_is_masked(lval, IDL_ENUMERATOR));
    assert(idl_is_masked(rval, IDL_ENUMERATOR));
    if (lval->node.parent != rval->node.parent)
      return -2; /* incompatible enums */
    return (lval->value < rval->value) ? -1 : (lval->value > rval->value);
  } else if (ltype == IDL_STRING) {
    int cmp;
    const char *lval, *rval;
    lval = ((idl_constval_t *)left)->value.str;
    rval = ((idl_constval_t *)right)->value.str;
    if (!lval || !rval)
      return (lval ? 1 : (rval ? -1 : 0));
    cmp = strcmp(lval, rval);
    return (cmp < 0 ? -1 : (cmp > 0 ? 1 : 0));
  }

  return -3; /* non-type or non-comparable type */
}
