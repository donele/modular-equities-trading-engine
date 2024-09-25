#include <mrtl/common/functions.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/types.h>
#include <mrtl/strategy/classic/config.h>
#include <mrtl/strategy/classic/symbol_data.h>
#include <mlog.h>


int market_taking_actions (
        struct Order * orders,
        uint8_t * dst_order_count,
        uint8_t max_order_count,
        const struct Predictions * predictions,
        const struct mbp_sigproc_args * args,
        struct cfgdb_db * cfgdb,
        uint64_t nsecs,
        struct SymbolData * symbol_data,
        const struct mbp_book * agg,
        void * strategy_config )
{
    *dst_order_count = 0;
    struct StrategyConfig * strat_conf = strategy_config;
    struct SymbolTradingData * std = symbol_data->trading_data;

    // TODO: Should the market_volume_fraction be a configurable parameter?
    // This is the fraction of market volume we are permitted to trade.
    // This fraction ( less than 1 ) could be set in the config.
    int32_t max_total_order_quantity = 0.2 * std->market_share_volume - std->share_volume;

    if ( max_total_order_quantity < 0 )
    {
        return 0;
    }

    // TODO: Should std->outstanding_order_share_position be checked here, when
    // calculating the max order sizes?  This will prevent us from trading too
    // much before we receive fills.

    if ( std->share_position > std->max_share_position ||
         std->share_position < -std->max_share_position )
    {
        log_notice( "Share position %d is larger than max allowed %d",
                std->share_position, std->max_share_position );
        return 0;
    }

    int32_t max_buy_order_quantity =
        MIN( std->max_share_position - std->share_position,
             max_total_order_quantity );

    int32_t max_sell_order_quantity =
        MIN( std->max_share_position + std->share_position,
             max_total_order_quantity );

    int64_t mid_price_fixed = mid_book_fixed( agg );

    double threshold_bps = 5.;
    int64_t threshold_fixed = mid_price_fixed * ( threshold_bps / BPS );

    // TODO: Fee model.
    int64_t fees_fixed = mid_price_fixed * ( 1. / BPS );  // 1bps fees

    float prediction = predictions->pred[0];
    prediction += predictions->restoring_force_adjustment;
    // TODO: This predicted_price implicitly uses the current spread, since it
    // is based off of the current mid price.  Try using the medMedSprd or
    // medSprd instead of current spread, since that is what we are more likely
    // to be marking against later.
    int64_t predicted_price = ( 1. + prediction / BPS ) * mid_price_fixed;

    if ( (agg->bid[0].px - threshold_fixed - fees_fixed) < predicted_price &&
         predicted_price < (agg->ask[0].px + threshold_fixed + fees_fixed) )
    {
        // The predicted price is not actionable on the aggregated book, so no
        // reason to check the individual books.
        return 0;
    }

    struct mbp_level ask[args->levels], bid[args->levels];
    struct mbp_book book = { .ask=ask, .bid=bid };
    const struct mkt * m = NULL;
    struct Order * o = NULL;
    int32_t buy_order_quantity = 0, sell_order_quantity = 0;
    uint8_t order_count = 0, buy_count = 0, sell_count = 0;
    int res;

    // TODO: Do trade sizing.  Track the outstanding quantity with each
    // order that is generated.

    for ( uint8_t mi = 0; mi < MKTID_MAX; ++mi )
    {
        m = mktdb_byid( cfgdb->mktdb, mi );

        if ( m == NULL || !m->enable )   { continue; }

        memset( ask, 0, sizeof(ask) );
        memset( bid, 0, sizeof(bid) );

        if ( ( res = args->getbook(symbol_data->sec_id, mi, &book) ) != 0 )
        {
            log_notice( "Unable to get book %u for symbol id %u, res = %d", mi, symbol_data->sec_id, res );
            continue;
        }

        // Note: Only consider the top of each book, at this time.
        if ( book_side_is_good( ask[0] ) &&
            predicted_price > ask[0].px + fees_fixed + threshold_fixed )
        {
            if ( nsecs < std->last_buy_order_nsecs + strat_conf->order_cooldown )
            {
                // Do not generate order, too soon.
                // TODO: Write these out?
            }
            else
            {
                o = &orders[order_count];
                o->status = ORDER_STATUS_NEW;
                o->nsecs = nsecs;
                o->price = ask[0].px;
                o->quantity = MIN( ask[0].sz, max_buy_order_quantity-buy_order_quantity );
                o->mkt_id = mi;
                o->sec_id = symbol_data->sec_id;
                o->side = BUY;

                o->predicted_price = predicted_price;
                o->prediction = prediction;

                buy_order_quantity += o->quantity;
                if ( buy_order_quantity > max_buy_order_quantity )  { break; }

                ++buy_count;
                ++order_count;
                if ( order_count == max_order_count )  { break; }  // TODO: Notify that we ran out of order slots.
            }
        }
        else if ( book_side_is_good( bid[0] ) &&
            predicted_price < bid[0].px - fees_fixed - threshold_fixed )
        {
            if ( nsecs < std->last_sell_order_nsecs + strat_conf->order_cooldown )
            {
                // In cooldown.
            }
            else
            {
                o = &orders[order_count];
                o->status = ORDER_STATUS_NEW;
                o->nsecs = nsecs;
                o->price = bid[0].px;
                o->quantity = MIN( bid[0].sz, max_sell_order_quantity-sell_order_quantity );
                o->mkt_id = mi;
                o->sec_id = symbol_data->sec_id;
                o->side = SELL;

                o->predicted_price = predicted_price;
                o->prediction = prediction;

                sell_order_quantity += o->quantity;
                if ( sell_order_quantity > max_sell_order_quantity )  { break; }

                ++sell_count;
                ++order_count;
                if ( order_count == max_order_count )  { break; }  // TODO: Notify that we ran out of order slots.
            }
        }
    }

    if ( buy_count > 0 )
    {
        std->last_buy_order_nsecs = nsecs;
        std->last_buy_price = agg->ask[0].px;
    }

    if ( sell_count > 0 )
    {
        std->last_sell_order_nsecs = nsecs;
        std->last_sell_price = agg->bid[0].px;
    }

    //if ( order_count > 0 )
    //{
    //    // DEBUG
    //    char dbg_string [256];
    //    write_prediction_to_string( dbg_string, 256, predictions );
    //    log_info( "PREDICTION %d %s", order_count, dbg_string );
    //}

    *dst_order_count = order_count;

    // TODO: Use pthread mutexes (not Linux mutexes) to update global state.

    return 0;
}


int market_making_actions (
        struct Order * orders,
        uint8_t * dst_order_count,
        uint8_t max_order_count,
        const struct Predictions * predictions,
        const struct mbp_sigproc_args * args,
        struct cfgdb_db * cfgdb,
        uint64_t nsecs,
        struct SymbolData * symbol_data,
        const struct mbp_book * agg,
        void * strategy_config )
{
    *dst_order_count = 0;
    struct StrategyConfig * strat_conf = strategy_config;
    struct SymbolTradingData * std = symbol_data->trading_data;
    struct SymbolStrategyData * s = symbol_data->strategy_data;

    if(s->send_non_marketable <= 0)
        return;

    int32_t max_total_order_quantity = 0.2 * std->market_share_volume - std->share_volume;

    if ( std->share_position > std->max_share_position ||
         std->share_position < -std->max_share_position )
    {
        log_notice( "Share position %d is larger than max allowed %d",
                std->share_position, std->max_share_position );
        return 0;
    }

    int32_t max_buy_order_quantity =
        MIN( std->max_share_position - std->share_position,
             max_total_order_quantity );

    int32_t max_sell_order_quantity =
        MIN( std->max_share_position + std->share_position,
             max_total_order_quantity );

    int64_t mid_price_fixed = mid_book_fixed( agg );
    int64_t nbbo_bid = agg->bid[0].px;
    int64_t nbbo_ask = agg->ask[0].px;

    double threshold_bps = 5.;
    int64_t threshold_fixed = mid_price_fixed * ( threshold_bps / BPS );

    // TODO: Fee model.
    int64_t fees_fixed = 0;

    float prediction = predictions->pred[0] + predictions->pred[1];
    prediction += predictions->restoring_force_adjustment;
    int64_t predicted_price = ( 1. + prediction / BPS ) * mid_price_fixed;

    double tick_size = 0.01; // TODO
    double spread = nbbo_ask - nbbo_bid;
    double minSpread = (AL_FLAGS_ALLOW_JOIN & s->send_non_marketable) == 0 ? 1.6 * tick_size : 0.6 * tick_size;
    if(spread > minSpread)
    {
        double forecast = 0.; // TODO
        //double forecast = stockParams.weight1m * preds.oneMin + preds.fiveMin+stockParams.weightLT * preds.tenMin + addAdjust;
        
        double pos_fact = 1. / (1. - s->ord_pos);
        double mid_price = 0.5 * (nbbo_ask + nbbo_bid);
        double fwd_price = mid_price * (1. + (1. - s->ord_forec) * pos_fact * forecast);

        double sel_const = s->ord_const * mid_price + s->ord_const_psh;
        double idealHSpread = pos_fact * sel_const + 0.5 * spread * (s->cost_liquid_param + pos_fact * s->sel_spread);

        int order_qty = 1 + ROUND(0.5 * s->max_qty * (1. + rand() / (double)RAND_MAX));
        if(order_qty < s->trd_lot_size && s->odd_lot_type != OLR_NONE)
            order_qty = s->trd_lot_size;

        // buy order

        // sell order


    }

    //restrict al-positon to +/- AL_POSITION_LIMIT
    //if(std->market_making_share_position + buy->qty > s->trade_limit)
    //{
    //    buy->qty = s->trade_limit - std->market_making_share_position;
    //}
    //if(std->market_making_share_position + sell->qty < -s->trade_limit)
    //{
    //    sell->qty = -s->trade_limit - std->market_making_share_position;
    //}

    //follow up-tick rule
    //if(sell->price < preds.minShortPrice && totPosition + sell->qty < 0)
    //{
    //    if(sell->price > 1.E7 * tick_size)
    //      sell->price = preds.minShortPrice;
    //  else
    //  {
    //        sell->price = (float)(tick_size * ROUND(preds.minShortPrice / tick_size));
    //  if(sell->price < preds.minShortPrice)
    //      sell->price = (float)(sell->price + tick_size);
    //  }
    //}

    return 0;
}


int strategy_generate_orders (
        struct Order * orders,
        uint8_t * dst_order_count,
        uint8_t max_order_count,
        const struct Predictions * predictions,
        const struct mbp_sigproc_args * args,
        struct cfgdb_db * cfgdb,
        uint64_t nsecs,
        struct SymbolData * symbol_data,
        const struct mbp_book * agg,
        void * strategy_config )
{
    int ret = 0;
    uint8_t order_count_making = 0, order_count_taking = 0;

    // TODO: Do some log writing if one of these returns an error.

    ret = market_making_actions(
            orders,
            &order_count_making,
            max_order_count,
            predictions,
            args,
            cfgdb,
            nsecs,
            symbol_data,
            agg,
            strategy_config );

    ret = market_taking_actions(
            &orders[order_count_making],
            &order_count_taking,
            max_order_count-order_count_making,
            predictions,
            args,
            cfgdb,
            nsecs,
            symbol_data,
            agg,
            strategy_config );

    *dst_order_count = order_count_making + order_count_taking;

    return ret;
}

