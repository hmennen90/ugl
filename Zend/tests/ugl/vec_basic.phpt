--TEST--
vec2 and vec3: operators, swizzling, immutability, type names, int32
--FILE--
<?php
namespace Game;
function len(vec3 $v): float32 { return $v->length(); }
function t($f) { try { var_dump($f()); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }
$a = vec3(1, 2, 3); $b = vec3(0.5, 0.25, 2);
var_dump($a);
t(fn() => $a + $b);
t(fn() => $a * 2);
t(fn() => 2 * $a);
t(fn() => $a / $b);
t(fn() => $a - 1);
t(fn() => -$a);
t(fn() => $a == vec3(1, 2, 3));
t(fn() => $a == $b);
t(fn() => $a === $a);
t(fn() => $a->x);
t(fn() => $a->zyx);
t(fn() => $a->xy);
t(fn() => $a->w);
t(fn() => isset($a->xz));
t(fn() => $a->dot($b));
t(fn() => $a->cross($b));
t(fn() => len($a));
t(fn() => len(vec2(1, 2)));
t(function() use ($a) { $a->x = 5; });
t(fn() => $a + vec2(1, 2));
t(fn() => $a % 2);
t(fn() => $a + "x");
t(fn() => new \vec2(3, 4));
t(fn() => (new \vec2(3, 4))->length());
t(fn() => vec3(0.1, 0.2, 0.3)->x);
t(fn() => (array) vec2(1, 2));
t(fn() => clone $a);
t(fn() => $a instanceof vec3);
$c = vec3(1, 1, 1); $c += $a; var_dump($c->x);
$c2 = $c; $c2 *= 2; var_dump($c->x, $c2->x);
function f32(float32 $x): float32 { return $x; }
function i32(int32 $x): int32 { return $x; }
t(fn() => i32(5)); t(fn() => i32(2147483647)); t(fn() => i32(2147483648)); t(fn() => i32(-2147483649));
t(fn() => i32("12")); t(fn() => i32(3.0)); t(fn() => i32(3.5)); t(fn() => i32("x")); t(fn() => i32(null));
function u(int32|float32 $x) { return $x; }
t(fn() => u(7)); t(fn() => u(5000000000)); t(fn() => u(0.1));
echo (new \ReflectionFunction('Game\u'))->getParameters()[0]->getType(), " ", get_class((new \ReflectionFunction('Game\u'))->getParameters()[0]->getType()), "\n";
foreach ((new \ReflectionFunction('Game\u'))->getParameters()[0]->getType()->getTypes() as $ty) echo $ty->getName(), ",";
echo "\n";
interface A { function m(): float; function n(float32 $x); function o(): int; function p(int $x); }
class B implements A { function m(): float32 { return 1.0; } function n(float $x) {} function o(): int32 { return 1; } function p(int32|int $x) {} }
echo "variance ok\n";
--EXPECTF--
object(vec3)#%d (3) {
  ["x"]=>
  float(1)
  ["y"]=>
  float(2)
  ["z"]=>
  float(3)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(1.5)
  ["y"]=>
  float(2.25)
  ["z"]=>
  float(5)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(2)
  ["y"]=>
  float(4)
  ["z"]=>
  float(6)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(2)
  ["y"]=>
  float(4)
  ["z"]=>
  float(6)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(2)
  ["y"]=>
  float(8)
  ["z"]=>
  float(1.5)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(0)
  ["y"]=>
  float(1)
  ["z"]=>
  float(2)
}
object(vec3)#%d (3) {
  ["x"]=>
  float(-1)
  ["y"]=>
  float(-2)
  ["z"]=>
  float(-3)
}
bool(true)
bool(false)
bool(true)
float(1)
object(vec3)#%d (3) {
  ["x"]=>
  float(3)
  ["y"]=>
  float(2)
  ["z"]=>
  float(1)
}
object(vec2)#%d (2) {
  ["x"]=>
  float(1)
  ["y"]=>
  float(2)
}

Warning: Undefined property: vec3::$w in %s on line 19
NULL
bool(true)
float(7)
object(vec3)#%d (3) {
  ["x"]=>
  float(3.25)
  ["y"]=>
  float(-0.5)
  ["z"]=>
  float(-0.75)
}
float(3.7416574954986572)
TypeError: Game\len(): Argument #%d ($v) must be of type vec3, vec2 given, called in %s on line 24
Error: Cannot modify vec3: vectors are immutable
TypeError: Unsupported operand types: vec3 + vec2
TypeError: Unsupported operand types: vec3 % int
TypeError: Unsupported operand types: vec3 + string
object(vec2)#%d (2) {
  ["x"]=>
  float(3)
  ["y"]=>
  float(4)
}
float(5)
float(0.10000000149011612)
array(2) {
  ["x"]=>
  float(1)
  ["y"]=>
  float(2)
}
Error: Trying to clone an uncloneable object of class vec3
bool(true)
float(2)
float(2)
float(4)
int(5)
int(2147483647)
TypeError: Game\i32(): Argument #%d ($x) must be of type int32, int given, called in %s on line 39
TypeError: Game\i32(): Argument #%d ($x) must be of type int32, int given, called in %s on line 39
int(12)
int(3)

Deprecated: Implicit conversion from float 3.5 to int loses precision in %s on line 38
int(3)
TypeError: Game\i32(): Argument #%d ($x) must be of type int32, string given, called in %s on line 40
TypeError: Game\i32(): Argument #%d ($x) must be of type int32, null given, called in %s on line 40
int(7)
float(5000000000)
float(0.10000000149011612)
float32|int32 ReflectionUnionType
int32,float32,
variance ok
