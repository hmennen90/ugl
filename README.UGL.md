# UGL

UGL is the runtime for the Unified Game Language. It is built from the sources of the PHP
language and the Zend Engine, adds game types (`float32`, `int32`, `vec2`, `vec3`, `vec4`,
`mat4`), file profiles (`declare(kind=…)`) and always compiles with strict types.

## Name

The PHP License 3.01 and the Zend License 2.00 do not allow derived products to be called
"PHP" or "Zend", or to carry these names. The product, its binary, its repository and the
strings it shows to users are called **UGL**; they come from `Zend/zend_ugl_branding.h`.
The copyright notices of the base sources stay as they are, and so does this
acknowledgment:

> This product includes PHP software, freely available from <https://www.php.net/software/>.

`PHP_*` constants (`PHP_VERSION`, `PHP_EOL`, …) are language identifiers and keep their
names; `PHP_VERSION` is the language level of the base sources.

## Build

    ./buildconf --force
    ./configure --disable-all --enable-cli --enable-ugl
    make -j

The binary is `sapi/cli/ugl`. The configuration file is `ugl.ini` (or `ugl-cli.ini`).

Without `--enable-ugl` the same tree builds the base language unchanged, except for two
changes that are not switchable: the 64-bit `zend_type.type_mask` and the missing operand
swap for `*` (see below). Such a build is only meant for running the unmodified test suites.

### Thread-safe build (ZTS) and JIT on Apple Silicon

`--enable-zts` works with UGL; the vector pool is per thread. On Apple Silicon the JIT is
enabled in ZTS builds too (upstream turns it off, GH-13400): the JIT buffer is a private
`MAP_JIT` mapping, and each thread switches write access with
`pthread_jit_write_protect_np()` instead of `mprotect()`.

Limit: this is for **one process with several threads** (CLI, embedding, FrankenPHP-style
servers). The private buffer is not shared, so it must not be used by servers that fork
workers and share the opcache segment (`op_array` handlers in shared memory would point into
another process's JIT buffer). `benchmark/ugl/threads/` has a thread stress test.

## Source files

UGL code lives in `.ugl` files. A `.ugl` file is code from the first byte: there is no opening tag
(`<?php`, `<?ugl` and `<?=` do not exist), no closing tag and no inline text. A first line starting with
`#!` is skipped, so scripts can be executable. The extension decides, also for `include` and `require`
from other files; `.php` files keep the opening tag and the old behaviour. An opening tag in a `.ugl`
file is a compile error.

    declare(kind='common');

    function twice(int $x): int
    {
        return $x * 2;
    }

Tools that parse PHP read a `.ugl` file by putting `<?php ` in front of it without a line break, which
keeps the line numbers (the tools in `ugl-tools` do this). Composer's PSR-4 loader and classmap look for
`.php` files; `.ugl` classes need their own autoloader or a `files` entry.

## What differs from the base language

- `strict_types` is always on; `declare(strict_types=0)` is a compile error.
- `float32`, `int32`, `vec2`, `vec3`, `vec4`, `mat4` are reserved names.
- `declare(kind='logic'|'common'|'data'|'shader')` restricts a file (`Zend/zend_ugl_profile.c`).
- `*` no longer swaps its operands for some operand type combinations, so that
  `mat4 * vec4` and `vec4 * mat4` differ. Error messages of `*` name the operands in source order.
- Extensions must be rebuilt: `zend_type.type_mask` is 64 bits wide. The module API number is
  different from the base language on purpose.

## Tests

- UGL tests live in `Zend/tests/ugl/` and need a build with `--enable-ugl`; they skip themselves
  otherwise.
- The unmodified suites (`Zend/tests`, `ext/reflection/tests`, `ext/opcache/tests`) run on a build
  *without* `--enable-ugl`, because strict types change the behaviour of almost any older test.
- Run `run-tests.php` with an ordinary interpreter and pass the UGL binary as
  `TEST_PHP_EXECUTABLE`; the runner itself is not written for strict types.
