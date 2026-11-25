<?php
$N = 2_000_000; $r = [];
$cases = [
 'c = a + b (new object)' => function() use ($N) { $a = vec3(1,2,3); $b = vec3(.5,.25,2); for ($i=0;$i<$N;$i++) $c = $a + $b; },
 'p += v (in place)'      => function() use ($N) { $p = vec3(0,0,0); $v = vec3(1,2,3); for ($i=0;$i<$N;$i++) $p += $v; },
 'p = p + v (in place)'   => function() use ($N) { $p = vec3(0,0,0); $v = vec3(1,2,3); for ($i=0;$i<$N;$i++) $p = $p + $v; },
 'p += v * 0.016'         => function() use ($N) { $p = vec3(0,0,0); $v = vec3(1,2,3); for ($i=0;$i<$N;$i++) $p += $v * 0.016; },
 'array p[i] += v[i]'     => function() use ($N) { $p = [0.,0.,0.]; $v = [1.,2.,3.]; for ($i=0;$i<$N;$i++) { $p[0] += $v[0]; $p[1] += $v[1]; $p[2] += $v[2]; } },
 'floats px += vx'        => function() use ($N) { $px=$py=$pz=0.; $vx=1.;$vy=2.;$vz=3.; for ($i=0;$i<$N;$i++) { $px += $vx; $py += $vy; $pz += $vz; } },
];
foreach ($cases as $k => $f) { $b = PHP_FLOAT_MAX; for ($j=0;$j<5;$j++) { $t=hrtime(true); $f(); $b=min($b,(hrtime(true)-$t)/1e6); } printf("  %-24s %7.2f\n", $k, $b); }
