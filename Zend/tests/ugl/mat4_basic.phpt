--TEST--
mat4 and vec4: products in both orders, transpose, inverse, errors
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
function near($a, $b) { return abs($a - $b) < 1e-4; }
function mchk($name, $m, array $e) { $ok = true; for ($i = 0; $i < 16; $i++) if (!near($m->get(intdiv($i, 4), $i % 4), $e[$i])) $ok = false; echo $name, ": ", $ok ? "ok" : "WRONG", "\n"; }
function vchk($name, $v, array $e) { $ok = true; foreach (['x','y','z','w'] as $i => $k) if (!near($v->$k, $e[$i])) $ok = false; echo $name, ": ", $ok ? "ok" : "WRONG", "\n"; }
$I = mat4();
mchk('identity', $I, [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
$t = mat4(1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1);        // translation by (5,6,7), column-major
vchk('t * point', $t * vec4(1, 2, 3, 1), [6, 8, 10, 1]);
vchk('t * direction', $t * vec4(1, 2, 3, 0), [1, 2, 3, 0]);
vchk('row vector', vec4(1, 2, 3, 1) * $t, [1, 2, 3, 5 + 12 + 21 + 1]);
$s = mat4(2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1);
mchk('t * s', $t * $s, [2,0,0,0, 0,3,0,0, 0,0,4,0, 5,6,7,1]);
mchk('s * t', $s * $t, [2,0,0,0, 0,3,0,0, 0,0,4,0, 10,18,28,1]);
mchk('scalar', $s * 2, [4,0,0,0, 0,6,0,0, 0,0,8,0, 0,0,0,2]);
mchk('scalar left', 2 * $s, [4,0,0,0, 0,6,0,0, 0,0,8,0, 0,0,0,2]);
mchk('div', $s / 2, [1,0,0,0, 0,1.5,0,0, 0,0,2,0, 0,0,0,0.5]);
mchk('add', $I + $I, [2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2]);
mchk('sub', $s - $I, [1,0,0,0, 0,2,0,0, 0,0,3,0, 0,0,0,0]);
var_dump(near($s->determinant(), 24));
mchk('s * s^-1', $s * $s->inverse(), [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
$r = mat4(0.8,0.6,0,0, -0.6,0.8,0,0, 0,0,1,0, 1,2,3,1);
mchk('r * r^-1', $r * $r->inverse(), [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
mchk('transpose', $t->transpose(), [1,0,0,5, 0,1,0,6, 0,0,1,7, 0,0,0,1]);
vchk('column', $t->column(3), [5, 6, 7, 1]);
var_dump($t == mat4(1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1), $t == $I);
function tryit($f) { try { var_dump($f()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }
tryit(fn() => mat4(1, 2, 3));
tryit(fn() => mat4(...array_fill(0, 16, "x")));
tryit(fn() => mat4(0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0)->inverse());
tryit(fn() => $t->get(4, 0));
tryit(fn() => $t->column(-1));
tryit(fn() => $t * vec3(1, 2, 3));
tryit(fn() => $t / $t);
tryit(fn() => $t * "x");
tryit(function() use ($t) { $t->m0 = 1; });
tryit(fn() => clone $t);
// vec4 basics
$w = vec4(1, 2, 3, 4);
var_dump($w->w, $w->wzyx->x, $w->xyzw->w, isset($w->w), vec3(1, 2, 3)->w ?? 'none');
vchk('vec4 ops', $w * 2 - vec4(1, 1, 1, 1), [1, 3, 5, 7]);
var_dump(near($w->dot($w), 30), near($w->length(), sqrt(30)));
var_dump((array) $w);
var_dump($w instanceof vec4, $t instanceof mat4);
echo (new ReflectionFunction('mat4'))->getReturnType(), " ", (new ReflectionMethod('mat4', 'column'))->getReturnType(), "\n";
--EXPECTF--
identity: ok
t * point: ok
t * direction: ok
row vector: ok
t * s: ok
s * t: ok
scalar: ok
scalar left: ok
div: ok
add: ok
sub: ok
bool(true)
s * s^-1: ok
r * r^-1: ok
transpose: ok
column: ok
bool(true)
bool(false)
ArgumentCountError: mat4() expects 0 or 16 arguments, 3 given
TypeError: mat4(): Argument #%d must be of type float, string given
ValueError: Mat4 is not invertible
ValueError: Mat4::get(): Argument #%d ($column) must be between 0 and 3
ValueError: Mat4::column(): Argument #%d ($column) must be between 0 and 3
TypeError: Unsupported operand types: Mat4 * Vec3
TypeError: Unsupported operand types: Mat4 / Mat4
TypeError: Unsupported operand types: Mat4 * string
Error: Cannot modify Mat4: values are immutable
Error: Trying to clone an uncloneable object of class Mat4
float(4)
float(4)
float(4)
bool(true)
string(4) "none"
vec4 ops: ok
bool(true)
bool(true)
array(4) {
  ["x"]=>
  float(1)
  ["y"]=>
  float(2)
  ["z"]=>
  float(3)
  ["w"]=>
  float(4)
}
bool(true)
bool(true)
Mat4 Vec4
