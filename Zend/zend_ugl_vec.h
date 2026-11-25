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

#ifdef ZEND_UGL

BEGIN_EXTERN_C()

extern ZEND_API zend_class_entry *zend_ce_ugl_vec2;
extern ZEND_API zend_class_entry *zend_ce_ugl_vec3;

/* Registers the classes vec2 and vec3 and the functions of the same name. */
void zend_register_ugl_vec(void);

/* Names that always refer to the global UGL classes, also inside namespaces. */
ZEND_API bool zend_is_ugl_class_name(const zend_string *name);

END_EXTERN_C()

#endif /* ZEND_UGL */

#endif /* ZEND_UGL_VEC_H */
