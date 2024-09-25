#include <mrtl/strategy/classic/config.h>
#include <mrtl/model/mart/mart.h>
#include <mrtl/common/constants.h>
#include <mlog.h>
#include <iniparser.h>

int strategy_init(
        void ** strategy_config,
        struct Mart * models,
        dictionary * cfg)
{
    struct StrategyConfig * c = calloc( 1, sizeof(struct StrategyConfig) );

    c->horizon1_from = iniparser_getint(cfg, "strategy:horizon1_from_seconds", 0) * SECONDS;
    c->horizon1_to   = iniparser_getint(cfg, "strategy:horizon1_to_seconds"  , 0) * SECONDS;
    c->horizon2_from = iniparser_getint(cfg, "strategy:horizon2_from_seconds", 0) * SECONDS;
    c->horizon2_to   = iniparser_getint(cfg, "strategy:horizon2_to_seconds"  , 0) * SECONDS;
    c->horizon3_from = iniparser_getint(cfg, "strategy:horizon3_from_seconds", 0) * SECONDS;
    c->horizon3_to   = iniparser_getint(cfg, "strategy:horizon3_to_seconds"  , 0) * SECONDS;
    c->horizon4_from = iniparser_getint(cfg, "strategy:horizon4_from_seconds", 0) * SECONDS;
    c->horizon4_to   = iniparser_getint(cfg, "strategy:horizon4_to_seconds"  , 0) * SECONDS;
    if ( c->horizon1_from == 0 && c->horizon1_to == 0 )
    {
        log_error("strategy_init() did not find valid strategy:horizon1_to_seconds in config.");
        //return -1;
    }
    else
    {
        log_notice("Strategy loaded horizon1 = %lu sec - %lu sec", c->horizon1_from/SECONDS, c->horizon1_to/SECONDS);
        log_notice("Strategy loaded horizon2 = %lu sec - %lu sec", c->horizon2_from/SECONDS, c->horizon2_to/SECONDS);
        log_notice("Strategy loaded horizon3 = %lu sec - %lu sec", c->horizon3_from/SECONDS, c->horizon3_to/SECONDS);
        log_notice("Strategy loaded horizon4 = %lu sec - %lu sec", c->horizon4_from/SECONDS, c->horizon4_to/SECONDS);
    }

    if(models)
    {
        // Load 1 minute cycle Model
        const char* omc_model_file = iniparser_getstring( cfg, "strategy:om_cycle_model_file", NULL);
        if(omc_model_file == NULL)
        {
            log_error( "strategy_init(): config file is missing strategy:om_cycle_model_file");
            return -1;
        }
        if(mart_load_lightgbm_from_file(&models[0], omc_model_file) < 0)
        {
            log_error("strategy_init(): error loading om cycle model file.");
            return -1;
        }

        // Load 1 minute trade event Model
        const char* omt_model_file = iniparser_getstring( cfg, "strategy:om_trade_model_file", NULL);
        if(omt_model_file == NULL)
        {
            log_error( "strategy_init(): config file is missing strategy:om_trade_model_file");
            return -1;
        }
        if(mart_load_lightgbm_from_file(&models[0], omt_model_file) < 0)
        {
            log_error("strategy_init(): error loading om trade model file.");
            return -1;
        }

        // Load 40 minute cycle Model
        const char* tmc_model_file = iniparser_getstring( cfg, "strategy:tm_cycle_model_file", NULL);
        if(tmc_model_file == NULL)
        {
            log_error( "strategy_init(): config file is missing strategy:tm_cycle_model_file");
            return -1;
        }
        if(mart_load_lightgbm_from_file(&models[0], tmc_model_file) < 0)
        {
            log_error("strategy_init(): error loading tm cycle model file.");
            return -1;
        }
    }
    else
    {
        log_notice( "Strategy did not load any models." );
    }


    *strategy_config = c;
    return 0;
}

