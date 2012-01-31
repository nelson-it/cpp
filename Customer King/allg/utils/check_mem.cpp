#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#if 0
dummy zum checken
#include <stdlib.h>
#include <stdio.h>

#include <new>

#define SIZE 100000
static void *values[SIZE];
static int    sizes[SIZE];
static int init = 1;


void *operator new(size_t size) throw(std::bad_alloc)
{
    unsigned int i;

    if ( init )
	for ( i=0; i< SIZE; ++i)
            values[i] = NULL;

    void *val = malloc(size);
    for ( i=0; i< SIZE; ++i)
        if ( values[i] == NULL )
        {
            values[i] = val;
            sizes[i] = size;
            break;
        }
    return val;
}

void operator delete(void *val) throw()
{
    unsigned int i;

    for ( i=0; i<SIZE; ++i)
        if ( values[i] == val ) { values[i] = NULL; break; }
    free(val);
}

void operator delete[](void *val) throw()
{
    unsigned int i;

    for ( i=0; i<SIZE; ++i)
        if ( values[i] == val ) { values[i] = NULL; break; }
    free(val);
}

void mem_restart()
{
    unsigned int i;

    for ( i=0; i< SIZE; ++i)
	values[i] = NULL;
}

void mem_check()
{
    unsigned int i;
    fprintf(stderr, "checkmem\n");
    for ( i=0; i< SIZE; ++i)
	if ( values[i] != NULL ) 
            fprintf(stderr, "not deleted %x %d\n", values[i], sizes[i]);
}

#endif
