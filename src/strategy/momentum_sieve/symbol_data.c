#include <mrtl/strategy/momentum_sieve/symbol_data.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/functions.h>
#include <mlog.h>
#include <stdio.h>


bool is_in_universe ( const struct StockCharacteristics * sc )
{
    return ( sc->prev_close * sc->shareout > 370000000 );
}


int strategy_init_symbol_datas (
        void ** arr,
        size_t * cnt,
        struct SymbolData * symbol_datas,
        const struct StockCharacteristics * stock_characteristics,
        const struct OrderParams * order_params,
        const struct MarketMakingParams * market_making_params,
        size_t n_stock_characteristics,
        void * strategy_config )
{
    log_notice("Allocating for symbol datas: %lu", n_stock_characteristics);

    struct SymbolStrategyData * symbol_strategy_datas = calloc(
            n_stock_characteristics, sizeof(struct SymbolStrategyData) );
    //struct StrategyConfig * strat_conf = strategy_config;

    size_t symbols_in_universe = 0;

    for ( size_t i = 0; i < n_stock_characteristics; i++ )
    {
        const struct StockCharacteristics * c = &stock_characteristics[i];
        struct SymbolStrategyData *         s = &symbol_strategy_datas[i];
        struct SymbolData *                 d = &symbol_datas[i];

        d->sec_id = i;

        if ( !c->ok )
        {
            s->stock_characteristics_ok = 0;
            continue;
        }

        if ( !is_in_universe( c ) )
        { continue; }

        s->volume                   = c->volume;
        s->median_volume            = c->med_volume;
        s->median_volatility        = 10000. * c->med_volatility;
        s->med_med_sprd             = c->med_med_sprd;
        s->median_n_quotes          = c->med_nquotes;
        s->median_n_trades          = c->med_ntrades;
        s->previous_open_price      = c->prev_open;
        s->previous_close_price     = c->prev_close;
        s->previous_high_price      = c->prev_high;
        s->previous_low_price       = c->prev_low;
        s->lot_size                 = c->lot_size;
        s->stock_characteristics_ok = 1;
        strcpy( s->ticker, c->ticker );

        d->in_universe   = 1;
        d->strategy_data = s;
        strcpy( d->ticker, s->ticker );

        ++symbols_in_universe;

        // Initialize any members of SymbolStrategyData for this symbol.

        s->midprice_moving_average_1s   = (struct TimeWeightedMovingAverage){ .K=1*SECONDS,   .ma_last=0, .t_last=0 };
        s->midprice_moving_average_10s  = (struct TimeWeightedMovingAverage){ .K=10*SECONDS,  .ma_last=0, .t_last=0 };
        s->midprice_moving_average_30s  = (struct TimeWeightedMovingAverage){ .K=30*SECONDS,  .ma_last=0, .t_last=0 };
        s->midprice_moving_average_100s = (struct TimeWeightedMovingAverage){ .K=100*SECONDS, .ma_last=0, .t_last=0 };
        s->midprice_moving_average_300s = (struct TimeWeightedMovingAverage){ .K=300*SECONDS, .ma_last=0, .t_last=0 };
        s->midprice_moving_average_900s = (struct TimeWeightedMovingAverage){ .K=900*SECONDS, .ma_last=0, .t_last=0 };

        //lag_record_init( &s->midprice_lags, 1*SECONDS, 1*HOURS, 0.1 );
    }

    log_notice( "Symbols in universe: %lu", symbols_in_universe );

    *arr = symbol_strategy_datas;
    *cnt = n_stock_characteristics;

    return 0;
}


int strategy_on_book_update (
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        uint16_t mkt_id,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct BBO * nbbo_prev,
        void * strategy_config,
        const struct Country* country)
{
    struct SymbolStrategyData * s = symbol_data->strategy_data;

    if ( symbol_data->in_universe < 1 )
    { return ERROR_NOT_IN_UNIVERSE; }

    if ( !s->stock_characteristics_ok )
    { return ERROR_BAD_SYMBOL; }

    if ( nbbo_prev->bid.px != agg->bid[0].px ||
         nbbo_prev->ask.px != agg->ask[0].px )
    {
        // NBBO has changed.

        if ( bbo_struct_is_good( *nbbo_prev ) )
        {
            double nbbo_prev_mid = mid( fix2dbl(nbbo_prev->bid.px), fix2dbl(nbbo_prev->ask.px) );

            twma_close_previous_value( &s->midprice_moving_average_1s,   nsecs, nbbo_prev_mid );
            twma_close_previous_value( &s->midprice_moving_average_10s,  nsecs, nbbo_prev_mid );
            twma_close_previous_value( &s->midprice_moving_average_30s,  nsecs, nbbo_prev_mid );
            twma_close_previous_value( &s->midprice_moving_average_100s, nsecs, nbbo_prev_mid );
            twma_close_previous_value( &s->midprice_moving_average_300s, nsecs, nbbo_prev_mid );
            twma_close_previous_value( &s->midprice_moving_average_900s, nsecs, nbbo_prev_mid );
        }
        else if ( s->midprice_moving_average_1s.t_last == 0 )
        {
            // There is no data in the twma's yet, and the previous price is no
            // good.  Use current price.

            double nbbo_new_mid = mid( fix2dbl(agg->bid[0].px), fix2dbl(agg->ask[0].px) );

            twma_close_previous_value( &s->midprice_moving_average_1s,   nsecs, nbbo_new_mid );
            twma_close_previous_value( &s->midprice_moving_average_10s,  nsecs, nbbo_new_mid );
            twma_close_previous_value( &s->midprice_moving_average_30s,  nsecs, nbbo_new_mid );
            twma_close_previous_value( &s->midprice_moving_average_100s, nsecs, nbbo_new_mid );
            twma_close_previous_value( &s->midprice_moving_average_300s, nsecs, nbbo_new_mid );
            twma_close_previous_value( &s->midprice_moving_average_900s, nsecs, nbbo_new_mid );
        }
    }

    return 0;
}


int strategy_on_trade (
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        uint16_t mkt_id,
        int64_t px,
        uint32_t sz,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        void * strategy_config,
        const struct Country* country)
{
    struct SymbolStrategyData * s = symbol_data->strategy_data;

    if ( symbol_data->in_universe < 1 )
    { return ERROR_NOT_IN_UNIVERSE; }

    if ( !s->stock_characteristics_ok )
    { return ERROR_BAD_SYMBOL; }

    double price = fix2dbl( px );

    if ( price > s->high_trade_price || s->last_trade_time == 0 )
    { s->high_trade_price = price; }

    if ( price < s->low_trade_price || s->last_trade_time == 0 )
    { s->low_trade_price = price; }

    double notional_volume = price * sz;

    s->intraday_share_volume    += sz;
    s->intraday_notional_volume += notional_volume;

    s->btwp5   = weighted_moving_average( s->btwp5, notional_volume, 1./5. );
    s->btq5    = weighted_moving_average( s->btq5, sz, 1./5. );
    s->btwp20  = weighted_moving_average( s->btwp20, notional_volume, 1./20. );
    s->btq20   = weighted_moving_average( s->btq20, sz, 1./20. );
    s->btwp200 = weighted_moving_average( s->btwp200, notional_volume, 1./200. );
    s->btq200  = weighted_moving_average( s->btq200, sz, 1./200. );

    s->btq10  = weighted_moving_average( s->btq10, sz, 1./10. );
    s->btq100 = weighted_moving_average( s->btq100, sz, 1./100. );

    int8_t fill_imb;

    if ( px == book->ask[0].px )
    {
        // Buy on the ask
        fill_imb = 1;
        
        s->intraday_buy_share_volume += sz;
    }
    else if ( px == book->bid[0].px )
    {
        // Sell on the bid
        fill_imb = -1;

        s->intraday_sell_share_volume += sz;
    }
    else
    {
        // Do we want a float?  That can be inside the spread?
        fill_imb = 0;
    }

    s->fill_imb_5  = weighted_moving_average( s->fill_imb_5,  fill_imb, 1./5. );
    s->fill_imb_20 = weighted_moving_average( s->fill_imb_20, fill_imb, 1./20. );
    s->fill_imb_50 = weighted_moving_average( s->fill_imb_50, fill_imb, 1./50. );


    s->last_updated     = nsecs;
    s->last_trade_time  = nsecs;
    s->last_trade_price = price;

    return 0;
}

