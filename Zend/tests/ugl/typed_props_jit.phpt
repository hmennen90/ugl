--TEST--
float32/int32 typed properties with the JIT (inline store and fallback)
--INI--
opcache.enable_cli=1
opcache.jit=tracing
opcache.jit_buffer_size=64M
opcache.jit_hot_func=1
opcache.jit_hot_loop=1
error_reporting=E_ALL & ~E_DEPRECATED
--EXTENSIONS--
opcache
--FILE--
<?php
class P { public float32 $f = 0.0; public int32 $i = 0; public ?float32 $n = null; public float $d = 0.0; }
function run(P $p, $vals) {
  $out = [];
  foreach ($vals as $v) {
    try { $p->f = $v; $out[] = $p->f; } catch (TypeError $e) { $out[] = 'TE'; }
    try { $p->i = $v; $out[] = $p->i; } catch (TypeError $e) { $out[] = 'TE'; }
    try { $p->n = $v; $out[] = $p->n; } catch (TypeError $e) { $out[] = 'TE'; }
  }
  return $out;
}
$p = new P;
for ($k = 0; $k < 50; $k++) { $r = run($p, [0.1, 5, 2147483647, 2147483648, -2147483649, "7", null, 16777217, 1e40, NAN]); }
echo md5(serialize($r)), "\n"; echo json_encode(array_map(fn($x)=>is_float($x)&&is_nan($x)?'nan':$x, $r)), "\n";
// hot monomorphic loops
$p = new P; $x = 0.1;
for ($j = 0; $j < 200; $j++) { $p->f = $x; $p->f = 3.25; }
var_dump($p->f);
$p->f = $x; var_dump($p->f);
for ($j = 0; $j < 200; $j++) { $p->i = $j; }
var_dump($p->i);
try { for ($j = 2147483640; $j < 2147483700; $j++) { $p->i = $j; } } catch (TypeError $e) { echo "TE at ", $j, " last ", $p->i, "\n"; }
$ref = &$p->f; for ($j = 0; $j < 5; $j++) { $p->f = 0.1; } var_dump($ref);
try { $ref = "x"; } catch (TypeError $e) { echo "ref TE\n"; }
--EXPECTF--
2fd664877f0674c1aee1761c67dedcef

float(3.25)
float(0.10000000149011612)
int(199)
TE at 2147483648 last 2147483647
float(0.10000000149011612)
ref TE
