--TEST--
vec2/vec3 component reads with and without the JIT (x/y/z fast path, mixed classes)
--INI--
opcache.enable_cli=1
opcache.jit=tracing
opcache.jit_buffer_size=64M
opcache.jit_hot_func=1
opcache.jit_hot_loop=1
--EXTENSIONS--
opcache
--FILE--
<?php
class P3 { public $x = 7.5; public $y = 8; public $z = "s"; }
function dot3(vec3 $a, vec3 $b) { return $a->x * $b->x + $a->y * $b->y + $a->z * $b->z; }
function len2(vec2 $a) { return $a->x * $a->x + $a->y * $a->y; }
$a = vec3(0.1, 0.7, 1.3); $b = vec3(0.3, 0.9, 2.1); $c = vec2(0.1, 0.7);
$u = new P3; $o = new stdClass; $o->x = 1.25;
$sum = 0.0; $sum2 = 0.0; $mixed = [];
for ($i = 0; $i < 3000; $i++) {
  $sum += dot3($a, $b); $sum2 += len2($c);
  // the same call site sees different classes
  foreach ([$a, $c, $u, $o] as $k => $obj) { if ($i % 1000 == 0) $mixed[] = [$obj->x, $obj->y ?? null]; }
}
var_dump($sum, $sum2);
echo json_encode($mixed), "\n";
// a temporary as operand is released
function mk() { return vec3(1.5, 2.5, 3.5); }
$t = 0.0; for ($i = 0; $i < 2000; $i++) { $t += mk()->y + vec2(4, 5)->x; }
var_dump($t);
// x/y/z of a vec2: z is not a component
var_dump(isset($c->z));
echo @$c->z === null ? "null\n" : "other\n";
// float32 rounding of the components: sum in double stays double
var_dump((0.1 + 0.7) === ($a->x + $a->y));
$h = vec3(16777217, 0.1, 0.2); var_dump($h->x, $h->y, $h->x + $h->y);
--EXPECTF--
float(10169.999250322151)
float(1499.9999508261144)
[[0.10000000149011612,0.699999988079071],[0.10000000149011612,0.699999988079071],[7.5,8],[1.25,null],[0.10000000149011612,0.699999988079071],[0.10000000149011612,0.699999988079071],[7.5,8],[1.25,null],[0.10000000149011612,0.699999988079071],[0.10000000149011612,0.699999988079071],[7.5,8],[1.25,null]]
float(13000)
bool(false)
null
bool(false)
float(16777216)
float(0.10000000149011612)
float(16777216.1)
