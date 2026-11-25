--TEST--
float32/int32 variance check 4
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php class P { public float32 $x = 1.0; } class C extends P { public float $x = 1.0; }
--EXPECTF--

Fatal error: Type of C::$x must be float32 (as in class P) in %s on line %d
