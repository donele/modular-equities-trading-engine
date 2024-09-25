#include <mrtl/common/ring_buffer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int ring_buffer_init
(
    struct RingBuffer * _rb,
    unsigned long _len,
    unsigned int _esz
)
{
    _rb->arr_len = _len;
    _rb->elem_sz = _esz;
    _rb->arr_first = NULL;

    ring_buffer_clear( _rb );

    return(0);
}


int ring_buffer_free
(
    struct RingBuffer * _rb
)
{
    if ( _rb )
    {
        _rb->arr_len   = 0;
        _rb->arr_last  = NULL;
        _rb->arr_end   = NULL;
        _rb->first     = NULL;
        _rb->last      = NULL;
        _rb->elem_sz   = 0;
        
        if ( _rb->arr_first )
        {
            free( _rb->arr_first );
            _rb->arr_first = NULL;
        }
    }

    return(0);
}


int ring_buffer_clear
(
    struct RingBuffer * _rb
)
{
    if ( _rb->arr_first )
    {
        free( _rb->arr_first );
    }

    _rb->arr_first = (char*) calloc( _rb->arr_len, _rb->elem_sz );
    _rb->arr_end   = _rb->arr_first + ( _rb->arr_len * _rb->elem_sz );
    _rb->arr_last  = _rb->arr_end - _rb->elem_sz;

    _rb->first = _rb->arr_first;
    _rb->last  = _rb->arr_first;
    _rb->len   = 0;

    return(0);
}


int ring_buffer_copy
(
    struct RingBuffer * _dst,
    const struct RingBuffer * _src
)
{
    _dst->arr_len   = _src->arr_len;
    _dst->elem_sz   = _src->elem_sz;
    _dst->len       = _src->len;

    if (_dst->arr_first)
    {
        free(_dst->arr_first);
    }

    _dst->arr_first = (char*) calloc ( _dst->arr_len, _dst->elem_sz );

    memcpy( _dst->arr_first, _src->arr_first, _dst->arr_len * _dst->elem_sz );

    _dst->arr_end   = _dst->arr_first + ( _dst->arr_len * _dst->elem_sz );
    _dst->arr_last  = _dst->arr_end - _dst->elem_sz;

    _dst->first     = _dst->arr_first;
    _dst->last      = _dst->arr_first + ( (_dst->len-1) * _dst->elem_sz );

    return(0);
}


int ring_buffer_grow
(
    struct RingBuffer * _rb
)
{
    unsigned long n;
    char * p;

    /* grow by the factor below. */
#define RING_BUFFER_GROWTH_FACTOR 1.5
#define RING_BUFFER_DEFAULT_SIZE  100
    n = (_rb->arr_len > 0) ?
        _rb->arr_len * RING_BUFFER_GROWTH_FACTOR :
        RING_BUFFER_DEFAULT_SIZE;

    /* printf("grow: new array size = %d\n", n); */

    p = (char*) calloc( n, _rb->elem_sz );

    if (_rb->last > _rb->first)
    {
        /* data does not wrap around end of array */
        memcpy(p, _rb->first, _rb->last+_rb->elem_sz-_rb->arr_first);
    }
    else
    {
        /* the data wraps around the end of array*/
        memcpy(p, _rb->first, _rb->arr_end-_rb->first);
        memcpy(p+(_rb->arr_end-_rb->first),
                _rb->arr_first,
                _rb->last+_rb->elem_sz-_rb->arr_first);
    }

    free(_rb->arr_first);

    _rb->arr_len   = n;
    _rb->arr_first = p;
    _rb->arr_end   = _rb->arr_first + (_rb->arr_len * _rb->elem_sz);
    _rb->arr_last  = _rb->arr_end - _rb->elem_sz;

    _rb->first = _rb->arr_first;
    _rb->last  = _rb->first + ((_rb->len-1) * _rb->elem_sz);

    return(0);
}


int ring_buffer_pop_front
(
    struct RingBuffer * _rb,
    int _n
)
{
    if ( _n <= 0 )
    {
        /* Do nothing */
    }
    else if ( _n < _rb->len )
    {
        unsigned int nbytes;
        nbytes = _n * _rb->elem_sz;

        if ( nbytes + _rb->first > _rb->arr_last )
        {
            /* wrap around */
            _rb->first = _rb->arr_first +
                (nbytes - (_rb->arr_end - _rb->first));
        }
        else
        {
            _rb->first += nbytes;
        }
        
        _rb->len -= _n;
    }
    else
    {
        /* removing all elements */
        _rb->first = _rb->last;
        _rb->len = 0;
    }

    return(0);
}


int ring_buffer_push_back
(
    struct RingBuffer * _rb,
    const void * _e
)
{
    if ( _rb->len == _rb->arr_len )
    {
        /* ring is filled */
        ring_buffer_grow(_rb);
        ring_buffer_push_back(_rb, _e);
    }
    else if ( 0 == _rb->len )
    {
        /* ring is empty, dont move _rb->last */
        _rb->first = _rb->arr_first;
        _rb->last  = _rb->first;
        memcpy(_rb->last, _e, _rb->elem_sz);
        ++_rb->len;
    }
    else if (_rb->last+_rb->elem_sz > _rb->arr_last)
    {
        /* wrap around, we are about to run off
         * the end of the array.
         * */
        _rb->last = _rb->arr_first;
        memcpy(_rb->last, _e, _rb->elem_sz);
        ++_rb->len;
    }
    else
    {
        /* ring still has room */
        _rb->last += _rb->elem_sz;
        memcpy(_rb->last, _e, _rb->elem_sz);
        ++_rb->len;
    }

    return(0);
}


void* ring_buffer_get
(
    const struct RingBuffer * _rb,
    unsigned long _idx
)
{
    void * p;

    if (_idx < _rb->len)
    {
        unsigned int idxbytes;
        idxbytes = _idx * _rb->elem_sz;

        if ( idxbytes + _rb->first > _rb->arr_last )
        {
            /* wrap around */
            p = _rb->arr_first +
                (idxbytes - (_rb->arr_end - _rb->first));
        }
        else
        {
            p = _rb->first + idxbytes;
        }
    }
    else
    {
        p = NULL;
    }

    return ( p );
}

/*
unsigned int ring_buffer_find_time
(
    const ring_buf * _rb,
    unsigned int _ms,
    unsigned int _i
)
{
    // return the index into ring_buf that
    // has the quote in effect at time _ms.
    // start with a guess of _i.

    const NBBO * q;

    q = (NBBO*) _rb->last;

    // special case, looking for latest state
    if ( q->msecs <= _ms )
    {
        return ( _rb->len-1 );
    }

    // general case, go searching

    // the guess is outside the length,
    // naively jump to the middle.
    if (_i >= _rb->len)
    {
        _i = _rb->len / 2;
    }

    q = ring_buffer_get(_rb, _i);

    if (NULL == q)
    {
        return(0);
    }

    // it could be either direction.
    if ( q->msecs >= _ms )
    {
        // too recent, look backward
        // will return 0 if it cannot go
        // back far enough.  this is intentional.
        while ( _i > 0 )
        {
            q = ring_buffer_get(_rb, _i);

            if (q->msecs < _ms)
            {
                break;
            }
            --_i;
        }
    }
    else if (q->msecs < _ms)
    {
        // too old, look forward
        while ( _i < _rb->len )
        {
            q = ring_buffer_get(_rb, _i);

            if (q->msecs >= _ms)
            {
                break;
            }
            ++_i;
        }
        --_i;
    }

    return(_i);
}
*/

