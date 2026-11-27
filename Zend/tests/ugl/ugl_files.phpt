--TEST--
.ugl files are code from the first byte; .php files still need the opening tag
--SKIPIF--
<?php if (!function_exists('vec3')) die('skip UGL build required'); ?>
--FILE--
<?php
$dir = __DIR__ . '/files';
function run(string $args): void {
    $out = [];
    exec(escapeshellarg(PHP_BINARY) . ' -n ' . $args . ' 2>&1', $out);
    foreach ($out as $line) {
        if ($line !== '' && !str_starts_with($line, 'Stack trace') && !str_starts_with($line, '#0')) {
            echo str_replace(__DIR__, '%s', preg_replace('~^(PHP )?(Fatal error: .*) in ((?:[A-Za-z]:)?[\\\\/]\S*[\\\\/])?(\S+) on line (\d+)$~', '$2 in $4 on line $5', $line)), "\n";
        }
    }
}
echo "-- plain file\n";           run(escapeshellarg("$dir/basic.ugl"));
echo "-- shebang line\n";         run(escapeshellarg("$dir/shebang.ugl"));
echo "-- opening tag\n";          run(escapeshellarg("$dir/with_tag.ugl"));
echo "-- include from .ugl\n";    run(escapeshellarg("$dir/include.ugl"));
echo "-- lint\n";                 run('-l ' . escapeshellarg("$dir/basic.ugl"));
echo "-- include from .php\n";
require "$dir/lib.ugl";
echo lib_value(), "\n";
echo "-- .php needs the tag\n";
$tmp = tempnam(sys_get_temp_dir(), 'ugl') . '.php';
file_put_contents($tmp, "echo 1;\n");
$out = [];
exec(escapeshellarg(PHP_BINARY) . ' -n ' . escapeshellarg($tmp) . ' 2>&1', $out);
unlink($tmp);
echo implode("|", $out), "\n";
?>
--EXPECT--
-- plain file
42
-- shebang line
2
-- opening tag
Fatal error: A .ugl file is code from the first byte: remove the opening tag in with_tag.ugl on line 1
-- include from .ugl
21
-- lint
No syntax errors detected in %s/files/basic.ugl
-- include from .php
7
-- .php needs the tag
echo 1;
