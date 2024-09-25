#include <mrtl/strategy/momentum_sieve/config.h>
#include <mrtl/model/mart/mart.h>
#include <mrtl/common/constants.h>
#include <mlog.h>
#include <iniparser.h>


int strategy_init (
        void ** strategy_config,
        struct Mart * models,
        dictionary * cfg )
{
    
    struct StrategyConfig * c = calloc( 1, sizeof(struct StrategyConfig) );

    c->horizon = iniparser_getint( cfg, "strategy:horizon_seconds", 0 ) * SECONDS;

    if ( c->horizon == 0 )
    {
        log_error( "strategy_init() did not find strategy:horizon_seconds in config." );
        return -1;
    }
    else
    { log_notice( "Strategy loaded horizon = %lu sec", c->horizon/SECONDS ); }

    if ( models )
    {
        // Load Trade Model
        const char * trade_model_file = iniparser_getstring( cfg, "strategy:trade_model_file", NULL );

        if ( trade_model_file == NULL )
        {
            log_error( "strategy_init(): config file is missing strategy:trade_model_file" );
            return -1;
        }

        if ( mart_load_lightgbm_from_file( &models[0], trade_model_file ) < 0 )
        {
            log_error( "strategy_init(): error loading trade model file." );
            return -1;
        }

        // Load Cycle Model
        const char * cycle_model_file = iniparser_getstring( cfg, "strategy:cycle_model_file", NULL );

        if ( cycle_model_file == NULL )
        {
            log_error( "strategy_init(): config file is missing strategy:cycle_model_file" );
            return -1;
        }

        if ( mart_load_lightgbm_from_file( &models[1], cycle_model_file ) < 0 )
        {
            log_error( "strategy_init(): error loading cycle model file." );
            return -1;
        }
    }
    else
    {
        log_notice( "Strategy did not load any models." );
    }

    c->order_cooldown = iniparser_getint( cfg, "strategy:order_cooldown_seconds", 15 ) * SECONDS;
    log_info( "Using order cooldown time: %lu sec", (c->order_cooldown/SECONDS) );

    c->restoring_force_strength_factor = iniparser_getdouble( cfg, "strategy:restoring_force_strength_factor", -1 );

    if ( c->restoring_force_strength_factor <= 0 )
    { log_error( "strategy_init(): error reading strategy:restoring_force_strength_factor from config file." ); }
    else
    { log_info( "Using restoring_force_strength_factor: %f", c->restoring_force_strength_factor ); }

    c->restoring_force_position_factor = iniparser_getdouble( cfg, "strategy:restoring_force_position_factor", -1 );

    if ( c->restoring_force_position_factor <= 0 )
    { log_error( "strategy_init(): error reading strategy:restoring_force_position_factor from config file." ); }
    else
    { log_info( "Using restoring_force_position_factor: %f", c->restoring_force_position_factor ); }


    *strategy_config = c;

    return 0;
}

