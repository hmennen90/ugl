--TEST--
vec2/3/4: chained expressions and compound assignments give the same results on every path
--FILE--
<?php
function chk($name, $v, array $e) { $ok = true; foreach (['x','y','z','w'] as $i => $k) { if (isset($e[$i]) && abs($v->$k - $e[$i]) > 1e-5) $ok = false; } echo $name, ": ", $ok ? "ok" : "WRONG ".json_encode([$v->x, $v->y, $v->z]), "\n"; }
$a = vec3(1, 2, 3); $b = vec3(4, 5, 6); $n = 2; $s = 0.5;
// chains where the producer's result is consumed by the next opcode
$p = vec3(1, 1, 1); $p = $p + $a * $s;               chk('p = p + a*s', $p, [1.5, 2, 2.5]);
$p = vec3(1, 1, 1); $p += $a * $s;                    chk('p += a*s', $p, [1.5, 2, 2.5]);
$p = vec3(1, 1, 1); $p -= $a * 2;                     chk('p -= a*2', $p, [-1, -3, -5]);
$p = vec3(2, 2, 2); $p *= $a * 2;                     chk('p *= a*2', $p, [4, 8, 12]);
$p = vec3(8, 8, 8); $p /= $a * 2;                     chk('p /= a*2', $p, [4, 2, 1.3333333]);
$p = vec3(1, 1, 1); $p = $a * 2 + $p;                 chk('a*2 + p', $p, [3, 5, 7]);
$p = $a * 2 + $b * 3;                                 chk('a*2 + b*3', $p, [14, 19, 24]);
$p = $a + $b + $a;                                    chk('a + b + a', $p, [6, 9, 12]);
$p = $a * 2 * $b;                                     chk('a*2*b', $p, [8, 20, 36]);
$p = ($a + $b) * ($b - $a);                           chk('(a+b)*(b-a)', $p, [15, 21, 27]);
$p = $a * 2 + 1;                                      chk('a*2 + 1', $p, [3, 5, 7]);
$p = $a * 2 + $n;                                     chk('a*2 + n', $p, [4, 6, 8]);
$p = $n * $a - $b;                                    chk('n*a - b', $p, [-2, -1, 0]);
$q = vec2(1, 2); $q = $q + vec2(1, 1) * 3;            chk('vec2', $q, [4, 5]);
$w = vec4(1, 2, 3, 4); $w = $w + vec4(1, 1, 1, 1) * 2; chk('vec4', $w, [3, 4, 5, 6]);
// consumers the producer must not defer to
try { $x = $a * 2 + vec2(1, 1); } catch (TypeError $e) { echo "TE dim\n"; }
try { $x = $a * 2 + [1]; } catch (TypeError $e) { echo "TE array\n"; }
try { $x = @($a * 2 + $undefined); } catch (TypeError $e) { echo "TE undef: ", $e->getMessage(), "\n"; }
// shared values and references
$p = vec3(1, 1, 1); $r = &$p; $p += $a * $s; chk('ref', $r, [1.5, 2, 2.5]);
$p = vec3(1, 1, 1); $keep = $p; $p += $a * $s; chk('p', $p, [1.5, 2, 2.5]); chk('keep', $keep, [1, 1, 1]);
$p = vec3(1, 1, 1); $w = WeakReference::create($p); $p = $p + $a * 2; chk('weak', $p, [3, 5, 7]); var_dump($w->get() === null);
// hot loop: results must not depend on the code path
$p = vec3(0, 0, 0); $v = vec3(1, 2, 3);
for ($i = 0; $i < 1000; $i++) { $p = $p + $v * 0.5; $p += $v * 0.25; $p -= $v * 0.125; }
chk('loop', $p, [625, 1250, 1875]);
function accel(vec3 $pos, vec3 $vel, float $dt): vec3 { return $pos + $vel * $dt; }
$p = vec3(0, 0, 0); for ($i = 0; $i < 500; $i++) $p = accel($p, $v, 0.5); chk('func', $p, [250, 500, 750]);
// alternating branches: the same opcode pair sometimes sees a vector, sometimes a number
$t = 0.0; for ($i = 0; $i < 200; $i++) { $o = $i % 2 ? $a : 3; $m = ($o * 2) + 1; $t += is_object($m) ? $m->x : $m; }
var_dump($t);
--EXPECTF--
p = p + a*s: ok
p += a*s: ok
p -= a*2: ok
p *= a*2: ok
p /= a*2: ok
a*2 + p: ok
a*2 + b*3: ok
a + b + a: ok
a*2*b: ok
(a+b)*(b-a): ok
a*2 + 1: ok
a*2 + n: ok
n*a - b: ok
vec2: ok
vec4: ok
TE dim
TE array
TE undef: Unsupported operand types: vec3 + null
ref: ok
p: ok
keep: ok
weak: ok
bool(true)
loop: ok
func: ok
float(1000)
