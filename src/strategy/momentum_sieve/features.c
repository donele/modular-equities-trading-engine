#include <mrtl/strategy/momentum_sieve/symbol_data.h>
#include <mrtl/common/functions.h>
#include <mrtl/common/country.h>
#include <mrtl/common/constants.h>
#include <math.h>
#include <mlog.h>


int book_signals (
        const struct mbp_book * agg,
        float * rsams,
        float * bid_size_ratios,
        float * ask_size_ratios,
        uint8_t n )  // TODO: Check this type against args->levels
{
    memset( rsams, 0, n*sizeof(float) );
    memset( bid_size_ratios, 0, n*sizeof(float) );
    memset( ask_size_ratios, 0, n*sizeof(float) );

    if ( agg->bid[0].sz == 0 || agg->ask[0].sz == 0 )
    { return 0; }

    float b, bs, a, as, qi, spr, midi, sam, rsam;

    float nbbo_bid_size = agg->bid[0].sz;
    float nbbo_ask_size = agg->ask[0].sz;
    double nbbo_mid = mid( fix2dbl(agg->bid[0].px), fix2dbl(agg->ask[0].px) );

    for ( uint8_t i = 0; i < n; i++ )
    {
        if ( !book_side_is_good( agg->bid[i] ) ||
             !book_side_is_good( agg->ask[i] ) )
        { continue; }

        b  = fix2dbl( agg->bid[i].px );
        bs = agg->bid[i].sz;

        a  = fix2dbl( agg->ask[i].px );
        as = agg->ask[i].sz;

        qi   = ( bs - as ) / ( bs + as );
        spr  = a - b;
        midi = mid( b, a );

        // size adjusted midprice
        sam = qi * spr / 2.f + midi;
        // relative size adjusted midprice
        rsam = return_in_bps( nbbo_mid, sam );

        rsams[i]           = rsam;
        bid_size_ratios[i] = bs / nbbo_bid_size;
        ask_size_ratios[i] = as / nbbo_ask_size;
    }

    return 0;
}


int strategy_write_sample_header( char * dst, size_t dst_len )
{
    const size_t feature_string_length = 16;
    const size_t minimum_dst_len = ((MAX_FEATURE_COUNT+MAX_TARGET_COUNT) * feature_string_length) + 128;

    if ( dst_len < minimum_dst_len )
    {
        log_notice( "Warning: In call to strategy_write_sample_header(), dst_len should be > %lu, set to %lu.\n",
                minimum_dst_len, dst_len);
        return -1;
    }

    snprintf( dst, dst_len, "ticker,nsecs,bidSize,bid,ask,askSize" );

    for ( int i = 0; i < MAX_TARGET_COUNT; i++ )
    {
        char yc [feature_string_length];
        snprintf( yc, feature_string_length, ",target%d", i );
        strcat( dst, yc );
    }

    for ( int i = 0; i < MAX_FEATURE_COUNT; i++ )
    {
        char xc [feature_string_length];
        snprintf( xc, feature_string_length, ",input%d", i );
        strcat( dst, xc );
    }

    return 0;
}


int strategy_generate_features (
        float * xs,
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        const struct Country * country,
        const struct mbp_book * agg,
        void * strategy_config )
{
    memset( xs, 0, MAX_FEATURE_COUNT*sizeof(float) );

    const struct SymbolStrategyData * s = symbol_data->strategy_data;

    struct StrategyConfig * strat_conf = strategy_config;

    if ( !s->stock_characteristics_ok )
    { return ERROR_BAD_SYMBOL; }

    if ( agg->ask[0].px < agg->bid[0].px )
    { return ERROR_SPREAD_NEGATIVE; }

    if ( agg->bid[0].px == agg->ask[0].px )
    { return ERROR_SPREAD_ZERO; }

    double nbbo_bid = fix2dbl( agg->bid[0].px );
    double nbbo_ask = fix2dbl( agg->ask[0].px );

    if ( spread_in_bps( nbbo_bid, nbbo_ask ) > SPREAD_LIMIT )
    { return ERROR_SPREAD_TOO_WIDE; }

    float rsams [5];
    float bid_size_ratios [5];
    float ask_size_ratios [5];
    book_signals( agg, rsams, bid_size_ratios, ask_size_ratios, 5 );

    double mid_price = mid( nbbo_bid, nbbo_ask );
    float spread = spread_in_bps( nbbo_bid, nbbo_ask );

    float hlspr = ( s->last_trade_time > 0 ) ?
        spread_in_bps( s->low_trade_price, s->high_trade_price ) : 0.f;

    float fraction_of_day = ((double)( nsecs - country->open_time ))
        / ((double)( country->close_time - country->open_time ));

    double vwap = ( s->intraday_share_volume > 0 ) ?
        s->intraday_notional_volume / s->intraday_share_volume : 0.f;

    xs[0] = spread;
    xs[1] = fraction_of_day;
    xs[2] = s->median_volatility;
    xs[3] = log10( s->median_volume );
    xs[4] = s->previous_close_price;
    xs[5] = hlspr;
    xs[6] = hlspr / s->median_volatility;  // relative volatility
    xs[7] = s->volume / s->median_volume;
    xs[8] = return_in_bps( s->previous_open_price, s->previous_close_price );  // mretIntraLag1
    xs[9] = log10( 10000. * s->med_med_sprd );
    xs[10] = ( s->previous_close_price > 0 ) ?  // hilo_lag1_ratio
            ( s->previous_high_price - s->previous_low_price ) / s->previous_close_price : 0.f;

    xs[11] = ( s->previous_high_price > s->previous_close_price && s->last_trade_time > 0 ) ?  // hilow_lag1
            2. * ( s->last_trade_price - mid( s->previous_high_price, s->previous_low_price ) ) / ( s->previous_high_price - s->previous_low_price ) : 0.f;

    xs[12] = ( s->intraday_share_volume > 0 ) ?  // vwap_intra_ret
            return_in_bps( vwap, mid_price ) : 0.f;

    xs[13] = s->intraday_share_volume / ( fraction_of_day * s->median_volume );  // TODO: for very small fraction_of_day, this can get huge.  Keep the signal zero for the first few minutes?

    xs[14] = ( s->intraday_share_volume > 0 ) ?  // intraday buy:sell ratio
            (s->intraday_buy_share_volume - s->intraday_sell_share_volume) / s->intraday_share_volume : 0.f;

    xs[15] = ( s->last_trade_time > 0 ) ?  // closeToTrade
            return_in_bps( s->previous_close_price, s->last_trade_price ) : 0.f;

    xs[16] = ( s->last_trade_price > 0 ) ?  // rtrd
            return_in_bps( s->last_trade_price, mid_price ) : 0.f;

    xs[17] = ( spread_in_bps(s->low_trade_price, s->high_trade_price) > 10. ) ?  // hilo
            ( 2. * ( s->last_trade_price - mid( s->high_trade_price, s->low_trade_price ) ) / ( s->high_trade_price-s->low_trade_price ) ) : 0.f;

    xs[18] = return_in_bps( s->previous_close_price, mid_price );

    xs[19] = s->btq10 * 1.e6 / s->median_volume;
    xs[20] = s->btq100 * 1.e6 / s->median_volume;

    xs[21] = (s->intraday_share_volume > 0 && s->btwp20 > 0.1) ?  // bollingerish20
        return_in_bps( vwap, s->btwp20 / s->btq20 ) : 0.f;

    xs[22] = (s->intraday_share_volume > 0 && s->btwp200 > 0.1) ?  // bollingerish200
        return_in_bps( vwap, s->btwp200 / s->btq200 ) : 0.f;

    xs[23] = s->btq5 > 0 ? return_in_bps( mid_price, s->btwp5 / s->btq5 ) : 0.f;
    xs[24] = s->btq20 > 0 ? return_in_bps( mid_price, s->btwp20 / s->btq20 ) : 0.f;

    xs[25] = s->fill_imb_5;
    xs[26] = s->fill_imb_20;
    xs[27] = s->fill_imb_50;

    xs[28] = rsams[0];
    xs[29] = rsams[1];
    xs[30] = rsams[2];
    xs[31] = rsams[3];
    xs[32] = rsams[4];

    xs[33] = bid_size_ratios[1];
    xs[34] = bid_size_ratios[2];
    xs[35] = bid_size_ratios[3];
    xs[36] = bid_size_ratios[4];

    xs[37] = ask_size_ratios[1];
    xs[38] = ask_size_ratios[2];
    xs[39] = ask_size_ratios[3];
    xs[40] = ask_size_ratios[4];

    xs[41] = quote_imbalance( agg->bid[0].sz, agg->ask[0].sz );
    xs[42] = nsecs_to_secs( nsecs - s->last_trade_time );

    xs[43] = return_in_bps( twma_calculate( &s->midprice_moving_average_1s,   nsecs, mid_price ), mid_price );
    xs[44] = return_in_bps( twma_calculate( &s->midprice_moving_average_10s,  nsecs, mid_price ), mid_price );
    xs[45] = return_in_bps( twma_calculate( &s->midprice_moving_average_30s,  nsecs, mid_price ), mid_price );
    xs[46] = return_in_bps( twma_calculate( &s->midprice_moving_average_100s, nsecs, mid_price ), mid_price );
    xs[47] = return_in_bps( twma_calculate( &s->midprice_moving_average_300s, nsecs, mid_price ), mid_price );
    xs[48] = return_in_bps( twma_calculate( &s->midprice_moving_average_900s, nsecs, mid_price ), mid_price );

    xs[49] = xs[43] / spread;
    xs[50] = xs[44] / spread;
    xs[51] = xs[45] / spread;
    
    //xs[] = ;
    //xs[] = ;
    //xs[] = ;
    //xs[] = ;
    //xs[] = ;


    int8_t features_finite = 1;

    for ( size_t i = 0; i < MAX_FEATURE_COUNT; i++ )
    {
        if ( !isfinite( xs[i] ) )
        {
            log_notice( "Warning: feature %d has value %f", i, xs[i] );
            features_finite = 0;
        }
    }

    return features_finite ? 0 : ERROR_FEATURE_NOT_FINITE;
}

