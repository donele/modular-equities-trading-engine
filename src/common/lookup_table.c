#include <mrtl/common/lookup_table.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void lookup_table_init( struct LookupTable * t )
{
    memset( t->entries, 0, sizeof(t->entries) );
    t->n = 0;
}


int compare_lookup_table_entries( const void * a, const void * b )
{
    const struct LookupTableEntry * ea = (const struct LookupTableEntry *) a;
    const struct LookupTableEntry * eb = (const struct LookupTableEntry *) b;

    if      ( ea->x < eb->x ) { return -1; }
    else if ( ea->x > eb->x ) { return  1; }
    else                      { return  0; }
}


size_t lookup_table_insert(
        struct LookupTable * t,
        double x,
        double y )
{
    if ( t->n == MAX_LOOKUP_TABLE_ENTRIES )
    { return 0; }

    struct LookupTableEntry * e = &t->entries[t->n];

    e->x = x;
    e->y = y;
    t->n += 1;

    qsort( t->entries, t->n, sizeof(struct LookupTableEntry), compare_lookup_table_entries );

    return t->n;
}


double lookup_table_interpolate(
        const struct LookupTable * t,
        double x )
{
    double y;
    const struct LookupTableEntry * first = &t->entries[0];
    const struct LookupTableEntry * last  = &t->entries[t->n-1];

    if ( x <= first->x )
    { y = first->y; }
    else if ( last->x <= x )
    { y = last->y; }
    else
    {
        double xp, xn, yp, yn;

        for ( size_t i = 1; i < t->n; ++i )
        {
            if ( x < t->entries[i].x )
            {
                xp = t->entries[i-1].x;
                yp = t->entries[i-1].y;
                xn = t->entries[i].x;
                yn = t->entries[i].y;
                break;
            }
        }

        y = ( ( x - xp ) / ( xn - xp ) ) * ( yn - yp ) + yp;
    }

    return y;
}


void lookup_table_print ( const struct LookupTable * t )
{
    printf( "Lookup Table: entries = %lu\n", t->n );

    for ( size_t i = 0; i < t->n; ++i )
    {
        printf( "  %f  =  %f\n", t->entries[i].x, t->entries[i].y );
    }
}

