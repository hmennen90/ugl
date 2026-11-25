--TEST--
float32 declaration type: rounding, coercion, references, reflection, int32
--FILE--
<?php
function p(float32 $a): float32 { return $a; }
function n(?float32 $a = null): ?float32 { return $a; }
function u(float32|string $a): float32|string { return $a; }
class C { public float32 $x = 0.0; public ?float32 $y = null; public function __construct(public float32 $z = 0.0) {} }
function t($f) { try { var_dump($f()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }
t(fn() => p(0.1));            // rounded
t(fn() => p(16777217));       // int -> float32 -> 16777216
t(fn() => p("1.5"));          // weak numeric string
t(fn() => p("abc"));          // TypeError
t(fn() => p(null));           // TypeError
t(fn() => n(null));
t(fn() => n(0.1));
t(fn() => u("x"));
t(fn() => u(0.1));
$c = new C(0.1); var_dump($c->z); $c->x = 0.1; var_dump($c->x); $c->y = 0.1; var_dump($c->y);
t(function() use ($c) { $c->x = "q"; });
t(function() use ($c) { $r = &$c->x; $r = 0.1; return $r; });  // ref: not exactly representable
t(function() use ($c) { $r = &$c->x; $r = 0.5; return $r; });
echo (new ReflectionFunction('p'))->getParameters()[0]->getType(), " ", (new ReflectionFunction('n'))->getReturnType(), " ", (new ReflectionFunction('u'))->getReturnType(), "\n";
var_dump((new ReflectionFunction('p'))->getParameters()[0]->getType()->isBuiltin());
(function(float32 $a) { var_dump($a); })(0.3);
function i32(int32 $x): int32 { return $x; }
function m(int32|float32|null $x): int32|float32|null { return $x; }
for ($k = 0; $k < 3; $k++) { // run hot so the JIT compiles it
  foreach ([5, 2147483647, 2147483648, -2147483649, 0.1, 16777217, "7", null] as $v) { try { var_dump(i32($v)); } catch (TypeError $e) { echo "TE\n"; } try { var_dump(m($v)); } catch (TypeError $e) { echo "TE\n"; } }
}
--EXPECTF--
float(0.10000000149011612)
float(16777216)
TypeError: p(): Argument #%d ($a) must be of type float32, string given, called in %s on line 9
TypeError: p(): Argument #%d ($a) must be of type float32, string given, called in %s on line 10
TypeError: p(): Argument #%d ($a) must be of type float32, null given, called in %s on line 11
NULL
float(0.10000000149011612)
string(1) "x"
float(0.10000000149011612)
float(0.10000000149011612)
float(0.10000000149011612)
float(0.10000000149011612)
TypeError: Cannot assign string to property C::$x of type float32
float(0.10000000149011612)
float(0.5)
float32 ?float32 string|float32
bool(true)
float(0.30000001192092896)
int(5)
int(5)
int(2147483647)
int(2147483647)
TE
float(2147483648)
TE
float(-2147483648)
TE
float(0.10000000149011612)
int(16777217)
int(16777217)
TE
TE
TE
NULL
int(5)
int(5)
int(2147483647)
int(2147483647)
TE
float(2147483648)
TE
float(-2147483648)
TE
float(0.10000000149011612)
int(16777217)
int(16777217)
TE
TE
TE
NULL
int(5)
int(5)
int(2147483647)
int(2147483647)
TE
float(2147483648)
TE
float(-2147483648)
TE
float(0.10000000149011612)
int(16777217)
int(16777217)
TE
TE
TE
NULL
