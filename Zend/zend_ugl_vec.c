/*
   +----------------------------------------------------------------------+
   | Zend Engine - UGL vector and matrix types                            |
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
#include "zend_ugl_branding.h"

#ifdef ZEND_UGL

#include <math.h>

/* vec2, vec3, vec4 and mat4: final, immutable value objects with float32 components.
 * Arithmetic is done in single precision (+ - * /, component-wise, scalars
 * broadcast), so results match what a GPU computes. */

ZEND_API zend_class_entry *zend_ce_ugl_vec2;
ZEND_API zend_class_entry *zend_ce_ugl_vec3;
ZEND_API zend_class_entry *zend_ce_ugl_vec4;
ZEND_API zend_class_entry *zend_ce_ugl_mat4;
static zend_object_handlers ugl_vec_handlers;
static zend_object_handlers ugl_mat_handlers;

static zend_always_inline zend_ugl_vec *ugl_vec_from_obj(zend_object *obj)
{
	return (zend_ugl_vec *) ((char *) obj - XtOffsetOf(zend_ugl_vec, std));
}

static zend_always_inline zend_ugl_mat *ugl_mat_from_obj(zend_object *obj)
{
	return (zend_ugl_mat *) ((char *) obj - XtOffsetOf(zend_ugl_mat, std));
}

static zend_always_inline bool ugl_is_vec(const zval *zv)
{
	return Z_TYPE_P(zv) == IS_OBJECT && Z_OBJ_P(zv)->handlers == &ugl_vec_handlers;
}

static zend_always_inline bool ugl_is_mat(const zval *zv)
{
	return Z_TYPE_P(zv) == IS_OBJECT && Z_OBJ_P(zv)->handlers == &ugl_mat_handlers;
}

/* Objects carry no references, so they can never be part of a cycle. */
#define UGL_GC_TYPE_INFO (GC_OBJECT | (GC_NOT_COLLECTABLE << GC_FLAGS_SHIFT))

static zend_class_entry *ugl_vec_ce(uint8_t dim)
{
	return dim == 2 ? zend_ce_ugl_vec2 : (dim == 3 ? zend_ce_ugl_vec3 : zend_ce_ugl_vec4);
}

/* ---- recycling pool for vectors ----
 * When the last reference goes away, zend_objects_store_del() calls dtor_obj with the
 * refcount raised to 1. Taking one more reference there keeps the object (and its
 * handle) alive, so it can be handed out again without emalloc, the object store and
 * zend_object_std_init(). */
#define UGL_VEC_POOL_MAX 1024
/* One pool per thread: an object belongs to the object store of the thread that made it. */
ZEND_TLS zend_ugl_vec *ugl_vec_pool[UGL_VEC_POOL_MAX];
ZEND_TLS uint32_t ugl_vec_pool_count;

void zend_ugl_vec_request_init(void)
{
	/* The objects themselves were freed with the previous request's object store. */
	ugl_vec_pool_count = 0;
}

static void ugl_vec_dtor_obj(zend_object *obj)
{
	if (ugl_vec_pool_count < UGL_VEC_POOL_MAX
	 && !(GC_FLAGS(obj) & IS_OBJ_WEAKLY_REFERENCED)
	 && !obj->properties) {
		ugl_vec_pool[ugl_vec_pool_count++] = ugl_vec_from_obj(obj);
		GC_ADDREF(obj);
	}
}

static zend_always_inline zend_ugl_vec *ugl_vec_alloc(zend_class_entry *ce, uint8_t dim)
{
	zend_ugl_vec *v;
	if (ugl_vec_pool_count) {
		v = ugl_vec_pool[--ugl_vec_pool_count];
		GC_TYPE_INFO(&v->std) = UGL_GC_TYPE_INFO;
		v->std.ce = ce;
		v->dim = dim;
		v->initialized = false;
		return v;
	}
	v = zend_object_alloc(sizeof(zend_ugl_vec), ce);
	v->dim = dim;
	zend_object_std_init(&v->std, ce);
	GC_TYPE_INFO(&v->std) = UGL_GC_TYPE_INFO;
	v->std.handlers = &ugl_vec_handlers;
	return v;
}

static zend_object *ugl_vec2_create(zend_class_entry *ce) { return &ugl_vec_alloc(ce, 2)->std; }
static zend_object *ugl_vec3_create(zend_class_entry *ce) { return &ugl_vec_alloc(ce, 3)->std; }
static zend_object *ugl_vec4_create(zend_class_entry *ce) { return &ugl_vec_alloc(ce, 4)->std; }

/* Explicit stores: a loop over dim is turned into a memmove() call by the compiler. */
static zend_always_inline void ugl_vec_store(zend_ugl_vec *v, const float *r)
{
	v->c[0] = r[0];
	v->c[1] = r[1];
	if (v->dim >= 3) {
		v->c[2] = r[2];
		if (v->dim == 4) {
			v->c[3] = r[3];
		}
	}
}

static void ugl_vec_new(zval *result, uint8_t dim, const float *c)
{
	zend_ugl_vec *v = ugl_vec_alloc(ugl_vec_ce(dim), dim);
	ugl_vec_store(v, c);
	v->initialized = true;
	ZVAL_OBJ(result, &v->std);
}

/* Evaluates "op1 <opcode> op2" for vectors and scalars in single precision.
 * Returns false if the operands or the operator are not supported. */
static zend_always_inline bool ugl_binop_eval(uint8_t opcode, zval *op1, zval *op2,
		zend_ugl_vec **pa, zend_ugl_vec **pb, float *r, uint8_t *pdim)
{
	zend_ugl_vec *a = NULL, *b = NULL;
	float sa = 0, sb = 0;

	if (opcode != ZEND_ADD && opcode != ZEND_SUB && opcode != ZEND_MUL && opcode != ZEND_DIV) {
		return false;
	}
	if (ugl_is_vec(op1)) {
		a = ugl_vec_from_obj(Z_OBJ_P(op1));
	} else if (Z_TYPE_P(op1) == IS_DOUBLE) {
		sa = (float) Z_DVAL_P(op1);
	} else if (Z_TYPE_P(op1) == IS_LONG) {
		sa = (float) Z_LVAL_P(op1);
	} else {
		return false;
	}
	if (ugl_is_vec(op2)) {
		b = ugl_vec_from_obj(Z_OBJ_P(op2));
	} else if (Z_TYPE_P(op2) == IS_DOUBLE) {
		sb = (float) Z_DVAL_P(op2);
	} else if (Z_TYPE_P(op2) == IS_LONG) {
		sb = (float) Z_LVAL_P(op2);
	} else {
		return false;
	}
	if ((!a && !b) || (a && b && a->dim != b->dim)) {
		return false;
	}

	uint8_t dim = a ? a->dim : b->dim;
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
	*pa = a;
	*pb = b;
	*pdim = dim;
	return true;
}

/* An object only this slot references can be overwritten instead of copied. A weak
 * reference would otherwise observe the change. */
static zend_always_inline bool ugl_can_overwrite(const zend_ugl_vec *v)
{
	return GC_REFCOUNT(&v->std) == 1 && !(GC_FLAGS(&v->std) & IS_OBJ_WEAKLY_REFERENCED);
}

/* Hands a temporary nobody else references straight back to the pool. The caller's
 * zval is cleared, so releasing it afterwards does nothing. */
static zend_always_inline void ugl_vec_recycle(zval *zv)
{
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(zv));
	if (ugl_vec_pool_count < UGL_VEC_POOL_MAX && ugl_can_overwrite(v) && !v->std.properties) {
		GC_ADD_FLAGS(&v->std, IS_OBJ_DESTRUCTOR_CALLED);
		ugl_vec_pool[ugl_vec_pool_count++] = v;
		ZVAL_UNDEF(zv);
	}
}

ZEND_API void zend_ugl_vec_release_slot(zval *var)
{
	if (ugl_is_vec(var)) {
		ugl_vec_recycle(var);
	}
}

/* Both operands are vectors of the same size: the operator is chosen once, outside the
 * component loop, and the common sizes are written out. */
static zend_always_inline bool ugl_vec_vec_fast(uint8_t opcode, const zend_ugl_vec *a, const zend_ugl_vec *b, float *r)
{
#define UGL_FAST(OP) \
	r[0] = a->c[0] OP b->c[0]; \
	r[1] = a->c[1] OP b->c[1]; \
	if (a->dim >= 3) { \
		r[2] = a->c[2] OP b->c[2]; \
		if (a->dim == 4) { \
			r[3] = a->c[3] OP b->c[3]; \
		} \
	} \
	return true;
	switch (opcode) {
		case ZEND_ADD: UGL_FAST(+)
		case ZEND_SUB: UGL_FAST(-)
		case ZEND_MUL: UGL_FAST(*)
		case ZEND_DIV: UGL_FAST(/)
	}
#undef UGL_FAST
	return false;
}

ZEND_API bool zend_ugl_vec_binary_op(uint8_t opcode, zval *result, zval *op1, zval *op2, uint32_t flags, zval *target)
{
	zend_ugl_vec *a, *b;
	float r[4];
	uint8_t dim;

	if (EXPECTED(ugl_is_vec(op1) && ugl_is_vec(op2)
	 && (a = ugl_vec_from_obj(Z_OBJ_P(op1)))->dim == (b = ugl_vec_from_obj(Z_OBJ_P(op2)))->dim)) {
		if (!ugl_vec_vec_fast(opcode, a, b, r)) {
			return false;
		}
		dim = a->dim;
	} else if (!ugl_binop_eval(opcode, op1, op2, &a, &b, r, &dim)) {
		return false;
	}

	/* A vector nobody else references can hold the result: the variable the next opcode
	 * assigns to, or a temporary that only this opcode references. The result slot must
	 * not alias an operand, the caller still releases both operands. */
	zend_ugl_vec *reuse = NULL;
	if (target && Z_TYPE_P(target) == IS_OBJECT && ugl_is_vec(target)) {
		zend_ugl_vec *t = ugl_vec_from_obj(Z_OBJ_P(target));
		if (t->dim == dim && ugl_can_overwrite(t)) {
			reuse = t;
		}
	}
	if (!reuse && result != op1 && result != op2) {
		if ((flags & ZEND_UGL_OP1_TMP) && a && ugl_can_overwrite(a)) {
			reuse = a;
		} else if ((flags & ZEND_UGL_OP2_TMP) && b && ugl_can_overwrite(b)) {
			reuse = b;
		}
	}
	if (reuse) {
		ugl_vec_store(reuse, r);
		ZVAL_OBJ_COPY(result, &reuse->std);
		return true;
	}
	ugl_vec_new(result, dim, r);
	return true;
}

ZEND_API bool zend_ugl_vec_fetch_component(zend_object *obj, const zend_string *name, zval *result)
{
	if (obj->handlers != &ugl_vec_handlers || ZSTR_LEN(name) != 1) {
		return false;
	}
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	char ch = ZSTR_VAL(name)[0];
	uint8_t idx;
	if (ch == 'w') {
		idx = 3;
	} else if (ch >= 'x' && ch <= 'z') {
		idx = (uint8_t) (ch - 'x');
	} else {
		return false;
	}
	if (idx >= v->dim) {
		return false;
	}
	ZVAL_DOUBLE(result, v->c[idx]);
	return true;
}

ZEND_API bool zend_ugl_vec_assign_op(uint8_t opcode, zval *var, zval *value)
{
	zend_ugl_vec *a, *b;
	float r[4];
	uint8_t dim;

	if (!ugl_is_vec(var) || !ugl_can_overwrite(ugl_vec_from_obj(Z_OBJ_P(var)))
	 || !ugl_binop_eval(opcode, var, value, &a, &b, r, &dim)) {
		return false;
	}
	ugl_vec_store(a, r);
	return true;
}

/* ---- operators (generic path, also used for $p op= x) ---- */

static zend_result ugl_mat_do_operation_ex(uint8_t opcode, zval *result, zval *op1, zval *op2);

static zend_result ugl_do_operation_ex(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	zend_ugl_vec *a, *b;
	float r[4];
	uint8_t dim;

	/* The engine asks only the left operand's handler: matrices are handled there too. */
	if (ugl_is_mat(op1) || ugl_is_mat(op2)) {
		return ugl_mat_do_operation_ex(opcode, result, op1, op2);
	}
	if (!ugl_binop_eval(opcode, op1, op2, &a, &b, r, &dim)) {
		return FAILURE;
	}
	ugl_vec_new(result, dim, r);
	return SUCCESS;
}

static zend_result ugl_do_operation(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	zval op1_copy;
	zend_result status;

	if (result == op1 && ugl_is_vec(op1)) {
		/* $p += ... on a vector nobody else holds: update it in place. */
		zend_ugl_vec *a, *b;
		float r[4];
		uint8_t dim;

		if (ugl_can_overwrite(ugl_vec_from_obj(Z_OBJ_P(op1)))
		 && ugl_binop_eval(opcode, op1, op2, &a, &b, r, &dim)) {
			ugl_vec_store(a, r);
			return SUCCESS;
		}
	}

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

/* Maps a swizzle like "x", "yx" or "wzz" to component indexes. Returns the length, or 0
 * if the name is not a valid swizzle for this vector. */
static zend_always_inline uint8_t ugl_parse_swizzle(const zend_ugl_vec *v, const zend_string *name, uint8_t *idx)
{
	size_t len = ZSTR_LEN(name);
	if (len == 0 || len > 4) {
		return 0;
	}
	for (size_t i = 0; i < len; i++) {
		char ch = ZSTR_VAL(name)[i];
		uint8_t k;
		if (ch == 'w') {
			k = 3;
		} else if (ch >= 'x' && ch <= 'z') {
			k = (uint8_t) (ch - 'x');
		} else {
			return 0;
		}
		if (k >= v->dim) {
			return 0;
		}
		idx[i] = k;
	}
	return (uint8_t) len;
}

static zval *ugl_read_property(zend_object *obj, zend_string *name, int type, void **cache_slot, zval *rv)
{
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	uint8_t idx[4];
	uint8_t n = ugl_parse_swizzle(v, name, idx);

	if (n == 0) {
		return zend_std_read_property(obj, name, type, cache_slot, rv);
	}
	if (n == 1) {
		ZVAL_DOUBLE(rv, v->c[idx[0]]);
		return rv;
	}
	float r[4];
	for (uint8_t i = 0; i < n; i++) {
		r[i] = v->c[idx[i]];
	}
	ugl_vec_new(rv, n, r);
	return rv;
}

static zval *ugl_write_property(zend_object *obj, zend_string *name, zval *value, void **cache_slot)
{
	zend_throw_error(NULL, "Cannot modify %s: values are immutable", ZSTR_VAL(obj->ce->name));
	return &EG(error_zval);
}

static void ugl_unset_property(zend_object *obj, zend_string *name, void **cache_slot)
{
	zend_throw_error(NULL, "Cannot unset property of %s: values are immutable", ZSTR_VAL(obj->ce->name));
}

static zval *ugl_get_property_ptr_ptr(zend_object *obj, zend_string *name, int type, void **cache_slot)
{
	return NULL;
}

static int ugl_has_property(zend_object *obj, zend_string *name, int has_set_exists, void **cache_slot)
{
	zend_ugl_vec *v = ugl_vec_from_obj(obj);
	uint8_t idx[4];
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
	static const char *names[4] = {"x", "y", "z", "w"};
	zend_array *ht = zend_new_array(v->dim);
	for (uint8_t i = 0; i < v->dim; i++) {
		zval tmp;
		ZVAL_DOUBLE(&tmp, v->c[i]);
		zend_hash_str_add_new(ht, names[i], 1, &tmp);
	}
	return ht;
}

/* ---- vector methods ---- */

static void ugl_parse_components(INTERNAL_FUNCTION_PARAMETERS, uint8_t dim, double *d)
{
	ZEND_PARSE_PARAMETERS_START(0, dim)
		Z_PARAM_OPTIONAL
		Z_PARAM_DOUBLE(d[0])
		Z_PARAM_DOUBLE(d[1])
		Z_PARAM_DOUBLE(d[2])
		Z_PARAM_DOUBLE(d[3])
	ZEND_PARSE_PARAMETERS_END();
}

static ZEND_NAMED_FUNCTION(ugl_vec_construct)
{
	zend_ugl_vec *v = ugl_vec_from_obj(Z_OBJ_P(ZEND_THIS));
	double d[4] = {0.0, 0.0, 0.0, 0.0};

	ugl_parse_components(INTERNAL_FUNCTION_PARAM_PASSTHRU, v->dim, d);
	if (EG(exception)) {
		RETURN_THROWS();
	}
	if (v->initialized) {
		zend_throw_error(NULL, "Cannot modify %s: values are immutable", ZSTR_VAL(v->std.ce->name));
		RETURN_THROWS();
	}
	for (uint8_t i = 0; i < v->dim; i++) {
		v->c[i] = (float) d[i];
	}
	v->initialized = true;
}

static void ugl_function_ctor(INTERNAL_FUNCTION_PARAMETERS, uint8_t dim)
{
	double d[4] = {0.0, 0.0, 0.0, 0.0};

	ugl_parse_components(INTERNAL_FUNCTION_PARAM_PASSTHRU, dim, d);
	if (EG(exception)) {
		RETURN_THROWS();
	}
	float c[4] = {(float) d[0], (float) d[1], (float) d[2], (float) d[3]};
	ugl_vec_new(return_value, dim, c);
}

ZEND_FUNCTION(vec2) { ugl_function_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, 2); }
ZEND_FUNCTION(vec3) { ugl_function_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, 3); }
ZEND_FUNCTION(vec4) { ugl_function_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, 4); }

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
	float len = sqrtf(sum), r[4];
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
	float r[4] = {
		v->c[1] * o->c[2] - v->c[2] * o->c[1],
		v->c[2] * o->c[0] - v->c[0] * o->c[2],
		v->c[0] * o->c[1] - v->c[1] * o->c[0],
		0.0f,
	};
	ugl_vec_new(return_value, 3, r);
}

/* ---- mat4 ---- */

static zend_object *ugl_mat_create(zend_class_entry *ce)
{
	zend_ugl_mat *m = zend_object_alloc(sizeof(zend_ugl_mat), ce);
	zend_object_std_init(&m->std, ce);
	GC_TYPE_INFO(&m->std) = UGL_GC_TYPE_INFO;
	m->std.handlers = &ugl_mat_handlers;
	return &m->std;
}

static void ugl_mat_new(zval *result, const float *m)
{
	zend_object *obj = ugl_mat_create(zend_ce_ugl_mat4);
	zend_ugl_mat *mat = ugl_mat_from_obj(obj);
	memcpy(mat->m, m, sizeof(mat->m));
	mat->initialized = true;
	ZVAL_OBJ(result, obj);
}

static const float ugl_identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

/* Values for a mat4: none (identity) or 16 numbers, column by column. */
static bool ugl_mat_parse_values(zval *args, uint32_t argc, float *m)
{
	if (argc == 0) {
		memcpy(m, ugl_identity, sizeof(ugl_identity));
		return true;
	}
	if (argc != 16) {
		zend_argument_count_error("mat4() expects 0 or 16 arguments, %u given", argc);
		return false;
	}
	for (uint32_t i = 0; i < 16; i++) {
		if (Z_TYPE(args[i]) == IS_DOUBLE) {
			m[i] = (float) Z_DVAL(args[i]);
		} else if (Z_TYPE(args[i]) == IS_LONG) {
			m[i] = (float) Z_LVAL(args[i]);
		} else {
			zend_argument_type_error(i + 1, "must be of type float, %s given", zend_zval_type_name(&args[i]));
			return false;
		}
	}
	return true;
}

ZEND_FUNCTION(mat4)
{
	zval *args = NULL;
	uint32_t argc = 0;
	float m[16];

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	if (!ugl_mat_parse_values(args, argc, m)) {
		RETURN_THROWS();
	}
	ugl_mat_new(return_value, m);
}

static ZEND_NAMED_FUNCTION(ugl_mat_construct)
{
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));
	zval *args = NULL;
	uint32_t argc = 0;
	float m[16];

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	if (!ugl_mat_parse_values(args, argc, m)) {
		RETURN_THROWS();
	}
	if (mat->initialized) {
		zend_throw_error(NULL, "Cannot modify mat4: values are immutable");
		RETURN_THROWS();
	}
	memcpy(mat->m, m, sizeof(m));
	mat->initialized = true;
}

static void ugl_mat_mul_mat(float *out, const float *a, const float *b)
{
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			float sum = 0;
			for (int k = 0; k < 4; k++) {
				sum += a[k * 4 + i] * b[j * 4 + k];
			}
			out[j * 4 + i] = sum;
		}
	}
}

static void ugl_mat_mul_vec(float *out, const float *m, const float *v)
{
	for (int i = 0; i < 4; i++) {
		float sum = 0;
		for (int k = 0; k < 4; k++) {
			sum += m[k * 4 + i] * v[k];
		}
		out[i] = sum;
	}
}

static void ugl_vec_mul_mat(float *out, const float *v, const float *m)
{
	for (int j = 0; j < 4; j++) {
		float sum = 0;
		for (int k = 0; k < 4; k++) {
			sum += v[k] * m[j * 4 + k];
		}
		out[j] = sum;
	}
}

static zend_result ugl_mat_do_operation_ex(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	const float *ma = ugl_is_mat(op1) ? ugl_mat_from_obj(Z_OBJ_P(op1))->m : NULL;
	const float *mb = ugl_is_mat(op2) ? ugl_mat_from_obj(Z_OBJ_P(op2))->m : NULL;
	zend_ugl_vec *va = ugl_is_vec(op1) ? ugl_vec_from_obj(Z_OBJ_P(op1)) : NULL;
	zend_ugl_vec *vb = ugl_is_vec(op2) ? ugl_vec_from_obj(Z_OBJ_P(op2)) : NULL;
	float sa = 0, sb = 0;
	bool a_scalar = false, b_scalar = false;
	float out[16];

	if (!ma && !va) {
		if (Z_TYPE_P(op1) == IS_DOUBLE) { sa = (float) Z_DVAL_P(op1); a_scalar = true; }
		else if (Z_TYPE_P(op1) == IS_LONG) { sa = (float) Z_LVAL_P(op1); a_scalar = true; }
		else { return FAILURE; }
	}
	if (!mb && !vb) {
		if (Z_TYPE_P(op2) == IS_DOUBLE) { sb = (float) Z_DVAL_P(op2); b_scalar = true; }
		else if (Z_TYPE_P(op2) == IS_LONG) { sb = (float) Z_LVAL_P(op2); b_scalar = true; }
		else { return FAILURE; }
	}

	if (ma && mb) {
		switch (opcode) {
			case ZEND_ADD: for (int i = 0; i < 16; i++) out[i] = ma[i] + mb[i]; break;
			case ZEND_SUB: for (int i = 0; i < 16; i++) out[i] = ma[i] - mb[i]; break;
			case ZEND_MUL: ugl_mat_mul_mat(out, ma, mb); break;
			default: return FAILURE;
		}
		ugl_mat_new(result, out);
		return SUCCESS;
	}
	if (ma && b_scalar) {
		if (opcode != ZEND_MUL && opcode != ZEND_DIV) {
			return FAILURE;
		}
		for (int i = 0; i < 16; i++) out[i] = opcode == ZEND_MUL ? ma[i] * sb : ma[i] / sb;
		ugl_mat_new(result, out);
		return SUCCESS;
	}
	if (mb && a_scalar) {
		if (opcode != ZEND_MUL) {
			return FAILURE;
		}
		for (int i = 0; i < 16; i++) out[i] = sa * mb[i];
		ugl_mat_new(result, out);
		return SUCCESS;
	}
	if (opcode == ZEND_MUL && ma && vb && vb->dim == 4) {
		ugl_mat_mul_vec(out, ma, vb->c);
		ugl_vec_new(result, 4, out);
		return SUCCESS;
	}
	if (opcode == ZEND_MUL && va && va->dim == 4 && mb) {
		ugl_vec_mul_mat(out, va->c, mb);
		ugl_vec_new(result, 4, out);
		return SUCCESS;
	}
	return FAILURE;
}

static zend_result ugl_mat_do_operation(uint8_t opcode, zval *result, zval *op1, zval *op2)
{
	zval op1_copy;
	zend_result status;

	if (opcode != ZEND_ADD && opcode != ZEND_SUB && opcode != ZEND_MUL && opcode != ZEND_DIV) {
		return FAILURE;
	}
	if (result == op1) {
		ZVAL_COPY_VALUE(&op1_copy, op1);
		op1 = &op1_copy;
	}
	status = ugl_mat_do_operation_ex(opcode, result, op1, op2);
	if (status == SUCCESS && op1 == &op1_copy) {
		zval_ptr_dtor(op1);
	}
	return status;
}

static int ugl_mat_compare(zval *o1, zval *o2)
{
	ZEND_COMPARE_OBJECTS_FALLBACK(o1, o2);
	return memcmp(ugl_mat_from_obj(Z_OBJ_P(o1))->m, ugl_mat_from_obj(Z_OBJ_P(o2))->m, 16 * sizeof(float)) == 0
		? 0 : ZEND_UNCOMPARABLE;
}

static zend_array *ugl_mat_get_properties_for(zend_object *obj, zend_prop_purpose purpose)
{
	zend_ugl_mat *mat = ugl_mat_from_obj(obj);
	zend_array *ht = zend_new_array(16);
	for (int i = 0; i < 16; i++) {
		char name[8];
		zval tmp;
		int len = snprintf(name, sizeof(name), "m%d", i);
		ZVAL_DOUBLE(&tmp, mat->m[i]);
		zend_hash_str_add_new(ht, name, len, &tmp);
	}
	return ht;
}

static zval *ugl_mat_write_property(zend_object *obj, zend_string *name, zval *value, void **cache_slot)
{
	zend_throw_error(NULL, "Cannot modify mat4: values are immutable");
	return &EG(error_zval);
}

static ZEND_NAMED_FUNCTION(ugl_mat_get)
{
	zend_long col, row;
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(col)
		Z_PARAM_LONG(row)
	ZEND_PARSE_PARAMETERS_END();

	if (col < 0 || col > 3 || row < 0 || row > 3) {
		zend_argument_value_error(col < 0 || col > 3 ? 1 : 2, "must be between 0 and 3");
		RETURN_THROWS();
	}
	RETURN_DOUBLE(mat->m[col * 4 + row]);
}

static ZEND_NAMED_FUNCTION(ugl_mat_column)
{
	zend_long col;
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(col)
	ZEND_PARSE_PARAMETERS_END();

	if (col < 0 || col > 3) {
		zend_argument_value_error(1, "must be between 0 and 3");
		RETURN_THROWS();
	}
	ugl_vec_new(return_value, 4, mat->m + col * 4);
}

static ZEND_NAMED_FUNCTION(ugl_mat_transpose)
{
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));
	float out[16];

	ZEND_PARSE_PARAMETERS_NONE();
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			out[r * 4 + c] = mat->m[c * 4 + r];
		}
	}
	ugl_mat_new(return_value, out);
}

/* Determinant and inverse by cofactors (the classic 4x4 expansion). Returns the
 * determinant; fills inv unless the matrix is singular. */
static float ugl_mat_inverse(const float *m, float *inv)
{
	float t[16];

	t[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	t[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	t[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
	t[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
	t[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	t[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	t[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
	t[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
	t[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
	t[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
	t[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
	t[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
	t[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
	t[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
	t[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
	t[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

	float det = m[0]*t[0] + m[1]*t[4] + m[2]*t[8] + m[3]*t[12];
	if (inv && det != 0.0f) {
		float inv_det = 1.0f / det;
		for (int i = 0; i < 16; i++) {
			inv[i] = t[i] * inv_det;
		}
	}
	return det;
}

static ZEND_NAMED_FUNCTION(ugl_mat_determinant)
{
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));

	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_DOUBLE(ugl_mat_inverse(mat->m, NULL));
}

static ZEND_NAMED_FUNCTION(ugl_mat_inverse_method)
{
	zend_ugl_mat *mat = ugl_mat_from_obj(Z_OBJ_P(ZEND_THIS));
	float inv[16];

	ZEND_PARSE_PARAMETERS_NONE();
	if (ugl_mat_inverse(mat->m, inv) == 0.0f) {
		zend_throw_exception(zend_ce_value_error, "mat4 is not invertible", 0);
		RETURN_THROWS();
	}
	ugl_mat_new(return_value, inv);
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_vec4_ctor, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, z, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, w, IS_DOUBLE, 0, "0.0")
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

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_fn_vec4, 0, 0, vec4, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, z, IS_DOUBLE, 0, "0.0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, w, IS_DOUBLE, 0, "0.0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_fn_mat4, 0, 0, mat4, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, values, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mat4_ctor, 0, 0, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, values, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec2_dot, 0, 1, IS_DOUBLE, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec2, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec3_dot, 0, 1, IS_DOUBLE, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec3, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec4_dot, 0, 1, IS_DOUBLE, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec4, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vec_length, 0, 0, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec2_normalize, 0, 0, vec2, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec3_normalize, 0, 0, vec3, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec4_normalize, 0, 0, vec4, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vec3_cross, 0, 1, vec3, 0)
	ZEND_ARG_OBJ_INFO(0, other, vec3, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mat4_get, 0, 2, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, column, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, row, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_mat4_column, 0, 1, vec4, 0)
	ZEND_ARG_TYPE_INFO(0, column, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_mat4_self, 0, 0, mat4, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry ugl_functions[] = {
	ZEND_FE(vec2, arginfo_fn_vec2)
	ZEND_FE(vec3, arginfo_fn_vec3)
	ZEND_FE(vec4, arginfo_fn_vec4)
	ZEND_FE(mat4, arginfo_fn_mat4)
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

static const zend_function_entry ugl_vec4_methods[] = {
	ZEND_NAMED_ME(__construct, ugl_vec_construct, arginfo_vec4_ctor, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(dot, ugl_vec_dot, arginfo_vec4_dot, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(length, ugl_vec_length, arginfo_vec_length, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(normalize, ugl_vec_normalize, arginfo_vec4_normalize, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry ugl_mat4_methods[] = {
	ZEND_NAMED_ME(__construct, ugl_mat_construct, arginfo_mat4_ctor, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(get, ugl_mat_get, arginfo_mat4_get, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(column, ugl_mat_column, arginfo_mat4_column, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(transpose, ugl_mat_transpose, arginfo_mat4_self, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(determinant, ugl_mat_determinant, arginfo_vec_length, ZEND_ACC_PUBLIC)
	ZEND_NAMED_ME(inverse, ugl_mat_inverse_method, arginfo_mat4_self, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

ZEND_API bool zend_is_ugl_class_name(const zend_string *name)
{
	/* Exactly the lower-case spellings: class names are case-insensitive, so a user class
	 * such as App\Math\Vec3 must keep resolving to itself. */
	return ZSTR_LEN(name) == 4
		&& (memcmp(ZSTR_VAL(name), "vec2", 4) == 0
		 || memcmp(ZSTR_VAL(name), "vec3", 4) == 0
		 || memcmp(ZSTR_VAL(name), "vec4", 4) == 0
		 || memcmp(ZSTR_VAL(name), "mat4", 4) == 0);
}

static zend_class_entry *ugl_register_class(const char *name, const zend_function_entry *methods,
		zend_object *(*create)(zend_class_entry *), const zend_object_handlers *handlers)
{
	zend_class_entry ce, *registered;
	INIT_CLASS_ENTRY_EX(ce, name, strlen(name), methods);
	registered = zend_register_internal_class_with_flags(&ce, NULL,
		ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE);
	registered->create_object = create;
	registered->default_object_handlers = handlers;
	return registered;
}

void zend_register_ugl_vec(void)
{
	memcpy(&ugl_vec_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	ugl_vec_handlers.offset = XtOffsetOf(zend_ugl_vec, std);
	ugl_vec_handlers.clone_obj = NULL;
	ugl_vec_handlers.dtor_obj = ugl_vec_dtor_obj;
	ugl_vec_handlers.compare = ugl_compare;
	ugl_vec_handlers.do_operation = ugl_do_operation;
	ugl_vec_handlers.read_property = ugl_read_property;
	ugl_vec_handlers.write_property = ugl_write_property;
	ugl_vec_handlers.unset_property = ugl_unset_property;
	ugl_vec_handlers.has_property = ugl_has_property;
	ugl_vec_handlers.get_property_ptr_ptr = ugl_get_property_ptr_ptr;
	ugl_vec_handlers.get_properties_for = ugl_get_properties_for;

	memcpy(&ugl_mat_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	ugl_mat_handlers.offset = XtOffsetOf(zend_ugl_mat, std);
	ugl_mat_handlers.clone_obj = NULL;
	ugl_mat_handlers.compare = ugl_mat_compare;
	ugl_mat_handlers.do_operation = ugl_mat_do_operation;
	ugl_mat_handlers.write_property = ugl_mat_write_property;
	ugl_mat_handlers.unset_property = ugl_unset_property;
	ugl_mat_handlers.get_property_ptr_ptr = ugl_get_property_ptr_ptr;
	ugl_mat_handlers.get_properties_for = ugl_mat_get_properties_for;

	zend_register_functions(NULL, ugl_functions, NULL, MODULE_PERSISTENT);
	zend_register_string_constant("UGL_VERSION", sizeof("UGL_VERSION") - 1, UGL_PRODUCT_VERSION, CONST_PERSISTENT, 0);

	zend_ce_ugl_vec2 = ugl_register_class("vec2", ugl_vec2_methods, ugl_vec2_create, &ugl_vec_handlers);
	zend_ce_ugl_vec3 = ugl_register_class("vec3", ugl_vec3_methods, ugl_vec3_create, &ugl_vec_handlers);
	zend_ce_ugl_vec4 = ugl_register_class("vec4", ugl_vec4_methods, ugl_vec4_create, &ugl_vec_handlers);
	zend_ce_ugl_mat4 = ugl_register_class("mat4", ugl_mat4_methods, ugl_mat_create, &ugl_mat_handlers);
}

#endif /* ZEND_UGL */
