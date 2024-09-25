#include <mrtl/strategy/classic/symbol_data.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/functions.h>
#include <mlog.h>
#include <stdio.h>

int strategy_init_symbol_datas(
        void ** arr,
        size_t * cnt,
        struct SymbolData * symbol_datas,
        const struct StockCharacteristics * stock_characteristics,
        const struct OrderParams * order_params,
        const struct MarketMakingParams * market_making_params,
        size_t n_stock_characteristics,
        void * strategy_config)
{
    log_notice("Allocating for symbol datas: %lu", n_stock_characteristics);

    struct SymbolStrategyData * symbol_strategy_datas = calloc(
            n_stock_characteristics, sizeof(struct SymbolStrategyData) );

    size_t symbols_in_universe = 0;

    for(int i = 0; i < n_stock_characteristics; ++i)
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
        d->in_universe   = 1;
        d->strategy_data = s;
        strcpy( d->ticker, c->ticker );

        s->volume            = c->volume;
        s->median_volume     = c->med_volume;
        s->median_volatility = 10000. * c->med_volatility;
        s->med_med_sprd      = c->med_med_sprd;
        s->median_n_quotes   = c->med_nquotes;
        s->median_n_trades   = c->med_ntrades;
        s->lot_size          = c->lot_size;
        s->p_volume          = c->volume;
        s->adjust            = (c->adjust > 0.) ? c->adjust : 1.;
        s->prev_adjust       = (c->prev_adjust > 0.) ? c->prev_adjust : 1.;
        s->tick_valid        = (c->tick_valid > 0) ? 1 : 0;

        s->p_open            = c->prev_open / s->adjust;
        s->p_close           = c->prev_close / s->adjust;
        s->p_high            = c->prev_high / s->adjust;
        s->p_low             = c->prev_low / s->adjust;

        s->pp_open           = c->prev_prev_open / s->adjust / s->prev_adjust;
        s->pp_close          = c->prev_prev_close / s->adjust / s->prev_adjust;
        s->pp_high           = c->prev_prev_high / s->adjust / s->prev_adjust;
        s->pp_low            = c->prev_prev_low / s->adjust / s->prev_adjust;

        double marketCap = c->shareout * c->prev_close;
        s->logCap = (marketCap > 1.) ? log(marketCap) : 0.;

        sprintf(s->ticker, c->ticker);
        s->stock_characteristics_ok = 1;

        // Initialize any members of SymbolInstrumentData for this symbol.

        s->nbbo_bid_mkt_id = '\0';
        s->nbbo_ask_mkt_id = '\0';
        s->last_book_time = 0;
        s->last_trade_time = 0;

        s->prev_mid = 0.;
        s->first_quote_mid = 0.;
        s->first_quote_time = 0;
        s->overnight_return_bps = 0.;
        s->last_trade_price = 0.;
        s->high_trade_price = 0.;
        s->low_trade_price = 0.;
        s->high_trade_price_900s = 0.;
        s->low_trade_price_900s = 0.;
        s->mid_before_lastTrade = 0.;
        s->mid_before_lastTradeBeforeNbbo = 0.;

        s->fill_imb = 0.;

        s->intraday_ntrades = 0.;
        s->intraday_share_volume = 0.;
        s->intraday_price_sum = 0.;
        s->intraday_notional_volume = 0.;
        s->intraday_buy_share_volume = 0.;
        s->intraday_sell_share_volume = 0.;

        s->midprice_moving_average_5s    = (struct TimeWeightedMovingAverage){ .K=5*SECONDS,    .ma_last=0, .t_last=0 };
        s->midprice_moving_average_15s   = (struct TimeWeightedMovingAverage){ .K=15*SECONDS,   .ma_last=0, .t_last=0 };
        s->midprice_moving_average_30s   = (struct TimeWeightedMovingAverage){ .K=30*SECONDS,   .ma_last=0, .t_last=0 };
        s->midprice_moving_average_60s   = (struct TimeWeightedMovingAverage){ .K=60*SECONDS,   .ma_last=0, .t_last=0 };
        s->midprice_moving_average_120s  = (struct TimeWeightedMovingAverage){ .K=120*SECONDS,  .ma_last=0, .t_last=0 };
        s->midprice_moving_average_300s  = (struct TimeWeightedMovingAverage){ .K=300*SECONDS,  .ma_last=0, .t_last=0 };
        s->midprice_moving_average_600s  = (struct TimeWeightedMovingAverage){ .K=600*SECONDS,  .ma_last=0, .t_last=0 };
        s->midprice_moving_average_1200s = (struct TimeWeightedMovingAverage){ .K=1200*SECONDS, .ma_last=0, .t_last=0 };
        s->midprice_moving_average_2400s = (struct TimeWeightedMovingAverage){ .K=2400*SECONDS, .ma_last=0, .t_last=0 };
        s->midprice_moving_average_4800s = (struct TimeWeightedMovingAverage){ .K=4800*SECONDS, .ma_last=0, .t_last=0 };
        s->midprice_moving_average_9600s = (struct TimeWeightedMovingAverage){ .K=9600*SECONDS, .ma_last=0, .t_last=0 };

        s->lacc_trade_qty_sum_15s    = (struct LeakyAccumulator){ .K=15*SECONDS,   .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_30s    = (struct LeakyAccumulator){ .K=30*SECONDS,   .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_60s    = (struct LeakyAccumulator){ .K=60*SECONDS,   .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_120s   = (struct LeakyAccumulator){ .K=120*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_300s   = (struct LeakyAccumulator){ .K=300*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_600s   = (struct LeakyAccumulator){ .K=600*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_trade_qty_sum_3600s  = (struct LeakyAccumulator){ .K=3600*SECONDS, .sum_last=0, .t_last=0 };

        s->lacc_trade_count_300s     = (struct LeakyAccumulator){ .K=300*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_trade_count_900s     = (struct LeakyAccumulator){ .K=900*SECONDS,  .sum_last=0, .t_last=0 };
        s->trade_price_moving_average_300s = (struct TimeWeightedMovingAverage){ .K=300*SECONDS, .ma_last=0, .t_last=0 };
        s->trade_price_moving_average_900s = (struct TimeWeightedMovingAverage){ .K=900*SECONDS, .ma_last=0, .t_last=0 };

        s->lacc_max_bid_size_200s  = (struct LeakyAccumulator){ .K=200*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_max_ask_size_200s  = (struct LeakyAccumulator){ .K=200*SECONDS,  .sum_last=0, .t_last=0 };
        s->lacc_max_bid_size_1200s = (struct LeakyAccumulator){ .K=1200*SECONDS, .sum_last=0, .t_last=0 };
        s->lacc_max_ask_size_1200s = (struct LeakyAccumulator){ .K=1200*SECONDS, .sum_last=0, .t_last=0 };

        ++symbols_in_universe;
    }

    if(market_making_params != NULL)
    {
        for(int i = 0; i < n_stock_characteristics; ++i)
        {
            const struct MarketMakingParams* m = &market_making_params[i];
            struct SymbolStrategyData* s = &symbol_datas[i];
            if(!m->ok)
                continue;

            s->ord_spread = m->ord_spread;
            s->ord_const = m->ord_const;
            s->ord_const_psh = m->ord_const_psh;
            s->ord_pos = m->ord_pos;
            s->ord_forec = m->ord_forec;

            s->send_non_marketable = m->send_non_marketable;
            s->sel_spread = m->sel_spread;
            s->sel_const = m->sel_const;
            s->sel_const_psh = m->sel_const_psh;
            s->sel_pos = m->sel_pos;
            s->sel_forec = m->sel_forec;
            s->sel_tof_day1 = m->sel_tof_day1;
            s->sel_tof_day2 = m->sel_tof_day2;
            s->insert_thres = m->insert_thres;
            s->cost_liquid_param = m->cost_liquid_param;
            s->default_routing = m->default_routing;

            s->trade_limit = m->trade_limit;
            s->max_qty = m->max_qty;
            s->asym = m->asym;
        }
    }

    if(order_params != NULL)
    {
        for(int i = 0; i < n_stock_characteristics; ++i)
        {
            const struct OrderParams* o = &order_params[i];
            struct SymbolStrategyData* s = &symbol_datas[i];
            if(!o->ok)
                continue;

            s->thres_in = o->thres_in;
            s->thres_out = o->thres_out;
            s->max_position = o->max_position;
            s->min_cancel_time = o->min_cancel_time;
            s->max_cancel_time = o->max_cancel_time;
            s->r_force_fact = o->r_force_fact;
            s->odd_lot_type = o->odd_lot_type;
            s->trd_lot_size = o->trd_lot_size;
            s->cc_factor = o->cc_factor;
            s->tick_size = o->tick_size;
            s->is_short_hedge = o->is_short_hedge;
        }
    }

    *arr = symbol_strategy_datas;
    *cnt = n_stock_characteristics;
    return 0;
}

int strategy_on_book_update(
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

    if(symbol_data->in_universe < 1)
        return ERROR_NOT_IN_UNIVERSE;
    if(!s->stock_characteristics_ok)
        return ERROR_BAD_SYMBOL;

    bool valid_book = bbo_is_good(book) && book->bid[0].px <= book->ask[0].px;
    bool valid_nbbo = bbo_is_good(agg);
    bool price_changed = nbbo_prev->bid.px != agg->bid[0].px || nbbo_prev->ask.px != agg->ask[0].px;
    bool size_changed = nbbo_prev->bid.sz != agg->bid[0].sz || nbbo_prev->ask.sz != agg->ask[0].sz;
    bool size_or_price_changed = price_changed || size_changed;
    if(s->last_trade_time > 0 && valid_book && valid_nbbo)
    {
        double nbbo_mid = mid( fix2dbl(agg->bid[0].px), fix2dbl(agg->ask[0].px));
        if(size_or_price_changed)
        {

            //first quote mid
            if(s->first_quote_time == 0)
            {
                s->first_quote_mid = nbbo_mid;
                s->first_quote_time = nsecs;
            }

            // need country
            if(nsecs < country->open_time + 900.*SECONDS)
                s->overnight_return_bps = return_in_bps(s->p_close, nbbo_mid);

            int bs = agg->bid[0].px;
            int as = agg->ask[0].px;
            if(bs > lacc_calculate(&s->lacc_max_bid_size_200s, nsecs, 0.))
                s->lacc_max_bid_size_200s  = (struct LeakyAccumulator) { .K=200*SECONDS,  .sum_last=bs, .t_last=nsecs };
            if(as > lacc_calculate(&s->lacc_max_ask_size_200s, nsecs, 0.))
                s->lacc_max_ask_size_200s  = (struct LeakyAccumulator) { .K=200*SECONDS,  .sum_last=as, .t_last=nsecs };
            if(bs > lacc_calculate(&s->lacc_max_bid_size_1200s, nsecs, 0.))
                s->lacc_max_bid_size_1200s = (struct LeakyAccumulator) { .K=1200*SECONDS, .sum_last=bs, .t_last=nsecs };
            if(as > lacc_calculate(&s->lacc_max_ask_size_1200s, nsecs, 0.))
                s->lacc_max_ask_size_1200s = (struct LeakyAccumulator) { .K=1200*SECONDS, .sum_last=as, .t_last=nsecs };

            //if ( price_changed ) // commenting out: consistent with the prod code. May not matter either way.
            {
                double twma_input_mid = 0.;
                if( s->prev_mid > 0.)
                {
                    twma_input_mid = s->prev_mid;
                }
                else if(s->midprice_moving_average_5s.t_last == 0)
                {
                    // There is no data in the twma's yet, and the previous price is no
                    // good.  Use current price.
                    twma_input_mid = nbbo_mid;
                }
                if(twma_input_mid > .0001)
                {
                    twma_close_previous_value( &s->midprice_moving_average_5s,    nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_15s,   nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_30s,   nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_60s,   nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_120s,  nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_300s,  nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_600s,  nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_1200s, nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_2400s, nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_4800s, nsecs, twma_input_mid );
                    twma_close_previous_value( &s->midprice_moving_average_9600s, nsecs, twma_input_mid );
                }
            }
        }

        s->mid_before_lastTradeBeforeNbbo = s->mid_before_lastTrade;
        s->prev_mid = nbbo_mid;
    }

    return 0;
}

int strategy_on_trade(
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

    if(symbol_data->in_universe < 1)
        return ERROR_NOT_IN_UNIVERSE;
    if(!s->stock_characteristics_ok)
        return ERROR_BAD_SYMBOL;

    double price = fix2dbl(px);

    if(price > .0001f && sz > 0)
    {
        lacc_close_previous_value(&s->lacc_trade_qty_sum_15s,    nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_30s,    nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_60s,    nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_120s,   nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_300s,   nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_600s,   nsecs, sz);
        lacc_close_previous_value(&s->lacc_trade_qty_sum_3600s,  nsecs, sz);

        lacc_close_previous_value(&s->lacc_trade_count_300s,     nsecs, 1.);
        lacc_close_previous_value(&s->lacc_trade_count_900s,     nsecs, 1.);
        twma_close_previous_value(&s->trade_price_moving_average_300s, nsecs, price);
        twma_close_previous_value(&s->trade_price_moving_average_900s, nsecs, price);
    }

    if(price > s->high_trade_price || s->last_trade_time == 0)
        s->high_trade_price = price;

    if(price < s->low_trade_price || s->last_trade_time == 0)
        s->low_trade_price = price;

    if(nsecs < 15.*MINUTES + country->open_time)
    {
        if(price > s->high_trade_price_900s || s->last_trade_time == 0)
            s->high_trade_price_900s = price;

        if(price < s->low_trade_price_900s || s->last_trade_time == 0)
            s->low_trade_price_900s = price;
    }

    s->intraday_ntrades         += 1.;
    s->intraday_share_volume    += sz;
    s->intraday_price_sum       += price;
    s->intraday_notional_volume += price * sz;

    int8_t fill_imb;

    if(px == book->ask[0].px)
    {
        // Buy on the ask
        fill_imb = 1;

        s->intraday_buy_share_volume += sz;
    }
    else if(px == book->bid[0].px)
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

    s->fill_imb = fill_imb;

    s->last_book_time   = nsecs;
    s->last_trade_time  = nsecs;
    s->last_trade_price = price;

    // for mrtrd
    if(bbo_is_good(agg) && book->bid[0].px <= book->ask[0].px)
        s->mid_before_lastTrade = mid(fix2dbl(agg->bid[0].px), fix2dbl(agg->ask[0].px));

    return 0;
}

