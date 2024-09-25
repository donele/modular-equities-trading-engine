#include <mrtl/common/functions.h>
#include <mrtl/common/constants.h>
#include <glob.h>
#include <mlog.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


double sgn ( double x )
{
    if ( x > 0. )  { return  1.; }
    if ( x < 0. )  { return -1.; }
    return 0.;
}


double clip ( double x, double clip, double error )
{
    if ( fabs(x) > error )  { return 0.; }
    if ( fabs(x) > clip )   { return sgn(x) * clip; }
    return x;
}


double mid ( double b, double a )
{ return 0.5 * ( b + a ); }


double mid_bbo_double ( struct BBO m )
{ return 0.5 * ( fix2dbl(m.bid.px) + fix2dbl(m.ask.px) ); }


int64_t mid_book_fixed ( const struct mbp_book * b )
{
    // Try to avoid overflow.
    return ( 0.5 * b->bid[0].px ) + ( 0.5 * b->ask[0].px );
}


double return_in_bps ( double x1, double x2 )
{ return 10000. * ((x2 / x1) - 1.); }


double spread_in_bps ( double b, double a )
{ return 20000. * ( a - b ) / ( a + b ); }


double spread_in_bps_book ( const struct mbp_book * b )
{ return spread_in_bps( fix2dbl(b->bid[0].px), fix2dbl(b->ask[0].px) ); }


double nsecs_to_secs ( uint64_t nsecs )
{ return ((double)nsecs) / SECONDS; }


double quote_imbalance ( double bid_size, double ask_size )
{
    return ( bid_size - ask_size )
         / ( bid_size + ask_size );
}

double value_in_range ( double low, double high, double x )
{
    return 2. * (x - mid(low, high)) / (high - low);
}

double weighted_moving_average ( double last, double current, double weight )
{
    if ( weight >= 1. )
    { return current; }
    else
    { return weight * (current - last) + last; }
}


bool book_side_is_good( struct mbp_level l )
{
    return ( 0 < l.px && l.px < maxpx && l.sz > 0 ) ? true : false;
}


bool bbo_struct_is_good ( struct BBO m )
{
    return ( book_side_is_good(m.bid) && book_side_is_good(m.ask) );
}


bool bbo_is_good ( const struct mbp_book * b )
{
    struct BBO bbo = { .bid = b->bid[0], .ask = b->ask[0] };
    return bbo_struct_is_good( bbo );
}


size_t get_midprice_time_index_at_or_before (
        uint64_t * time_array,
        size_t time_array_len,
        uint64_t nsecs )
{
    size_t i;

    if ( nsecs < time_array[0] )
    {
        // There is no index indicating a time before nsecs.
        i = SIZE_MAX;
    }
    else if ( nsecs > time_array[time_array_len-1] )
    {
        // If nsecs is larger than the largest midprice_time, return
        // the index of the last time.
        i = time_array_len - 1;
    }
    else
    {
        for ( i = 0; i < time_array_len; i++ )
        {
            if ( nsecs == time_array[i] )
            {
                break;
            }
            else if ( nsecs < time_array[i] )
            {
                // We went one too far.
                // We have already checked that we cannot be on i == 0.
                i--;
                break;
            }
        }
    }

    return i;
}


size_t get_midprice_time_index_just_after (
        uint64_t * time_array,
        size_t time_array_len,
        uint64_t nsecs )
{
    size_t i;

    if ( nsecs > time_array[time_array_len-1] )
    {
        // There is no index indicating a time after nsecs.
        i = SIZE_MAX;
    }
    else
    {
        for ( i = 0; i < time_array_len; i++ )
        {
            if ( nsecs < time_array[i] )
            {
                break;
            }
        }
    }

    return i;
}


double get_midprice_at_time_index (
        double * midprice_array,
        size_t time_array_len,
        uint16_t sec_id,
        size_t time_index )
{
    return midprice_array[(sec_id*time_array_len)+time_index];
}


int hfstock_connect( db_t * _db, const struct Country * _country )
{
    char cmd [128];
    snprintf( cmd, 128, "host=%s dbname=%s",
            _country->dbhost, _country->dbname );

    int res;
    if ( 0 != ( res = db_open( _db, cmd ) ) )
    {
        char * err_str;
        db_error( _db, &res, &err_str );
        log_error( "ERROR: Unable to open hfstock database.  Error: '%s',  using command: '%s'", err_str, cmd );
        free( err_str );
        return -1;
    }

    return 0;
}


int equitydata_connect( db_t * _db, const struct Country * _country )
{
    char cmd [128];
    snprintf( cmd, 128, "host=%s dbname=equitydata", _country->dbhost );

    if ( db_open( _db, cmd ) )
    {
        log_error( "ERROR: Unable to open equitydata database using command: %s", cmd );
        return -1;
    }

    return 0;
}


int file_to_string( char ** dst, const char * filename )
{
    FILE * f = fopen( filename, "r" );

    if ( f == NULL )
    {
        // Could get errno, if needed.
        return ERROR_FILE_ERROR;
    }

    fseek( f, 0, SEEK_END );
    long fsize = ftell( f );
    fseek( f, 0, SEEK_SET );

    char * s = malloc( fsize + 1 );
    if ( s == NULL )
    {
        log_error( "mart_load_from_file() could not malloc %ld bytes for reading %s.",
                (fsize+1), filename );
        return -1;
    }

    fread( s, 1, fsize, f );
    if ( ferror( f ) )
    {
        log_error( "mart_load_from_file() error (errno %d) on fread of %s",
                errno, filename );
        clearerr( f );
        return -1;
    }

    fclose( f );
    s[ fsize ] = '\0';

    *dst = s;

    return fsize+1;
}


int load_corporate_action_symbols (
        struct TickerExchangePair ** dst,
        const struct Country * country,
        int32_t idate )
{
    size_t max_sql_length = 1024;
    char sql [max_sql_length];

    snprintf( sql, max_sql_length,
            "SELECT RTRIM(symbol) AS ticker, RTRIM(exchange) AS exchange "
                "FROM corpaction WHERE exdate = '%d' AND exchange = '%c'",
            idate, country->market_letter );

    printf( "%s\n", sql );

    struct db_fld fields [] = {
        { "ticker", -1, offsetof(struct TickerExchangePair,ticker), DBCHAR, sizeof((*dst)->ticker) },
        { "exchange", -1, offsetof(struct TickerExchangePair,exch), DBCHAR, sizeof((*dst)->exch) }
    };

    size_t rcnt = 0;
    int res;

    db_t hfsdb;
    if ( 0 != ( res = hfstock_connect( &hfsdb, country ) ) )
    {
        log_error( "load_corporate_action_symbols hfstock_connect() returned %d", res );
        return -1;
    }

    if ( 0 != ( res = db_read( hfsdb, sql, fields, 2, sizeof(struct TickerExchangePair), dst, &rcnt ) ) )
    {
        char * err_str;
        db_error( hfsdb, &res, &err_str );
        log_error( "load_corporate_action_symbols db_read() returned %d, with error: '%s'", res, err_str );
        db_close( hfsdb );
        free( err_str );
        return -1;
    }

    db_close( hfsdb );

    return 0;
}


int filter_corporate_action_symbols (
        const struct TickerExchangePair * tes,
        struct StockCharacteristics * scs )
{
    // TODO

    return 0;
}


int link_symbol_trading_datas (
        struct SymbolData * s,
        struct SymbolTradingData * t,
        size_t n )
{
    for ( size_t i = 0; i < n; i++ )
    {
        if ( s[i].in_universe > 0 )
        { s[i].trading_data = &t[i]; }
    }

    return 0;
}


int load_stock_characteristics (
        struct StockCharacteristics ** scs,
        size_t n,
        sidb_t sidb,
        uint16_t set_id,
        int32_t idate,
        const struct Country * country )
{
    int previous_idate = country_previous_trading_day( country, idate );
    int previous_previous_idate = country_previous_trading_day( country, previous_idate );

    size_t max_sql_length = 2048;
    char sql [max_sql_length];

    snprintf( sql, max_sql_length,
            "SELECT C1.idate AS prev_idate, RTRIM(C1.symbol) AS ticker, "
                "RTRIM(C1.uniqueID) AS uniqueID, "
                "C1.medvolume as medvolume, C1.volume as volume, C1.medvolatility as medvolatility, "
                "C1.medmedsprd as medmedsprd, C1.mednquotes as mednquotes, C1.medntrades as medntrades, "
                "C1.market as market, C1.lotsize as lotsize, C1.shareout as shareout, "
                "C1.adjust as prev_adjust, C0.adjust as adjust, C0.tickvalid as tickvalid, "
                "C1.open_ as prev_open, C1.close_ as prev_close, C1.high as prev_high, C1.low as prev_low, "
                "C2.open_ as prev_prev_open, C2.close_ as prev_prev_close, C2.high as prev_prev_high, C2.low as prev_prev_low "
            "FROM stockcharacteristics C1 "
            "left join (SELECT symbol, open_, close_, high, low from stockcharacteristics where idate = %d and market = '%c') C2 on C1.symbol = C2.symbol "
            "left join (SELECT symbol, adjust, tickvalid from stockcharacteristics where idate = %d and market = '%c') C0 on C1.symbol = C0.symbol "
                "WHERE C1.idate = %d "
                    "AND C1.inuniverse = 1 "
                    "AND C1.open_ > 0 "
                    "AND C1.close_ > 0 "
                    "AND C1.low > 0 "
                    "AND C1.low < C1.high "
                    "AND C1.market = '%c' "
                    "AND C1.medVolume > 0 "
                    "AND C1.medVolatility > 0 "
                    "AND C1.medMedSprd > 0",
            previous_previous_idate, country->market_letter,
            idate, country->market_letter,
            previous_idate, country->market_letter );


    struct StockCharacteristics * scs_tmp = NULL;
    size_t rcnt = 0;

    struct db_fld fields [] = {
        { "prev_idate", -1, offsetof(struct StockCharacteristics,prev_idate), DBINT32, 0 },
        { "ticker", -1, offsetof(struct StockCharacteristics,ticker), DBCHAR, sizeof(scs_tmp->ticker) },
        { "medvolume", -1, offsetof(struct StockCharacteristics,med_volume), DBDBL, 0 },
        { "volume", -1, offsetof(struct StockCharacteristics,volume), DBDBL, 0 },
        { "medvolatility", -1, offsetof(struct StockCharacteristics,med_volatility), DBDBL, 0 },
        { "medmedsprd", -1, offsetof(struct StockCharacteristics,med_med_sprd), DBDBL, 0 },
        { "mednquotes", -1, offsetof(struct StockCharacteristics,med_nquotes), DBDBL, 0 },
        { "medntrades", -1, offsetof(struct StockCharacteristics,med_ntrades), DBDBL, 0 },
        { "tickvalid", -1, offsetof(struct StockCharacteristics,tick_valid), DBINT16, 0 },
        { "adjust", -1, offsetof(struct StockCharacteristics,adjust), DBDBL, 0 },
        { "prev_adjust", -1, offsetof(struct StockCharacteristics,prev_adjust), DBDBL, 0 },
        { "prev_open", -1, offsetof(struct StockCharacteristics,prev_open), DBDBL, 0 },
        { "prev_close", -1, offsetof(struct StockCharacteristics,prev_close), DBDBL, 0 },
        { "prev_high", -1, offsetof(struct StockCharacteristics,prev_high), DBDBL, 0 },
        { "prev_low", -1, offsetof(struct StockCharacteristics,prev_low), DBDBL, 0 },
        { "prev_prev_open", -1, offsetof(struct StockCharacteristics,prev_prev_open), DBDBL, 0 },
        { "prev_prev_close", -1, offsetof(struct StockCharacteristics,prev_prev_close), DBDBL, 0 },
        { "prev_prev_high", -1, offsetof(struct StockCharacteristics,prev_prev_high), DBDBL, 0 },
        { "prev_prev_low", -1, offsetof(struct StockCharacteristics,prev_prev_low), DBDBL, 0 },
        { "market", -1, offsetof(struct StockCharacteristics,market), DBCHAR, 1 },
        { "lotsize", -1, offsetof(struct StockCharacteristics,lot_size), DBINT32, 0 },
        { "shareout", -1, offsetof(struct StockCharacteristics,shareout), DBINT64, 0 }
    };

    int res;

    db_t hfsdb;
    if ( 0 != ( res = hfstock_connect( &hfsdb, country ) ) )
    {
        log_error( "load_stock_characteristics hfstock_connect() returned %d", res );
        return -1;
    }

    if ( 0 != ( res = db_read( hfsdb, sql, fields, 22, sizeof(struct StockCharacteristics), &scs_tmp, &rcnt ) ) )
    {
        char * err_str;
        db_error( hfsdb, &res, &err_str );
        log_error( "load_stock_characteristics db_read() returned %d, with error: '%s'", res, err_str );
        db_close( hfsdb );
        free( err_str );
        return -1;
    }
    else if ( rcnt == 0 )
    {
        log_error( "load_stock_characteristics db_read() return rcnt = %d", rcnt );
        db_close( hfsdb );
        return -1;
    }
    else
    { log_notice( "Reading %lu rows from stockcharacteristics", rcnt ); }

    db_close( hfsdb );

    // Go through all rows, and grab only those from the correct set, and
    // arrange them according to sec_id.
    *scs = (struct StockCharacteristics *) calloc( n, sizeof(struct StockCharacteristics) );

    int loaded = 0;
    int in_other_sets = 0;

    for ( size_t r = 0; r < rcnt; r++ )
    {
        struct StockCharacteristics * sc_tmp = &scs_tmp[r];
        int ok;

        if ( sc_tmp->prev_open > 0                  &&
             sc_tmp->prev_close > 0                 &&
             sc_tmp->prev_low > 0                   &&
             sc_tmp->prev_high > sc_tmp->prev_low   &&
             sc_tmp->med_volume > 0                 &&
             sc_tmp->med_med_sprd > 0               &&
             sc_tmp->med_volatility > 0 )
        { ok = 1; }
        else
        { ok = 0; }

        if ( !ok )
        {
            log_notice( "Bad stockcharacteristics data for %s on %d: open:%d high:%f low:%f close:%f medVolume:%f medMedSprd:%f medVolatility:%f",
                    sc_tmp->ticker, idate, sc_tmp->prev_open, sc_tmp->prev_high, sc_tmp->prev_low, sc_tmp->prev_close,
                    sc_tmp->med_volume, sc_tmp->med_med_sprd, sc_tmp->med_volatility );
            continue;
        }

        const struct si * sym_info = sidb_byname( sidb, ' ', sc_tmp->ticker, strlen(sc_tmp->ticker) );

        if ( !sym_info )
        {
            log_notice( "Ticker '%s' was not found in the sidb", sc_tmp->ticker );
            continue;
        }

        if ( UINT16_MAX != set_id && sym_info->set_id != set_id )
        {
            // We only want symbols from the specified set.
            log_debug( "Ticker '%s' is in set %u, not %u",
                    sc_tmp->ticker, sym_info->set_id, set_id );
            ++in_other_sets;
            continue;
        }

        if ( sym_info->sec_id >= n )
        {
            log_info( "sec_id for %s is %d, but should be less than %lu",
                    sc_tmp->ticker, sym_info->sec_id, n );
            continue;
        }

        struct StockCharacteristics * sc = &(*scs)[sym_info->sec_id];

        if ( sc->ok )
        {
            log_notice( "Revisiting %d %s", sym_info->sec_id, sc->ticker );
        }

        memcpy( sc, sc_tmp, sizeof(struct StockCharacteristics) );
        sc->ok = 1;

        ++loaded;
    }

    log_notice( "Loaded %d rows from stockcharacteristics, %d were in other symbol sets. Last date of %d",
            loaded, in_other_sets, scs_tmp[0].prev_idate );

    if ( scs_tmp )  { free( scs_tmp ); }

    return 0;
}


int load_order_params (
        struct OrderParams ** op,
        size_t n,
        sidb_t sidb,
        int16_t set_id,
        int32_t idate,
        const struct Country * country )
{
    size_t max_sql_length = 2048;
    char sql [max_sql_length];

    snprintf( sql, max_sql_length,
            "SELECT idate, RTRIM(symbol) as ticker, thresin, thresout, maxposition, mincanceltime, maxcanceltime, "
            "rforcefact, oddlottype, trdlotsize, ccfactor, ticksize, isshorthedge "
            "FROM hforderparams "
            "WHERE idate = (SELECT DISTINCT MAX(IDATE) FROM hforderparams WHERE idate <= %d AND exchange = '%c') "
            "AND exchange = '%c' ",
            idate, country->market_letter, country->market_letter
            );

    struct OrderParams * op_tmp = NULL;
    size_t rcnt = 0;

    struct db_fld fields [] = {
        { "idate", -1, offsetof(struct OrderParams,idate), DBINT32, 0 },
        { "ticker", -1, offsetof(struct OrderParams,ticker), DBCHAR, sizeof(op_tmp->ticker) },
        { "thresin", -1, offsetof(struct OrderParams,thres_in), DBDBL, 0 },
        { "thresout", -1, offsetof(struct OrderParams,thres_out), DBDBL, 0 },
        { "maxposition", -1, offsetof(struct OrderParams,max_position), DBINT32, 0 },
        { "mincanceltime", -1, offsetof(struct OrderParams,min_cancel_time), DBINT32, 0 },
        { "maxcanceltime", -1, offsetof(struct OrderParams,max_cancel_time), DBINT32, 0 },
        { "rforcefact", -1, offsetof(struct OrderParams,r_force_fact), DBDBL, 0 },
        { "ticksize", -1, offsetof(struct OrderParams,tick_size), DBDBL, 0 },
        { "isshorthedge", -1, offsetof(struct OrderParams,is_short_hedge), DBINT16, 0 }
    };

    int res;

    db_t hfsdb;
    if ( 0 != ( res = hfstock_connect( &hfsdb, country ) ) )
    {
        log_error( "load_order_params hfstock_connect() returned %d", res );
        return -1;
    }

    if ( 0 != ( res = db_read( hfsdb, sql, fields, 10, sizeof(struct OrderParams), &op_tmp, &rcnt ) ) )
    {
        char * err_str;
        db_error( hfsdb, &res, &err_str );
        log_error( "load_order_params db_read() returned %d, with error: '%s'", res, err_str );
        db_close( hfsdb );
        free( err_str );
        return -1;
    }
    else if ( rcnt == 0 )
    {
        log_error( "load_order_params db_read() return rcnt = %d", rcnt );
        db_close( hfsdb );
        return -1;
    }
    else
    { log_notice( "Reading %lu rows from hforderparams", rcnt ); }

    db_close( hfsdb );

    // Go through all rows, and grab only those from the correct set, and
    // arrange them according to sec_id.
    *op = (struct OrderParams *) calloc( n, sizeof(struct OrderParams) );

    int loaded = 0;
    int in_other_sets = 0;

    for ( size_t r = 0; r < rcnt; r++ )
    {
        struct OrderParams * o_tmp = &op_tmp[r];
        int ok = 1;

        if(true) // TODO: what to check?
            ok = 1;
        else
            ok = 0;

        if(!ok)
        {
            log_notice("Bad hforderparams data for %s on %d",
                    o_tmp->ticker, idate);
            continue;
        }

        const struct si * sym_info = sidb_byname( sidb, ' ', o_tmp->ticker, strlen(o_tmp->ticker) );

        if ( !sym_info )
        {
            log_notice( "Ticker '%s' was not found in the sidb", o_tmp->ticker );
            continue;
        }

        if ( sym_info->set_id != set_id )
        {
            // We only want symbols from the specified set.
            log_debug( "Ticker '%s' is in set %u, not %u",
                    o_tmp->ticker, sym_info->set_id, set_id );
            ++in_other_sets;
            continue;
        }

        if ( sym_info->sec_id >= n )
        {
            log_info( "sec_id for %s is %d, but should be less than %lu",
                    o_tmp->ticker, sym_info->sec_id, n );
            continue;
        }

        struct OrderParams * o = &(*op)[sym_info->sec_id];

        if ( o->ok )
        {
            log_notice( "Revisiting %d %s", sym_info->sec_id, o->ticker );
        }

        memcpy( o, o_tmp, sizeof(struct MarketMakingParams) );
        o->ok = 1;

        ++loaded;
    }

    log_notice( "Loaded %d rows from hforderparams, %d were in other symbol sets. Last date of %d",
            loaded, in_other_sets, op_tmp[0].idate );

    if ( op_tmp )  { free( op_tmp ); }

    return 0;
}

int load_market_making_params (
        struct MarketMakingParams ** mmp,
        size_t n,
        sidb_t sidb,
        int16_t set_id,
        int32_t idate,
        const struct Country * country )
{
    size_t max_sql_length = 2048;
    char sql [max_sql_length];

    snprintf( sql, max_sql_length,
            "SELECT idate, RTRIM(symbol) as ticker, sendnonmarketable, tradelimit, maxqty, asym, ordspread, ordconst, ordconstpsh, ordpos, ordforec, "
            "selspread, selconst, selconstpsh, selpos, selforec, seltofday1, seltofday2, insertthres, costliquidparam, defaultouting, "
            "FROM hfalparams "
            "WHERE idate = (SELECT DISTINCT MAX(IDATE) FROM hfalparams WHERE idate <= %d AND exchange = '%c') "
            "AND exchange = '%c' ",
            idate, country->market_letter, country->market_letter
            );

    struct MarketMakingParams * mmp_tmp = NULL;
    size_t rcnt = 0;

    struct db_fld fields [] = {
        { "idate", -1, offsetof(struct MarketMakingParams,idate), DBINT32, 0 },
        { "ticker", -1, offsetof(struct MarketMakingParams,ticker), DBCHAR, sizeof(mmp_tmp->ticker) },
        { "sendnonmarketable", -1, offsetof(struct MarketMakingParams,send_non_marketable), DBINT32, 0 },
        { "tradelimit", -1, offsetof(struct MarketMakingParams,trade_limit), DBINT32, 0 },
        { "maxqty", -1, offsetof(struct MarketMakingParams,max_qty), DBINT32, 0 },
        { "asym", -1, offsetof(struct MarketMakingParams,asym), DBDBL, 0 },
        { "ordspread", -1, offsetof(struct MarketMakingParams,ord_spread), DBDBL, 0 },
        { "ordconst", -1, offsetof(struct MarketMakingParams,ord_const), DBDBL, 0 },
        { "ordconstpsh", -1, offsetof(struct MarketMakingParams,ord_const_psh), DBDBL, 0 },
        { "ordpos", -1, offsetof(struct MarketMakingParams,ord_pos), DBDBL, 0 },
        { "ordforec", -1, offsetof(struct MarketMakingParams,ord_forec), DBDBL, 0 },
        { "selspread", -1, offsetof(struct MarketMakingParams,sel_spread), DBDBL, 0 },
        { "selconst", -1, offsetof(struct MarketMakingParams,sel_const), DBDBL, 0 },
        { "selconstpsh", -1, offsetof(struct MarketMakingParams,sel_const_psh), DBDBL, 0 },
        { "selpos", -1, offsetof(struct MarketMakingParams,sel_pos), DBDBL, 0 },
        { "selforec", -1, offsetof(struct MarketMakingParams,sel_forec), DBDBL, 0 },
        { "seltofday1", -1, offsetof(struct MarketMakingParams,sel_tof_day1), DBDBL, 0 },
        { "seltofday2", -1, offsetof(struct MarketMakingParams,sel_tof_day2), DBDBL, 0 },
        { "insertthres", -1, offsetof(struct MarketMakingParams,insert_thres), DBDBL, 0 },
        { "costliquidparam", -1, offsetof(struct MarketMakingParams,cost_liquid_param), DBDBL, 0 },
        { "defaultrouting", -1, offsetof(struct MarketMakingParams,default_routing), DBINT16, 0 }
    };

    int res;

    db_t hfsdb;
    if ( 0 != ( res = hfstock_connect( &hfsdb, country ) ) )
    {
        log_error( "load_market_making_params hfstock_connect() returned %d", res );
        return -1;
    }

    if ( 0 != ( res = db_read( hfsdb, sql, fields, 21, sizeof(struct MarketMakingParams), &mmp_tmp, &rcnt ) ) )
    {
        char * err_str;
        db_error( hfsdb, &res, &err_str );
        log_error( "load_market_making_params db_read() returned %d, with error: '%s'", res, err_str );
        db_close( hfsdb );
        free( err_str );
        return -1;
    }
    else if ( rcnt == 0 )
    {
        log_error( "load_market_making_params db_read() return rcnt = %d", rcnt );
        db_close( hfsdb );
        return -1;
    }
    else
    { log_notice( "Reading %lu rows from hfalparams", rcnt ); }

    db_close( hfsdb );

    // Go through all rows, and grab only those from the correct set, and
    // arrange them according to sec_id.
    *mmp = (struct MarketMakingParams *) calloc( n, sizeof(struct MarketMakingParams) );

    int loaded = 0;
    int in_other_sets = 0;

    for ( size_t r = 0; r < rcnt; r++ )
    {
        struct MarketMakingParams * mm_tmp = &mmp_tmp[r];
        int ok = 1;

        if(true) // TODO: what to check?
            ok = 1;
        else
            ok = 0;

        if(!ok)
        {
            log_notice("Bad hfalparams data for %s on %d",
                    mm_tmp->ticker, idate);
            continue;
        }

        const struct si * sym_info = sidb_byname( sidb, ' ', mm_tmp->ticker, strlen(mm_tmp->ticker) );

        if ( !sym_info )
        {
            log_notice( "Ticker '%s' was not found in the sidb", mm_tmp->ticker );
            continue;
        }

        if ( sym_info->set_id != set_id )
        {
            // We only want symbols from the specified set.
            log_debug( "Ticker '%s' is in set %u, not %u",
                    mm_tmp->ticker, sym_info->set_id, set_id );
            ++in_other_sets;
            continue;
        }

        if ( sym_info->sec_id >= n )
        {
            log_info( "sec_id for %s is %d, but should be less than %lu",
                    mm_tmp->ticker, sym_info->sec_id, n );
            continue;
        }

        struct MarketMakingParams * mm = &(*mmp)[sym_info->sec_id];

        if ( mm->ok )
        {
            log_notice( "Revisiting %d %s", sym_info->sec_id, mm->ticker );
        }

        memcpy( mm, mm_tmp, sizeof(struct MarketMakingParams) );
        mm->ok = 1;

        ++loaded;
    }

    log_notice( "Loaded %d rows from hfalparams, %d were in other symbol sets. Last date of %d",
            loaded, in_other_sets, mmp_tmp[0].idate );

    if ( mmp_tmp )  { free( mmp_tmp ); }

    return 0;
}


static int read_positions_from_file (
        struct SymbolData * symbol_datas,
        size_t * count,
        double * cash,
        uint16_t set_id,
        sidb_t sidb,
        const char * filename )
{
    *count = 0;

    FILE * f = fopen( filename, "r" );

    if ( f == NULL )
    {
        log_error( "read_positions_from_file(): error opening file %s", filename );
        return ERROR_FILE_ERROR;
    }

    size_t cnt = 0;
    int32_t idx, quantity;
    char ticker [16];
    float notional;

    while ( fscanf( f, "%d %s %d %f", &idx, ticker, &quantity, &notional ) != EOF )
    {
        log_debug( "READ POSITION FROM FILE: %s = %d", ticker, quantity );

        if ( idx == CASH_POSITION_INDEX )
        {
            *cash = notional;
            continue;
        }

        const struct si * sym_info = sidb_byname( sidb, ' ', ticker, strlen(ticker) );

        if ( !sym_info )
        {
            log_debug( "Ticker '%s' was not found in the sidb", ticker );
            continue;
        }

        if ( sym_info->set_id != set_id )
        {
            // We only want symbols from the specified set.
            continue;
        }

        struct SymbolTradingData * std = symbol_datas[sym_info->sec_id].trading_data;

        std->share_position = quantity;

        ++cnt;
    }

    fclose( f );

    log_info( "read_positions_from_file(): read %lu positions from %s", cnt, filename );

    *count = cnt;

    return 0;
}


int read_positions_from_files (
        struct SymbolData * symbol_datas,
        double * total_cash_position,
        size_t * file_count,
        const struct Country * country,
        int32_t prev_idate,
        uint16_t set_id,
        sidb_t sidb,
        const char * dir )
{
    char pattern [512];
    snprintf( pattern, 512, "%s/%s_positions_set*_%d.txt", dir, country->name, prev_idate );

    glob_t globs;
    int ret = glob( pattern, GLOB_TILDE|GLOB_BRACE, NULL, &globs );

    switch ( ret )
    {
        case GLOB_NOMATCH:
            log_notice( "read_positions_from_files(): did not find any position files for pattern %s", pattern );
            ret = 0;
            break;
        case GLOB_NOSPACE:
            log_error( "read_positions_from_files(): ran out of memory for pattern %s", pattern);
            ret = ERROR_FILE_ERROR;
            break;
        case GLOB_ABORTED:
            log_error( "read_positions_from_files(): hit read error for pattern %s", pattern );
            ret = ERROR_FILE_ERROR;
            break;
    }

    if ( ret != 0 )
    {
        globfree( &globs );
        return ret;
    }

    if ( globs.gl_pathc == 0 )
    {
        globfree( &globs );
        log_notice( "read_positions_from_files(): did not find any position files from trade date %d", prev_idate );
        return 0;
    }

    size_t total_loaded_positions = 0;
    size_t cnt;
    double cash;
    *total_cash_position = 0;
    *file_count = 0;

    for ( size_t i = 0; i < globs.gl_pathc; ++i )
    {
        ret = read_positions_from_file( symbol_datas, &cnt, &cash, set_id, sidb, globs.gl_pathv[i] );

        if ( ret != 0 )
        {
            log_error( "read_positions_from_files(): call to read_positions_from_file() returned %d", ret );
            break;
        }

        *total_cash_position += cash;
        *file_count += 1;

        total_loaded_positions += cnt;
    }

    globfree( &globs );

    log_notice( "Loaded %lu positions from files in %s from idate %d", total_loaded_positions, dir, prev_idate );

    return 0;
}

