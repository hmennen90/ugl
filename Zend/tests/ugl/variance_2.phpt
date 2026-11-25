--TEST--
float32/int32 variance check 2
--FILE--
<?php class P { function f(float32 $x) {} } class C extends P { function f(int32 $x) {} }
--EXPECTF--

Fatal error: Declaration of C::f(int32 $x) must be compatible with P::f(float32 $x) in %s on line %d
