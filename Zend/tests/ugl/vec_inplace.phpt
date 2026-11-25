--TEST--
vec2/vec3: += and p = p + x update unshared vectors in place
--FILE--
<?php
function chk($name, $v, $x, $y, $z) { echo $name, ": ", ($v->x === (float)$x && $v->y === (float)$y && $v->z === (float)$z) ? "ok" : "WRONG ".$v->x.",".$v->y.",".$v->z, "\n"; }
$v = vec3(1, 2, 3);
// $p += expr
$p = vec3(0, 0, 0); $id = spl_object_id($p);
for ($i = 0; $i < 10; $i++) { $p += $v * 0.5; }
chk('compound', $p, 5, 10, 15); echo spl_object_id($p) === $id ? "same object\n" : "new object\n";
// $p = $p + expr
$p = vec3(0, 0, 0); $id = spl_object_id($p);
for ($i = 0; $i < 10; $i++) { $p = $p + $v * 0.5; }
chk('assign', $p, 5, 10, 15); echo spl_object_id($p) === $id ? "same object\n" : "new object\n";
// shared values must not change
$p = vec3(1, 1, 1); $q = $p; $p += $v; chk('p', $p, 2, 3, 4); chk('q', $q, 1, 1, 1);
$p = vec3(1, 1, 1); $q = $p; $p = $p + $v; chk('p2', $p, 2, 3, 4); chk('q2', $q, 1, 1, 1);
$a = $b = vec3(2, 2, 2); $a *= 3; chk('a', $a, 6, 6, 6); chk('b', $b, 2, 2, 2);
$arr = [vec3(1, 1, 1)]; $e = $arr[0]; $arr[0] += $v; chk('arr', $arr[0], 2, 3, 4); chk('e', $e, 1, 1, 1);
// self operations
$p = vec3(1, 2, 3); $p += $p; chk('self+', $p, 2, 4, 6);
$p = vec3(1, 2, 3); $p = $p * $p; chk('self*', $p, 1, 4, 9);
$p = vec3(1, 2, 3); $p = $p - $p; chk('self-', $p, 0, 0, 0);
// weak references block in-place updates
$p = vec3(1, 1, 1); $w = WeakReference::create($p); $p += $v; var_dump($w->get() === null);
$p = vec3(1, 1, 1); $w = WeakReference::create($p); $old = $p; $p = $p + $v; chk('weak-old', $old, 1, 1, 1);
// references and closures
$p = vec3(1, 1, 1); $r = &$p; $p += $v; chk('ref', $r, 2, 3, 4);
$p = vec3(1, 1, 1); $f = function() use ($p) { return $p; }; $p += $v; chk('closure', $f(), 1, 1, 1);
// dimension / type errors leave the variable alone
$p = vec3(1, 1, 1); try { $p += vec2(1, 1); } catch (TypeError $e) { echo "TE\n"; } chk('after error', $p, 1, 1, 1);
// a vec2 never turns into a vec3 or the other way round
$p = vec2(1, 1); $p += 2; var_dump($p instanceof vec2, count((array)$p));
// scalar - vec and scalar / vec keep their operand order
$p = vec3(1, 2, 4); $p = 8 / $p; chk('scalar/vec', $p, 8, 4, 2);
$p = vec3(1, 2, 4); $p = 8 - $p; chk('scalar-vec', $p, 7, 6, 4);
--EXPECTF--
compound: ok
same object
assign: ok
same object
p: ok
q: ok
p2: ok
q2: ok
a: ok
b: ok
arr: ok
e: ok
self+: ok
self*: ok
self-: ok
bool(true)
weak-old: ok
ref: ok
closure: ok
TE
after error: ok
bool(true)
int(2)
scalar/vec: ok
scalar-vec: ok
