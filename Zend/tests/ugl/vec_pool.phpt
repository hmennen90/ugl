--TEST--
vec2/vec3: recycled objects keep their values, weakrefs and memory behave
--FILE--
<?php
// stress: recycling must not change values or leak
$sum = 0.0;
for ($i = 0; $i < 300000; $i++) { $a = vec3($i, 1, 2); $b = vec2($i, 2); $c = $a * 2 + $a; $d = $b + $b; $sum += $c->x + $d->y; }
var_dump($sum);
$m0 = memory_get_usage();
for ($i = 0; $i < 300000; $i++) { $a = vec3($i, 1, 2); $c = $a + $a; }
var_dump(memory_get_usage() - $m0 < 200000);
// weak references must still be notified
$v = vec3(1, 2, 3); $wr = WeakReference::create($v); var_dump($wr->get() === $v); unset($v); var_dump($wr->get());
// objects kept alive are never recycled
$keep = []; for ($i = 0; $i < 1000; $i++) { $keep[] = vec3($i, $i, $i); }
for ($i = 0; $i < 5000; $i++) { $t = vec3(-1, -1, -1) + vec3(1, 1, 1); }
$ok = true; foreach ($keep as $i => $k) { if ($k->x !== (float)$i) $ok = false; }
var_dump($ok);
// type confusion between vec2 and vec3 slots
$x = vec3(1, 2, 3); unset($x); $y = vec2(7, 8); var_dump($y instanceof vec2, $y->y, count((array)$y));
var_dump(gc_collect_cycles());
$p = vec3(0, 0, 0); $v = vec3(1, 2, 3); for ($i = 0; $i < 1000; $i++) $p = $p + $v * 0.5; var_dump($p->z);
--EXPECTF--
float(135000750000)
bool(true)
bool(true)
NULL
bool(true)
bool(true)
float(8)
int(2)
int(0)
float(1500)
