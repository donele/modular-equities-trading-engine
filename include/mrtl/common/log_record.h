#ifndef _MRTL_COMMON_LAG_RECORD_H_
#define _MRTL_COMMON_LAG_RECORD_H_

#include <mrtl/common/ring_buffer.h>
#include <stdint.h>
#include <stddef.h>


struct LagElement
{
    uint64_t t;
    double   x;
};


struct LagBuffer
{
    struct RingBuffer rb;
    struct LagBuffer * next;
    uint64_t window;
    uint64_t stepping;
    uint32_t element_count;
};


#define LAG_RECORD_MAX_BUFFERS 16

struct LagRecord
{
    struct LagBuffer buffers [LAG_RECORD_MAX_BUFFERS];
    uint64_t t_last;
    uint64_t min_lag;
    uint64_t max_lag;
    double resolution;
    uint8_t buffer_count;
};

int lag_record_init( struct LagRecord * r,
        uint64_t min_lag, uint64_t max_lag, double resolution );

int lag_record_free( struct LagRecord * r );

int lag_record_insert( struct LagRecord * r, uint64_t t, double x );

int lag_record_lookup( const struct LagRecord * r, uint64_t t, double * dst );

int write_lag_record_to_string( char * dst, size_t dst_len, const struct LagRecord * r );

#endif  // _MRTL_COMMON_LAG_RECORD_H_

