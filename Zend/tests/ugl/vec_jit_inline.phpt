--TEST--
UGL: tracing JIT inlines vec2/3/4 arithmetic and gives the interpreter's results bit for bit
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip opcache required');
if (!function_exists('vec3')) die('skip UGL only');
?>
--INI--
opcache.enable_cli=1
opcache.jit=tracing
opcache.jit_buffer_size=16M
opcache.jit_hot_loop=1
opcache.jit_hot_func=1
opcache.jit_hot_side_exit=1
opcache.file_update_protection=0
--FILE--
<?php
function bits(vec3|vec4|vec2 $v): string {
    $s = '';
    foreach (['x', 'y', 'z', 'w'] as $i => $c) {
        if ($i < ($v instanceof vec2 ? 2 : ($v instanceof vec3 ? 3 : 4))) $s .= bin2hex(pack('g', $v->$c));
    }
    return $s;
}

function run(): array {
    $out = [];
    $a = vec3(1.1, 2.2, 3.3); $b = vec3(0.7, -1.3, 9.9); $r = vec3(0, 0, 0);
    $a4 = vec4(1.1, 2.2, 3.3, 4.4); $b4 = vec4(0.1, 0.2, 0.3, 0.7); $r4 = vec4(0, 0, 0, 0);
    $a2 = vec2(1.1, 2.2); $b2 = vec2(0.5, 0.0); $r2 = vec2(0, 0);
    for ($i = 0; $i < 200; $i++) {
        $r = $a + $b; $r = $r - $a; $r = $r * $b; $r = $r / $b;
        $r4 = $a4 + $b4; $r4 = $r4 * $b4; $r4 = $r4 / $b4;
        $r2 = $a2 + $b2; $r2 = $r2 / $b2;          // x / 0.0: INF
        $a = $a + $b;                              // target is an operand
        $keep = $r;                                // two references: $r must stay untouched
        $r = $a - $b;
        $a4 += $b4; $a2 -= $b2;                    // in place
        $out[] = bits($keep) . bits($r) . bits($r4) . bits($r2) . bits($a4) . bits($a2);
    }
    return [md5(implode('', $out)), bits($a), bits($keep), bits($r)];
}
$first = run();
$second = run();
var_dump($first === $second);
echo implode(' ', $first), "\n";

// The class changes while the trace is hot: the guards must fall back to the handler.
function mixed(array $items): array {
    $res = [];
    foreach ($items as [$p, $q]) {
        $t = vec2(0, 0);
        $t = $p + $q;
        $res[] = get_debug_type($t);
    }
    return $res;
}
$items = [];
for ($i = 0; $i < 40; $i++) $items[] = [vec3(1, 2, 3), vec3(1, 1, 1)];
$items[] = [vec2(1, 2), vec2(3, 4)];
$items[] = [vec3(1, 2, 3), vec3(1, 1, 1)];
var_dump(array_count_values(mixed($items)));

// A shared target is never modified in place.
function shared(): array {
    $a = vec3(1, 1, 1); $b = vec3(2, 2, 2); $r = vec3(0, 0, 0); $copy = $r;
    for ($i = 0; $i < 50; $i++) { $r = $a + $b; }
    return [$copy->x, $r->x];
}
var_dump(shared());

// A reference variable takes the generic path.
function ref(): float {
    $a = vec3(1, 1, 1); $b = vec3(2, 2, 2); $r = vec3(0, 0, 0); $alias = &$r;
    for ($i = 0; $i < 50; $i++) { $r = $a + $b; }
    return $alias->x;
}
var_dump(ref());
// A vector with a float or an integer, constant or variable; the scalar's type changes mid-loop.
function scalars(): array {
    $out = [];
    $a = vec3(1.1, -2.2, 3.3); $r = vec3(0, 0, 0);
    $a4 = vec4(1.5, 2.5, 3.5, 4.5); $r4 = vec4(0, 0, 0, 0);
    $f = 0.016; $n = 3;
    for ($i = 0; $i < 120; $i++) {
        $r = $a * 0.5; $o1 = bits($r);
        $r = 2 * $a; $o2 = bits($r);
        $r = $a / 3; $o3 = bits($r);
        $r = $a + $f; $o4 = bits($r);
        $r = $f - $a; $o5 = bits($r);
        $r = $a * $n; $o6 = bits($r);
        $r4 = $a4 / 0; $o7 = bits($r4);          // division by integer zero: INF lanes
        $a4 *= 0.1; $a4 += 1.0; $o8 = bits($a4);
        if ($i === 60) { $f = 7; $n = 0.25; }    // double <-> int swap while the trace is hot
        $out[] = $o1 . $o2 . $o3 . $o4 . $o5 . $o6 . $o7 . $o8;
    }
    return [md5(implode('', $out)), bits($a4)];
}
echo implode(' ', scalars()), "\n";

// Runs of operations: the intermediate vectors stay in registers, the result must not change.
function chains(): array {
    $out = [];
    $p = vec3(0, 0, 0); $v = vec3(1, 2, 3); $w = vec3(-0.5, 0.25, 8); $q = vec3(0, 0, 0);
    $p4 = vec4(1, 1, 1, 1); $v4 = vec4(0.1, 0.2, 0.3, 0.4); $q4 = vec4(0, 0, 0, 0);
    $keep = null;
    for ($i = 0; $i < 150; $i++) {
        $p = $p + $v * 0.016;                    // MUL, ADD, ASSIGN
        $q = ($p + $v) * ($w - $v) / 3;          // two temporaries meet
        $p += $w * 0.5 + $v;                     // longer run into ASSIGN_OP
        $q = $q - 2 * $w;                        // scalar on the left
        $p4 = $p4 * 0.5 + $v4 * $v4;
        $q4 = $p4 / ($v4 + 1.0);
        if ($i === 100) { $keep = $p; }          // $p shared from here on: runs must not touch it
        $o = bits($p) . bits($q) . bits($p4) . bits($q4);
        $out[] = $o;
    }
    return [md5(implode('', $out)), bits($p), bits($keep)];
}
echo implode(' ', chains()), "\n";

// A run with a failing step: the class of an operand changes while the trace is hot.
function broken(): string {
    $r = [];
    $a = vec3(1, 2, 3); $b = vec3(1, 1, 1); $c = vec2(1, 1); $t = vec3(0, 0, 0);
    for ($i = 0; $i < 60; $i++) {
        $o = $i < 50 ? $b : $c;
        try {
            $t = $a + $o * 2.0;
            $r[] = get_debug_type($t);
        } catch (Throwable $e) {
            $r[] = get_class($e);
        }
    }
    return implode(',', array_count_values($r) ? array_map(fn($k, $n) => "$k=$n", array_keys(array_count_values($r)), array_count_values($r)) : []);
}
echo broken(), "\n";

// The target is still unset when the trace is recorded (first iteration), and holds a scalar later.
function fresh(): string {
    $a = vec3(1, 2, 3); $b = vec3(0.5, 0.25, 2); $s = '';
    for ($i = 0; $i < 80; $i++) {
        $c = $a + $b * 2.0;
        $s .= bits($c);
        if ($i === 40) { $c = 5; }               // not a vector any more: the next round allocates
        if ($i === 41) { $s .= gettype($c); }
    }
    return md5($s);
}
echo fresh(), "\n";

// mat4 and swizzles inside runs, with negative zero, infinities and a shared target.
function mbits(mat4 $m): string {
    $s = '';
    foreach ([vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1)] as $e) {
        $c = $m * $e;
        $s .= bits($c);
    }
    return $s;
}
function matrices(): string {
    $m = mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, -0.0, 7, 1);
    $n = mat4(0.5, -0.0, 2, 0, 0.25, 1, 0, 3, 0, 0, 1e30, 0, 1, 2, 3, 1);
    $v = vec4(1.5, -2.25, 3, 1); $a = vec3(0.1, -0.0, 3); $r = vec4(0, 0, 0, 0); $q = mat4(); $z = vec3(0, 0, 0);
    $s = '';
    for ($i = 0; $i < 80; $i++) {
        $r = $m * $v;
        $q = $m * $n;
        $r = $v * $n + $m * $v * 0.5;
        $m = $m * 0.5 + $n * 0.25;
        $z = $a->zyx * 2.0 + $a;
        $r = $r + $a->xyzx;
        $n *= $m;
        $v *= $q;
        if ($i === 40) { $keep = $q; }
        $s .= bits($r) . bits($z) . bits($v) . mbits($q);
    }
    return md5($s . mbits($m) . mbits($n) . mbits($keep));
}
echo matrices(), "\n";
?>
--EXPECT--
bool(true)
2dceb9f89d2da406388ef80468d1a4e2 87190d4374e680c3b5e9f744 0033333f0066a6bf80661e41 54660c430e4080c3e8acf644
array(2) {
  ["vec3"]=>
  int(41)
  ["vec2"]=>
  int(1)
}
array(2) {
  [0]=>
  float(0)
  [1]=>
  float(3)
}
float(3)
7841ad64461617747e73a0a3d4ef1348 e4388e3fe4388e3fe4388e3fe4388e3f
8111e6a4e1a6d08f448059b3b6277c63 becce5426dc6a14358268444 5dbb9a425bdb59433df63144
vec3=50,TypeError=10
8375eada230b13ec3f66092597502f2c
cf94511f6bfdad085f96c25310eec4a8
