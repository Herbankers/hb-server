#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "kech.h"

const pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);

	draw_fini();
	exit(1);
}

int main(int argc, char **argv)
{
	pthread_t dthread;

	if (pthread_create(&dthread, NULL, draw, NULL))
		die("unable to create drawing thread\n");

	pthread_join(dthread, NULL);

	pthread_exit(NULL);

	return 0;
}
