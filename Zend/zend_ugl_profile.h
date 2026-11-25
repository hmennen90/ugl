/*
   +----------------------------------------------------------------------+
   | Zend Engine - UGL file profiles (declare(kind=...))                  |
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

#ifndef ZEND_UGL_PROFILE_H
#define ZEND_UGL_PROFILE_H

#include "zend.h"
#include "zend_ast.h"

#ifdef ZEND_UGL

typedef enum {
	ZEND_UGL_PROFILE_LOGIC  = 0,  /* unrestricted PHP */
	ZEND_UGL_PROFILE_COMMON = 1,  /* shared logic, runs on the CPU and as a shader */
	ZEND_UGL_PROFILE_DATA   = 2,  /* data description and IO: strings, exceptions and null stay */
	ZEND_UGL_PROFILE_SHADER = 3,  /* compiled for the GPU */
} zend_ugl_profile;

/* Returns false if the name is not one of logic, common, data, shader. */
ZEND_API bool zend_ugl_profile_from_string(const zend_string *name, zend_ugl_profile *profile);

/* Checks a whole file against a profile. Raises a compile error for the first violation. */
ZEND_API void zend_ugl_check_profile(zend_ast *file_ast, zend_ugl_profile profile);

#endif /* ZEND_UGL */

#endif /* ZEND_UGL_PROFILE_H */
