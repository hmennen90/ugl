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

#include "zend.h"
#include "zend_ast.h"
#include "zend_compile.h"
#include "zend_ugl_vec.h"
#include "zend_ugl_profile.h"
#include "zend_language_parser.h"
#include "zend_smart_str.h"

#ifdef ZEND_UGL

/* A profile restricts a file to a subset of PHP. The checks run once, on the AST of the
 * whole file, when the declare(kind=...) statement is compiled. What they catch is what
 * can be decided without types: unlike a static analyser they never guess.
 *
 *   data    references, global, eval, variable variables and dynamic names, class shape,
 *           new of anything but value types
 *   common  data + strings, null, exceptions, array appends, recursion
 *   shader  common + closures
 */
#define UGL_RULE_REF        (1u << 0)  /* references in every form */
#define UGL_RULE_GLOBAL     (1u << 1)  /* global, $GLOBALS */
#define UGL_RULE_EVAL       (1u << 2)
#define UGL_RULE_DYNAMIC    (1u << 3)  /* $$x, $o->$n, $f(), new $c, ... */
#define UGL_RULE_CLASS      (1u << 4)  /* only final readonly classes and enums */
#define UGL_RULE_NEW        (1u << 5)  /* new only for value types */
#define UGL_RULE_STRING     (1u << 6)
#define UGL_RULE_NULL       (1u << 7)  /* null, ?T, ??, ?-> */
#define UGL_RULE_EXCEPTION  (1u << 8)
#define UGL_RULE_APPEND     (1u << 9)  /* $a[] */
#define UGL_RULE_RECURSION  (1u << 10)
#define UGL_RULE_CLOSURE    (1u << 11)

#define UGL_RULES_DATA (UGL_RULE_REF|UGL_RULE_GLOBAL|UGL_RULE_EVAL|UGL_RULE_DYNAMIC|UGL_RULE_CLASS|UGL_RULE_NEW)
#define UGL_RULES_COMMON (UGL_RULES_DATA|UGL_RULE_STRING|UGL_RULE_NULL|UGL_RULE_EXCEPTION|UGL_RULE_APPEND|UGL_RULE_RECURSION)
#define UGL_RULES_SHADER (UGL_RULES_COMMON|UGL_RULE_CLOSURE)

static const char *ugl_profile_names[] = {"logic", "common", "data", "shader"};

ZEND_API bool zend_ugl_profile_from_string(const zend_string *name, zend_ugl_profile *profile)
{
	for (int i = 0; i < 4; i++) {
		if (zend_string_equals_cstr(name, ugl_profile_names[i], strlen(ugl_profile_names[i]))) {
			*profile = (zend_ugl_profile) i;
			return true;
		}
	}
	return false;
}

typedef struct {
	zend_ugl_profile profile;
	uint32_t         rules;
	HashTable        value_classes; /* lower-case names of the final readonly classes and enums of the file */
	HashTable        graph;         /* caller key => HashTable of callee keys, for the recursion check */
	zend_string     *cur_class;     /* lower-case short name, NULL outside classes */
	HashTable       *cur_edges;     /* callees of the function being walked */
	zend_string     *stack[128];    /* the search path of the recursion check */
	uint32_t         depth;
} ugl_ctx;

static ZEND_COLD ZEND_NORETURN void ugl_fail(ugl_ctx *c, zend_ast *ast, const char *what)
{
	if (ast) {
		CG(zend_lineno) = zend_ast_get_lineno(ast);
	}
	zend_error_noreturn(E_COMPILE_ERROR, "%s not allowed in code declared with kind='%s'",
		what, ugl_profile_names[c->profile]);
}

#define UGL_FAIL_IF(c, rule, ast, what) do { if ((c)->rules & (rule)) ugl_fail((c), (ast), (what)); } while (0)

/* Last segment of a possibly namespaced name, lower-cased (a new string). */
static zend_string *ugl_short_lower(zend_string *name)
{
	const char *s = ZSTR_VAL(name), *p = strrchr(s, '\\');
	p = p ? p + 1 : s;
	zend_string *tmp = zend_string_init(p, strlen(p), 0);
	zend_string *low = zend_string_tolower(tmp);
	zend_string_release(tmp);
	return low;
}

static bool ugl_name_is(zend_ast *ast, const char *name)
{
	return ast && ast->kind == ZEND_AST_ZVAL && Z_TYPE_P(zend_ast_get_zval(ast)) == IS_STRING
		&& zend_binary_strcasecmp(Z_STRVAL_P(zend_ast_get_zval(ast)), Z_STRLEN_P(zend_ast_get_zval(ast)),
			name, strlen(name)) == 0;
}

/* ---- types ---- */

static void ugl_check_type(ugl_ctx *c, zend_ast *type)
{
	if (!type) {
		return;
	}
	if (type->kind == ZEND_AST_TYPE_UNION || type->kind == ZEND_AST_TYPE_INTERSECTION) {
		zend_ast_list *list = zend_ast_get_list(type);
		for (uint32_t i = 0; i < list->children; i++) {
			ugl_check_type(c, list->child[i]);
		}
		return;
	}
	if (type->kind == ZEND_AST_ZVAL && Z_TYPE_P(zend_ast_get_zval(type)) == IS_STRING) {
		zend_string *n = Z_STR_P(zend_ast_get_zval(type));
		if ((c->rules & UGL_RULE_STRING) && zend_string_equals_literal_ci(n, "string")) {
			ugl_fail(c, type, "The string type and strings are");
		}
		if ((c->rules & UGL_RULE_NULL) && zend_string_equals_literal_ci(n, "null")) {
			ugl_fail(c, type, "The null type and null are");
		}
	}
	if ((c->rules & UGL_RULE_NULL) && (type->attr & ZEND_TYPE_NULLABLE)) {
		ugl_fail(c, type, "Nullable types and null are");
	}
}

/* ---- strings: only literals in value positions, never names ---- */

static bool ugl_is_value_position(zend_ast_kind parent, uint32_t idx)
{
	switch (parent) {
		case ZEND_AST_ARG_LIST:
		case ZEND_AST_ARRAY_ELEM:
		case ZEND_AST_EXPR_LIST:
		case ZEND_AST_RETURN:
		case ZEND_AST_ECHO:
		case ZEND_AST_PRINT:
		case ZEND_AST_UNARY_OP:
		case ZEND_AST_UNARY_PLUS:
		case ZEND_AST_UNARY_MINUS:
		case ZEND_AST_BINARY_OP:
		case ZEND_AST_GREATER:
		case ZEND_AST_GREATER_EQUAL:
		case ZEND_AST_AND:
		case ZEND_AST_OR:
		case ZEND_AST_COALESCE:
		case ZEND_AST_CONDITIONAL:
		case ZEND_AST_MATCH_ARM:
		case ZEND_AST_PIPE:
		case ZEND_AST_CAST:
		case ZEND_AST_YIELD:
		case ZEND_AST_UNPACK:
		case ZEND_AST_IF_ELEM:
		case ZEND_AST_SWITCH_CASE:
			return true;
		case ZEND_AST_ASSIGN:
		case ZEND_AST_ASSIGN_OP:
		case ZEND_AST_NAMED_ARG:
		case ZEND_AST_PROP_ELEM:
		case ZEND_AST_CONST_ELEM:
		case ZEND_AST_ENUM_CASE:
		case ZEND_AST_DIM:
		case ZEND_AST_MATCH:
		case ZEND_AST_SWITCH:
			return idx == 1 || (parent == ZEND_AST_MATCH && idx == 0) || (parent == ZEND_AST_SWITCH && idx == 0);
		case ZEND_AST_PARAM:
		case ZEND_AST_ARROW_FUNC:
			return idx == 2;
		case ZEND_AST_STATIC:
			return idx == 1;
		default:
			return false;
	}
}

/* ---- call graph, for the recursion check ---- */

static HashTable *ugl_edges_for(ugl_ctx *c, zend_string *key)
{
	zval *zv = zend_hash_find(&c->graph, key);
	if (!zv) {
		HashTable *ht = emalloc(sizeof(HashTable));
		zend_hash_init(ht, 4, NULL, NULL, 0);
		zval tmp;
		ZVAL_PTR(&tmp, ht);
		zv = zend_hash_add_new(&c->graph, key, &tmp);
	}
	return Z_PTR_P(zv);
}

static zend_string *ugl_key_func(zend_string *name)
{
	zend_string *low = ugl_short_lower(name);
	zend_string *key = zend_strpprintf(0, "f:%s", ZSTR_VAL(low));
	zend_string_release(low);
	return key;
}

static zend_string *ugl_key_method(zend_string *cls_lower, zend_string *method)
{
	zend_string *m = zend_string_tolower(method);
	zend_string *key = zend_strpprintf(0, "m:%s::%s", ZSTR_VAL(cls_lower), ZSTR_VAL(m));
	zend_string_release(m);
	return key;
}

static void ugl_add_edge(ugl_ctx *c, zend_string *callee_key)
{
	if (c->cur_edges) {
		zval one;
		ZVAL_TRUE(&one);
		zend_hash_add(c->cur_edges, callee_key, &one);
	}
	zend_string_release(callee_key);
}

static void ugl_note_call(ugl_ctx *c, zend_ast *ast)
{
	if (!c->cur_edges) {
		return;
	}
	if (ast->kind == ZEND_AST_CALL && ast->child[0] && ast->child[0]->kind == ZEND_AST_ZVAL
	 && Z_TYPE_P(zend_ast_get_zval(ast->child[0])) == IS_STRING) {
		ugl_add_edge(c, ugl_key_func(Z_STR_P(zend_ast_get_zval(ast->child[0]))));
	} else if (ast->kind == ZEND_AST_STATIC_CALL && c->cur_class
	 && ast->child[0] && ast->child[0]->kind == ZEND_AST_ZVAL
	 && ast->child[1] && ast->child[1]->kind == ZEND_AST_ZVAL
	 && Z_TYPE_P(zend_ast_get_zval(ast->child[0])) == IS_STRING
	 && Z_TYPE_P(zend_ast_get_zval(ast->child[1])) == IS_STRING) {
		zend_string *cls = Z_STR_P(zend_ast_get_zval(ast->child[0]));
		zend_string *cls_low = ugl_short_lower(cls);
		if (zend_string_equals_literal(cls_low, "self") || zend_string_equals_literal(cls_low, "static")
		 || zend_string_equals(cls_low, c->cur_class)) {
			ugl_add_edge(c, ugl_key_method(c->cur_class, Z_STR_P(zend_ast_get_zval(ast->child[1]))));
		}
		zend_string_release(cls_low);
	} else if (ast->kind == ZEND_AST_METHOD_CALL && c->cur_class
	 && ast->child[0] && ast->child[0]->kind == ZEND_AST_VAR && ugl_name_is(ast->child[0]->child[0], "this")
	 && ast->child[1] && ast->child[1]->kind == ZEND_AST_ZVAL
	 && Z_TYPE_P(zend_ast_get_zval(ast->child[1])) == IS_STRING) {
		ugl_add_edge(c, ugl_key_method(c->cur_class, Z_STR_P(zend_ast_get_zval(ast->child[1]))));
	}
}

/* "f:name" or "m:class::name" as it is written in source. */
static void ugl_append_key(smart_str *out, zend_string *key)
{
	smart_str_appends(out, ZSTR_VAL(key) + 2);
	smart_str_appends(out, "()");
}

/* Depth-first search for a cycle among the functions and methods of the file. */
static bool ugl_find_cycle(ugl_ctx *c, zend_string *key, HashTable *state, smart_str *path)
{
	zval mark, *edges_zv;

	ZVAL_LONG(&mark, 1);
	zend_hash_add_new(state, key, &mark);
	if (c->depth < 128) {
		c->stack[c->depth++] = key;
	}

	edges_zv = zend_hash_find(&c->graph, key);
	if (edges_zv) {
		HashTable *edges = Z_PTR_P(edges_zv);
		zend_string *callee;
		ZEND_HASH_FOREACH_STR_KEY(edges, callee) {
			if (!zend_hash_exists(&c->graph, callee)) {
				continue; /* not defined in this file */
			}
			zval *st = zend_hash_find(state, callee);
			if (st && Z_LVAL_P(st) == 1) {
				uint32_t from = 0;
				while (from < c->depth && !zend_string_equals(c->stack[from], callee)) {
					from++;
				}
				for (uint32_t k = from; k < c->depth; k++) {
					ugl_append_key(path, c->stack[k]);
					smart_str_appends(path, " -> ");
				}
				ugl_append_key(path, callee);
				return true;
			}
			if (!st && ugl_find_cycle(c, callee, state, path)) {
				return true;
			}
		} ZEND_HASH_FOREACH_END();
	}
	if (c->depth) {
		c->depth--;
	}
	ZVAL_LONG(&mark, 2);
	zend_hash_update(state, key, &mark);
	return false;
}

static void ugl_check_recursion(ugl_ctx *c)
{
	HashTable state;
	zend_string *key;

	zend_hash_init(&state, 8, NULL, NULL, 0);
	ZEND_HASH_FOREACH_STR_KEY(&c->graph, key) {
		if (zend_hash_exists(&state, key)) {
			continue;
		}
		smart_str path = {0};
		c->depth = 0;
		if (ugl_find_cycle(c, key, &state, &path)) {
			smart_str_0(&path);
			char *msg = estrdup(ZSTR_VAL(path.s));
			smart_str_free(&path);
			zend_hash_destroy(&state);
			zend_error_noreturn(E_COMPILE_ERROR,
				"Recursion is not allowed in code declared with kind='%s' (call cycle: %s)",
				ugl_profile_names[c->profile], msg);
		}
		smart_str_free(&path);
	} ZEND_HASH_FOREACH_END();
	zend_hash_destroy(&state);
}

/* ---- the walk ---- */

static void ugl_walk(ugl_ctx *c, zend_ast *ast, zend_ast_kind parent, uint32_t idx);

static void ugl_collect_value_classes(ugl_ctx *c, zend_ast *ast)
{
	if (!ast || ast->kind == ZEND_AST_ZVAL) {
		return;
	}
	if (ast->kind == ZEND_AST_CLASS) {
		zend_ast_decl *d = (zend_ast_decl *) ast;
		if (d->name && ((d->flags & ZEND_ACC_ENUM)
		 || ((d->flags & ZEND_ACC_FINAL) && (d->flags & ZEND_ACC_READONLY_CLASS)))) {
			zval one;
			ZVAL_TRUE(&one);
			zend_string *low = ugl_short_lower(d->name);
			zend_hash_add(&c->value_classes, low, &one);
			zend_string_release(low);
		}
		return;
	}
	if (zend_ast_is_list(ast)) {
		zend_ast_list *list = zend_ast_get_list(ast);
		for (uint32_t i = 0; i < list->children; i++) {
			ugl_collect_value_classes(c, list->child[i]);
		}
	} else if (!zend_ast_is_special(ast)) {
		/* classes live in statement lists and namespace bodies */
		if (ast->kind == ZEND_AST_NAMESPACE) {
			ugl_collect_value_classes(c, ast->child[1]);
		}
	}
}

static void ugl_check_new(ugl_ctx *c, zend_ast *ast)
{
	zend_ast *cls = ast->child[0];

	if (cls && cls->kind == ZEND_AST_CLASS) {
		return; /* anonymous class: reported by the class check */
	}
	if (!cls || cls->kind != ZEND_AST_ZVAL || Z_TYPE_P(zend_ast_get_zval(cls)) != IS_STRING) {
		UGL_FAIL_IF(c, UGL_RULE_DYNAMIC, ast, "Dynamic class names are");
		return;
	}
	if (!(c->rules & UGL_RULE_NEW)) {
		return;
	}
	zend_string *name = Z_STR_P(zend_ast_get_zval(cls));
	zend_string *low = ugl_short_lower(name);
	const char *last = zend_memrchr(ZSTR_VAL(name), '\\', ZSTR_LEN(name));
	zend_string *short_name = last ? zend_string_init(last + 1, ZSTR_VAL(name) + ZSTR_LEN(name) - (last + 1), 0)
		: zend_string_copy(name);
	bool ok = zend_hash_exists(&c->value_classes, low)
		|| zend_is_ugl_class_name(short_name)
		|| zend_string_equals_literal(low, "self")
		|| zend_string_equals_literal(low, "static");
	zend_string_release(low);
	zend_string_release(short_name);
	if (!ok) {
		ugl_fail(c, ast, "Heap objects (new of anything but a final readonly class of this file, vec2/vec3/vec4/mat4) are");
	}
}

static void ugl_walk_params(ugl_ctx *c, zend_ast *params)
{
	if (!params) {
		return;
	}
	zend_ast_list *list = zend_ast_get_list(params);
	for (uint32_t i = 0; i < list->children; i++) {
		zend_ast *p = list->child[i];
		if (!p) {
			continue;
		}
		if (p->attr & ZEND_PARAM_REF) {
			UGL_FAIL_IF(c, UGL_RULE_REF, p, "References are");
		}
		ugl_check_type(c, p->child[0]);
		ugl_walk(c, p->child[2], ZEND_AST_PARAM, 2);
	}
}

static void ugl_walk_decl(ugl_ctx *c, zend_ast *ast)
{
	zend_ast_decl *d = (zend_ast_decl *) ast;

	switch (ast->kind) {
		case ZEND_AST_CLASS: {
			if (c->rules & UGL_RULE_CLASS) {
				bool is_enum = (d->flags & ZEND_ACC_ENUM) != 0;
				if (d->flags & ZEND_ACC_INTERFACE) {
					ugl_fail(c, ast, "Interfaces are");
				}
				if (d->flags & ZEND_ACC_TRAIT) {
					ugl_fail(c, ast, "Traits are");
				}
				if (d->flags & ZEND_ACC_ANON_CLASS) {
					ugl_fail(c, ast, "Anonymous classes are");
				}
				if (!is_enum && !((d->flags & ZEND_ACC_FINAL) && (d->flags & ZEND_ACC_READONLY_CLASS))) {
					ugl_fail(c, ast, "Classes that are not final readonly are");
				}
				if (d->child[0]) {
					ugl_fail(c, ast, "Inheritance (extends) is");
				}
			}
			zend_string *prev_class = c->cur_class;
			c->cur_class = d->name ? ugl_short_lower(d->name) : NULL;
			ugl_walk(c, d->child[2], ZEND_AST_CLASS, 2);
			if (c->cur_class) {
				zend_string_release(c->cur_class);
			}
			c->cur_class = prev_class;
			return;
		}
		case ZEND_AST_FUNC_DECL:
		case ZEND_AST_METHOD: {
			if (d->flags & ZEND_ACC_RETURN_REFERENCE) {
				UGL_FAIL_IF(c, UGL_RULE_REF, ast, "References are");
			}
			HashTable *prev_edges = c->cur_edges;
			zend_string *key = NULL;
			if (ast->kind == ZEND_AST_FUNC_DECL && d->name) {
				key = ugl_key_func(d->name);
			} else if (ast->kind == ZEND_AST_METHOD && d->name && c->cur_class) {
				key = ugl_key_method(c->cur_class, d->name);
			}
			c->cur_edges = key ? ugl_edges_for(c, key) : NULL;
			if (key) {
				zend_string_release(key);
			}
			ugl_walk_params(c, d->child[0]);
			ugl_check_type(c, d->child[3]);
			ugl_walk(c, d->child[2], ast->kind, 2);
			c->cur_edges = prev_edges;
			return;
		}
		case ZEND_AST_CLOSURE:
		case ZEND_AST_ARROW_FUNC:
			UGL_FAIL_IF(c, UGL_RULE_CLOSURE, ast, "Closures and arrow functions are");
			if (d->flags & ZEND_ACC_RETURN_REFERENCE) {
				UGL_FAIL_IF(c, UGL_RULE_REF, ast, "References are");
			}
			if (d->child[1]) {
				zend_ast_list *uses = zend_ast_get_list(d->child[1]);
				for (uint32_t i = 0; i < uses->children; i++) {
					if (uses->child[i] && (uses->child[i]->attr & ZEND_BIND_REF)) {
						UGL_FAIL_IF(c, UGL_RULE_REF, uses->child[i], "References are");
					}
				}
			}
			ugl_walk_params(c, d->child[0]);
			ugl_check_type(c, d->child[3]);
			ugl_walk(c, d->child[2], ast->kind, 2);
			return;
		default:
			/* property hooks */
			for (int i = 0; i < 5; i++) {
				ugl_walk(c, d->child[i], ast->kind, i);
			}
			return;
	}
}

static void ugl_walk(ugl_ctx *c, zend_ast *ast, zend_ast_kind parent, uint32_t idx)
{
	if (!ast) {
		return;
	}
	if (ast->kind == ZEND_AST_ZVAL) {
		if ((c->rules & UGL_RULE_STRING) && Z_TYPE_P(zend_ast_get_zval(ast)) == IS_STRING
		 && ugl_is_value_position(parent, idx)) {
			ugl_fail(c, ast, "String literals are");
		}
		return;
	}
	if (zend_ast_is_special(ast) && !zend_ast_is_decl(ast)) {
		return;
	}
	if (zend_ast_is_decl(ast)) {
		ugl_walk_decl(c, ast);
		return;
	}
	if (zend_ast_is_list(ast)) {
		/* attributes are metadata, not values */
		if (ast->kind == ZEND_AST_ATTRIBUTE_LIST || ast->kind == ZEND_AST_ATTRIBUTE_GROUP
		 || ast->kind == ZEND_AST_USE || ast->kind == ZEND_AST_NAME_LIST) {
			return;
		}
		if (ast->kind == ZEND_AST_ENCAPS_LIST) {
			UGL_FAIL_IF(c, UGL_RULE_STRING, ast, "Interpolated strings are");
		}
		zend_ast_list *list = zend_ast_get_list(ast);
		for (uint32_t i = 0; i < list->children; i++) {
			ugl_walk(c, list->child[i], ast->kind, i);
		}
		return;
	}

	switch (ast->kind) {
		case ZEND_AST_DECLARE:
			/* the declare itself carries the profile name as a string */
			ugl_walk(c, ast->child[1], ast->kind, 1);
			return;
		case ZEND_AST_NAMESPACE:
			ugl_walk(c, ast->child[1], ast->kind, 1);
			return;
		case ZEND_AST_USE:
		case ZEND_AST_GROUP_USE:
		case ZEND_AST_HALT_COMPILER:
		case ZEND_AST_CONST_ENUM_INIT:
			return;
		case ZEND_AST_ASSIGN_REF:
		case ZEND_AST_REF:
			UGL_FAIL_IF(c, UGL_RULE_REF, ast, "References are");
			break;
		case ZEND_AST_ARRAY_ELEM:
			if (ast->attr == 1) {
				UGL_FAIL_IF(c, UGL_RULE_REF, ast, "References are");
			}
			break;
		case ZEND_AST_GLOBAL:
			UGL_FAIL_IF(c, UGL_RULE_GLOBAL, ast, "global statements are");
			break;
		case ZEND_AST_INCLUDE_OR_EVAL:
			if (ast->attr == ZEND_EVAL) {
				UGL_FAIL_IF(c, UGL_RULE_EVAL, ast, "eval is");
			}
			break;
		case ZEND_AST_VAR:
			if (ast->child[0] && ast->child[0]->kind != ZEND_AST_ZVAL) {
				UGL_FAIL_IF(c, UGL_RULE_DYNAMIC, ast, "Variable variables are");
			}
			if (ugl_name_is(ast->child[0], "GLOBALS")) {
				UGL_FAIL_IF(c, UGL_RULE_GLOBAL, ast, "$GLOBALS is");
			}
			break;
		case ZEND_AST_STATIC_PROP:
		case ZEND_AST_PROP:
		case ZEND_AST_NULLSAFE_PROP:
			if (ast->child[1] && (ast->child[1]->kind != ZEND_AST_ZVAL
			 || (ast->child[1]->attr & ZEND_UGL_AST_BRACED_NAME))) {
				UGL_FAIL_IF(c, UGL_RULE_DYNAMIC, ast, "Dynamic property names are");
			}
			if (ast->kind == ZEND_AST_NULLSAFE_PROP) {
				UGL_FAIL_IF(c, UGL_RULE_NULL, ast, "The nullsafe operator and null are");
			}
			break;
		case ZEND_AST_METHOD_CALL:
		case ZEND_AST_NULLSAFE_METHOD_CALL:
		case ZEND_AST_STATIC_CALL:
			if (ast->child[1] && (ast->child[1]->kind != ZEND_AST_ZVAL
			 || (ast->child[1]->attr & ZEND_UGL_AST_BRACED_NAME))) {
				UGL_FAIL_IF(c, UGL_RULE_DYNAMIC, ast, "Dynamic method names are");
			}
			if (ast->kind == ZEND_AST_NULLSAFE_METHOD_CALL) {
				UGL_FAIL_IF(c, UGL_RULE_NULL, ast, "The nullsafe operator and null are");
			}
			ugl_note_call(c, ast);
			break;
		case ZEND_AST_CALL:
			/* $f(...) calls a closure or a name held in a variable: closures are fine
			 * where closures are, the string case is caught by the string rule. */
			if (ast->child[0] && ast->child[0]->kind != ZEND_AST_ZVAL) {
				UGL_FAIL_IF(c, UGL_RULE_CLOSURE, ast, "Calls through variables are");
			}
			ugl_note_call(c, ast);
			break;
		case ZEND_AST_NEW:
			ugl_check_new(c, ast);
			break;
		case ZEND_AST_THROW:
		case ZEND_AST_TRY:
		case ZEND_AST_CATCH:
			UGL_FAIL_IF(c, UGL_RULE_EXCEPTION, ast, "Exceptions (throw, try, catch) are");
			break;
		case ZEND_AST_COALESCE:
		case ZEND_AST_ASSIGN_COALESCE:
			UGL_FAIL_IF(c, UGL_RULE_NULL, ast, "The null coalescing operator and null are");
			break;
		case ZEND_AST_CONST:
			if (ugl_name_is(ast->child[0], "null")) {
				UGL_FAIL_IF(c, UGL_RULE_NULL, ast, "The null type and null are");
			}
			break;
		case ZEND_AST_DIM:
			if (!ast->child[1]) {
				UGL_FAIL_IF(c, UGL_RULE_APPEND, ast, "Array appends ($a[] = ...) are");
			}
			break;
		case ZEND_AST_BINARY_OP:
		case ZEND_AST_ASSIGN_OP:
			if (ast->attr == ZEND_CONCAT) {
				UGL_FAIL_IF(c, UGL_RULE_STRING, ast, "String concatenation and strings are");
			}
			break;
		case ZEND_AST_PROP_GROUP:
			ugl_check_type(c, ast->child[0]);
			break;
		case ZEND_AST_CAST:
			if (ast->attr == IS_STRING) {
				UGL_FAIL_IF(c, UGL_RULE_STRING, ast, "String casts and strings are");
			}
			break;
		case ZEND_AST_MAGIC_CONST:
			if (ast->attr != T_LINE) {
				UGL_FAIL_IF(c, UGL_RULE_STRING, ast, "Magic constants such as __FILE__ and __CLASS__ (strings) are");
			}
			break;
		case ZEND_AST_STATIC:
			/* static $x = ...; keeps its name child out of the value positions */
			ugl_walk(c, ast->child[1], ast->kind, 1);
			return;
		default:
			break;
	}

	uint32_t n = zend_ast_get_num_children(ast);
	for (uint32_t i = 0; i < n; i++) {
		ugl_walk(c, ast->child[i], ast->kind, i);
	}
}

static void ugl_free_edges(zval *zv)
{
	HashTable *ht = Z_PTR_P(zv);
	zend_hash_destroy(ht);
	efree(ht);
}

ZEND_API void zend_ugl_check_profile(zend_ast *file_ast, zend_ugl_profile profile)
{
	ugl_ctx c;

	if (profile == ZEND_UGL_PROFILE_LOGIC) {
		return;
	}
	memset(&c, 0, sizeof(c));
	c.profile = profile;
	c.rules = profile == ZEND_UGL_PROFILE_DATA ? UGL_RULES_DATA
		: (profile == ZEND_UGL_PROFILE_COMMON ? UGL_RULES_COMMON : UGL_RULES_SHADER);
	zend_hash_init(&c.value_classes, 8, NULL, NULL, 0);
	zend_hash_init(&c.graph, 8, NULL, ugl_free_edges, 0);

	ugl_collect_value_classes(&c, file_ast);
	ugl_walk(&c, file_ast, ZEND_AST_STMT_LIST, 0);
	if (c.rules & UGL_RULE_RECURSION) {
		ugl_check_recursion(&c);
	}

	zend_hash_destroy(&c.value_classes);
	zend_hash_destroy(&c.graph);
}

#endif /* ZEND_UGL */
