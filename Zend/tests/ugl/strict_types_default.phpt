--TEST--
UGL compiles every file with strict_types; strict_types=0 is not allowed
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
function f(int $x): int { return $x; }
function g(float $x): float { return $x; }
function t(callable $c) { try { var_dump($c()); } catch (TypeError $e) { echo preg_replace('/, called in.*/', '', $e->getMessage()), "\n"; } }

// no declare needed: scalars are not coerced
t(fn() => f("5"));
t(fn() => f(5.0));
t(fn() => f(true));
t(fn() => strlen(123));
t(fn() => str_repeat("ab", "2"));
// int to float widening is the one allowed conversion
t(fn() => g(3));
t(fn() => f(5));
// eval'd code is strict as well
t(fn() => eval('return f("7");'));
// an explicit strict_types=1 is fine, strict_types=0 is a compile error
foreach (['declare(strict_types=1); echo "one\n";', 'declare(strict_types=0); echo "zero\n";'] as $code) {
    $file = tempnam(sys_get_temp_dir(), 'ugl');
    file_put_contents($file, "<?php $code\n");
    $out = [];
    exec(escapeshellarg(PHP_BINARY) . ' -n ' . escapeshellarg($file) . ' 2>&1', $out);
    unlink($file);
    foreach ($out as $line) {
        if ($line !== '' && !str_starts_with($line, 'Stack trace') && !str_starts_with($line, '#0')) {
            echo preg_replace('~ in /.* on line (\d+)$~', ' (line $1)', $line), "\n";
        }
    }
}
?>
--EXPECT--
f(): Argument #1 ($x) must be of type int, string given
f(): Argument #1 ($x) must be of type int, float given
f(): Argument #1 ($x) must be of type int, true given
strlen(): Argument #1 ($string) must be of type string, int given
str_repeat(): Argument #2 ($times) must be of type int, string given
float(3)
int(5)
f(): Argument #1 ($x) must be of type int, string given
one
Fatal error: strict_types=0 is not allowed: UGL always compiles with strict types (line 1)
