/*
   +----------------------------------------------------------------------+
   | Zend Engine - UGL vector types                                       |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.zend.com/license/2_00.txt.                               |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_API.h"
#include "zend_exceptions.h"
#include "zend_ugl_vec.h"

#ifdef ZEND_UGL

#include <math.h>

/* vec2 and vec3: final, immutable value objects with float32 components.
 * Arithmetic is done in single precision (+ - * /, component-wise, scalars
 * broadcast), so results match what a GPU computes. */

typedef struct _zend_ugl_vec {
	float       c[3];
	uint8_t     dim;
	bool        initialized;
	zend_object std;
} zend_ugl_vec;

ZEND_API zend_class_entry *zend_ce_ugl_vec2;
ZEND_API zend_class_entry *zend_ce_ugl_vec3;
static zend_object_handlers ugl_vec_handlers;

static zend_always_inline zend_ugl_vec *ugl_vec_from_obj(zend_object *obj)
{
	return (zend_ugl_vec *) ((char *) obj - XtOffsetOf(zend_ugl_vec, std));
}

static zend_always_inline bool ugl_is_vec(const zval *zv)
{
	return Z_TYPE_P(zv) == IS_OBJECT && Z_OBJ_P(zv)->handlers == &ugl_vec_handlers;
}

static zend_object *ugl_vec_create_ex(zend_class_entry *ce, uint8_t dim)
{
	zend_ugl_vec *v = zend_object_alloc(sizeof(zend_ugl_vec), ce);
	v->dim = dim;
	zend_object_std_init(&v->std, ce);
	v->std.handlers = &ugl_vec_handlers;
	return &v->std;
}

static zend_object *ugl_vec2_create(zend_class_entry *ce) { return ugl_vec_create_ex(ce, 2); }
static zend_object *ugl_vec3_create(zend_class_entry *ce) { return ugl_vec_create_ex(ce, 3); }

static void ugl_vec_new(zval *result, uint8_t dim, const float *c)
{
	zend_object *obj = ugl_vec_create_ex(dim == 2 ? zend_ce_ugl_vec2 : zend_ce_ugl_vec3, dim);
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	for (uint8_t i = 0; i < dim; i++) {
		v->c[i] = c[i];
	}
	v->initialized = true;
	ZVAL_OBJ(result, obj);
}

/* ---- operators ---- */

static zend_result ugl_do_operation_ex(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	if (opcode != ZEND_ADD && opcode != ZEND_SUB && opcode != ZEND_MUL && opcode != ZEND_DIV) {
		return FAILURE;
	}

	zend_ugl_vec *a = NULL, *b = NULL;
	float sa = 0, sb = 0;

	if (ugl_is_vec(op1)) {
		a = ugl_vec_from_obj(Z_OBJ_P(op1));
	} else if (Z_TYPE_P(op1) == IS_DOUBLE) {
		sa = (float) Z_DVAL_P(op1);
	} else if (Z_TYPE_P(op1) == IS_LONG) {
		sa = (float) Z_LVAL_P(op1);
	} else {
		return FAILURE;
	}
	if (ugl_is_vec(op2)) {
		b = ugl_vec_from_obj(Z_OBJ_P(op2));
	} else if (Z_TYPE_P(op2) == IS_DOUBLE) {
		sb = (float) Z_DVAL_P(op2);
	} else if (Z_TYPE_P(op2) == IS_LONG) {
		sb = (float) Z_LVAL_P(op2);
	} else {
		return FAILURE;
	}
	if (a && b && a->dim != b->dim) {
		return FAILURE;
	}

	uint8_t dim = a ? a->dim : b->dim;
	float r[3];
	for (uint8_t i = 0; i < dim; i++) {
		float x = a ? a->c[i] : sa;
		float y = b ? b->c[i] : sb;
		switch (opcode) {
			case ZEND_ADD: r[i] = x + y; break;
			case ZEND_SUB: r[i] = x - y; break;
			case ZEND_MUL: r[i] = x * y; break;
			default:       r[i] = x / y; break;
		}
	}
	ugl_vec_new(result, dim, r);
	return SUCCESS;
}

static zend_result ugl_do_operation(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	zval op1_copy;
	zend_result status;

	if (result == op1) {
		ZVAL_COPY_VALUE(&op1_copy, op1);
		op1 = &op1_copy;
	}
	status = ugl_do_operation_ex(opcode, result, op1, op2);
	if (status == SUCCESS && op1 == &op1_copy) {
		zval_ptr_dtor(op1);
	}
	return status;
}

static int ugl_compare(zval *o1, zval *o2)
{
	ZEND_COMPARE_OBJECTS_FALLBACK(o1, o2);

	zend_ugl_vec *a = ugl_vec_from_obj(Z_OBJ_P(o1));
	zend_ugl_vec *b = ugl_vec_from_obj(Z_OBJ_P(o2));
	if (a->dim != b->dim) {
		return ZEND_UNCOMPARABLE;
	}
	for (uint8_t i = 0; i < a->dim; i++) {
		if (a->c[i] != b->c[i]) {
			return ZEND_UNCOMPARABLE;
		}
	}
	return 0;
}

/* ---- properties and swizzling ---- */

/* Maps a swizzle like "x", "yx" or "zzz" to component indexes. Returns the length,
 * or 0 if the name is not a valid swizzle for this vector. */
static zend_always_inline uint8_t ugl_parse_swizzle(const zend_ugl_vec *v, const zend_string *name, uint8_t *idx)
{
	size_t len = ZSTR_LEN(name);
	if (len == 0 || len > 3) {
		return 0;
	}
	for (size_t i = 0; i < len; i++) {
		char ch = ZSTR_VAL(name)[i];
		if (ch < 'x' || ch > 'z' || (uint8_t) (ch - 'x') >= v->dim) {
			return 0;
		}
		idx[i] = (uint8_t) (ch - 'x');
	}
	return (uint8_t) len;
}

static zval *ugl_read_property(zend_object *obj, zend_string *name, int type, void **cache_slot, zval *rv)
{
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	uint8_t idx[3];
	uint8_t n = ugl_parse_swizzle(v, name, idx);

	if (n == 0) {
		return zend_std_read_property(obj, name, type, cache_slot, rv);
	}
	if (n == 1) {
		ZVAL_DOUBLE(rv, v->c[idx[0]]);
		return rv;
	}
	float r[3];
	for (uint8_t i = 0; i < n; i++) {
		r[i] = v->c[idx[i]];
	}
	ugl_vec_new(rv, n, r);
	return rv;
}

static zval *ugl_write_property(zend_object *obj, zend_string *name, zval *value, void **cache_slot)
{
	zend_throw_error(NULL, "Cannot modify %s: vectors are immutable", ZSTR_VAL(obj->ce->name));
	return &EG(error_zval);
}

static void ugl_unset_property(zend_object *obj, zend_string *name, void **cache_slot)
{
	zend_throw_error(NULL, "Cannot unset property of %s: vectors are immutable", ZSTR_VAL(obj->ce->name));
}

static zval *ugl_get_property_ptr_ptr(zend_object *obj, zend_string *name, int type, void **cache_slot)
{
	return NULL;
}

static int ugl_has_property(zend_object *obj, zend_string *name, int has_set_exists, void **cache_slot)
{
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	uint8_t idx[3];
	uint8_t n = ugl_parse_swizzle(v, name, idx);

	if (n == 0) {
		return zend_std_has_property(obj, name, has_set_exists, cache_slot);
	}
	if (has_set_exists == ZEND_PROPERTY_NOT_EMPTY) {
		for (uint8_t i = 0; i < n; i++) {
			if (v->c[idx[i]] != 0.0f) {
				return 1;
			}
		}
		return 0;
	}
	return 1;
}

static zend_array *ugl_get_properties_for(zend_object *obj, zend_prop_purpose purpose)
{
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	static const char *names[3] = {"x", "y", "z"};
	zend_array *ht = zend_new_array(v->dim);
	for (uint8_t i = 0; i < v->dim; i++) {
		zval tmp;
		ZVAL_DOUBLE(&tmp, v->c[i]);
		zend_hash_str_add_new(ht, names[i], 1, &tmp);
	}
	return ht;
}

/* ---- methods ---- */

static void ugl_construct(INTERNAL_FUNCTION_PARAMETERS)
{
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));
	double d[3] = {0.0, 0.0, 0.0};
	uint32_t max_args = v->dim;

	ZEND_PARSE_PARAMETERS_START(0, max_args)
		Z_PARAM_OPTIONAL
		Z_PARAM_DOUBLE(d[0])
		Z_PARAM_DOUBLE(d[1])
		Z_PARAM_DOUBLE(d[2])
	ZEND_PARSE_PARAMETERS_END();

	if (v->initialized) {
		zend_throw_error(NULL, "Cannot modify %s: vectors are immutable", ZSTR_VAL(v->std.ce->name));
		RETURN_THROWS();
	}
	for (uint8_t i = 0; i < v->dim; i++) {
		v->c[i] = (float) d[i];
	}
	v->initialized = true;
}

static void ugl_function_ctor(INTERNAL_FUNCTION_PARAMETERS, uint8_t dim)
{
	double d[3] = {0.0, 0.0, 0.0};
	uint32_t max_args = dim;

	ZEND_PARSE_PARAMETERS_START(0, max_args)
		Z_PARAM_OPTIONAL
		Z_PARAM_DOUBLE(d[0])
		Z_PARAM_DOUBLE(d[1])
		Z_PARAM_DOUBLE(d[2])
	ZEND_PARSE_PARAMETERS_END();

	float c[3] = {(float) d[0], (float) d[1], (float) d[2]};
	ugl_vec_new(return_value, dim, c);
}

ZEND_FUNCTION(vec2) { ugl_function_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, 2); }
ZEND_FUNCTION(vec3) { ugl_function_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, 3); }

static ZEND_NAMED_FUNCTION(ugl_vec_construct)
{
	ugl_construct(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static ZEND_NAMED_FUNCTION(ugl_vec_dot)
{
	zend_object *other;
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(other, v->std.ce)
	ZEND_PARSE_PARAMETERS_END();

	zend_ugl_vec *o = ugl_vec_from_obj(other);
	float sum = 0;
	for (uint8_t i = 0; i < v->dim; i++) {
		sum += v->c[i] * o->c[i];
	}
	RETURN_DOUBLE(sum);
}

static ZEND_NAMED_FUNCTION(ugl_vec_length)
{
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));
	ZEND_PARSE_PARAMETERS_NONE();

	float sum = 0;
	for (uint8_t i = 0; i < v->dim; i++) {
		sum += v->c[i] * v->c[i];
	}
	RETURN_DOUBLE(sqrtf(sum));
}

static ZEND_NAMED_FUNCTION(ugl_vec_normalize)
{
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));
	ZEND_PARSE_PARAMETERS_NONE();

	float sum = 0;
	for (uint8_t i = 0; i < v->dim; i++) {
		sum += v->c[i] * v->c[i];
	}
	float len = sqrtf(sum), r[3];
	for (uint8_t i = 0; i < v->dim; i++) {
		r[i] = len == 0.0f ? 0.0f : v->c[i] / len;
	}
	ugl_vec_new(return_value, v->dim, r);
}

static ZEND_NAMED_FUNCTION(ugl_vec_cross)
{
	zend_object *other;
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(other, zend_ce_ugl_vec3)
	ZEND_PARSE_PARAMETERS_END();

	zend_ugl_vec *o = ugl_vec_from_obj(other);
	float r[3] = {
		v->c[1] * o->c[2] - v->c[2] * o->c[1],
		v->c[2] * o->c[0] - v->c[0] * o->c[2],
		v->c[0] * o->c[1] - v->c[1] * o->c[0],
	};
	ugl_vec_new(return_value, 3, r);
}

/* ---- registration ---- */

ZEND_BEGIN_ARG_INFO_EX(arginfo_vec2_ctor, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vec3_ctor, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, z, IS_DOUBLE, 0, "0.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_fn_vec2, 0, 0, vec2, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_fn_vec3, 0, 0, vec3, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, z, IS_DOUBLE, 0, "0.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec2_dot, 0, 1, IS_DOUBLE, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec2, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec3_dot, 0, 1, IS_DOUBLE, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec3, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec_length, 0, 0, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec2_normalize, 0, 0, vec2, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec3_normalize, 0, 0, vec3, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec3_cross, 0, 1, vec3, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec3, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry ugl_functions[] = {
	ZEND_FE(vec2, arginfo_fn_vec2)
	ZEND_FE(vec3, arginfo_fn_vec3)
	ZEND_FE_END
};

static const zend_function_entry ugl_vec2_methods[] = {
	ZEND_NAMED_ME(__construct, ugl_vec_construct, arginfo_vec2_ctor, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(dot, ugl_vec_dot, arginfo_vec2_dot, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(length, ugl_vec_length, arginfo_vec_length, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(normalize, ugl_vec_normalize, arginfo_vec2_normalize, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry ugl_vec3_methods[] = {
	ZEND_NAMED_ME(__construct, ugl_vec_construct, arginfo_vec3_ctor, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(dot, ugl_vec_dot, arginfo_vec3_dot, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(cross, ugl_vec_cross, arginfo_vec3_cross, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(length, ugl_vec_length, arginfo_vec_length, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(normalize, ugl_vec_normalize, arginfo_vec3_normalize, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

ZEND_API bool zend_is_ugl_class_name(const zend_string *name)
{
	return ZSTR_LEN(name) == 4
		&& (zend_binary_strcasecmp(ZSTR_VAL(name), 4, "vec2", 4) == 0
		 || zend_binary_strcasecmp(ZSTR_VAL(name), 4, "vec3", 4) == 0);
}

void zend_register_ugl_vec(void)
{
	zend_class_entry ce;
	const uint32_t flags = ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;

	memcpy(&ugl_vec_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	ugl_vec_handlers.offset = XtOffsetOf(zend_ugl_vec, std);
	ugl_vec_handlers.clone_obj = NULL;
	ugl_vec_handlers.compare = ugl_compare;
	ugl_vec_handlers.do_operation = ugl_do_operation;
	ugl_vec_handlers.read_property = ugl_read_property;
	ugl_vec_handlers.write_property = ugl_write_property;
	ugl_vec_handlers.unset_property = ugl_unset_property;
	ugl_vec_handlers.has_property = ugl_has_property;
	ugl_vec_handlers.get_property_ptr_ptr = ugl_get_property_ptr_ptr;
	ugl_vec_handlers.get_properties_for = ugl_get_properties_for;

	zend_register_functions(NULL, ugl_functions, NULL, MODULE_PERSISTENT);

	INIT_CLASS_ENTRY(ce, "vec2", ugl_vec2_methods);
	zend_ce_ugl_vec2 = zend_register_internal_class_with_flags(&ce, NULL, flags);
	zend_ce_ugl_vec2->create_object = ugl_vec2_create;
	zend_ce_ugl_vec2->default_object_handlers = &ugl_vec_handlers;

	INIT_CLASS_ENTRY(ce, "vec3", ugl_vec3_methods);
	zend_ce_ugl_vec3 = zend_register_internal_class_with_flags(&ce, NULL, flags);
	zend_ce_ugl_vec3->create_object = ugl_vec3_create;
	zend_ce_ugl_vec3->default_object_handlers = &ugl_vec_handlers;
}

#endif /* ZEND_UGL */
