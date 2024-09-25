#ifndef _MRTL_RING_BUFFER_H_
#define _MRTL_RING_BUFFER_H_


struct RingBuffer
{
    char * arr_first;
    char * arr_last;  /* last struct in the array */
    char * arr_end;   /* pointer beyond end of array */
    unsigned long arr_len;  /* length in number of structs, not bytes */
    unsigned int elem_sz;
    
    char * first;  /* first struct in active buffer */
    char * last;   /* last struct in active buffer */
    unsigned long len;  /* active length in number of structs, not bytes */
};


int ring_buffer_init
(
    struct RingBuffer *,
    unsigned long,
    unsigned int
);


int ring_buffer_free ( struct RingBuffer * );

int ring_buffer_clear ( struct RingBuffer * );

int ring_buffer_grow ( struct RingBuffer * );

int ring_buffer_copy ( struct RingBuffer *, const struct RingBuffer * );

int ring_buffer_push_back ( struct RingBuffer *, const void * );

int ring_buffer_pop_front ( struct RingBuffer *, int );

void* ring_buffer_get ( const struct RingBuffer *, unsigned long );

unsigned int ring_buffer_find_time
(
    const struct RingBuffer *,
    unsigned int,
    unsigned int
);


#endif

