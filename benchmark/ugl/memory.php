<?php
final readonly class UV2 { public function __construct(public float $x, public float $y) {} }
final readonly class UV3 { public function __construct(public float $x, public float $y, public float $z) {} }
final readonly class UV4 { public function __construct(public float $x, public float $y, public float $z, public float $w) {} }
$N = 500_000;
$cases = [
 'float (zval)'       => fn($i) => 1.5 + $i,
 'array [x,y]'        => fn($i) => [1.5, 2.5 + $i],
 'array [x,y,z]'      => fn($i) => [1.5, 2.5, 3.5 + $i],
 'userland UV2'       => fn($i) => new UV2(1.5, 2.5 + $i),
 'userland UV3'       => fn($i) => new UV3(1.5, 2.5, 3.5 + $i),
 'userland UV4'       => fn($i) => new UV4(1.5, 2.5, 3.5, 4.5 + $i),
];
if (function_exists('vec3')) { $cases += [
 'core vec2'          => fn($i) => vec2(1.5, 2.5 + $i),
 'core vec3'          => fn($i) => vec3(1.5, 2.5, 3.5 + $i),
]; }
foreach ($cases as $k => $f) {
  gc_collect_cycles(); $a = []; $m0 = memory_get_usage();
  for ($i = 0; $i < $N; $i++) $a[] = $f($i);
  $m1 = memory_get_usage();
  // subtract the 16 byte slot of the outer array element
  printf("%-14s %6.1f bytes/value\n", $k, ($m1 - $m0) / $N - 16);
  unset($a);
}
