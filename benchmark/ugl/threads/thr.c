/* Thread stress test for the JIT in a ZTS build: N threads run the same .ugl file, each in its
 * own request, while the tracing JIT compiles concurrently.
 *
 *   ./configure --disable-all --enable-ugl --enable-zts --enable-embed=static && make
 *   cc -O1 -o thr -I. -Imain -ITSRM -IZend -Iext -Iext/date/lib \
 *      benchmark/ugl/threads/thr.c libs/libphp.a -lresolv -lpthread
 *   printf 'opcache.enable=1\nopcache.jit=tracing\nopcache.jit_buffer_size=64M\n' > jit.ini
 *   PHPINI=$PWD/jit.ini WORK=benchmark/ugl/threads/work.ugl ./thr
 *
 * Every thread must print the same line. Add opcache.jit_hot_loop=1 and the other
 * jit_hot_* settings (all 1) to compile as early as possible. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define N 6

static void *worker(void *arg)
{
	long id = (long) arg;
	ts_resource(0);
	if (php_request_startup() == FAILURE) { fprintf(stderr, "thread %ld: startup failed\n", id); return NULL; }
	zend_first_try {
		zend_file_handle fh;
		zend_stream_init_filename(&fh, getenv("WORK"));
		php_execute_script(&fh);
		zend_destroy_file_handle(&fh);
	} zend_catch {
		fprintf(stderr, "thread %ld: bailout\n", id);
	} zend_end_try();
	php_request_shutdown(NULL);
	ts_free_thread();
	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t t[N];
	php_embed_module.php_ini_path_override = getenv("PHPINI");
	PHP_EMBED_START_BLOCK(argc, argv)
		for (long i = 0; i < N; i++) pthread_create(&t[i], NULL, worker, (void *) i);
		for (int i = 0; i < N; i++) pthread_join(t[i], NULL);
		printf("alle %d Threads fertig\n", N);
	PHP_EMBED_END_BLOCK()
	return 0;
}
