#include <stdint.h>


struct StrategyConfig
{
    float restoring_force_strength_factor;
    float restoring_force_position_factor;

    uint64_t horizon;
    uint64_t order_cooldown;
};

./include/mrtl/strategy/momentum_sieve/symbol_data.h
#ifndef _MRTL_STRATEGY_MOMENTUM_SIEVE_SYMBOL_DATA_H_
#define _MRTL_STRATEGY_MOMENTUM_SIEVE_SYMBOL_DATA_H_

#include <mrtl/common/moving_averages.h>
#include <mrtl/common/lag_record.h>
#include <stdint.h>


struct SymbolStrategyData
{
    struct TimeWeightedMovingAverage midprice_moving_average_1s;
    struct TimeWeightedMovingAverage midprice_moving_average_10s;
    struct TimeWeightedMovingAverage midprice_moving_average_30s;
    struct TimeWeightedMovingAverage midprice_moving_average_100s;
    struct TimeWeightedMovingAverage midprice_moving_average_300s;
    struct TimeWeightedMovingAverage midprice_moving_average_900s;

    //struct LagRecord midprice_lags;

    uint64_t last_updated;
    uint64_t last_trade_time;

    double last_trade_price;
    double high_trade_price;
    double low_trade_price;

    double btwp5, btwp20, btq5, btq10, btq20, btq100, btwp200, btq200;
    double fill_imb_5, fill_imb_20, fill_imb_50;

    float intraday_share_volume;
    float intraday_notional_volume;
    float intraday_buy_share_volume;
    float intraday_sell_share_volume;

    float volume;
    float median_volume;
    float median_volatility;
    float med_med_sprd;
    float median_n_quotes;
    float median_n_trades;
    float previous_open_price;
    float previous_close_price;
    float previous_high_price;
    float previous_low_price;
    int32_t lot_size;

    uint16_t book_count;

    char ticker [8];

    char nbbo_bid_mkt_id;
    char nbbo_ask_mkt_id;

    int8_t stock_characteristics_ok;
};


#endif  // _MRTL_STRATEGY_MOMENTUM_SIEVE_SYMBOL_DATA_H_

