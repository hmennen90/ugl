--TEST--
float32/int32 variance check 5
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php class P { function f(): float32|int32 { return 1; } } class C extends P { function f(): int32 { return 1; } } echo "ok";
--EXPECTF--
ok