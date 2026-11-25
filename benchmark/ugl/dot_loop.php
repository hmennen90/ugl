<?php
final readonly class U3 { public function __construct(public float $x, public float $y, public float $z) {} }
$N = 3_000_000;
$a = $argv[1] ?? 'core';
$t = hrtime(true);
if ($a === 'core') {
  $p = vec3(0.1, 0.7, 1.3); $q = vec3(0.3, 0.9, 2.1); $s = 0.0;
  for ($i = 0; $i < $N; $i++) { $s += $p->x * $q->x + $p->y * $q->y + $p->z * $q->z; }
} elseif ($a === 'method') {
  $p = vec3(0.1, 0.7, 1.3); $q = vec3(0.3, 0.9, 2.1); $s = 0.0;
  for ($i = 0; $i < $N; $i++) { $s += $p->dot($q); }
} elseif ($a === 'user') {
  $p = new U3(0.1, 0.7, 1.3); $q = new U3(0.3, 0.9, 2.1); $s = 0.0;
  for ($i = 0; $i < $N; $i++) { $s += $p->x * $q->x + $p->y * $q->y + $p->z * $q->z; }
} elseif ($a === 'array') {
  $p = [0.1, 0.7, 1.3]; $q = [0.3, 0.9, 2.1]; $s = 0.0;
  for ($i = 0; $i < $N; $i++) { $s += $p[0] * $q[0] + $p[1] * $q[1] + $p[2] * $q[2]; }
} else {
  $px=0.1;$py=0.7;$pz=1.3;$qx=0.3;$qy=0.9;$qz=2.1; $s = 0.0;
  for ($i = 0; $i < $N; $i++) { $s += $px * $qx + $py * $qy + $pz * $qz; }
}
printf("%-7s %8.2f ms  result %.6f\n", $a, (hrtime(true) - $t) / 1e6, $s);
