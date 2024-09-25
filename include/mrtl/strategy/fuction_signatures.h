#ifndef _MRTL_STRATEGY_FUNCTION_SIGNATURES_H_
#define _MRTL_STRATEGY_FUNCTION_SIGNATURES_H_

#include <mrtl/common/types.h>
#include <mrtl/common/country.h>
#include <mrtl/model/mart/mart.h>
#include <stdint.h>

/*
 * strategy_init() must allocate a struct that will hold any 
 * configuration needed by the strategy functions, which will be available from
 * the dictionary `cfg`.  This information is read from the `[strategy]` section
 * of the `MRTL_AGENT_CONFIG` file.  The strategy config struct will be passed to
 * each of the other `strategy_*` functions (see function signatures below).
 * For example, to set the target horizon for the strategy, a variable and value
 * should be in the config file, then `strategy_init()` should read that
 * value from `cfg` and assign it to a field in the allocated strategy
 * configuration struct.
 *
 * Required function name: strategy_init
 * */
typedef int (*strategy_init_func)(
        void ** strategy_config,
        struct Mart * models,
        dictionary * cfg );


/* strategy_init_symbol_datas() must allocate an array of SymbolStrategyData
 * structs (one for each symbol) that will be updated by strategy_on_trade() and
 * strategy_on_book_update().  The struct
 * definition will be specific to each strategy, i.e. the symbol data structure
 * for Momentum & Sieve will be different than the symbol data structure for
 * Classic.  This function should also load any external data required for these
 * SymbolStrategyData structs, such as from `stockcharacteristics`.
 *
 * Required function name: strategy_init_symbol_datas
 * */
typedef int (*strategy_init_symbol_datas_func)(
        void ** arr,
        size_t * cnt,
        struct SymbolData * symbol_datas,
        const struct StockCharacteristics * stock_characteristics,
        const struct OrderParams * order_params,
        const struct MarketMakingParams * market_making_params,
        size_t n_stock_characteristics,
        void * strategy_config );


/* strategy_on_trade() should update the symbolinstrumentdata structure
 * for the symbol with the trade.  the strategy decides what needs to be
 * recorded on the symbol data structure when there is a trade.  
 *
 * Required function name: strategy_on_trade
 * */
typedef int (*strategy_on_trade_func)(
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        uint16_t mkt_id,
        int64_t px,
        uint32_t sz,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        void * strategy_config,
        const struct Country * country);


/* strategy_on_book_update() should update the SymbolStrategyData
 * structure for the symbol that with the book update.  The strategy decides
 * what needs to be recorded on the symbol data structure when there is a book
 * update.
 *
 * Required function name: strategy_on_book_update
 * */
typedef int (*strategy_on_book_update_func)(
        const struct SymbolData * symbol_datas,
        uint64_t nsecs,
        uint16_t mkt_id,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct BBO * nbbo_prev,
        void * strategy_config,
        const struct Country * country);


/* strategy_generate_features() uses the SymbolStrategyData structure to
 * calculate features which will be passed to themodel.
 *
 * Required function name: strategy_generate_features
 * */
typedef int (*strategy_generate_features_func)(
        float * dst,
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        const struct Country * country,
        const struct mbp_book * agg,
        void * strategy_config );


/* strategy_calculate_target() uses the Sample struct and an array of midprices
 * to calculate the target for the strategy.
 *
 * `smpl` is the Sample structure whose target will be calculated.  It has an
 * array of targets to store the calculated target(s).
 *
 * `midprice_times` is an array of times at which midprices are available.  This
 * is the same for all symbols, and these times are related to the configuration
 * file variable `agent:midprice_sample_period_seconds`.
 *
 * `times_count` is the number of elements in the `midprice_times` array.
 *
 * `midprices` is an array of the midprices of all stocks, sampled at each
 * `midprice_times` element value.  Since this array has midprices for all
 * stocks, it is possible to use it for calculating hedged targets.  The
 * dimensions of `midprices` is: `symbol_count x midprice_times`
 *
 * There are three helper functions available for calculating targets.  Examples
 * of use can be found in `src/strategy/momentum_sieve/targets.c`.
 
 1. `get_midprice_time_index_at_or_before()` takes the array `midprice_times`
    and a time `nsecs` whose index should be looked up.  It returns the
    index to the largest element of `midprice_times` whose value is
    less-than-or-equal to `nsecs`.  If `nsecs` is before the first element of
    `midprice_times`, then there is no index which satisfies the criteria and
    `SIZE_MAX` is returned.  If `nsecs` is after the last element, then the
    index of the last element is returned.
 
 2. `get_midprice_time_index_just_after()` is similar to
    `get_midprice_time_index_at_or_before()`, but returns the index to the
    smallest element of `midprice_times` whose value is strictly greater-than
    `nsecs`.  If `nsecs` is before the first element of `midprice_times`, then
    zero is returned, to indicate the first element.  If `nsecs` is after the
    last element of `midprice_times`, then there is no index which satisfies
    the criteria and `SIZE_MAX` is returned.
 
 3. `get_midprice_at_time_index()` takes the array `midprices` and its
    dimensions, along with a symbol id, and a time index from one of the two
    functions above, and returns the midprice of that symbol at that time index.
 
 For example, to calculate the target for a sample using the horizon in the
 `MRTL_AGENT_CONFIG` file, these functions can be used as follows in the
 `strategy_calculate_target()` function definition.  Note that the
 `strategy_load_config()` function must have already set the `horizon` field
 of the strategy config struct.  
    
```
        int strategy_calculate_target (
                struct Sample * smpl,
                uint64_t * midprice_times,
                size_t times_count,
                double * midprices,
                size_t symbol_count,
                const struct Country * country,
                void * strategy_config )
        {
            struct StrategyConfig * strat_conf = strategy_config;
        
            // First, use the end time of the target horizon to look up the index
            // for the target.
            size_t target_time_index = get_midprice_time_index_at_or_before(
                    midprice_times, times_count, ( smpl->nsecs + strat_conf->horizon ) );
        
            if ( target_time_index == SIZE_MAX )
            {
                smpl->good = 0;
                return 0;
            }
        
            // Then, use the index of the target to look up the midprice.
            double target_midprice = get_midprice_at_time_index(
                    midprices, times_count, smpl->sym_id, target_time_index );
       
            // Calculate a simple return from the midprice of the sample to the
            // midprice we looked up for the index at the end of the target
            // horizon.
            smpl->targets[0] = return_in_bps( mid_bbo( smpl->nbbo ), target_midprice );
        
            return 0;
        }
```
 *
 * Required function name: strategy_calculate_target
 * */
typedef int (*strategy_calculate_target_func)(
        struct Sample * smpl,
        struct SymbolData* symbol_datas,
        uint64_t * midprice_times,
        size_t times_count,
        double * midprices,
        size_t symbol_count,
        const struct Country * country,
        void * strategy_config );


/* strategy_generate_orders() creates orders by populating `orders` and setting
 * `dst_order_count` accordingly.
 * 
 * Required function name: strategy_generate_orders
 * */
typedef int (*strategy_generate_orders_func)(
        struct Order * orders,
        uint8_t * dst_order_count,
        uint8_t max_order_count,
        const struct Predictions * predictions,
        const struct mbp_sigproc_args * args,
        struct cfgdb_db * cfgdb,
        uint64_t nsecs,
        struct SymbolData * symbol_data,
        const struct mbp_book * agg,
        void * strategy_config );


/* strategy_adjust_predictions() populates fields in the `predictions` struct
 * with values for restoring force, risk adjustments, etc.
 * 
 * Required function name: strategy_adjust_predictions
 * */
typedef int (*strategy_adjust_predictions_func)(
        struct Predictions * predictions,
        const struct SymbolData * symbol_data,
        const struct GlobalTradingData * global_trading_data,
        const void * strategy_config );


/*
 * strategy_predict() takes the models and features and populates the `preds`
 * in the `predictions` struct.
 *
 * Required function name: strategy_predict
 * */
typedef int (*strategy_predict_func)(
        struct Predictions * predictions,
        struct Mart * models,
        float * features,
        enum OrderSchedType event_type,
        void * strategy_config );

/*
 * strategy_write_sample_header() writes the header line for the sample file
 * that is generated by the sampler.
 *
 * The actual writing of each sample line to the file, is done in
 * write_sample_to_string(), which is in src/common/types.c
 *
 * The header line *must* begin with: ticker,nsecs,bidSize,bid,ask,askSize
 * The next fields are targets that are written, then the inputs.
 *
 * See write_sample_to_string() for details on how samples are written.
 *
 * Required function name: strategy_write_sample_header
 * */
typedef int (*strategy_write_sample_header_func)(
        char * dst,
        size_t dst_len );

#endif  // _MRTL_STRATEGY_FUNCTION_SIGNATURES_H_

