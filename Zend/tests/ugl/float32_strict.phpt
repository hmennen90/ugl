--TEST--
float32 declaration type in strict_types mode
--FILE--
<?php declare(strict_types=1);
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
