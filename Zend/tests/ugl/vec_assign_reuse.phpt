--TEST--
vec2/vec4: the assignment target takes the result, self assignment and sharing stay correct
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
declare(strict_types=1);
$a = vec4(1, 2, 3, 4); $a = $a; var_dump($a->x, $a->w);
function f() { $x = vec2(5, 6); $x = $x; $y = $x; $x = $y; return $x; }
var_dump(f()->y);
$b = vec4(10, 20, 30, 40);
$r = $a + $b; $s = $r; $r = $a + $b; $r = $r + $r;   // $s keeps the old object
echo $s->x, ' ', $r->x, ' ', $a->x, "\n";
$r = vec3(1, 1, 1); $r = $a + $b; echo get_class($r), ' ', $r->w, "\n"; // other size
$w = WeakReference::create($r); $r = $a - $b;
var_dump($w->get() === null);
$q = $a; $q = $q * 2; echo $a->x, ' ', $q->x, "\n";
$ref = &$r; $r = $a + $b; echo $ref->x, "\n";
$arr = [$a]; $a = $a + $b; echo $arr[0]->x, ' ', $a->x, "\n";
for ($i = 0; $i < 3; $i++) { $x = $a + $b; $y[] = $x; }
echo $y[0]->x, ' ', $y[1]->x, ' ', $y[2]->x, "\n";
?>
--EXPECT--
float(1)
float(4)
float(6)
11 22 1
Vec4 44
bool(true)
1 2
11
1 11
21 21 21
