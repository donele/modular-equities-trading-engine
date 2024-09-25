#include <mrtl/common/constants.h>
#include <mrtl/common/types.h>
#include <mrtl/common/functions.h>
#include <mlog.h>
#include <math.h>
#include <stdio.h>


void write_bbo_to_string (
        char * dst,
        size_t dst_len,
        const struct BBO * b )
{
    snprintf( dst, dst_len, "%u  %f -- %f  %u",
            b->bid.sz, fix2dbl(b->bid.px), fix2dbl(b->ask.px), b->ask.sz );
}


void write_prediction_to_string (
        char * dst,
        size_t dst_len,
        const struct Predictions * p )
{
    if ( dst_len < 128 )
    { log_notice( "Warning: write_prediction_to_string() dst_len is less than 128" ); }

    char pred_str [128];
    char s [16];

    snprintf( pred_str, 128, "%d", p->type );

    for ( int i = 0; i < MAX_TARGET_COUNT; i++ )
    {
        snprintf( s, 16, ",%f", p->pred[i] );
        strcat( pred_str, s );
    }

    snprintf( s, 16, ",%f", p->restoring_force_adjustment );
    strcat( pred_str, s );

    snprintf( s, 16, ",%f", p->factor_risk_adjustment );
    strcat( pred_str, s );

    strncpy( dst, pred_str, dst_len );
}


void write_sample_to_string ( char * dst, size_t dst_len, const struct Sample * s )
{
    const size_t feature_string_length = 16;
    const size_t minimum_dst_len = ((MAX_FEATURE_COUNT+MAX_TARGET_COUNT) * feature_string_length) + 128;

    if ( dst_len < minimum_dst_len )
    {
        log_notice( "Warning: In call to write_features_to_string(), dst_len should be > %lu, set to %lu.\n",
                minimum_dst_len, dst_len);
    }

    snprintf( dst, dst_len, "%s,%lu,%u,%f,%f,%u",
            s->ticker,
            s->nsecs,
            s->nbbo.bid.sz,
            fix2dbl(s->nbbo.bid.px),
            fix2dbl(s->nbbo.ask.px),
            s->nbbo.ask.sz );

    for ( int i = 0; i < MAX_TARGET_COUNT; i++ )
    {
        char yc [feature_string_length];
        snprintf( yc, feature_string_length, ",%f", s->targets[i] );
        strcat( dst, yc );
    }

    for ( int i = 0; i < MAX_FEATURE_COUNT; i++ )
    {
        char xc [feature_string_length];
        snprintf( xc, feature_string_length, ",%f", s->features[i] );
        strcat( dst, xc );
    }
}


void write_order_to_string ( char * dst, size_t dst_len, const struct Order * o )
{
    snprintf( dst, dst_len, "%lu %lu %s %d %d %.4f %u %u %u %.4f",
        o->id,
        o->nsecs,
        o->ticker,
        o->order_schedule_type,
        o->side,
        fix2dbl(o->price),
        o->quantity,
        o->mkt_id,
        o->sec_id,
        fix2dbl(o->predicted_price) );
}

int symbol_trading_data_init ( struct SymbolTradingData * std )
{
    pthread_rwlock_init( &std->lock, NULL );

    return 0;
}


int symbol_trading_data_fini ( struct SymbolTradingData * std )
{
    pthread_rwlock_destroy( &std->lock );

    return 0;
}


int symbol_trading_data_on_fill(
        struct SymbolTradingData * std,
        struct Order * o,
        const struct Fill * f )
{
    int err = 0;

    if ( f->quantity > o->quantity )
    {
        log_error( "symbol_trading_data_on_fill(): Order %lu has quantity %lu, but received fill quantity %lu",
                o->id, o->quantity, f->quantity );
        err = ERROR_BAD_FILL;
        return err;
    }

    o->quantity -= f->quantity;

    if ( o->quantity == 0 )
    { o->status = ORDER_STATUS_FILL; }
    else
    { o->status = ORDER_STATUS_PARTIAL_FILL; }

    pthread_rwlock_wrlock( &std->lock );

    if ( o->side == BUY )
    {
        std->share_volume += f->quantity;
        std->share_position += f->quantity;
        std->outstanding_order_share_position -= f->quantity;
        std->last_buy_fill_nsecs = f->nsecs;
    }
    else if ( o->side == SELL )
    {
        std->share_volume += f->quantity;
        std->share_position -= f->quantity;
        std->outstanding_order_share_position += f->quantity;
        std->last_sell_fill_nsecs = f->nsecs;
    }
    else
    {
        log_error( "symbol_trading_data_on_fill(): Unknown side for order %lu on %s at time %lu",
                o->id, o->ticker, o->nsecs);
        err = ERROR_BAD_SIDE;
    }

    pthread_rwlock_unlock( &std->lock );

    return err;
}

int symbol_trading_data_on_cancel(
        struct SymbolTradingData * std,
        struct Order * o,
        uint32_t remaining_quantity )
{
    int err = 0;

    o->status = ORDER_STATUS_CANCEL;

    pthread_rwlock_wrlock( &std->lock );

    if ( o->side == BUY )
    {
        std->outstanding_order_share_position -= remaining_quantity;
    }
    else if ( o->side == SELL )
    {
        std->outstanding_order_share_position += remaining_quantity;
    }
    else
    {
        log_error( "symbol_trading_data_on_cancel(): Unknown side for order %lu on %s at time %lu",
                o->id, o->ticker, o->nsecs );
        err = ERROR_BAD_SIDE;
    }

    pthread_rwlock_unlock( &std->lock );

    return err;
}


int symbol_trading_data_on_market_trade (
        struct SymbolTradingData * std,
        uint64_t nsecs,
        uint64_t sz )
{
    int err = 0;

    pthread_rwlock_wrlock( &std->lock );

    std->last_market_trade_nsecs = nsecs;
    std->market_share_volume += sz;

    pthread_rwlock_unlock( &std->lock );

    return err;
}


int global_trading_data_init ( struct GlobalTradingData * gtd )
{
    pthread_rwlock_init( &gtd->lock, NULL );

    memset( gtd, 0, sizeof(struct GlobalTradingData) );

    return 0;
}


int global_trading_data_fini ( struct GlobalTradingData * gtd )
{
    pthread_rwlock_destroy( &gtd->lock );

    return 0;
}


int global_trading_data_update (
    struct GlobalTradingData * gtd,
    struct SymbolTradingData * std,
    const struct BBO * nbbos,
    size_t symbol_count,
    uint64_t nsecs )
{
    // This updates the net notional position and gross notional position, and
    // includes recent nbbo prices.  It should be called periodically and will
    // look at all positions and prices across all symbols.

    pthread_rwlock_wrlock( &gtd->lock );

    gtd->net_notional_position = 0.;
    gtd->gross_notional_position = 0.;

    for ( size_t i = 0; i < symbol_count; ++i )
    {
        pthread_rwlock_rdlock( &std[i].lock );
        double symbol_notional_position = std[i].share_position * mid_bbo_double( nbbos[i] );
        pthread_rwlock_unlock( &std[i].lock );

        gtd->net_notional_position += symbol_notional_position;

        gtd->gross_notional_position += fabs( symbol_notional_position );
    }

    gtd->last_update_nsecs = nsecs;

    pthread_rwlock_unlock( &gtd->lock );

    return 0;    
}


int global_trading_data_get (
    struct GlobalTradingData * gtd,
    uint64_t * last_update_nsecs,
    double * net_notional_position,
    double * notional_volume,
    float * cash )
{
    pthread_rwlock_rdlock( &gtd->lock );

    *last_update_nsecs       = gtd->last_update_nsecs;
    *net_notional_position   = gtd->net_notional_position;
    *notional_volume         = gtd->notional_volume;
    *cash                    = gtd->cash;

    pthread_rwlock_unlock( &gtd->lock );

    return 0;
}


int global_trading_data_on_fill (
    struct GlobalTradingData * gtd,
    struct Order * o,
    const struct Fill * f )
{
    // This updates for a single fill.

    int err = 0;

    double notional_in_currency = fix2dbl(f->price) * f->quantity;

    pthread_rwlock_wrlock( &gtd->lock );

    if ( o->side == BUY )
    {
        gtd->cash -= notional_in_currency + f->fees_in_currency;
        gtd->net_notional_position += notional_in_currency;
        gtd->notional_volume += notional_in_currency;
    }
    else if ( o->side == SELL )
    {
        gtd->cash += notional_in_currency - f->fees_in_currency;
        gtd->net_notional_position -= notional_in_currency;
        gtd->notional_volume += notional_in_currency;
    }
    else
    {
        log_error( "global_trading_data_on_fill(): Unknown side for order %lu on %s at time %lu",
                o->id, o->ticker, o->nsecs );
        err = ERROR_BAD_SIDE;
    }

    gtd->gross_notional_position += notional_in_currency;

    pthread_rwlock_unlock( &gtd->lock );

    return err;
}


void symbol_data_init ( struct SymbolData * s )
{
    s->ticker[0]     = '\0';
    s->sec_id        = UINT16_MAX;
    s->in_universe   = -1;
    s->trading_data  = NULL;
    s->strategy_data = NULL;
}

