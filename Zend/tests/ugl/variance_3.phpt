--TEST--
float32/int32 variance check 3
--FILE--
<?php class P { function f(): float32 { return 1.0; } } class C extends P { function f(): float { return 1.0; } }
--EXPECTF--

Fatal error: Declaration of C::f(): float must be compatible with P::f(): float32 in %s on line %d
