Benchmarks for the UGL types. `types_vec_lang.php.in` has the placeholders
`%I32%` and `%F32%`: replace them with `int32`/`float32` for a build with
`--enable-ugl`, or with `int`/`float` to run the same file on a stock build.

    sed 's/%I32%/int32/g; s/%F32%/float32/g' types_vec_lang.php.in > ugl.php
    php -n ugl.php types 1    # groups: types, vec, lang; 2nd argument 1 = vec2/vec3 exist

## JIT runs, fuzz and thread tests

The tracing JIT compiles runs of vector arithmetic as one unit (`zend_jit_ugl_vec_chain`):
`$p = $p + $v * $dt`, `$p += ...`, `mat4` products, swizzles such as `$v->zyx`, `vec2()` / `vec3()`
`vec4()` calls, `$v->dot($w)` / `dot($a, $b)` and vectors in typed properties (`$this->pos += $this->vel * $dt`). Every operand is checked
at run time; anything unexpected runs the original opcodes.

- `vec_jit.ugl` times the cases (best of 5, milliseconds for 2M iterations). Run it with
  `-d opcache.enable_cli=1 -d opcache.jit=tracing -d opcache.jit_buffer_size=64M
  -d opcache.file_update_protection=0`; the last option matters, a file written in the last two
  seconds is otherwise not cached and never reaches the JIT. `-d opcache.enable_cli=0` gives the
  interpreter.
- `compare_php.py UGL PHP` runs the `vec` group of `types_vec_lang.php.in` on UGL and on stock PHP
  (user class instead of `vec3`) and prints the ratios.
- `fuzz/run-fuzz.sh UGL [first [last]]` runs random vector programs (`fuzz/fuzz.php`) with the
  interpreter and with every JIT threshold at 1 and compares the bits of the results, once with the
  vectors in variables and once with half of them in typed properties. Needs no build beyond UGL.
- `threads/run-threads.sh UGL THR` runs such programs on 8 threads of a ZTS build while the JIT
  compiles them and checks every thread against the interpreter. `THR` is built from `threads/thr.c`
  (the comment at its top has the commands). The default is 24 programs, 3 rounds: 576 thread runs.

Not compiled: `$this->pos = $other` keeps the identity of the object and so stays a plain assignment,
and properties that are readonly, have asymmetric visibility or are not typed
`vec2`/`vec3`/`vec4`/`mat4`. A standalone `$d = $a->dot($b)` is one opcode without a call, but not
fused with anything. `$v->dot($w)` is compiled to one opcode only for a plain variable with a simple
argument; a user class with a `dot(&$x)` method gets the value and the usual warning from that form.
