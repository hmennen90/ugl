--TEST--
UGL: tracing JIT fuses vectors in typed properties, with the interpreter's results bit for bit
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
function bits(vec2|vec3|vec4 $v): string {
    $s = '';
    foreach (['x', 'y', 'z', 'w'] as $i => $c) {
        if ($i < ($v instanceof vec2 ? 2 : ($v instanceof vec3 ? 3 : 4))) $s .= bin2hex(pack('g', $v->$c));
    }
    return $s;
}

class Body {
    public function __construct(public vec3 $pos, public vec3 $vel, public ?vec3 $opt = null, public vec4 $col = new vec4(0, 0, 0, 1)) {}
    public function step(float $dt): void { $this->pos += $this->vel * $dt; }
    public function step2(float $dt): void { $this->pos = $this->pos + $this->vel * $dt; }
    public function step3(float $dt): void { $this->col = $this->col * 0.5 + vec4(1, 1, 1, 1) * 0.25; }
    public function accel(vec3 $a, float $dt): void { $this->vel += $a * $dt; $this->pos = $this->pos + $this->vel->zyx * $dt; }
}
final class Locked {
    public function __construct(public readonly vec3 $pos, public private(set) vec3 $hidden) {}
    public function move(vec3 $d): void { $this->hidden += $d * 2.0; }
    public function tryMove(vec3 $d): void { $this->pos += $d * 2.0; }
}
class Sub extends Body {}

function run(): array {
    $out = [];
    $b = new Body(vec3(0, 0, 0), vec3(1, 2, 3));
    $s = new Sub(vec3(1, 1, 1), vec3(-0.5, 0.25, 8));
    $keep = null;
    for ($i = 0; $i < 150; $i++) {
        $b->step(0.016);
        $s->step2(0.5);
        $s->accel(vec3(0, -9.8, 0), 0.016);
        $b->step3(1.0);
        $b->pos += $s->vel * 0.1;                 // outside the class, from another object
        $b->pos = $b->pos - $s->pos * 0.001;
        if ($i === 80) { $keep = $b->pos; }       // the property's vector is shared from here on
        if ($i === 100) { $b->opt = vec3(1, 1, 1); }
        if ($b->opt !== null) { $b->opt = $b->opt * 1.01; }
        $out[] = bits($b->pos) . bits($s->pos) . bits($s->vel) . bits($b->col) . ($b->opt ? bits($b->opt) : '-');
    }
    return [md5(implode('', $out)), bits($keep), bits($b->pos)];
}
echo implode(' ', run()), "\n";

// readonly and asymmetric visibility are never changed in place
$l = new Locked(vec3(1, 2, 3), vec3(0, 0, 0));
$msgs = [];
for ($i = 0; $i < 60; $i++) {
    try { $l->tryMove(vec3(1, 1, 1)); } catch (Error $e) { $msgs[$e->getMessage()] = ($msgs[$e->getMessage()] ?? 0) + 1; }
    try { $l->hidden += vec3(1, 1, 1); } catch (Error $e) { $msgs[$e->getMessage()] = ($msgs[$e->getMessage()] ?? 0) + 1; }
    $l->move(vec3(1, 1, 1));
}
ksort($msgs);
foreach ($msgs as $m => $n) echo "$n x $m\n";
echo bits($l->pos), ' ', bits($l->hidden), "\n";

// an uninitialised typed property and a reference to a property take the generic path
class Lazy { public vec3 $v; public vec3 $w; public function __construct() { $this->w = vec3(1, 1, 1); } public function go(): string {
    $r = '';
    for ($i = 0; $i < 40; $i++) {
        try { $this->w += $this->v * 2.0; $r .= 'ok'; } catch (Error $e) { $r .= 'E'; if ($i === 5) { $this->v = vec3(1, 2, 3); } }
    }
    return $r . ' ' . bits($this->w);
} }
echo (new Lazy)->go(), "\n";
$b = new Body(vec3(1, 1, 1), vec3(1, 1, 1));
$ref = &$b->pos;
for ($i = 0; $i < 40; $i++) { $b->step(1.0); }
echo bits($ref), ' ', bits($b->pos), "\n";
?>
--EXPECT--
20755d9bd7f4b01358ff8bf81d161894 68821bc0122b23c2ed655d42 e79277c0e232fac25aaea342
60 x Cannot modify private(set) property Locked::$hidden from global scope
60 x Cannot modify readonly property Locked::$pos
0000803f0000004000004040 0000f0420000f0420000f042
EEEEEEokokokokokokokokokokokokokokokokokokokokokokokokokokokokokokokokokok 00008a420000094300004d43
000024420000244200002442 000024420000244200002442
