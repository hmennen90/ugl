--TEST--
float32/int32 variance check 1
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php class P { function f(): int { return 1; } } class C extends P { function f(): float32 { return 1.0; } }
--EXPECTF--

Fatal error: Declaration of C::f(): float32 must be compatible with P::f(): int in %s on line %d
