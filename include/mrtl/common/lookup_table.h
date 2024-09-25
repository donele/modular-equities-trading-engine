#ifndef _MRTL_COMMON_LOOKUP_TABLE_H_
#define _MRTL_COMMON_LOOKUP_TABLE_H_

#include <stddef.h>


struct LookupTableEntry
{
    double x;
    double y;
};


#define MAX_LOOKUP_TABLE_ENTRIES 32

struct LookupTable
{
    struct LookupTableEntry entries [MAX_LOOKUP_TABLE_ENTRIES];
    size_t n;
};


void lookup_table_init ( struct LookupTable * );

size_t lookup_table_insert ( struct LookupTable *, double x, double y );

double lookup_table_interpolate ( const struct LookupTable *, double x );

void lookup_table_print ( const struct LookupTable * );


#endif  // _MRTL_COMMON_LOOKUP_TABLE_H_

