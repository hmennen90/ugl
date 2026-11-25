Benchmarks for the UGL types. `types_vec_lang.php.in` has the placeholders
`%I32%` and `%F32%`: replace them with `int32`/`float32` for a build with
`--enable-ugl`, or with `int`/`float` to run the same file on a stock build.

    sed 's/%I32%/int32/g; s/%F32%/float32/g' types_vec_lang.php.in > ugl.php
    php -n ugl.php types 1    # groups: types, vec, lang; 2nd argument 1 = vec2/vec3 exist
