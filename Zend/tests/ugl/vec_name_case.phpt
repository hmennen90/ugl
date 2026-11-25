--TEST--
Only the lower-case names vec2/vec3/vec4/mat4 are the built-in types; a user class Vec3 in a namespace stays itself
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
function builtin(vec3 $v): vec3 { return $v + $v; }
function mat(mat4 $m): mat4 { return $m; }

$u = user(new Vec3(1.0));
var_dump(get_class($u), $u->x);
$b = builtin(\vec3(1, 2, 3));
var_dump(get_class($b), $b->x);
var_dump(get_class(mat(\mat4())));
var_dump(get_class(new Mat4));
try {
    user(\vec3(1, 2, 3));
} catch (\TypeError $e) {
    echo substr($e->getMessage(), 0, 63), "\n";
}
?>
--EXPECT--
string(13) "App\Math\Vec3"
float(2)
string(4) "vec3"
float(2)
string(4) "mat4"
string(13) "App\Math\Mat4"
App\Math\user(): Argument #1 ($v) must be of type App\Math\Vec3
