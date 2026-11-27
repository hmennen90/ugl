--TEST--
UGL: $v->dot($w) as one opcode keeps the semantics of the method call
--FILE--
<?php
function t($f) { try { $r = $f(); echo get_debug_type($r), ' ', is_float($r) ? $r : (is_object($r) ? 'obj' : json_encode($r)), "\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; } }
$a = vec3(1,2,3); $b = vec3(4,5,6); $c = vec2(1,1);
t(fn() => $a->dot($b));
t(fn() => $a->dot($c));
t(fn() => $a->dot(1));
t(fn() => $a->dot());
t(fn() => $a->dot($b, $c));
t(fn() => $a->dot(vec3(1,1,1)));
t(fn() => $a->dot($b * 2.0));
$n = null; $i = 5; $s = "str";
t(fn() => $n->dot($b));
t(fn() => $i->dot($b));
t(fn() => $s->dot($b));
t(fn() => $a->Dot($b));
t(fn() => $a->dot(...[$b]));
t(fn() => $a->dot(v: $b));
class U { public function dot($x) { return "U:" . json_encode($x); } }
class P { private function dot($x) { return "private"; } public function in() { return $this->dot(1); } }
class M { public function __call($n, $a) { return "call:$n:" . count($a); } }
class R { public function dot(&$x) { $x = "modified"; return 1; } }
class S { public static function dot($x) { return "static:$x"; } }
$u = new U; $m = new M; $p = new P; $st = new S;
t(fn() => $u->dot(7));
t(fn() => $m->dot(7));
t(fn() => $p->dot(7));
t(fn() => $p->in());
t(fn() => $st->dot(7));
// A method taking its argument by reference is called with the value (and the usual warning): the form
// $v->dot($w) is one opcode that the optimizer reads as not writing to $w.
set_error_handler(function ($no, $msg) { echo 'warning: ', $msg, "\n"; return true; });
function byref() { $r = new R; $v = 'orig'; $r->dot($v); return $v; }
t(fn() => byref());
restore_error_handler();
function thrower() { $o = new class { function dot($x) { throw new RuntimeException("boom"); } }; try { $o->dot(1); } catch (RuntimeException $e) { return $e->getMessage() . ':' . $e->getTrace()[0]['function'] . ':' . $e->getLine(); } }
t(fn() => thrower());
// an argument that changes the receiver keeps the order of the call: the receiver is read first
function order() { $a = vec3(1, 1, 1); $b = vec3(2, 2, 2); $r = $a->dot($a = $b); return $r; }
t(fn() => order());
// a vector used again after the call is untouched, and the result is a plain float
function again() { $a = vec3(1, 2, 3); $b = vec3(0.5, 0.25, 2); $r = $a->dot($b) + $a->dot($b); return [$r, $a->x]; }
t(fn() => again());
?>
--EXPECT--
float 32
TypeError: Vec3::dot(): Argument #1 ($other) must be of type Vec3, Vec2 given
TypeError: Vec3::dot(): Argument #1 ($other) must be of type Vec3, int given
ArgumentCountError: Vec3::dot() expects exactly 1 argument, 0 given
ArgumentCountError: Vec3::dot() expects exactly 1 argument, 2 given
float 6
float 64
Error: Call to a member function dot() on null
Error: Call to a member function dot() on int
Error: Call to a member function dot() on string
float 32
float 32
Error: Unknown named parameter $v
string "U:7"
string "call:dot:1"
Error: Call to private method P::dot() from global scope
string "private"
string "static:7"
warning: R::dot(): Argument #1 ($x) must be passed by reference, value given
string "orig"
string "boom:dot:35"
float 6
array [14,1]
