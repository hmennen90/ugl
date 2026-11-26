#!/usr/bin/env python3
"""Compares vector code on UGL with the same work written with user classes on stock PHP.

    python3 benchmark/ugl/compare_php.py /path/to/ugl /path/to/php

The php binary needs opcache with the JIT. Both run benchmark/ugl/types_vec_lang.php.in (group
'vec'): UGL with the built-in vec3 (second argument 1), stock PHP with a readonly user class
UV3 (second argument 0). The best of 5 runs per case is compared, with and without the JIT.
"""
import json, subprocess, sys, tempfile, os

if len(sys.argv) != 3:
    sys.exit(__doc__)
ugl, php = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
template = open(os.path.join(here, 'types_vec_lang.php.in')).read()
tmp = tempfile.mkdtemp()
files = {}
for name, i32, f32 in (('ugl', 'int32', 'float32'), ('php', 'int', 'float')):
    path = os.path.join(tmp, name + '.php')
    open(path, 'w').write(template.replace('%I32%', i32).replace('%F32%', f32))
    files[name] = path
JIT = ['-d', 'opcache.enable_cli=1', '-d', 'opcache.jit=tracing', '-d', 'opcache.jit_buffer_size=64M',
       '-d', 'opcache.file_update_protection=0']


def best(binary, script, has_core, jit):
    result = {}
    for _ in range(5):
        out = subprocess.run([binary, '-n'] + (JIT if jit else ['-d', 'opcache.enable_cli=0']) +
                             [script, 'vec', has_core], capture_output=True, text=True).stdout.strip().splitlines()[-1]
        for k, v in json.loads(out[out.index('{'):]).items():
            result[k] = min(result.get(k, 1e9), v)
    return result


rows = [('create', 'user create', 'core create'), ('add', 'user add', 'core add'),
        ('mul scalar', 'user mul scalar', 'core mul scalar'), ('vec2 add', 'user vec2 add', 'core vec2 add'),
        ('dot', 'user dot', 'core dot'), ('particle p + v*s', 'user particle', 'core particle'),
        ('particle p += v*s', 'user particle', 'core particle +=')]
for jit in (True, False):
    php_r = best(php, files['php'], '0', jit)
    ugl_r = best(ugl, files['ugl'], '1', jit)
    print('JIT' if jit else 'Interpreter', '(ms for 2M iterations; PHP user class vs UGL vec3)')
    for name, user, core in rows:
        print(f'  {name:20s} PHP {php_r[user]:7.1f}  UGL {ugl_r[core]:6.1f}  ->  {php_r[user] / ugl_r[core]:5.1f}x')
    print(f"  (PHP with arrays: particle {php_r['array particle']:.1f} ms, with plain floats: {php_r['floats particle']:.1f} ms)")
