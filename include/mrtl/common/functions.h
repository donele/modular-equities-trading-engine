#ifndef _MRTL_COMMON_FUNCTIONS_H_
#define _MRTL_COMMON_FUNCTIONS_H_

#include <mrtl/common/country.h>
#include <mrtl/common/types.h>
#include <sidb.h>
#include <dbw/dbw.h>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

double sgn ( double );

double clip ( double, double, double );

double mid ( double, double );

double mid_bbo_double ( struct BBO );

int64_t mid_book_fixed ( const struct mbp_book * );

double return_in_bps ( double, double );

double spread_in_bps ( double, double );

double spread_in_bps_book ( const struct mbp_book * );

double nsecs_to_secs ( uint64_t nsecs );

double quote_imbalance ( double, double );

double value_in_range ( double, double, double );

double weighted_moving_average ( double, double, double );

bool book_side_is_good( struct mbp_level );

bool bbo_struct_is_good ( struct BBO );

bool bbo_is_good ( const struct mbp_book * );

size_t get_midprice_time_index_at_or_before ( uint64_t * time_array, size_t time_array_len, uint64_t nsecs );

size_t get_midprice_time_index_just_after ( uint64_t * time_array, size_t time_array_len, uint64_t nsecs );

double get_midprice_at_time_index ( double * midprice_array, size_t time_array_len, uint16_t sym_id, size_t time_index );

int hfstock_connect ( db_t *, const struct Country * );

int equitydata_connect ( db_t *, const struct Country * );

int file_to_string ( char **, const char * );

int load_corporate_action_symbols ( struct TickerExchangePair **, const struct Country *, int32_t );

int filter_corporate_action_symbols ( const struct TickerExchangePair *, struct StockCharacteristics * );

int link_symbol_trading_datas ( struct SymbolData *, struct SymbolTradingData *, size_t );

int load_stock_characteristics(
        struct StockCharacteristics **,
        size_t n,
        sidb_t sidb,
        uint16_t set_id,
        int32_t idate,
        const struct Country * );

int load_order_params (
        struct OrderParams ** op,
        size_t n,
        sidb_t sidb,
        int16_t set_id,
        int32_t idate,
        const struct Country * country );

int load_market_making_params (
        struct MarketMakingParams ** mmp,
        size_t n,
        sidb_t sidb,
        int16_t set_id,
        int32_t idate,
        const struct Country * country );

int read_positions_from_files (
        struct SymbolData * symbol_datas,
        double * total_cash_position,
        size_t * file_count,
        const struct Country * country,
        int32_t prev_idate,
        uint16_t set_id,
        sidb_t sidb,
        const char * dir );

#endif // _MRTL_FUNCTIONS_H_

