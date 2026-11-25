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

#ifndef ZEND_UGL_VEC_H
#define ZEND_UGL_VEC_H

#include "zend.h"
#include "zend_portability.h"

#ifdef ZEND_UGL

BEGIN_EXTERN_C()

extern ZEND_API zend_class_entry *zend_ce_ugl_vec2;
extern ZEND_API zend_class_entry *zend_ce_ugl_vec3;

/* Registers the classes vec2 and vec3 and the functions of the same name. */
void zend_register_ugl_vec(void);
void zend_ugl_vec_request_init(void);

/* Names that always refer to the global UGL classes, also inside namespaces. */
ZEND_API bool zend_is_ugl_class_name(const zend_string *name);

/* VM fast path for + - * / with at least one vec2/vec3 operand. Returns false if the
 * operands are not supported (the generic path then reports the error). A temporary
 * operand (free1/free2) that nobody else references is reused for the result. */
ZEND_API bool zend_ugl_vec_binary_op(uint8_t opcode, zval *result, zval *op1, zval *op2, bool free1, bool free2);

END_EXTERN_C()

#else

static zend_always_inline bool zend_ugl_vec_binary_op(uint8_t opcode, zval *result, zval *op1, zval *op2, bool free1, bool free2)
{
	return false;
}

#endif /* ZEND_UGL */

#endif /* ZEND_UGL_VEC_H */
