--TEST--
vec2/vec3: temporaries are reused, operands and shared values stay untouched
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
function mk() { return vec3(1, 1, 1); }
$a = vec3(1, 2, 3); $b = vec3(0.5, 0.5, 0.5);
$r1 = $a * 2 + $a;          // temporary reused
$r2 = ($a + $a) + ($b + $b);
$r3 = mk() + vec3(2, 2, 2);
$r4 = 2 * ($a - $b);
$r5 = ($a * 2) / ($b * 4);
$r6 = $a + $b * 2;
$keep = $a * 2;             // shared by $keep and $tmp below
$r7 = ($tmp = $keep) + $a;  // $keep must stay untouched
function show($n, $v) { echo $n, ": ", $v->x, ",", $v->y, ",", $v->z, "\n"; }
show('a', $a); show('b', $b); show('r1', $r1); show('r2', $r2); show('r3', $r3); show('r4', $r4);
show('r5', $r5); show('r6', $r6); show('keep', $keep); show('tmp', $tmp); show('r7', $r7);
$p = vec3(0, 0, 0); $v = vec3(1, 2, 3);
for ($i = 0; $i < 100; $i++) { $p = $p + $v * 0.5; }
show('p', $p); show('v', $v);
echo spl_object_id($a) === spl_object_id($r1) ? "same\n" : "distinct\n";
try { var_dump($a + [1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { var_dump(vec2(1, 2) * vec3(1, 2, 3)); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
--EXPECTF--
a: 1,2,3
b: 0.5,0.5,0.5
r1: 3,6,9
r2: 3,5,7
r3: 3,3,3
r4: 1,3,5
r5: 1,2,3
r6: 2,3,4
keep: 2,4,6
tmp: 2,4,6
r7: 3,6,9
p: 50,100,150
v: 1,2,3
distinct
Unsupported operand types: vec3 + array
Unsupported operand types: vec2 * vec3
