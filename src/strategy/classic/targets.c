#include <mrtl/strategy/classic/symbol_data.h>
#include <mrtl/strategy/classic/config.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/types.h>
#include <mrtl/common/functions.h>

float calculate_target(
        uint64_t horizonFrom,
        uint64_t horizonTo,
        struct Sample * smpl,
        double midprice,
        uint64_t nsecs,
        uint16_t sec_id,
        uint64_t * midprice_times,
        size_t times_count,
        double * midprices,
        const struct Country * country )
{
    float ret = 0.f;
    if(horizonFrom >= horizonTo)
        return ret;
    float limit_target_error = 20000.f;
    float limit_target_clip = 25. * pow((horizonTo - horizonFrom) / SECONDS, .5);

    // Price at the horizon start
    if(midprice <= 0. || horizonFrom > 0)
    {
        // If the target starts during lunch, set it to end of lunch.
        uint64_t target_start_nsecs = MIN(nsecs + horizonFrom, country->close_time - 1);
        if(country_has_lunch(country) && target_start_nsecs > country->lunch_start && target_start_nsecs < country->lunch_end)
            target_start_nsecs = country->lunch_end;

        //size_t mid_price_index = get_midprice_time_index_at_or_before( midprice_times, times_count, target_start_nsecs);
        size_t mid_price_index = (target_start_nsecs - country->open_time) / SECONDS;
        midprice = get_midprice_at_time_index( midprices, times_count, sec_id, mid_price_index );
    }

    // If the target would go into lunch, cut it short.
    uint64_t target_end_nsecs = MIN(nsecs + horizonTo, country->close_time - 1);
    if(country_has_lunch( country ) && target_end_nsecs > country->lunch_start && target_end_nsecs < country->lunch_end)
        target_end_nsecs = country->lunch_start;

    //size_t target_window_end_index = get_midprice_time_index_at_or_before( midprice_times, times_count, target_end_nsecs );
    size_t target_window_end_index = (target_end_nsecs - country->open_time) / SECONDS;
    double endprice = get_midprice_at_time_index( midprices, times_count, sec_id, target_window_end_index);

    if(midprice > 0. && endprice > 0.)
        ret = return_in_bps(midprice, endprice);
    else if(smpl != NULL)
        smpl->good = 0;

    ret = clip(ret, limit_target_clip, limit_target_error);

    return ret;
}

float calculate_market_return(
        uint64_t horizonFrom,
        uint64_t horizonTo,
        uint64_t nsecs,
        uint64_t * midprice_times,
        size_t times_count,
        double * midprices,
        int* univ_symbol_index,
        size_t univ_symbol_count,
        const struct Country * country )
{
    bool lookup_index = false;

    float limit_target_error = 20000.f;
    float limit_target_clip = 25. * pow((horizonTo - horizonFrom) / SECONDS, .5);

    // Price at the horizon start
    // If the target starts during lunch, set it to end of lunch.
    uint64_t target_start_nsecs = MIN(nsecs + horizonFrom, country->close_time - 1);
    if(country_has_lunch(country) && target_start_nsecs > country->lunch_start && target_start_nsecs < country->lunch_end)
        target_start_nsecs = country->lunch_end;

    size_t mid_price_index = 0;
    if(lookup_index)
        mid_price_index = get_midprice_time_index_at_or_before( midprice_times, times_count, target_start_nsecs);
    else
        mid_price_index = (target_start_nsecs - country->open_time) / SECONDS;

    // If the target would go into lunch, cut it short.
    uint64_t target_end_nsecs = MIN(nsecs + horizonTo, country->close_time - 1);
    if(country_has_lunch( country ) && target_end_nsecs > country->lunch_start && target_end_nsecs < country->lunch_end)
        target_end_nsecs = country->lunch_start;

    size_t target_window_end_index = 0;
    if(lookup_index)
        target_window_end_index = get_midprice_time_index_at_or_before( midprice_times, times_count, target_end_nsecs );
    else
        target_window_end_index = (target_end_nsecs - country->open_time) / SECONDS;

    double market_return_sum = 0.;
    for(int i = 0; i < univ_symbol_count; ++i)
    {
        int sec_id = univ_symbol_index[i];
        double midprice = get_midprice_at_time_index( midprices, times_count, sec_id, mid_price_index );
        double endprice = get_midprice_at_time_index( midprices, times_count, sec_id, target_window_end_index);
        if(midprice > 0. && endprice > 0.)
        {
            double ret = return_in_bps(midprice, endprice);
            ret = clip(ret, limit_target_clip, limit_target_error);
            market_return_sum += ret;
        }
    }
    double market_return = market_return_sum / univ_symbol_count;
    return market_return;
}

int strategy_calculate_target(
        struct Sample * smpl,
        struct SymbolData* symbol_datas,
        uint64_t * midprice_times,
        size_t times_count,
        double * midprices,
        size_t symbol_count,
        const struct Country * country,
        void * strategy_config )
{
    struct StrategyConfig * strat_conf   = strategy_config;

    if(!country_during_continuous_session(country, smpl->nsecs))
    {
        smpl->good = 0;
        return 0;
    }

    // Calculate targets
    double midprice = mid_bbo_double(smpl->nbbo);
    double rawTarget1 = calculate_target(strat_conf->horizon1_from, strat_conf->horizon1_to, smpl, midprice, smpl->nsecs, smpl->sec_id, midprice_times, times_count, midprices, country);
    double rawTarget2 = calculate_target(strat_conf->horizon2_from, strat_conf->horizon2_to, smpl, midprice, smpl->nsecs, smpl->sec_id, midprice_times, times_count, midprices, country);
    double rawTarget3 = calculate_target(strat_conf->horizon3_from, strat_conf->horizon3_to, smpl, midprice, smpl->nsecs, smpl->sec_id, midprice_times, times_count, midprices, country);
    double rawTarget4 = calculate_target(strat_conf->horizon4_from, strat_conf->horizon4_to, smpl, midprice, smpl->nsecs, smpl->sec_id, midprice_times, times_count, midprices, country);

    // calcualte market return
    bool reuse_index = true;
    int* univ_symbol_index = calloc(symbol_count, sizeof(int));
    int univ_symbol_count = 0;
    for(int sec_id = 0; sec_id < symbol_count; ++sec_id)
    {
        if(symbol_datas[sec_id].in_universe == 1)
            univ_symbol_index[univ_symbol_count++] = sec_id;
    }

    double marketReturnSum1 = 0.;
    double marketReturnSum2 = 0.;
    double marketReturnSum3 = 0.;
    double marketReturnSum4 = 0.;
    if(strat_conf->horizon1_to > 300*SECONDS)
        marketReturnSum1 = calculate_market_return(strat_conf->horizon1_from, strat_conf->horizon1_to, smpl->nsecs, midprice_times, times_count, midprices, univ_symbol_index, univ_symbol_count, country); 
    if(strat_conf->horizon2_to > 300*SECONDS)
        marketReturnSum2 = calculate_market_return(strat_conf->horizon2_from, strat_conf->horizon2_to, smpl->nsecs, midprice_times, times_count, midprices, univ_symbol_index, univ_symbol_count, country); 
    if(strat_conf->horizon3_to > 300*SECONDS)
        marketReturnSum3 = calculate_market_return(strat_conf->horizon3_from, strat_conf->horizon3_to, smpl->nsecs, midprice_times, times_count, midprices, univ_symbol_index, univ_symbol_count, country); 
    if(strat_conf->horizon4_to > 300*SECONDS)
        marketReturnSum4 = calculate_market_return(strat_conf->horizon4_from, strat_conf->horizon4_to, smpl->nsecs, midprice_times, times_count, midprices, univ_symbol_index, univ_symbol_count, country); 
    free(univ_symbol_index);

    double hedgedTarget1 = rawTarget1 - marketReturnSum1 / symbol_count;
    double hedgedTarget2 = rawTarget2 - marketReturnSum2 / symbol_count;
    double hedgedTarget3 = rawTarget3 - marketReturnSum3 / symbol_count;
    double hedgedTarget4 = rawTarget4 - marketReturnSum4 / symbol_count;
    smpl->targets[0] = hedgedTarget1;
    smpl->targets[1] = hedgedTarget2;
    smpl->targets[2] = hedgedTarget3;
    smpl->targets[3] = hedgedTarget4;
    return 0;
}

