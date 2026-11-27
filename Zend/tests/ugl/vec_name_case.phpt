--TEST--
Vec2/Vec3/Vec4/Mat4 are plain global classes: in a namespace a user class of the same name stays itself, the built-in one needs \ or use
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
namespace App\Math;

final class Vec3 {
    public function __construct(public float $x = 0.0) {}
}
final class Mat4 {}

function user(Vec3 $v): Vec3 { return new Vec3($v->x + 1); }
function builtin(\Vec3 $v): \Vec3 { return $v + $v; }
function mat(\Mat4 $m): \Mat4 { return $m; }

$u = user(new Vec3(1.0));
var_dump(get_class($u), $u->x);
$b = builtin(\vec3(1, 2, 3));
var_dump(get_class($b), $b->x);
var_dump(get_class(mat(\mat4())));
var_dump(get_class(new Mat4));
var_dump($b instanceof \vec3, $b instanceof Vec3);
try {
    user(\vec3(1, 2, 3));
} catch (\TypeError $e) {
    echo substr($e->getMessage(), 0, 63), "\n";
}
?>
--EXPECT--
string(13) "App\Math\Vec3"
float(2)
string(4) "Vec3"
float(2)
string(4) "Mat4"
string(13) "App\Math\Mat4"
bool(true)
bool(false)
App\Math\user(): Argument #1 ($v) must be of type App\Math\Vec3
