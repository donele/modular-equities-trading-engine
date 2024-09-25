#ifndef _MRTL_COMMON_TYPES_H_
#define _MRTL_COMMON_TYPES_H_

#include <mrtl/common/constants.h>
#include <mbpsig.h>
#include <stdint.h>


struct BBO
{
    struct mbp_level bid;
    struct mbp_level ask;
};

void write_bbo_to_string( char *, size_t, const struct BBO * );


#define MAX_FEATURE_COUNT 128
#define MAX_TARGET_COUNT 4


enum PredictionType
{
    CLASSIC = 1,
    MOMENTUM_SIEVE,
    ETS
};


struct Predictions
{
    enum PredictionType type;
    float pred [MAX_TARGET_COUNT];
    float restoring_force_adjustment;
    float factor_risk_adjustment;
};


void write_prediction_to_string ( char *, size_t, const struct Predictions * );


struct Sample
{
    struct BBO nbbo;
    uint64_t nsecs;
    uint64_t target_times [MAX_TARGET_COUNT];
    float targets         [MAX_TARGET_COUNT];
    float features        [MAX_FEATURE_COUNT];
    uint16_t sec_id;
    char ticker           [TICKERLEN];
    int8_t good;
};

void write_sample_header_to_string ( char *, size_t );

void write_sample_to_string ( char *, size_t, const struct Sample * );


struct TickerExchangePair
{
    char ticker [TICKERLEN];
    char exch [4];
};


struct StockCharacteristics
{
    double volume;
    double med_volume;
    double med_volatility;
    double med_med_sprd;
    double med_nquotes;
    double med_ntrades;
    double prev_open;
    double prev_close;
    double prev_high;
    double prev_low;
    double prev_prev_open;
    double prev_prev_close;
    double prev_prev_high;
    double prev_prev_low;
    double adjust;
    double prev_adjust;
    int tick_valid;
    int lot_size;
    int shareout;
    int prev_idate;
    char ticker [8];
    char market;
    int8_t ok;
};

enum OddLotRestrictionType {OLR_NONE,OLR_NYSE,OLR_TSX};

struct OrderParams
{
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

    int idate;
    char ticker [8];
    int8_t ok;
};

struct MarketMakingParams
{
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

    int idate;
    char ticker [8];
    int8_t ok;
};


enum OrderSchedType
{
    ORDER_SCHEDULE_TYPE_CYCLE       = 1,
    ORDER_SCHEDULE_TYPE_NBBO_EVENT  = 2,
    ORDER_SCHEDULE_TYPE_TRADE_EVENT = 4
};


enum OrderStatus
{
    ORDER_STATUS_NEW          = 1,
    ORDER_STATUS_OPEN         = 2,
    ORDER_STATUS_PARTIAL_FILL = 3,
    ORDER_STATUS_FILL         = 4,
    ORDER_STATUS_CANCEL       = 5,
    ORDER_STATUS_REJECT       = 6
};


struct Order
{
    size_t id;
    uint64_t nsecs;
    int64_t price;
    int64_t predicted_price;
    uint32_t quantity;
    enum OrderSchedType order_schedule_type;
    float prediction;
    uint16_t mkt_id;
    uint16_t sec_id;
    int8_t side;
    int8_t status;

    char ticker [TICKERLEN];
};

void write_order_to_string( char * dst, size_t dst_len, const struct Order * o );


struct Fill
{
    size_t order_id;
    uint64_t nsecs;
    int64_t price;
    uint32_t quantity;
    float fees_in_currency;
    float pnl_in_currency;
    uint16_t mkt_id;
    uint16_t sec_id;
    int8_t side;
    char ticker [TICKERLEN];
};

void write_fill_to_string ( char * dst, size_t dst_len, const struct Fill * f );


#define MAX_OPEN_ORDERS_PER_SYMBOL 16

struct SymbolTradingData
{
    pthread_rwlock_t lock;

    uint64_t last_buy_order_nsecs;
    uint64_t last_buy_fill_nsecs;
    int64_t  last_buy_price;
    uint64_t last_sell_order_nsecs;
    uint64_t last_sell_fill_nsecs;
    int64_t  last_sell_price;
    uint64_t last_market_trade_nsecs;

    uint64_t market_share_volume;
    uint64_t share_volume;
    int32_t share_position;
    int32_t market_making_share_position;
    int32_t balance_share_position;
    int32_t outstanding_order_share_position;
    int32_t max_share_position;

    struct Order open_orders [MAX_OPEN_ORDERS_PER_SYMBOL];
};

int symbol_trading_data_init ( struct SymbolTradingData * );

int symbol_trading_data_fini ( struct SymbolTradingData * );

int symbol_trading_data_on_fill(
        struct SymbolTradingData * std,
        struct Order * o,
        const struct Fill * f );

int symbol_trading_data_on_cancel(
        struct SymbolTradingData * std,
        struct Order * o,
        uint32_t remaining_quantity );

int symbol_trading_data_on_market_trade (
        struct SymbolTradingData * std,
        uint64_t nsecs,
        uint64_t sz );

struct GlobalTradingData
{
    pthread_rwlock_t lock;

    uint64_t last_update_nsecs;
    double max_gross_notional_position;
    double gross_notional_position;
    double net_notional_position;
    double notional_volume;
    float cash;
};

int global_trading_data_init( struct GlobalTradingData * gtd );

int global_trading_data_fini( struct GlobalTradingData * gtd );

int global_trading_data_update(
    struct GlobalTradingData * gtd,
    struct SymbolTradingData * std,
    const struct BBO * nbbos,
    size_t symbol_count,
    uint64_t nsecs );

int global_trading_data_get(
    struct GlobalTradingData * gtd,
    uint64_t * last_update_nsecs,
    double * net_notional_position,
    double * notional_volume,
    float * cash );

int global_trading_data_on_fill(
    struct GlobalTradingData * gtd,
    struct Order * o,
    const struct Fill * f );


struct SymbolData
{
    uint16_t sec_id;
    char     ticker [MAX_SYMBOL_LENGTH];
    int8_t   in_universe;

    struct SymbolTradingData * trading_data;
    void * strategy_data;
};

void symbol_data_init ( struct SymbolData * );


#endif  // _MRTL_COMMON_TYPES_H_

