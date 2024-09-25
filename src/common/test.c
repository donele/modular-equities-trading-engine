#include <mrtl/common/constants.h>
#include <mrtl/common/lookup_table.h>
#include <mrtl/common/lag_record.h>
#include <stdio.h>

int main(int argc, char ** argv )
{
    struct LookupTable tbl;
    lookup_table_init( &tbl );

    lookup_table_insert( &tbl, -10, -10 );
    lookup_table_insert( &tbl, -5, -5 );
    lookup_table_insert( &tbl, 7.2, 113.1982 );
    lookup_table_insert( &tbl, -9, -20 );
    lookup_table_insert( &tbl, 7.3, 114.8 );

    lookup_table_print( &tbl );

    printf( "Interpolation tests:\n" );

    double x;
    x = 17.121;  printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = 7.121;   printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = 7.21;    printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = 36.0;    printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = 498;     printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = -19;     printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = -7;      printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = -0.0019; printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );
    x = 0.019;   printf( "  %f -> %f\n", x, lookup_table_interpolate( &tbl, x ) );


    struct LagRecord lr;
    lag_record_init( &lr, 1*SECONDS, 1*HOURS, 0.1 );

    x = 1.;

    for ( uint64_t t = 9*HOURS; t < 12*HOURS; t += 0.11*SECONDS )
    {
        lag_record_insert( &lr, t, x );
        x += 1.f;
    }

    char str [1024*32];  // this needs to be pretty big.
    write_lag_record_to_string( str, 4096, &lr );
    printf( str );

    int res;
    double y;
    uint64_t t;

    t = 9.01*HOURS;    res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 10*HOURS;      res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 10.999*HOURS;  res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11*HOURS;      res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.01*HOURS;   res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.2*HOURS;    res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.7132*HOURS; res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.99*HOURS;   res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.999*HOURS;  res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );
    t = 11.9999*HOURS; res = lag_record_lookup( &lr, t, &y );  printf( "looked up value (res = %d): %lu  %f\n", res, t, y );

    lag_record_free( &lr );

    return 0;
}

