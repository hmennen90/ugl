--TEST--
float32/int32 variance check 2
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php class P { function f(float32 $x) {} } class C extends P { function f(int32 $x) {} }
--EXPECTF--

Fatal error: Declaration of C::f(int32 $x) must be compatible with P::f(float32 $x) in %s on line %d
