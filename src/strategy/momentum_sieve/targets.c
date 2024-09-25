#include <mrtl/strategy/momentum_sieve/symbol_data.h>
#include <mrtl/strategy/momentum_sieve/config.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/types.h>
#include <mrtl/common/functions.h>


int strategy_calculate_target (
        struct Sample * smpl,
        struct SymbolData* symbol_datas,
        uint64_t * midprice_times,
        size_t times_count,
        double * midprices,
        size_t symbol_count,
        const struct Country * country,
        void * strategy_config )
{
    struct StrategyConfig * strat_conf = strategy_config;

    if ( !country_during_continuous_session( country, smpl->nsecs ) )
    {
        smpl->good = 0;
        return 0;
    }

    float breakout_factor    = 2.;
    float retreat_factor     = 0.15;
    float limit_target_error = 4000.f;
    float limit_target_clip  = 2000.f;

    size_t target_window_start_index = get_midprice_time_index_just_after(
            midprice_times, times_count, smpl->nsecs );

    // If the target would go into lunch, cut it short.
    uint64_t target_end_nsecs = smpl->nsecs + strat_conf->horizon;

    if ( country_has_lunch( country ) &&
         smpl->nsecs < country->lunch_start &&
         country->lunch_start < target_end_nsecs )
    {
        target_end_nsecs = country->lunch_start;
    }

    size_t target_window_end_index = get_midprice_time_index_at_or_before(
            midprice_times, times_count, target_end_nsecs );

    if ( target_window_start_index == SIZE_MAX ||
           target_window_end_index == SIZE_MAX )
    {
        smpl->good = 0;
        return 0;
    }

    double fees_bps = 3.;

    double nbbo_bid = fix2dbl(smpl->nbbo.bid.px), nbbo_ask = fix2dbl(smpl->nbbo.ask.px);

    double midprice = mid_bbo_double( smpl->nbbo );
    double spread_dollars = nbbo_ask - nbbo_bid;
    double breakout_dollars = (breakout_factor * spread_dollars) + (midprice * fees_bps/BPS);
    double ask_hurdle = nbbo_ask + breakout_dollars;
    double bid_hurdle = nbbo_bid - breakout_dollars;
    double ret = 0., extreme_ret = 0.;
    int breakout = 0;
    size_t i;

    for ( i = target_window_start_index; i <= target_window_end_index; i++ )
    {
        double p = get_midprice_at_time_index( midprices, times_count, smpl->sec_id, i );

        ret = return_in_bps( midprice, p );

        if ( !breakout )
        {
            extreme_ret = ret;

            if      ( p > ask_hurdle )  { breakout =  1; }
            else if ( p < bid_hurdle )  { breakout = -1; }
        }
        else if ( breakout > 0 )
        {
            // Upward breakout
            if ( ret > extreme_ret )  { extreme_ret = ret; }

            if ( ret < (1.-retreat_factor) * extreme_ret )
            {
                // Price has retreated from high.
                extreme_ret = ret;
                break;
            }
        }
        else if ( breakout < 0 )
        {
            // Downward breakout
            if ( ret < extreme_ret )  { extreme_ret = ret; }

            if ( ret > (1.-retreat_factor) * extreme_ret )
            {
                // Price has retreated from low.
                extreme_ret = ret;
                break;
            }
        }
    }

    if ( i > target_window_end_index )  { extreme_ret = ret; }

    smpl->targets[0] = extreme_ret;
    smpl->targets[0] = clip( smpl->targets[0], limit_target_clip, limit_target_error );

    double price_at_horizon = get_midprice_at_time_index(
            midprices, times_count, smpl->sec_id, target_window_end_index );

    smpl->targets[1] = return_in_bps( midprice, price_at_horizon ) - extreme_ret;
    smpl->targets[1] = clip( smpl->targets[1], limit_target_clip, limit_target_error );

    return 0;
}

