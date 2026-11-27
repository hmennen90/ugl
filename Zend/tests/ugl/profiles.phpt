--TEST--
declare(kind=...): what each profile (data, common, shader) rejects and what it lets through
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
$cases = [
	['common', 'ref assign', '$a = 1; $b = &$a;'],
	['common', 'ref param', 'function f(int &$x) {} '],
	['common', 'ref return', 'function &f() { static $x = 1; return $x; }'],
	['common', 'ref foreach', '$a = [1]; foreach ($a as &$v) {}'],
	['common', 'ref array elem', '$a = 1; $b = [&$a];'],
	['common', 'ref closure use', '$a = 1; $f = function() use (&$a) {};'],
	['data', 'ref data', '$a = 1; $b = &$a;'],
	['common', 'global', 'function f() { global $x; }'],
	['common', 'GLOBALS', '$x = $GLOBALS[\'a\'];'],
	['common', 'eval', 'eval(\'1;\');'],
	['data', 'eval data', 'eval(\'1;\');'],
	['common', 'varvar', '$a = 1; $$a = 2;'],
	['common', 'dyn prop', 'final readonly class P { function __construct(public int $x = 1) {} } $p = new P; $n = 1; $v = $p->$n;'],
	['common', 'dyn method', 'final readonly class P { function m(): int { return 1; } } $p = new P; $m = 1; $p->$m();'],
	['shader', 'dyn call', '$f = 1; $f(1);'],
	['common', 'dyn new', '$c = 1; $o = new $c;'],
	['common', 'string literal', '$a = \'x\';'],
	['common', 'string arg', 'function f($x) { return 1; } f(\'a\');'],
	['common', 'string type', 'function f(string $x): int { return 1; }'],
	['common', 'string ret type', 'function f(): string { return 1; }'],
	['common', 'string union', 'function f(int|string $x) {}'],
	['common', 'interp', '$a = 1; $b = "v=$a";'],
	['common', 'concat', '$x = 1; $a = $x . $x;'],
	['common', 'concat assign', '$a = 1; $a .= 2;'],
	['data', 'string in data', '$a = \'x\'; $b = "v=$a"; function f(string $s): ?string { return $s . \'y\'; } echo f(\'a\');'],
	['common', 'null', '$a = null;'],
	['common', 'nullable type', 'function f(?int $x) {}'],
	['common', 'null type', 'function f(int|null $x) {}'],
	['common', 'coalesce', '$a = 1; $b = $a ?? 2;'],
	['common', 'nullsafe', 'final readonly class P { public int $x; function __construct() { $this->x = 1; } } $p = new P; $v = $p?->x;'],
	['common', 'throw', 'function f() { throw new Exception(); }'],
	['common', 'try', 'try { $a = 1; } finally { $b = 2; }'],
	['data', 'exceptions in data', 'function f(int $x): int { if ($x < 0) { throw new InvalidArgumentException(\'x\'); } return $x; } try { f(-1); } catch (Exception $e) { echo \'caught\'; }'],
	['common', 'append', '$a = [1]; $a[] = 2;'],
	['common', 'recursion direct', 'function f(int $n): int { return $n <= 0 ? 0 : f($n - 1); }'],
	['common', 'recursion mutual', 'function a(int $n): int { return $n <= 0 ? 0 : b($n - 1); } function b(int $n): int { return a($n); }'],
	['common', 'recursion method', 'final readonly class C { function m(int $n): int { return $n <= 0 ? 0 : $this->m($n - 1); } }'],
	['common', 'recursion static', 'final readonly class C { static function m(int $n): int { return $n <= 0 ? 0 : self::m($n - 1); } }'],
	['data', 'recursion data ok', 'function f(int $n): int { return $n <= 0 ? 0 : f($n - 1); } echo f(5);'],
	['common', 'interface', 'interface I {}'],
	['common', 'trait', 'trait T {}'],
	['common', 'plain class', 'class C {}'],
	['common', 'final only', 'final class C {}'],
	['common', 'readonly only', 'readonly class C {}'],
	['common', 'abstract', 'abstract class C {}'],
	['common', 'anon class', '$o = new class {};'],
	['common', 'extends', 'final readonly class B {} final readonly class C extends B {}'],
	['common', 'new heap', '$o = new ArrayObject();'],
	['data', 'new heap data', '$o = new ArrayObject();'],
	['shader', 'closure', '$f = function(int $x): int { return $x; };'],
	['shader', 'arrow', '$f = fn(int $x): int => $x;'],
	['common', 'closure ok', '$f = fn(int $x): int => $x * 2; echo $f(2);'],
	['common', 'value class ok', 'final readonly class P { function __construct(public float $x, public float $y) {} function len2(): float { return $this->x * $this->x + $this->y * $this->y; } } $p = new P(3.0, 4.0); echo $p->len2();'],
	['shader', 'value class shader ok', 'final readonly class P { function __construct(public float $x, public float $y) {} function add(P $o): P { return new P($this->x + $o->x, $this->y + $o->y); } } $p = (new P(1.0, 2.0))->add(new P(3.0, 4.0)); echo $p->x, $p->y;'],
	['common', 'enum ok', 'enum Dir: int { case Up = 1; case Down = 2; } echo Dir::Down->value;'],
	['common', 'enum pure', 'enum Dir { case Up; case Down; } echo Dir::Down === Dir::Down ? 1 : 0;'],
	['common', 'vec ok', '$a = vec3(1, 2, 3); $b = $a * 2 + vec3(1, 1, 1); echo $b->x, $b->z; $m = mat4(); $v = $m * vec4(1, 2, 3, 1); echo $v->w; $n = new vec2(1, 2); echo $n->y;'],
	['common', 'match named pipe ok', 'function sq(int $x): int { return $x * $x; } function inc(int $x): int { return $x + 1; } $r = 3 |> sq(...) |> inc(...); echo match(true) { $r > 5 => 1, default => 0 }, sq(x: 4);'],
	['common', 'int32 float32 ok', 'function f(float32 $x, int32 $i): float32 { return $x * $i; } echo f(0.1, 3);'],
	['common', 'fixed array ok', '$a = [1, 2, 3]; $a[1] = 5; $s = 0; foreach ($a as $v) { $s += $v; } echo $s;'],
	['common', 'loops ok', '$s = 0; for ($i = 0; $i < 10; $i++) { if ($i % 2) { continue; } $s += $i; } while ($s > 5) { $s -= 3; } echo $s;'],
	['common', 'static var ok', 'function f(): int { static $n = 0; $n++; return $n; } f(); echo f();'],
	['common', 'const ok', 'const K = 3; final readonly class C { const Z = 2; public function __construct(public int $a = K) {} } echo (new C())->a + C::Z;'],
	['common', 'namespace ok', 'namespace Game; function f(int $x): int { return $x + 1; } echo f(1);'],
	['logic', 'logic unrestricted', '$a = \'x\'; $b = &$a; eval(\'1;\'); echo $a;'],
	['common', 'string in attribute ok', '#[Attribute] final readonly class A { function __construct(public int $v = 0) {} } #[A(v: 1)] function f(): int { return 1; } echo f();'],
	['common', 'braced prop name', 'final readonly class P { function __construct(public int $x = 1) {} } $p = new P; $v = $p->{\'x\'};'],
	['common', 'braced method name', 'final readonly class P { function m(): int { return 1; } } $p = new P; $v = $p->{\'m\'}();'],
	['common', 'plain prop name', 'final readonly class P { function __construct(public int $x = 1) {} } $p = new P; $v = $p->x;'],
	['common', 'arrow fn string', '$f = fn() => \'x\';'],
	['common', 'static string', 'function f() { static $s = \'x\'; }'],
	['common', 'string cast', '$a = (string) 1;'],
	['common', 'int cast', '$a = (int) 1.5;'],
	['common', 'magic file', '$a = __FILE__;'],
	['common', 'magic class', 'final readonly class P { function m() { return __CLASS__; } }'],
	['common', 'magic line', '$a = __LINE__;'],
];
// the declare statement itself
$cases[] = ['logic', 'bad value', "declare(kind='bogus');"];
foreach ($cases as [$profile, $name, $code]) {
    $file = tempnam(sys_get_temp_dir(), 'ugl');
    $header = $name === 'bad value' ? "<?php\n" : "<?php declare(kind='$profile');\n";
    file_put_contents($file, $header . $code . "\n");
    $out = [];
    exec(escapeshellarg(PHP_BINARY) . ' -n ' . escapeshellarg($file) . ' 2>&1', $out);
    unlink($file);
    $first = '';
    foreach ($out as $line) {
        if (str_contains($line, 'Fatal error')) { $first = $line; break; }
    }
    $msg = $first === '' ? 'ok' : preg_replace('~ in (?:[A-Za-z]:)?[\\\\/].* on line (\d+)$~', ' (line $1)', preg_replace('/^.*?Fatal error: /', '', $first));
    echo "$name [$profile]: $msg\n";
}
?>
--EXPECT--
ref assign [common]: References are not allowed in code declared with kind='common' (line 2)
ref param [common]: References are not allowed in code declared with kind='common' (line 2)
ref return [common]: References are not allowed in code declared with kind='common' (line 2)
ref foreach [common]: References are not allowed in code declared with kind='common' (line 2)
ref array elem [common]: References are not allowed in code declared with kind='common' (line 2)
ref closure use [common]: References are not allowed in code declared with kind='common' (line 2)
ref data [data]: References are not allowed in code declared with kind='data' (line 2)
global [common]: global statements are not allowed in code declared with kind='common' (line 2)
GLOBALS [common]: $GLOBALS is not allowed in code declared with kind='common' (line 2)
eval [common]: eval is not allowed in code declared with kind='common' (line 2)
eval data [data]: eval is not allowed in code declared with kind='data' (line 2)
varvar [common]: Variable variables are not allowed in code declared with kind='common' (line 2)
dyn prop [common]: Dynamic property names are not allowed in code declared with kind='common' (line 2)
dyn method [common]: Dynamic method names are not allowed in code declared with kind='common' (line 2)
dyn call [shader]: Calls through variables are not allowed in code declared with kind='shader' (line 2)
dyn new [common]: Dynamic class names are not allowed in code declared with kind='common' (line 2)
string literal [common]: String literals are not allowed in code declared with kind='common' (line 2)
string arg [common]: String literals are not allowed in code declared with kind='common' (line 2)
string type [common]: The string type and strings are not allowed in code declared with kind='common' (line 2)
string ret type [common]: The string type and strings are not allowed in code declared with kind='common' (line 2)
string union [common]: The string type and strings are not allowed in code declared with kind='common' (line 2)
interp [common]: Interpolated strings are not allowed in code declared with kind='common' (line 2)
concat [common]: String concatenation and strings are not allowed in code declared with kind='common' (line 2)
concat assign [common]: String concatenation and strings are not allowed in code declared with kind='common' (line 2)
string in data [data]: ok
null [common]: The null type and null are not allowed in code declared with kind='common' (line 2)
nullable type [common]: Nullable types and null are not allowed in code declared with kind='common' (line 2)
null type [common]: The null type and null are not allowed in code declared with kind='common' (line 2)
coalesce [common]: The null coalescing operator and null are not allowed in code declared with kind='common' (line 2)
nullsafe [common]: The nullsafe operator and null are not allowed in code declared with kind='common' (line 2)
throw [common]: Exceptions (throw, try, catch) are not allowed in code declared with kind='common' (line 2)
try [common]: Exceptions (throw, try, catch) are not allowed in code declared with kind='common' (line 2)
exceptions in data [data]: Heap objects (new of anything but a final readonly class of this file, Vec2/Vec3/Vec4/Mat4) are not allowed in code declared with kind='data' (line 2)
append [common]: Array appends ($a[] = ...) are not allowed in code declared with kind='common' (line 2)
recursion direct [common]: Recursion is not allowed in code declared with kind='common' (call cycle: f() -> f()) (line 1)
recursion mutual [common]: Recursion is not allowed in code declared with kind='common' (call cycle: a() -> b() -> a()) (line 1)
recursion method [common]: Recursion is not allowed in code declared with kind='common' (call cycle: c::m() -> c::m()) (line 1)
recursion static [common]: Recursion is not allowed in code declared with kind='common' (call cycle: c::m() -> c::m()) (line 1)
recursion data ok [data]: ok
interface [common]: Interfaces are not allowed in code declared with kind='common' (line 2)
trait [common]: Traits are not allowed in code declared with kind='common' (line 2)
plain class [common]: Classes that are not final readonly are not allowed in code declared with kind='common' (line 2)
final only [common]: Classes that are not final readonly are not allowed in code declared with kind='common' (line 2)
readonly only [common]: Classes that are not final readonly are not allowed in code declared with kind='common' (line 2)
abstract [common]: Classes that are not final readonly are not allowed in code declared with kind='common' (line 2)
anon class [common]: Anonymous classes are not allowed in code declared with kind='common' (line 2)
extends [common]: Inheritance (extends) is not allowed in code declared with kind='common' (line 2)
new heap [common]: Heap objects (new of anything but a final readonly class of this file, Vec2/Vec3/Vec4/Mat4) are not allowed in code declared with kind='common' (line 2)
new heap data [data]: Heap objects (new of anything but a final readonly class of this file, Vec2/Vec3/Vec4/Mat4) are not allowed in code declared with kind='data' (line 2)
closure [shader]: Closures and arrow functions are not allowed in code declared with kind='shader' (line 2)
arrow [shader]: Closures and arrow functions are not allowed in code declared with kind='shader' (line 2)
closure ok [common]: ok
value class ok [common]: ok
value class shader ok [shader]: ok
enum ok [common]: ok
enum pure [common]: ok
vec ok [common]: ok
match named pipe ok [common]: ok
int32 float32 ok [common]: ok
fixed array ok [common]: ok
loops ok [common]: ok
static var ok [common]: ok
const ok [common]: ok
namespace ok [common]: ok
logic unrestricted [logic]: ok
string in attribute ok [common]: ok
braced prop name [common]: Dynamic property names are not allowed in code declared with kind='common' (line 2)
braced method name [common]: Dynamic method names are not allowed in code declared with kind='common' (line 2)
plain prop name [common]: ok
arrow fn string [common]: String literals are not allowed in code declared with kind='common' (line 2)
static string [common]: String literals are not allowed in code declared with kind='common' (line 2)
string cast [common]: String casts and strings are not allowed in code declared with kind='common' (line 2)
int cast [common]: ok
magic file [common]: Magic constants such as __FILE__ and __CLASS__ (strings) are not allowed in code declared with kind='common' (line 2)
magic class [common]: Magic constants such as __FILE__ and __CLASS__ (strings) are not allowed in code declared with kind='common' (line 2)
magic line [common]: ok
bad value [logic]: kind declaration must have one of the values 'logic', 'common', 'data' or 'shader' (line 2)
