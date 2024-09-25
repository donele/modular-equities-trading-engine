#ifndef _MRTL_CLASSIC_SYMBOL_DATA_H_
#define _MRTL_CLASSIC_SYMBOL_DATA_H_
#include <mrtl/common/moving_averages.h>
#include <stdint.h>

struct SymbolStrategyData
{
    // chara
    float volume;
    float median_volume;
    float median_volatility;
    float med_med_sprd;
    float median_n_quotes;
    float median_n_trades;
    int32_t lot_size;
    float p_volume;
    float adjust;
    float prev_adjust;
    float logCap;
    int tick_valid;

    float p_open;
    float p_close;
    float p_high;
    float p_low;

    float pp_open;
    float pp_close;
    float pp_high;
    float pp_low;

    int8_t stock_characteristics_ok;

    char ticker [8];
    char nbbo_bid_mkt_id;
    char nbbo_ask_mkt_id;
    uint64_t last_book_time;
    uint64_t last_trade_time;

    double prev_mid;
    uint64_t first_quote_time;
    double first_quote_mid;
    double overnight_return_bps;
    double last_trade_price;
    double high_trade_price;
    double low_trade_price;
    double high_trade_price_900s;
    double low_trade_price_900s;
    double mid_before_lastTrade;
    double mid_before_lastTradeBeforeNbbo;

    double fill_imb;

    double intraday_ntrades;
    double intraday_share_volume;
    double intraday_price_sum;
    double intraday_notional_volume;
    double intraday_buy_share_volume;
    double intraday_sell_share_volume;


    // Returns
    struct TimeWeightedMovingAverage midprice_moving_average_5s;
    struct TimeWeightedMovingAverage midprice_moving_average_15s;
    struct TimeWeightedMovingAverage midprice_moving_average_30s;
    struct TimeWeightedMovingAverage midprice_moving_average_60s;
    struct TimeWeightedMovingAverage midprice_moving_average_120s;
    struct TimeWeightedMovingAverage midprice_moving_average_300s;
    struct TimeWeightedMovingAverage midprice_moving_average_600s;
    struct TimeWeightedMovingAverage midprice_moving_average_1200s;
    struct TimeWeightedMovingAverage midprice_moving_average_2400s;
    struct TimeWeightedMovingAverage midprice_moving_average_4800s;
    struct TimeWeightedMovingAverage midprice_moving_average_9600s;

    // Volume momentum
    struct LeakyAccumulator lacc_trade_qty_sum_15s;
    struct LeakyAccumulator lacc_trade_qty_sum_30s;
    struct LeakyAccumulator lacc_trade_qty_sum_60s;
    struct LeakyAccumulator lacc_trade_qty_sum_120s;
    struct LeakyAccumulator lacc_trade_qty_sum_300s;
    struct LeakyAccumulator lacc_trade_qty_sum_600s;
    struct LeakyAccumulator lacc_trade_qty_sum_3600s;

    // Bollinger
    struct LeakyAccumulator lacc_trade_count_300s;
    struct LeakyAccumulator lacc_trade_count_900s;
    struct TimeWeightedMovingAverage trade_price_moving_average_300s;
    struct TimeWeightedMovingAverage trade_price_moving_average_900s;

    // qIMax
    struct LeakyAccumulator lacc_max_bid_size_200s;
    struct LeakyAccumulator lacc_max_ask_size_200s;
    struct LeakyAccumulator lacc_max_bid_size_1200s;
    struct LeakyAccumulator lacc_max_ask_size_1200s;

    // Order Params
    double thres_in;
    double thres_out;
    int max_position;
    int min_cancel_time;
    int max_cancel_time;
    double r_force_fact;
    int odd_lot_type;
    int trd_lot_size;
    double cc_factor;
    double tick_size;
    int is_short_hedge;

    // Market Making Params
    double ord_spread;
    double ord_const;
    double ord_const_psh;
    double ord_pos;
    double ord_forec;
    int send_non_marketable;
    double sel_spread;
    double sel_const;
    double sel_const_psh;
    double sel_pos;
    double sel_forec;
    double sel_tof_day1;
    double sel_tof_day2;
    double insert_thres;
    double cost_liquid_param;
    short default_routing;
    int trade_limit;
    int max_qty;
    double asym;
};

#endif

