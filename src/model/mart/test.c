#include <mrtl/model/mart/mart.h>
#include <mrtl/common/functions.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int parse_next_line ( float * dst, size_t * n, char * line )
{
    size_t ignore = 9;  // number of columns in the input file before input0.
    size_t i = 0, di = 0;

    char * tok_save;
    char * tok = strtok_r( line, ",", &tok_save );

    while ( tok != NULL )
    {
        if ( i >= ignore )
        {
            dst[di] = atof( tok );
            ++di;
        }

        tok = strtok_r( NULL, ",", &tok_save );
        ++i;
    }

    *n = di;

    return 0;
}


int main ( int argc, char * argv [] )
{
    char input_filename [1024];
    char model_filename [1024];
    int header = 0;

    for ( int a = 1; a < argc; a++ )
    {
        if ( !strncmp("-i", argv[a], 4) )
        { strncpy( input_filename, argv[++a], 1024-1 ); }
        else if ( !strncmp("-h", argv[a], 4) )
        { header = 1; }
        else if ( !strncmp("-m", argv[a], 4) )
        { strncpy( model_filename, argv[++a], 1024-1 ); }
        else
        { fprintf( stderr, "Unknown option: %s, exiting.\n", argv[a] ); }
    }

    struct Mart mart;
    int err = mart_load_lightgbm_from_file( &mart, model_filename );

    if ( err < 0 )
    { fprintf( stderr, "Error loading LightGBM model from %s\n", model_filename ); }


    char * file_string = NULL;
    file_to_string( &file_string, input_filename );

    float * xs = (float *) calloc( 1024, sizeof(float) );
    size_t xcnt;
    float prediction;

    size_t line_number = 0;
    char * line_save;
    char * line = strtok_r( file_string, "\n\r", &line_save );

    while ( line != NULL )
    {
        ++line_number;

        if ( line_number > 1 || header == 0 )
        {
            parse_next_line( xs, &xcnt, line );
            mart_predict( &mart, &prediction, xs );
            printf( "%f\n", prediction );
        }

        line = strtok_r( NULL, "\n\r", &line_save );
    }

    free( xs );
    free( file_string );

    printf( "Read %lu lines from %s\n", line_number, input_filename );

    return 0;
}

