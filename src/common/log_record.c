#include <mrtl/common/lag_record.h>
#include <mrtl/common/constants.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static int lag_buffer_init (
        struct LagBuffer * b,
        uint64_t window,
        uint64_t stepping )
{
    b->window   = window;
    b->stepping = stepping;
    b->next     = NULL;

    b->element_count = b->window / b->stepping;

    // Add a few extra elements to the ring buffer, to reduce the chances that
    // it needs to grow.
    ring_buffer_init( &b->rb, b->element_count+4, sizeof(struct LagElement) );

    return 0;
}


static int lag_buffer_set_next ( struct LagBuffer * b, struct LagBuffer * next )
{
    b->next = next;
    return 0;
}


static int lag_buffer_free ( struct LagBuffer * b )
{
    ring_buffer_free( &b->rb );
    return 0;
}


static int lag_buffer_insert (
        struct LagBuffer * b,
        const struct LagElement * e )
{
    struct LagElement * latest_element =
        (struct LagElement *) b->rb.last;

    if ( e->t / b->stepping != latest_element->t / b->stepping )
    {
        // Add a new element
        ring_buffer_push_back( &b->rb, e );

        // Pop items off, and send to next buffer.
        struct LagElement * earliest_element =
            (struct LagElement *) b->rb.first;

        while ( b->window < e->t - earliest_element->t )
        {
            if ( b->next )
            { lag_buffer_insert( b->next, earliest_element ); }

            ring_buffer_pop_front( &b->rb, 1 );

            earliest_element = (struct LagElement *) b->rb.first;
        }
    }
    else
    {
        // Update existing element
        memcpy( latest_element, e, sizeof(struct LagElement) );
    }

    return 0;
}


static int lag_buffer_lookup (
        const struct LagBuffer * b,
        uint64_t t,
        double * dst )
{
    if ( b->rb.len == 0 )  { return -1; }

    const struct LagElement * oldest_element =
        (const struct LagElement *) b->rb.first;

    if ( t < oldest_element->t )  { return -2; }

    // Simple linear reverse search for the first element whose time is
    // less than t.
    for ( unsigned long i = b->rb.len-1; i >= 0; --i )
    {
        const struct LagElement * e =
            (const struct LagElement *) ring_buffer_get( &b->rb, i );

        if ( e->t <= t )
        {
            *dst = e->x;
            break;
        }
    }

    return 0;
}


static int write_lag_buffer_to_string (
        char * dst,
        size_t dst_len,
        const struct LagBuffer * b )
{
    snprintf( dst, dst_len, "buffer window: %llu ms, stepping: %llu ms\n",
            b->window/MILLISECONDS, b->stepping/MILLISECONDS );

    for ( unsigned long i = 0; i < b->rb.len; ++i )
    {
        const struct LagElement * e = (const struct LagElement *) ring_buffer_get( &b->rb, i );
        char str [128];
        snprintf( str, 128, "  %lu  %f\n", e->t, e->x );
        strncat( dst, str, dst_len );
    }

    return 0;
}


int lag_record_init (
        struct LagRecord * r,
        uint64_t min_lag,
        uint64_t max_lag,
        double resolution )
{
    r->min_lag      = min_lag;
    r->max_lag      = max_lag;
    r->resolution   = resolution;
    r->buffer_count = 0;

    uint64_t buf_window   = 2 * r->min_lag;
    uint64_t stepping     = r->min_lag * r->resolution;
    uint64_t total_window = buf_window;

    lag_buffer_init( &r->buffers[0], buf_window, stepping );
    ++r->buffer_count;

    while ( total_window < r->max_lag )
    {
        if ( r->buffer_count >= LAG_RECORD_MAX_BUFFERS )
        {
            fprintf( stderr, "lag_record_init() ran out of buffers before reaching maximum lag." );
            return -1;
        }

        buf_window *= 2;
        stepping    = buf_window * resolution;

        lag_buffer_init( &r->buffers[r->buffer_count], buf_window, stepping );

        lag_buffer_set_next( &r->buffers[r->buffer_count-1], &r->buffers[r->buffer_count] );

        total_window += buf_window;
        ++r->buffer_count;
    }

    return 0;
}


int lag_record_free ( struct LagRecord * r )
{
    for ( uint8_t i = 0; i < r->buffer_count; ++i )
    {
        lag_buffer_free( &r->buffers[i] );
    }

    r->buffer_count = 0;

    return 0;
}


int lag_record_insert (
        struct LagRecord * r,
        uint64_t t,
        double x )
{
    r->t_last = t;

    struct LagElement e = { .t = t, .x = x };

    lag_buffer_insert( &r->buffers[0], &e );

    return 0;
}


int lag_record_lookup (
        const struct LagRecord * r,
        uint64_t t,
        double * dst )
{
    const struct LagBuffer * most_recent_buffer = &r->buffers[0];

    if ( t < r->t_last - r->max_lag )       { return -1; }
    if ( most_recent_buffer->rb.len == 0 )  { return -2; }

    uint64_t buf_max_lag = 0;
    int res = 0;

    for ( uint8_t i = 0; i < r->buffer_count; ++i )
    {
        const struct LagBuffer * b = &r->buffers[i];

        buf_max_lag += b->window;

        if ( r->t_last - buf_max_lag < t )
        {
            res = lag_buffer_lookup( b, t, dst );
            break;
        }
    }

    return res;
}


int write_lag_record_to_string (
        char * dst,
        size_t dst_len,
        const struct LagRecord * r )
{
    snprintf( dst, dst_len, "lag record: %u buffers, min lag: %lu, max lag: %lu, last insert: %lu\n",
            r->buffer_count, (r->t_last-r->min_lag), (r->t_last-r->max_lag), r->t_last );

    for ( uint8_t i = 0; i < r->buffer_count; ++i )
    {
        char str [1024];
        write_lag_buffer_to_string( str, 1024, &r->buffers[i] );
        strncat( dst, str, dst_len );
    }

    return 0;
}

