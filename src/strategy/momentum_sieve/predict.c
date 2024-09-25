#include <mrtl/strategy/momentum_sieve/config.h>
#include <mrtl/model/mart/mart.h>
#include <mrtl/common/types.h>
#include <math.h>
#include <mlog.h>


int strategy_predict (
        struct Predictions * predictions,
        struct Mart * models,
        float * features,
        enum OrderSchedType event_type,
        void * strategy_config )
{
    //struct StrategyConfig * strat_conf = strategy_config;

    memset( predictions->pred, 0, sizeof(predictions->pred) );
    predictions->restoring_force_adjustment = 0.f;
    predictions->factor_risk_adjustment = 0.f;

    if ( ORDER_SCHEDULE_TYPE_TRADE_EVENT == event_type )
    { mart_predict( &models[0], &predictions->pred[0], features ); }
    else if ( ORDER_SCHEDULE_TYPE_CYCLE == event_type )
    { mart_predict( &models[1], &predictions->pred[0], features ); }

    predictions->type = MOMENTUM_SIEVE;

    return 0;
}


#include <mrtl/strategy/momentum_sieve/symbol_data.h>


int restoring_force (
    float * dst,
    const struct SymbolData * sd,
    const struct GlobalTradingData * gtd,
    const struct StrategyConfig * strat_conf )
{
    const struct SymbolTradingData * std = sd->trading_data;

    double rForceBasePosition = strat_conf->restoring_force_position_factor * gtd->max_gross_notional_position;
    double share_position = std->share_position;

    if ( std->max_share_position < 1 )
    { *dst = 0.f; }
    else
    {
        *dst = -1. * ( strat_conf->restoring_force_strength_factor *
                    exp( gtd->gross_notional_position / rForceBasePosition ) *
                    ( share_position / std->max_share_position ) );
    }

    return 0;
}


int strategy_adjust_predictions (
        struct Predictions * predictions,
        const struct SymbolData * symbol_data,
        const struct GlobalTradingData * global_trading_data,
        const void * strategy_config )
{
    const struct StrategyConfig * strat_conf  = strategy_config;

    restoring_force( &predictions->restoring_force_adjustment, symbol_data, global_trading_data, strat_conf );

    return 0;
}

