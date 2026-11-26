/* Thread stress test for the JIT in a ZTS build: THREADS threads (default 8) run the same script,
 * each in its own request, while the tracing JIT compiles concurrently.
 *
 *   ./configure --disable-all --enable-ugl --enable-zts --enable-opcache --enable-embed=static && make
 *   cc -O1 -o thr -I. -Imain -ITSRM -IZend -Iext -Iext/date/lib \
 *      benchmark/ugl/threads/thr.c libs/libphp.a -lresolv -lpthread
 *   PHPINI=jit.ini WORK=script.php [THREADS=8] ./thr
 *
 * Every thread must print the same line. run-threads.sh does the whole round: it generates random
 * vector programs, takes the result of the interpreter as the reference and checks every thread
 * against it, with every JIT threshold at 1 so that traces are compiled while the threads run. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


static int N = 8;

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
	pthread_t t[64];
	if (getenv("THREADS") && atoi(getenv("THREADS")) > 0 && atoi(getenv("THREADS")) <= 64) N = atoi(getenv("THREADS"));
	php_embed_module.php_ini_path_override = getenv("PHPINI");
	PHP_EMBED_START_BLOCK(argc, argv)
		for (long i = 0; i < N; i++) pthread_create(&t[i], NULL, worker, (void *) i);
		for (int i = 0; i < N; i++) pthread_join(t[i], NULL);
		printf("all %d threads done\n", N);
	PHP_EMBED_END_BLOCK()
	return 0;
}
