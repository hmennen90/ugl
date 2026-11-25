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

#ifndef ZEND_UGL_VEC_H
#define ZEND_UGL_VEC_H

#include "zend.h"
#include "zend_portability.h"
#include "zend_compile.h"

#ifdef ZEND_UGL

/* vec2, vec3 and vec4: final, immutable value objects with float32 components. The
 * layout is public because the JIT reads the components directly. */
typedef struct _zend_ugl_vec {
	float       c[4];
	uint8_t     dim;
	bool        initialized;
	zend_object std;
} zend_ugl_vec;

/* Offset of c[0] relative to the zend_object pointer. */
#define ZEND_UGL_VEC_C_OFFSET \
	((intptr_t) XtOffsetOf(zend_ugl_vec, c) - (intptr_t) XtOffsetOf(zend_ugl_vec, std))

BEGIN_EXTERN_C()

extern ZEND_API zend_class_entry *zend_ce_ugl_vec2;
extern ZEND_API zend_class_entry *zend_ce_ugl_vec3;
extern ZEND_API zend_class_entry *zend_ce_ugl_vec4;
extern ZEND_API zend_class_entry *zend_ce_ugl_mat4;

/* Registers vec2, vec3, vec4, mat4 and the functions of the same name. */
void zend_register_ugl_vec(void);
void zend_ugl_vec_request_init(void);

/* Names that always refer to the global UGL classes, also inside namespaces. */
ZEND_API bool zend_is_ugl_class_name(const zend_string *name);

/* VM fast path for + - * / with at least one vector operand. Returns false if the
 * operands are not supported (the generic path then reports the error). A temporary
 * operand that nobody else references is reused for the result. */
#define ZEND_UGL_OP1_TMP    1  /* op1 is a TMP/VAR the caller releases after the opcode */
#define ZEND_UGL_OP2_TMP    2  /* op2 is a TMP/VAR the caller releases after the opcode */
#define ZEND_UGL_OP1_ASSIGN 4  /* op1 is a CV that the next opcode overwrites with the result */
ZEND_API bool zend_ugl_vec_binary_op(uint8_t opcode, zval *result, zval *op1, zval *op2, uint32_t flags);

/* ZEND_ASSIGN_OP: updates a vector in place for $p op= x. Returns false if that is not
 * possible (shared vector, other operand types, ...), the generic path then runs. */
ZEND_API bool zend_ugl_vec_assign_op(uint8_t opcode, zval *var, zval *value);

END_EXTERN_C()

/* "$p = $p <op> x": the opcode after the arithmetic assigns its result to the variable
 * that was the left operand, so that variable's old value is dead afterwards. */
# define ZEND_UGL_ASSIGNS_RESULT_TO_OP1(opline) \
	((opline)->op1_type == IS_CV \
	 && ((opline) + 1)->opcode == ZEND_ASSIGN \
	 && ((opline) + 1)->op1_type == IS_CV \
	 && ((opline) + 1)->op1.var == (opline)->op1.var \
	 && ((opline) + 1)->op2_type == IS_TMP_VAR \
	 && ((opline) + 1)->op2.var == (opline)->result.var)

#else

# define ZEND_UGL_ASSIGNS_RESULT_TO_OP1(opline) 0
# define ZEND_UGL_OP1_TMP 1
# define ZEND_UGL_OP2_TMP 2
# define ZEND_UGL_OP1_ASSIGN 4

static zend_always_inline bool zend_ugl_vec_assign_op(uint8_t opcode, zval *var, zval *value)
{
	return false;
}

static zend_always_inline bool zend_ugl_vec_binary_op(uint8_t opcode, zval *result, zval *op1, zval *op2, uint32_t flags)
{
	return false;
}

#endif /* ZEND_UGL */

#endif /* ZEND_UGL_VEC_H */
