--TEST--
float32/int32 variance check 5
--FILE--
<?php class P { function f(): float32|int32 { return 1; } } class C extends P { function f(): int32 { return 1; } } echo "ok";
--EXPECTF--
ok