#include <mrtl/agent/load_strategy_library.h>
#include <dlfcn.h>
#include <mlog.h>
#include <errno.h>


int link_function ( void * library, const char * function_name, void ** dst )
{
    if ( dst == NULL )
    {
        log_notice( "agent::link_function() not linking function: %s", function_name );
        return 0;
    }

    *dst = dlsym( library, function_name );

    if ( *dst == NULL )
    {
        log_error( "agent::link_function() could not find: %s", function_name );
        errno = ELIBBAD;
        return -1;
    }
    else
    {
        log_notice( "agent::link_function() linked function: %s", function_name );
    }

    return 0;
}


int load_strategy_library (
        const char * library_file_name,
        void ** library,
        strategy_init_func                           * strategy_init,
        strategy_predict_func                        * strategy_predict,
        strategy_adjust_predictions_func             * strategy_adjust_predictions,
        strategy_init_symbol_datas_func   * strategy_init_symbol_datas,
        strategy_on_trade_func                       * strategy_on_trade,
        strategy_on_book_update_func                 * strategy_on_book_update,
        strategy_generate_features_func              * strategy_generate_features,
        strategy_calculate_target_func               * strategy_calculate_target,
        strategy_generate_orders_func                * strategy_generate_orders,
        strategy_write_sample_header_func            * strategy_write_sample_header
        )
{
    *library = dlopen( library_file_name, RTLD_LAZY );

    if ( NULL == *library )
    {
        log_error( "agent_init() cannot load strategy library: '%s'", library_file_name );
        errno = ENOENT;
        return -1;
    }

    if ( link_function( *library, "strategy_init", (void**)strategy_init ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_predict", (void**)strategy_predict ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_adjust_predictions", (void**)strategy_adjust_predictions ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_init_symbol_datas", (void**)strategy_init_symbol_datas ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_on_trade", (void**)strategy_on_trade ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_on_book_update", (void**)strategy_on_book_update ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_generate_features", (void**)strategy_generate_features ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_calculate_target", (void**)strategy_calculate_target ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_generate_orders", (void**)strategy_generate_orders ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }

    if ( link_function( *library, "strategy_write_sample_header", (void**)strategy_write_sample_header ) < 0 )
    {
        log_error( "load_strategy_library() failed to load function" );
        return -1;
    }


    return 0;
}

