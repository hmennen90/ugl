<?php
// Differential fuzz over vec2/3/4, mat4, swizzles, vec2()/vec3() calls and numbers: a random program
// has to give the same bits with and without the JIT.
//
//   ugl fuzz.php x SEED [MODE [PROPS]]
//     MODE   dump    print the generated function after the result
//            script  print a complete script instead of running it (run-threads.sh uses this)
//     PROPS  1       keep about half of the vectors and matrices in typed properties of an object
//
// The float and integer variables change in every iteration (the JIT keeps such variables in registers).
// dot() calls and sums or products of plain floats are part of the scalars.
// A program is only worth comparing when the interpreter finishes it: run-fuzz.sh counts the seeds
// that end in an error. The JIT settings are in run-fuzz.sh.
mt_srand((int)($argv[2] ?? 1));
$vals = ['0.5','1.5','-2.25','3','0.1','7','-0.75','-0.0','0','1e20','-1e-30','0.3333333'];
function num() { global $vals; return $vals[mt_rand(0, count($vals)-1)]; }
function vlit($d) { $c=[]; for($i=0;$i<$d;$i++) $c[]=num(); return "vec$d(".implode(',',$c).")"; }
function mlit() { $c=[]; for($i=0;$i<16;$i++) $c[]=num(); return "mat4(".implode(',',$c).")"; }
$vecs = [2=>['$a2','$b2'], 3=>['$a3','$b3'], 4=>['$a4','$b4']];
$mats = ['$m','$n','$k'];
$sc = ['0.5','2','$f','$g','0.016','3.0','-1.5','16777217','9007199254740993','-9007199254740993','$h','$a3->x','$b4->w','dot($a2, $b2)','dot($a3, $b3)','dot($a4, $b4)','(2.0 * dot($a3, $a3))','($f + dot($a2, $b2))','($f - $f * 0.5)','(1e300 * $f * $f)'];
function scal() { global $sc; return $sc[mt_rand(0,count($sc)-1)]; }
function genV($d, $depth) {
  global $vecs;
  $r = mt_rand(0, 8);
  if ($depth <= 0 || $r == 0) return $vecs[$d][mt_rand(0,1)];
  $op = ['+','-','*','/'][mt_rand(0,3)];
  if ($r == 1) return '('.genV($d,$depth-1)." $op ".scal().')';
  if ($r == 2) return '('.scal()." $op ".genV($d,$depth-1).')';
  if ($r == 3 && $d == 4) return '('.genM($depth-1).' * '.genV(4,$depth-1).')';
  if ($r == 4 && $d == 4) return '('.genV(4,$depth-1).' * '.genM($depth-1).')';
  if ($r == 6 && $d <= 3) { $a=[]; for($i=0;$i<$d;$i++) $a[]=scal(); return "vec$d(".implode(', ',$a).")"; }
  if ($r == 5) { // swizzle of a vector with enough components
    $src = mt_rand($d, 4); $chars = ['x','y','z','w']; $s='';
    for ($i=0;$i<$d;$i++) $s .= $chars[mt_rand(0,$src-1)];
    return '('.$vecs[$src][mt_rand(0,1)].'->'.$s.')';
  }
  return '('.genV($d,$depth-1)." $op ".genV($d,$depth-1).')';
}
function genM($depth) {
  global $mats;
  $r = mt_rand(0, 5);
  if ($depth <= 0 || $r == 0) return $mats[mt_rand(0,2)];
  if ($r == 1) return '('.genM($depth-1).' '.['*','/'][mt_rand(0,1)].' '.scal().')';
  if ($r == 2) return '('.scal().' * '.genM($depth-1).')';
  return '('.genM($depth-1).' '.['+','-','*'][mt_rand(0,2)].' '.genM($depth-1).')';
}
$body='';
for ($s=0;$s<8;$s++) {
  $t = mt_rand(0, 3);
  if ($t == 3) { $tgt=$mats[mt_rand(0,2)]; $e=genM(mt_rand(1,3)); if (mt_rand(0,2)==0) { if (mt_rand(0,1)) $body .= "    $tgt ".['+=','-=','*='][mt_rand(0,2)]." ".genM(mt_rand(0,2)).";\n"; else $body .= "    $tgt ".['*=','/='][mt_rand(0,1)]." ".scal().";\n"; } else $body .= "    $tgt = $e;\n"; continue; }
  $d = $t + 2 > 4 ? 4 : $t + 2; $d = [2,3,4][$t % 3];
  $tgt = $vecs[$d][mt_rand(0,1)];
  $e = genV($d, mt_rand(1,3));
  if (mt_rand(0,2)==0) { $rhs = ($d==4 && mt_rand(0,3)==0) ? genM(mt_rand(0,2)) : (mt_rand(0,1) ? genV($d, mt_rand(0,2)) : scal()); $op = ['+=','-=','*=','/='][mt_rand(0,3)]; if ($d==4 && strpbrk($rhs, 'mnk') !== false && $rhs[0]=='(' ) { $rhs = scal(); } elseif ($d==4 && in_array($rhs, $GLOBALS['mats'])) { $op = '*='; } $body .= "    $tgt $op $rhs;\n"; }
  else $body .= "    $tgt = $e;\n";
}
$out = "";
foreach ([2,3,4] as $d) foreach ([0,1] as $j) { $out .= "pack('g".$d."', ".implode(',', array_map(fn($i)=>$vecs[$d][$j].'->'.['x','y','z','w'][$i], range(0,$d-1))).") . "; }
$outm = ''; foreach ($mats as $m) { $outm .= "pack('g16', ".implode(',', array_map(fn($i)=>"$m"."[$i]", range(0,15))).") . "; }
// mat4 has no ArrayAccess: read components through a helper
$propnames = ['a2'=>'vec2','b2'=>'vec2','a3'=>'vec3','b3'=>'vec3','a4'=>'vec4','b4'=>'vec4','m'=>'mat4','n'=>'mat4','k'=>'mat4'];
$usep = (int)($argv[4] ?? 0);
$code = "function mbits(\$m) { \$s=''; foreach ([vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), vec4(0,0,0,1)] as \$e) { \$c = \$m * \$e; \$s .= pack('g4', \$c->x, \$c->y, \$c->z, \$c->w); } return \$s; }\nfunction run() {\n  \$a2=".vlit(2)."; \$b2=".vlit(2)."; \$a3=".vlit(3)."; \$b3=".vlit(3)."; \$a4=".vlit(4)."; \$b4=".vlit(4)."; \$m=".mlit()."; \$n=".mlit()."; \$k=".mlit()."; \$f=0.25; \$g=3; \$h=-12345678901; \$out=[];\n  for (\$i=0;\$i<100;\$i++) {\n$body    \$f = \$f * 1.001 + 0.0001; \$g = \$g + 1; \$h = \$h + 7;\n    if (\$i==60) { \$keep=\$a3; \$keep2=\$m; }\n    if (\$i==80) { \$f=2; \$g=0.5; }\n    \$out[] = ".$out."'';\n  }\n  return md5(implode('', \$out)).md5(mbits(\$m).mbits(\$n).mbits(\$k));\n}\n";
if ($usep) {
  $cls = "final class O { public vec2 \$a2; public vec2 \$b2; public vec3 \$a3; public vec3 \$b3; public vec4 \$a4; public vec4 \$b4; public mat4 \$m; public mat4 \$n; public mat4 \$k; }\n";
  $pos = strpos($code, 'function run()'); $head = substr($code, 0, $pos); $tail = substr($code, $pos);
  foreach ($propnames as $nm => $t) { if (mt_rand(0,1)) { $tail = preg_replace('/\\$'.$nm.'\\b/', '$o->'.$nm, $tail); } else { $keepvar[$nm]=1; } }
  $code = $head . $tail;
  // the ones left as variables need initialisation of the object properties they are mixed with: all are assigned below
  $code = str_replace('function run() {', 'function run() { $o = new O;', $code);
  foreach ($propnames as $nm => $t) { if (!empty($keepvar[$nm])) { /* plain local */ } }
  $code = $cls . $code;
}
if (($argv[3] ?? '') === 'script') { echo "<?php\n", $code, "echo run(), \"\\n\";\n"; exit; }
$f = sys_get_temp_dir().'/fz2_'.getmypid().'_'.mt_rand().'.php'; file_put_contents($f, "<?php\n".$code); require $f; unlink($f);
try { echo run(), "\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
if (($argv[3] ?? '') === 'dump') echo $code;
