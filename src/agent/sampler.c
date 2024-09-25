#include <mrtl/agent/sampler.h>
#include <mrtl/agent/load_strategy_library.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/types.h>
#include <mrtl/common/country.h>
#include <mrtl/common/functions.h>
#include <iniparser.h>
#include <math.h>
#include <dlfcn.h>
#include <mlog.h>
#include <errno.h>


void * stratlib;

strategy_init_func                strategy_init;
strategy_init_symbol_datas_func   strategy_init_symbol_datas;
strategy_on_trade_func            strategy_on_trade;
strategy_on_book_update_func      strategy_on_book_update;
strategy_generate_features_func   strategy_generate_features;
strategy_calculate_target_func    strategy_calculate_target;
strategy_write_sample_header_func strategy_write_sample_header;


int agent_init (
        mbp_sigproc_t * proct,
        const struct mbp_sigproc_args * args,
        dictionary * cfg )
{
    int res;

    const char * strategy_library_file = iniparser_getstring( cfg, "strategy:library", NULL );

    res = load_strategy_library (
            strategy_library_file,
            &stratlib,
            &strategy_init,
            NULL,
            NULL,
            &strategy_init_symbol_datas,
            &strategy_on_trade,
            &strategy_on_book_update,
            &strategy_generate_features,
            &strategy_calculate_target,
            NULL,
            &strategy_write_sample_header );

    if ( res != 0 )
    {
        log_error( "sampler::agent_init() had error from load_strategy_library(): %d", res );
        return res;
    }

    log_trace( "Loaded strategy library: %s", strategy_library_file );

    struct Sampler * sampler = calloc( 1, sizeof(struct Sampler) );

    if ( sampler == NULL )
    {
        log_error( "sampler::agent_init() could not allocate sampler." );
        return -1;
    }

    sampler->args = *args;

    if ( (res = cfgdb_get(sampler->args.cfgdbt, &sampler->cfgdb)) != 0 )
    {
        free( sampler );
        return res;
    }

    if ( country_init( &sampler->country, cfg ) < 0 )
    {
        log_error( "country_init() error." );
        return -1;
    }

    const char * date_string = getenv( "DT" );

    if ( NULL == date_string )
    {
        log_error( "Environment variable DT must be set." );
        return -1;
    }

    sampler->idate = atoi( date_string );

    if ( sampler->idate < 20050000 || 20500000 < sampler->idate )
    {
        log_error( "Unexpected date in environment variable DT: %s", date_string );
        return -1;
    }

    log_notice( "Current trade date: %d", sampler->idate );

    res = country_date_ok( &sampler->country, sampler->idate );
    if ( res < 0 )
    { return res; }
    else if ( !res )
    {
        log_error( "agent_init(): Tick data is not okay for %s on %d",
                sampler->country.name, sampler->idate );
        return -1;
    }

    int32_t previous_trading_idate = country_previous_trading_day( &sampler->country, sampler->idate );

    if ( previous_trading_idate < 0 )
    {
        log_error( "sampler::agent_init() cannot find previous trading day for %d, function returned %d",
                sampler->idate, previous_trading_idate );
        return -1;
    }
    else
    { log_notice( "Previous trading date is %d", previous_trading_idate ); }

    const char * output_directory = iniparser_getstring( cfg, "agent:output_directory", "." );
    strncpy( sampler->output_directory, output_directory, 256 );
    log_info( "Writing output to %s", sampler->output_directory );

    sampler->do_cycle_sampling = iniparser_getint( cfg, "agent:do_cycle_sampling", 0 );
    sampler->do_trade_sampling = iniparser_getint( cfg, "agent:do_trade_sampling", 0 );
    sampler->do_nbbo_sampling  = iniparser_getint( cfg, "agent:do_nbbo_sampling", 0 );

    log_info( "Sampling types:  cycle:%d  trade:%d  nbbo:%d",
            sampler->do_cycle_sampling, sampler->do_trade_sampling, sampler->do_nbbo_sampling );

    uint64_t cycle_sample_period = iniparser_getint( cfg, "agent:cycle_sample_period_seconds", 0 ) * SECONDS;
    if ( sampler->do_cycle_sampling && cycle_sample_period == 0 )
    {
        log_error( "agent_init() did not find 'agent:cycle_sample_period_seconds', when agent:do_cycle_sampling = 1" );
        return -1;
    }

    sampler->transient_cutoff = iniparser_getint( cfg, "agent:transient_cutoff_milliseconds", 5 ) * MILLISECONDS;
    log_info( "Using sample transient cutoff: %lu ms", (sampler->transient_cutoff/MILLISECONDS) );

    if ( (res = strategy_init( &sampler->strategy_config, NULL, cfg )) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init()" );
        return res;
    }

    struct StockCharacteristics * scs;
    if ( ( res = load_stock_characteristics( &scs, MAX_SYMBOLS, sampler->cfgdb.sidb,
            sampler->args.set_id, sampler->idate, &sampler->country ) ) )
    {
        log_error( "agent_init(): error in call to load_stock_characteristics(): returned %d", res );
        sleep(1);
        return res;
    }

    struct TickerExchangePair * tes;
    if ( ( res = load_corporate_action_symbols( &tes, &sampler->country, sampler->idate ) ) < 0 )
    {
        log_error( "agent_init(): error in call to load_corporate_action_symbols(): returned %d", res );
        sleep(1);
        return res;
    }

    if ( ( res = filter_corporate_action_symbols( tes, scs ) ) < 0 )
    {
        log_error( "agent_init(): error in call to filter_corporate_action_symbols(): return %d", res );
        sleep(1);
        return res;
    }
    else
    { log_notice( "Filtered %d symbols with corporate actions.", res ); }

    free( tes );

    sampler->symbol_datas = calloc( MAX_SYMBOLS, sizeof(struct SymbolData) );

    if ( sampler->symbol_datas == NULL )
    {
        log_error( "agent_init(): could not allocate symbol_datas" );
        return -1;
    }

    for ( size_t i = 0; i < MAX_SYMBOLS; i++ )
    { symbol_data_init( &sampler->symbol_datas[i] ); }

    if ( ( res = strategy_init_symbol_datas( &sampler->symbol_strategy_datas, &sampler->max_symbols,
                    sampler->symbol_datas, scs, NULL, NULL, MAX_SYMBOLS, sampler->strategy_config ) ) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init_symbol_datas(): returned %d", res );
        return res;
    }

    free( scs );

    sampler->nbbos = calloc( sampler->max_symbols, sizeof(struct BBO) );

    if ( sampler->nbbos == NULL )
    {
        log_error( "agent_init() could not allocate nbbos" );
        return -1;
    }

    task_manager_init( &sampler->task_manager );

    uint64_t open_time  = sampler->country.open_time;
    uint64_t close_time = sampler->country.close_time;

    uint64_t midprice_sample_period = iniparser_getint( cfg, "agent:midprice_sample_period_seconds", 1 ) * SECONDS;
    sampler->midprice_count = ( close_time - open_time ) / midprice_sample_period;

    sampler->midprice_times = calloc( sampler->midprice_count, sizeof(uint64_t) );

    if ( sampler->midprice_times == NULL )
    {
        log_error( "agent_init() could not allocate midprice_times" );
        return -1;
    }

    sampler->midprices = calloc(
            sampler->midprice_count*sampler->max_symbols, sizeof(double) );

    if ( sampler->midprices == NULL )
    {
        log_error( "agent_init() could not allocate midprices" );
        return -1;
    }

    sampler->first_trade_times = calloc( sampler->max_symbols, sizeof(uint64_t) );

    if ( sampler->first_trade_times == NULL )
    {
        log_error( "agent_init() could not allocate first_trade_times" );
        return -1;
    }


    // Add tasks to save the midprices of all stocks every 1s.
    size_t i = 0;
    for ( uint64_t nsecs = open_time; nsecs < close_time; nsecs += midprice_sample_period, i++ )
    {
        sampler->midprice_times[i] = nsecs;
        task_manager_add_task( &sampler->task_manager, 0, nsecs, save_midprice_universe, NULL );
    }

    // Add the cycle sample tasks.
    if ( sampler->do_cycle_sampling )
    {
        //for ( uint64_t nsecs = open_time + 1*SECONDS; nsecs < close_time; nsecs += cycle_sample_period )
        for ( uint64_t nsecs = open_time + cycle_sample_period; nsecs < close_time; nsecs += cycle_sample_period )
        {
            task_manager_add_task( &sampler->task_manager, 0, nsecs, agent_on_cycle, NULL );
        }
    }

    sampler->max_samples = 10e6;

    sampler->samples = calloc( sampler->max_samples, sizeof(struct Sample) );

    if ( sampler->samples == NULL )
    {
        log_error( "sampler::agent_init(): Failed to allocate samples" );
        return -1;
    }

    sampler->sample_count = 0;


    *proct = sampler;

    return 0;
}


int agent_on_trade (
        mbp_sigproc_t proct,
        uint64_t nsecs,
        uint16_t sec_id,
        uint16_t mkt_id,
        int64_t px,
        uint32_t sz,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct mbpupd * u )
{
    struct Sampler * sampler = proct;
    const struct Country * c = &sampler->country;

    struct SymbolData * symbol_data = &sampler->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    //log_notice( "trade time: %llu ms", nsecs/MILLISECONDS );

    if ( !sampler->first_trade_time ) // && nsecs < c->open_time + 30*MINUTES )
    {
        sampler->first_trade_time = nsecs;

        log_notice( "Open time: %llu ms, first trade time: %llu ms",
                c->open_time/MILLISECONDS, nsecs/MILLISECONDS );

        if ( nsecs < c->open_time - 1*HOURS )
        { log_notice( "First trade might be early" ); }
        else if ( c->open_time + 1*MINUTES < nsecs )
        { log_notice( "First trade might be late" ); }
    }

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    // Check that sec_id is valid.
    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( sampler->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL )  { return 0; }
    if ( !sym_info->enable ) { return 0; }

    if ( !sampler->first_trade_times[sec_id] )
    {
        sampler->first_trade_times[sec_id] = nsecs;
        //log_notice( "  first trade %u at %lu", sec_id, sampler->first_trade_times[sec_id] );
    }

    int err = strategy_on_trade( symbol_data, nsecs, mkt_id,
            px, sz, book, agg, sampler->strategy_config, c );

    if ( err == ERROR_NOT_IN_UNIVERSE )
    { return 0; }
    else if ( err == ERROR_BAD_SYMBOL )
    { return 0; }
    else if ( err < 0 )
    {
        log_error( "agent_on_trade(): call to strategy_on_trade() returned: %d", err );
        return err;
    }

    // Note: Do not check continuation (u->nxt) flag or u->rdy flag here, as they may be
    // set (or may not be set) on some exchanges when the trade arrives.

    if ( sampler->do_trade_sampling               &&
         sampler->first_trade_times[sec_id] > 0   &&
         bbo_struct_is_good( sampler->nbbos[sec_id] ) )
    {
        sampler_generate_sample( sampler, sec_id, nsecs, agg );
    }

    return 0;
}


int agent_on_book_update (
        mbp_sigproc_t proct,
        uint64_t nsecs,
        uint16_t sec_id,
        uint16_t mkt_id,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct mbpupd * u )
{
    struct Sampler * sampler = proct;
    const struct Country * c = &sampler->country;

    struct SymbolData * symbol_data = &sampler->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    // Check that sec_id is valid.
    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( sampler->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL )  { return 0; }
    if ( !sym_info->enable ) { return 0; }

    struct BBO * nbbo = &sampler->nbbos[sec_id];

    // Note: strategy_on_book_update() is getting the old nbbo

    int err = strategy_on_book_update( symbol_data, nsecs, mkt_id,
            book, agg, nbbo, sampler->strategy_config, c );

    if ( err == ERROR_NOT_IN_UNIVERSE )
    { return 0; }
    else if ( err == ERROR_BAD_SYMBOL )
    { return 0; }
    else if ( err < 0 )
    {
        log_error( "agent_on_book_update(): call to strategy_on_book_update() returned: %d", err );
        return err;
    }

    bool nbbo_is_new = false;

    if ( nbbo->bid.px != agg->bid[0].px ||
         nbbo->bid.sz != agg->bid[0].sz ||
         nbbo->ask.px != agg->ask[0].px ||
         nbbo->ask.sz != agg->ask[0].sz )
    {
        nbbo_is_new = true;
    }

    if ( nbbo_is_new )
    {
        memcpy( &nbbo->bid, &agg->bid[0], sizeof(nbbo->bid) );
        memcpy( &nbbo->ask, &agg->ask[0], sizeof(nbbo->ask) );
    }

    if ( !u->nxt                                &&
         u->rdy                                 &&
         sampler->do_nbbo_sampling              &&
         sampler->first_trade_times[sec_id] > 0 &&
         nbbo_is_new                            &&
         bbo_struct_is_good( sampler->nbbos[sec_id] ) )
    {
        sampler_generate_sample( sampler, sec_id, nsecs, agg );
    }

    return 0;
}


static int agent_on_cycle (
        void * sampler_void_ptr,
        uint64_t nsecs,
        void * ignore )
{
    struct Sampler * sampler = sampler_void_ptr;
    const struct Country * c = &sampler->country;

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    struct mbp_level ask[sampler->args.levels], bid[sampler->args.levels];
    struct mbp_book agg = { .ask = ask, .bid = bid };

    for ( size_t sec_id = 0; sec_id < sampler->max_symbols; sec_id++ )
    {
        // Check that sec_id is valid.
        const struct si * sym_info = NULL;
        size_t sym_info_count;

        // TODO: Can this be replaces with a call to sidb_byset(), outside this
        // loop?
        sidb_byid( sampler->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL )  { continue; }
        if ( !sym_info->enable ) { continue; }

        struct SymbolData * symbol_data = &sampler->symbol_datas[sec_id];

        if ( symbol_data->in_universe < 0 )
        { continue; }

        struct BBO * nbbo = &sampler->nbbos[sec_id];

        if ( sampler->first_trade_times[sec_id] > 0 &&
             bbo_struct_is_good( *nbbo ) )
        {
            memset( ask, 0, sizeof(ask) );
            memset( bid, 0, sizeof(bid) );

            int err = sampler->args.getbook( sec_id, UINT16_MAX, &agg );

            if ( !err )
            { sampler_generate_sample( sampler, sec_id, nsecs, &agg ); }
            else
            {
                log_notice( "Unable to get MBP agg book for ticker %s (sec_id:%d) at time %lu, error code: %d",
                        sym_info->ticker, sec_id, nsecs, err );
            }
        }
    }

    return 0;
}


int agent_on_pre ( mbp_sigproc_t sampler_void_ptr, uint64_t nsecs )
{
    struct Sampler * sampler = sampler_void_ptr;

    task_manager_run_tasks_before( &sampler->task_manager, nsecs, sampler_void_ptr );

    return 0;
}


int agent_fini ( mbp_sigproc_t proct )
{
    struct Sampler * sampler = proct;

    char output_filename [512];
    snprintf( output_filename, 512, "%s/samples_%s_set%d_%d.txt",
            sampler->output_directory, sampler->country.name,
            sampler->args.set_id, sampler->idate );
    FILE * out = fopen( output_filename, "w" );

    if ( out == NULL )
    {
        log_error( "agent_fini() cannot open output file: %s", output_filename );
    }
    else
    {
        // Calculate targets

        char sample_string [4096];

        strategy_write_sample_header( sample_string, 4096 );
        fprintf( out, "%s\n", sample_string );

        for ( size_t i = 0; i < sampler->max_samples; i++ )
        {
            struct Sample * s = &sampler->samples[i];

            if ( s->good )
            {
                strategy_calculate_target( s,
                        sampler->symbol_datas,
                        sampler->midprice_times, sampler->midprice_count,
                        sampler->midprices, sampler->max_symbols,
                        &sampler->country, sampler->strategy_config );

                sampler_check_sample_for_problems( sampler, s );
            }

            if ( s->good )
            {
                write_sample_to_string( sample_string, 4096, s );
                fprintf( out, "%s\n", sample_string );
            }
        }

        fclose( out );
    }


    cfgdb_destroy( &sampler->cfgdb );
    task_manager_fini( &sampler->task_manager );
    free( sampler->strategy_config );
    free( sampler->symbol_datas );
    free( sampler->symbol_strategy_datas );
    free( sampler->nbbos );
    free( sampler->midprice_times );
    free( sampler->midprices );
    free( sampler->first_trade_times );
    free( sampler );

    dlclose( stratlib );

    return 0;
}


static int sampler_generate_sample (
        struct Sampler * sampler,
        uint16_t sec_id,
        uint64_t nsecs,
        const struct mbp_book * agg )
{
    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( sampler->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL )  { return ERROR_NO_SYMBOL_INFO; }

    float features [MAX_FEATURE_COUNT] = { 0.f };

    struct SymbolData * symbol_data = &sampler->symbol_datas[sec_id];

    int err = strategy_generate_features( features, symbol_data,
            nsecs, &sampler->country, agg, sampler->strategy_config );

    if      ( err == ERROR_NOT_IN_UNIVERSE )     { return 0; }
    else if ( err == ERROR_BAD_SYMBOL )          { return 0; }
    else if ( err == ERROR_SPREAD_NEGATIVE )     { return 0; }
    else if ( err == ERROR_SPREAD_ZERO )         { return 0; }
    else if ( err == ERROR_SPREAD_TOO_WIDE )     { return 0; }
    else if ( err == ERROR_BAD_SAMPLE )          { return 0; }
    else if ( err == ERROR_FEATURE_NOT_FINITE )  { return 0; }
    else if ( err < 0 )
    {
        log_error( "sampler_generate_sample(): call to strategy_generate_features() returned: %d", err );
        return err;
    }

    struct Sample * smpl = sampler_get_next_available_sample( sampler );

    if ( NULL == smpl )
    {
        log_error( "sampler_generate_sample(): call to sampler_get_next_available_sample() failed." );
        return -1;
    }

    memcpy( smpl->features, features, MAX_FEATURE_COUNT*sizeof(float) );

    smpl->nbbo   = sampler->nbbos[sec_id];
    smpl->nsecs  = nsecs;
    smpl->sec_id = sec_id;
    smpl->good   = 1;
    strncpy( smpl->ticker, sym_info->ticker, TICKERLEN );

    task_manager_add_task( &sampler->task_manager, nsecs,
            nsecs+sampler->transient_cutoff, check_sample_for_transient, smpl );

    return 0;
}


int32_t sampler_set_midprice (
        struct Sampler * sampler,
        uint16_t sec_id,
        uint64_t nsecs,
        double px,
        int32_t start_index )
{
    if ( nsecs < sampler->midprice_times[start_index] )
    {
        log_notice( "Warning: sampler_set_midprice() expected nsecs to be greater than "
                "the time of the start_index.");
    }

    int32_t time_index;

    for ( time_index = start_index; time_index < sampler->midprice_count; time_index++ )
    {
        if ( nsecs == sampler->midprice_times[time_index] )
        { break; }
    }

    if ( time_index == sampler->midprice_count )  { return -1; }

    sampler->midprices[sec_id*sampler->midprice_count + time_index] = px;

    return time_index;
}


static int save_midprice_universe (
        void * sampler_void_ptr,
        uint64_t nsecs,
        void * ignore )
{
    // Save the midprices of all stocks to sampler->midprices.

    struct Sampler * sampler = sampler_void_ptr;

    int32_t last_time_index = 0;

    for ( size_t sec_id = 0; sec_id < sampler->max_symbols; sec_id++ )
    {
        const struct si * sym_info = NULL;
        size_t sym_info_count;

        sidb_byid( sampler->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL )  { continue; }
        if ( !sym_info->enable ) { continue; }

        struct BBO * nbbo = &sampler->nbbos[sec_id];

        if ( bbo_struct_is_good( *nbbo ) )
        {
            double bid = fix2dbl( nbbo->bid.px );
            double ask = fix2dbl( nbbo->ask.px );
            last_time_index = sampler_set_midprice(
                    sampler, sec_id, nsecs, mid(bid,ask), last_time_index );
        }
    }

    return 0;
}


static struct Sample * sampler_get_next_available_sample ( struct Sampler * sampler )
{
    struct Sample * s;

    if ( sampler->sample_count < sampler->max_samples )
    {
        s = &sampler->samples[sampler->sample_count];
        ++(sampler->sample_count);
    }
    else
    {
        s = NULL;
    }

    return s;
}


static int check_sample_for_transient (
        void * sampler_void_ptr,
        uint64_t nsecs,
        void * sample_void_ptr )
{
    struct Sampler * sampler = sampler_void_ptr;
    struct Sample  * smpl    = (struct Sample *) sample_void_ptr;
    struct BBO nbbo          = sampler->nbbos[smpl->sec_id];

    if ( nbbo.bid.px < smpl->nbbo.bid.px )
    {
        // Current nbbo bid has fallen.
        smpl->good = 0;
    }
    else if ( nbbo.ask.px > smpl->nbbo.ask.px )
    {
        // Current nbbo ask has risen.
        smpl->good = 0;
    }

    return 0;
}


static int sampler_check_sample_for_problems (
        struct Sampler * sampler,
        struct Sample * smpl )
{
    const struct Country * c = &sampler->country;

    // Check that the sample is from the continuous session.
    if ( !country_during_continuous_session( c, smpl->nsecs ) )
    {
        log_notice( "sampler_check_sample_for_problems(): sample is outside the continuous session." );
        smpl->good = 0;
    }

    // Check that all targets end during the continuous session.
    for ( size_t t = 0; t < MAX_TARGET_COUNT; ++t )
    {
        uint64_t target_end_time = smpl->target_times[t];

        if ( target_end_time && !country_during_continuous_session( c, target_end_time ) )
        {
            log_notice( "sampler_check_sample_for_problems(): sample target %lu is outside the continuous session.", t );
            smpl->good = 0;
        }
    }

    // Check that targets do not cross lunch
    if ( country_has_lunch( c ) )
    {
        for ( size_t t = 0; t < MAX_TARGET_COUNT; ++t )
        {
            uint64_t target_end_time = smpl->target_times[t];

            if ( target_end_time &&
                 smpl->nsecs < c->lunch_start &&
                 c->lunch_start < target_end_time )
            {
                log_notice( "sampler_check_sample_for_problems(): sample target %lu crosses lunch.", t );
                smpl->good = 0;
            }
        }
    }

    return 0;
}

