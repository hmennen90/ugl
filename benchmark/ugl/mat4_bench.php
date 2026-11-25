<?php
$N = 1_000_000;
function mulMV(array $m, array $v): array { // userland column-major mat4 * vec4 on arrays
  return [$m[0]*$v[0]+$m[4]*$v[1]+$m[8]*$v[2]+$m[12]*$v[3], $m[1]*$v[0]+$m[5]*$v[1]+$m[9]*$v[2]+$m[13]*$v[3],
          $m[2]*$v[0]+$m[6]*$v[1]+$m[10]*$v[2]+$m[14]*$v[3], $m[3]*$v[0]+$m[7]*$v[1]+$m[11]*$v[2]+$m[15]*$v[3]];
}
function mulMM(array $a, array $b): array {
  $o = []; for ($j = 0; $j < 4; $j++) for ($i = 0; $i < 4; $i++) { $s = 0.0; for ($k = 0; $k < 4; $k++) $s += $a[$k*4+$i] * $b[$j*4+$k]; $o[$j*4+$i] = $s; } return $o;
}
$mv = [1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1]; $vv = [1,2,3,1];
$cases = [
 'array mat4*vec4' => function() use ($N,$mv,$vv) { for ($i=0;$i<$N;$i++) $r = mulMV($mv, $vv); },
 'core  mat4*vec4' => function() use ($N) { $m = mat4(1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1); $v = vec4(1,2,3,1); for ($i=0;$i<$N;$i++) $r = $m * $v; },
 'array mat4*mat4' => function() use ($N,$mv) { for ($i=0;$i<$N/4;$i++) $r = mulMM($mv, $mv); },
 'core  mat4*mat4' => function() use ($N) { $m = mat4(1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1); for ($i=0;$i<$N/4;$i++) $r = $m * $m; },
 'core  vec4 + vec4' => function() use ($N) { $a = vec4(1,2,3,4); $b = vec4(4,3,2,1); for ($i=0;$i<$N;$i++) $r = $a + $b; },
];
foreach ($cases as $k => $f) { $b = PHP_FLOAT_MAX; for ($j=0;$j<5;$j++) { $t=hrtime(true); $f(); $b=min($b,(hrtime(true)-$t)/1e6); } printf("  %-18s %7.2f ms\n", $k, $b); }
