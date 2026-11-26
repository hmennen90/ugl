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
