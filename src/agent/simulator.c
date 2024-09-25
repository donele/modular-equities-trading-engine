#include <mrtl/agent/simulator.h>
#include <mrtl/agent/load_strategy_library.h>
#include <mrtl/common/constants.h>
#include <mrtl/common/functions.h>
#include <mlog.h>
#include <math.h>
#include <dlfcn.h>


void * stratlib;

strategy_init_func               strategy_init;
strategy_predict_func            strategy_predict;
strategy_adjust_predictions_func strategy_adjust_predictions;
strategy_init_symbol_datas_func  strategy_init_symbol_datas;
strategy_on_trade_func           strategy_on_trade;
strategy_on_book_update_func     strategy_on_book_update;
strategy_generate_features_func  strategy_generate_features;
strategy_generate_orders_func    strategy_generate_orders;


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
            &strategy_predict,
            &strategy_adjust_predictions,
            &strategy_init_symbol_datas,
            &strategy_on_trade,
            &strategy_on_book_update,
            &strategy_generate_features,
            NULL,
            &strategy_generate_orders,
            NULL );

    if ( res != 0 )
    {
        log_error( "simulator::agent_init() had error from load_strategy_library(): %d", res );
        return res;
    }

    log_trace( "Loaded strategy library: %s", strategy_library_file );

    struct Simulator * simulator = (struct Simulator *) calloc( 1, sizeof(struct Simulator) );
    simulator->args = *args;

    if ( (res = cfgdb_get(simulator->args.cfgdbt, &simulator->cfgdb)) != 0 )
    {
        free( simulator );
        return res;
    }

    if ( country_init( &simulator->country, cfg ) < 0 )
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

    simulator->idate = atoi( date_string );

    if ( simulator->idate < 20050000 || 20500000 < simulator->idate )
    {
        log_error( "Unexpected date in environment variable DT: %s", date_string );
        return -1;
    }

    res = country_date_ok( &simulator->country, simulator->idate );
    if ( res < 0 )
    { return res; }
    else if ( !res )
    {
        log_error( "agent_init(): Tick data is not okay for %s on %d",
                simulator->country.name, simulator->idate );
        return -1;
    }

    global_trading_data_init( &simulator->global_trading_data );

    const char * output_directory = iniparser_getstring( cfg, "agent:output_directory", "." );
    strncpy( simulator->output_directory, output_directory, 255 );
    log_info( "Writing output to %s", simulator->output_directory );

    char orders_file_name [512];
    snprintf( orders_file_name, 512, "%s/%s_orders_set%d_%d.txt",
        simulator->output_directory, simulator->country.name, simulator->args.set_id,
        simulator->idate);
    simulator->orders_file = fopen( orders_file_name, "w" );

    if ( simulator->orders_file != NULL )
    { log_info( "Writing orders to %s", orders_file_name ); }
    else
    {
        log_error( "simulator::agent_init() unable to open orders file %s", orders_file_name );
        return ERROR_FILE_ERROR;
    }

    char intraday_file_name [512];
    snprintf( intraday_file_name, 512, "%s/%s_intraday_set%d_%d.txt",
        simulator->output_directory, simulator->country.name, simulator->args.set_id,
        simulator->idate );
    simulator->intraday_file = fopen( intraday_file_name, "w" );

    if ( simulator->intraday_file != NULL )
    {
        log_info( "Writing intraday file to %s", intraday_file_name );
        fprintf( simulator->intraday_file, "seconds nsecs last_update_nsecs notional_volume net_notional_position cash intraday_pnl pnl\n" );
        fflush( simulator->intraday_file );
    }
    else
    {
        log_error( "simulator::agent_init() unable to open intraday file %s", intraday_file_name );
        return ERROR_FILE_ERROR;
    }

    simulator->do_cycle_trading = iniparser_getint( cfg, "agent:do_cycle_trading", 0 );
    simulator->do_trade_trading = iniparser_getint( cfg, "agent:do_trade_trading", 0 );
    simulator->do_nbbo_trading  = iniparser_getint( cfg, "agent:do_nbbo_trading", 0 );

    log_info( "Simulating order schedule types:  cycle:%d  trade:%d  nbbo:%d",
            simulator->do_cycle_trading, simulator->do_trade_trading, simulator->do_nbbo_trading );

    simulator->exchange_latency = iniparser_getint( cfg, "agent:exchange_latency_milliseconds", 2 ) * MILLISECONDS;
    log_info( "Using exchange latency: %lu ms", (simulator->exchange_latency/MILLISECONDS) );

    simulator->open_wait = iniparser_getint( cfg, "agent:open_wait_seconds", 30 ) * SECONDS;
    log_info( "Using open wait: %lu sec", (simulator->open_wait/SECONDS) );

    simulator->max_notional_position = iniparser_getint( cfg, "agent:max_notional_position", 10000 );
    log_info( "Using maximum notional position: %f", simulator->max_notional_position );

    if ( (res = strategy_init( &simulator->strategy_config, simulator->models, cfg )) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init()" );
        return res;
    }

    // TODO: Load stockcharacteristics data, pass it to
    // strategy_init_symbol_datas(), and use the previous close
    // prices to calculate max share positions.

    struct StockCharacteristics * scs;
    res = load_stock_characteristics( &scs, MAX_SYMBOLS, simulator->cfgdb.sidb,
            simulator->args.set_id, simulator->idate, &simulator->country );

    // TODO: load_corporate_action_symbols() and
    // filter_corporate_action_symbols()

    simulator->symbol_datas = calloc( MAX_SYMBOLS, sizeof(struct SymbolData) );

    for ( size_t i = 0; i < MAX_SYMBOLS; i++ )
    { symbol_data_init( &simulator->symbol_datas[i] ); }

    struct OrderParams* op;
    res = load_order_params( &op, MAX_SYMBOLS, simulator->cfgdb.sidb,
            simulator->args.set_id, simulator->idate, &simulator->country );

    struct MarketMakingParams* mmp;
    res = load_market_making_params( &mmp, MAX_SYMBOLS, simulator->cfgdb.sidb,
            simulator->args.set_id, simulator->idate, &simulator->country );

    if ( (res = strategy_init_symbol_datas( &simulator->symbol_strategy_datas, &simulator->max_symbols,
                    simulator->symbol_datas, scs, op, mmp, MAX_SYMBOLS, simulator->strategy_config ) ) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init_symbol_datas()" );
        return res;
    }

    log_notice( "Loaded symbol data for %lu symbols.", simulator->max_symbols );

    simulator->max_orders = 1e6;
    simulator->orders = calloc( simulator->max_orders, sizeof(struct Order) );
    simulator->fills = calloc( simulator->max_orders, sizeof(struct Fill) );
    simulator->nbbos = calloc( simulator->max_symbols, sizeof(struct BBO) );
    simulator->symbol_trading_datas = calloc( simulator->max_symbols, sizeof(struct SymbolTradingData) );

    if ( simulator->symbol_trading_datas == NULL )
    {
        log_error( "simulator::agent_init() unable to alloc symbol_trading_datas" );
        return -1;
    }

    for ( size_t i = 0; i < simulator->max_symbols; ++i )
    { symbol_trading_data_init( &simulator->symbol_trading_datas[i] ); }

    link_symbol_trading_datas( simulator->symbol_datas, simulator->symbol_trading_datas, simulator->max_symbols );

    int32_t previous_trading_idate = country_previous_trading_day(
            &simulator->country, simulator->idate );

    if ( previous_trading_idate < 0 )
    {
        log_error( "simulator::agent_init() cannot find previous trading day for %d, function call returned %d",
                simulator->idate, previous_trading_idate );
        return -1;
    }
    else
    { log_notice( "Previous trading date is %d", previous_trading_idate ); }

    double total_cash_position;
    size_t position_file_count;

    if ( (res = read_positions_from_files( simulator->symbol_datas, &total_cash_position, &position_file_count,
                    &simulator->country, previous_trading_idate, simulator->args.set_id,
                    simulator->cfgdb.sidb, simulator->output_directory )) < 0 )
    {
        log_error( "simulator::agent_init() unable to read positions from files" );
        return -1;
    }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    for ( size_t sec_id = 0; sec_id < simulator->max_symbols; ++sec_id )
    {
        sidb_byid( simulator->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL || !sym_info->enable )
        { continue; }

        struct SymbolTradingData * std = simulator->symbol_datas[sec_id].trading_data;

        std->max_share_position =
            floor( simulator->max_notional_position / scs[sec_id].prev_close ); // Adjust by 'adjust' factor?

        simulator->global_trading_data.max_gross_notional_position += simulator->max_notional_position;
    }

    simulator->global_trading_data.cash = total_cash_position / position_file_count;

    free( scs );

    task_manager_init( &simulator->task_manager );

    //
    // Set up state writing throughout the day.  Extra writes near the open and close.
    //

    // Near the open.
    for ( uint64_t nsecs = simulator->country.open_time + 1*SECONDS;
            nsecs < simulator->country.open_time + 1*MINUTES;
            nsecs += 1*SECONDS )
    {
        task_manager_add_task( &simulator->task_manager, 0, nsecs,
            simulator_write_trading_state, simulator->intraday_file );
    }

    // Main portion of the day.    
    for ( uint64_t nsecs = simulator->country.open_time + 1*MINUTES;
            nsecs < simulator->country.close_time - 1*MINUTES;
            nsecs += 1*MINUTES )
    {
        task_manager_add_task( &simulator->task_manager, 0, nsecs,
            simulator_write_trading_state, simulator->intraday_file );
    }

    // Near the close.
    for ( uint64_t nsecs = simulator->country.close_time - 1*MINUTES;
            nsecs < simulator->country.close_time;
            nsecs += 1*SECONDS )
    {
        task_manager_add_task( &simulator->task_manager, 0, nsecs,
            simulator_write_trading_state, simulator->intraday_file );
    }

    // Add the cycle trading tasks.
    for ( uint64_t nsecs = simulator->country.open_time;
            nsecs < simulator->country.close_time;
            nsecs += 1*SECONDS )
    {
        task_manager_add_task( &simulator->task_manager, 0, nsecs,
            agent_on_cycle, NULL );
    }

    // TODO: Add updating the global state, global_trading_data_update()

    *proct = simulator;

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
    struct Simulator * simulator = proct;
    const struct Country * c = &simulator->country;

    struct SymbolData * symbol_data = &simulator->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    if ( !simulator->first_trade_time )
    {
        simulator->first_trade_time = nsecs;

        log_notice( "First trade time: %llu ms", nsecs/MILLISECONDS );

        if ( c->open_time < nsecs - 1*MINUTES )
        { log_notice( "First trade might be late" ); }
    }

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( simulator->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL || !sym_info->enable )
    { return 0; }

    struct SymbolTradingData * std  = symbol_data->trading_data;

    symbol_trading_data_on_market_trade( std, nsecs, sz );

    int err = strategy_on_trade( symbol_data, nsecs, mkt_id,
            px, sz, book, agg, simulator->strategy_config, c );

    if ( err == ERROR_BAD_SYMBOL )
    { return 0; }
    else if ( err < 0 )
    {
        log_error( "agent_on_trade(): call to strategy_update_symbol_data_trade() returned: %d", err );
        return err;
    }

    pthread_rwlock_rdlock( &std->lock );

    bool market_trade_since_last_order =
        std->last_buy_order_nsecs < std->last_market_trade_nsecs &&
        std->last_sell_order_nsecs < std->last_market_trade_nsecs;

    bool book_update_since_last_order =
        std->last_buy_price != agg->ask[0].px &&
        std->last_sell_price != agg->bid[0].px;

    pthread_rwlock_unlock( &std->lock );

    // Note: Do not check continuation (u->nxt) flag or u->rdy flag here, as they may be
    // set (or may not be set) on some exchanges when the trade arrives.

    // This could be split into a series of if-elseif's so we can log the
    // impact of these conditions.
    if ( simulator->do_trade_trading                   &&
         bbo_is_good( agg )                            &&
         market_trade_since_last_order                 &&
         book_update_since_last_order                  &&
         c->open_time + simulator->open_wait < nsecs   &&
         ( !country_has_lunch( c )                     ||
           nsecs < c->lunch_start                      ||
           c->lunch_end + simulator->open_wait < nsecs )
       )
    {
        float features [MAX_FEATURE_COUNT] = { 0.f };

        int err = strategy_generate_features( features, symbol_data,
                nsecs, &simulator->country, agg, simulator->strategy_config );

        if ( err == ERROR_BAD_SYMBOL )
        { return 0; }
        else if ( err == ERROR_SPREAD_TOO_WIDE )
        { return 0; }
        else if ( err == ERROR_FEATURE_NOT_FINITE )
        { return 0; }
        else if ( err < 0 )
        {
            log_notice( "agent_on_trade(): call to strategy_generate_features() returned: %d", err );
            return err;
        }

        struct Predictions predictions;
        strategy_predict( &predictions, simulator->models,
                features, ORDER_SCHEDULE_TYPE_TRADE_EVENT, simulator->strategy_config );

        err = strategy_adjust_predictions( &predictions, symbol_data,
                &simulator->global_trading_data, simulator->strategy_config );

        if ( err < 0 )
        {
            log_notice( "agent_on_trade(): call to strategy_adjust_predictions() returned: %d", err );
            return err;
        }

        struct Order orders [16];
        uint8_t order_count = 0;

        strategy_generate_orders(
            orders,
            &order_count,
            16,
            &predictions,
            &simulator->args,
            &simulator->cfgdb,
            nsecs,
            symbol_data,
            agg,
            simulator->strategy_config );

        pthread_rwlock_wrlock( &std->lock );

        for ( uint8_t i = 0; i < order_count; i++ )
        {
            struct Order * o = &orders[i];
            strncpy( o->ticker, sym_info->ticker, TICKERLEN );
            o->order_schedule_type = ORDER_SCHEDULE_TYPE_TRADE_EVENT;
            simulator_send_order( simulator, o );

            if ( o->side == BUY )
            { std->outstanding_order_share_position += o->quantity; }
            else if ( o->side == SELL )
            { std->outstanding_order_share_position -= o->quantity; }
        }

        pthread_rwlock_unlock( &std->lock );
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
    struct Simulator * simulator = proct;
    const struct Country * c = &simulator->country;

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    struct SymbolData * symbol_data = &simulator->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( simulator->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL || !sym_info->enable )
    { return 0; }

    // TODO: Move into a function like: simulator_on_book_update()
    struct BBO * nbbo = &simulator->nbbos[sec_id];

    int err = strategy_on_book_update( symbol_data, nsecs, mkt_id,
            book, agg, nbbo, simulator->strategy_config, c );

    if ( err == ERROR_BAD_SYMBOL )
    { return 0; }
    else if ( err < 0 )
    {
        log_error( "agent_on_book_update(): call to strategy_update_symbol_data_book_update() returned: %d", err );
        return err;
    }

    int8_t nbbo_is_new = 0;

    if ( nbbo->bid.px != agg->bid[0].px ||
         nbbo->bid.sz != agg->bid[0].sz ||
         nbbo->ask.px != agg->ask[0].px ||
         nbbo->ask.sz != agg->ask[0].sz )
    {
        nbbo_is_new = 1;
    }

    memcpy( &nbbo->bid, &agg->bid[0], sizeof(nbbo->bid) );
    memcpy( &nbbo->ask, &agg->ask[0], sizeof(nbbo->ask) );

    struct SymbolTradingData * std = symbol_data->trading_data;

    // Reset the last buy and sell price if they have changed, to allow orders
    // to be sent.

    pthread_rwlock_wrlock( &std->lock );

    if ( std->last_buy_price &&
         std->last_buy_price != nbbo->ask.px )
    { std->last_buy_price = 0; }

    if ( std->last_sell_price &&
         std->last_sell_price != nbbo->bid.px )
    { std->last_sell_price = 0; }

    pthread_rwlock_unlock( &std->lock );

    // TODO: call the strategy?  Need several conditions met, like not during
    // lunch, not before the open_wait, !u->nxt, etc.  See
    // agent_on_trade().

    return 0;
}


static int agent_on_cycle (
        void * simulator_void_ptr,
        uint64_t nsecs,
        void * ignore )
{
    struct Simulator * simulator = simulator_void_ptr;

    // TODO: This needs to be done regularly, not just in this function.
    // When?  Anytime there is a fill?  And regularly, e.g. each 5s?
    global_trading_data_update(
        &simulator->global_trading_data,
        simulator->symbol_trading_datas,
        simulator->nbbos,
        simulator->max_symbols,
        nsecs );

    const struct Country * c = &simulator->country;

    if ( !simulator->do_cycle_trading    ||
         !country_during_continuous_session( c, nsecs ) )
    {
        return 0;
    }

    const struct si * sym_info = NULL;
    size_t sym_info_count;
    struct mbp_level ask[simulator->args.levels], bid[simulator->args.levels];
    struct mbp_book agg = { .ask = ask, .bid = bid };

    for ( size_t sec_id = 0; sec_id < simulator->max_symbols; sec_id++ )
    {
        sidb_byid( simulator->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL || !sym_info->enable )
        { continue; }

        memset( ask, 0, sizeof(ask) );
        memset( bid, 0, sizeof(bid) );

        int err = simulator->args.getbook( sec_id, UINT16_MAX, &agg );

        if ( err != 0 )
        { log_notice( "Unable to get MBP agg book for sec_id: %d", sec_id ); }

        struct SymbolData * symbol_data = &simulator->symbol_datas[sec_id];

        if ( symbol_data->in_universe < 0 )
        { continue; }

        struct SymbolTradingData * std = symbol_data->trading_data;

        pthread_rwlock_rdlock( &std->lock );

        bool market_trade_since_last_order =
            std->last_buy_order_nsecs < std->last_market_trade_nsecs &&
            std->last_sell_order_nsecs < std->last_market_trade_nsecs;

        bool book_update_since_last_order =
            std->last_buy_price != agg.ask[0].px &&
            std->last_sell_price != agg.bid[0].px;

        pthread_rwlock_unlock( &std->lock );

        if ( bbo_is_good( &agg )          &&
            market_trade_since_last_order &&
            book_update_since_last_order )
        {
            float features [MAX_FEATURE_COUNT] = { 0.f };

            int err = strategy_generate_features( features, symbol_data,
                    nsecs, &simulator->country, &agg, simulator->strategy_config );

            if ( err == ERROR_NO_STOCKCHARACTERISTICS )
            { return 0; }
            else if ( err == ERROR_SPREAD_TOO_WIDE )
            { return 0; }
            else if ( err == ERROR_FEATURE_NOT_FINITE )
            { return 0; }
            else if ( err < 0 )
            {
                log_notice( "agent_on_trade(): call to strategy_generate_features() returned: %d", err );
                return err;
            }

            struct Predictions predictions;
            strategy_predict( &predictions, simulator->models,
                    features, ORDER_SCHEDULE_TYPE_TRADE_EVENT, simulator->strategy_config );

            err = strategy_adjust_predictions( &predictions, symbol_data,
                    &simulator->global_trading_data, simulator->strategy_config );

            if ( err < 0 )
            {
                log_notice( "agent_on_trade(): call to strategy_adjust_predictions() returned: %d", err );
                return err;
            }

            struct Order orders [16];
            uint8_t order_count = 0;

            strategy_generate_orders(
                orders,
                &order_count,
                16,
                &predictions,
                &simulator->args,
                &simulator->cfgdb,
                nsecs,
                symbol_data,
                &agg,
                simulator->strategy_config );

            pthread_rwlock_wrlock( &std->lock );

            for ( uint8_t i = 0; i < order_count; i++ )
            {
                struct Order * o = &orders[i];
                strncpy( o->ticker, sym_info->ticker, TICKERLEN );
                o->order_schedule_type = ORDER_SCHEDULE_TYPE_TRADE_EVENT;
                simulator_send_order( simulator, o );

                if ( o->side == BUY )
                { std->outstanding_order_share_position += o->quantity; }
                else if ( o->side == SELL )
                { std->outstanding_order_share_position -= o->quantity; }
            }

            pthread_rwlock_unlock( &std->lock );
        }
    }

    return 0;
}


int agent_on_pre ( mbp_sigproc_t proct, uint64_t nsecs )
{
    struct Simulator * simulator = proct;

    task_manager_run_tasks_before( &simulator->task_manager, nsecs, proct );

    return 0;
}


int agent_fini ( mbp_sigproc_t proct )
{
    struct Simulator * simulator = proct;

    // Write the fills file.

    char fills_file_name [512];
    snprintf( fills_file_name, 512, "%s/%s_fills_set%d_%d.txt",
        simulator->output_directory, simulator->country.name,
        simulator->args.set_id, simulator->idate);
    FILE * fills_file = fopen( fills_file_name, "w" );

    if ( fills_file != NULL )
    { log_info( "Writing fills to %s", fills_file_name ); }
    else
    {
        log_error( "simulator::agent_fini() unable to open fills file %s", fills_file_name );
        return ERROR_FILE_ERROR;
    }

    char fill_str [256];

    for ( size_t fi = 0; fi < simulator->fill_count; fi++ )
    {
        struct Fill * f = &simulator->fills[fi];

        double close_price = mid_bbo_double( simulator->nbbos[f->sec_id] );

        f->pnl_in_currency = ( f->side == BUY ) ?
            (( close_price - fix2dbl(f->price) ) * f->quantity ) - f->fees_in_currency :
            (( fix2dbl(f->price) - close_price ) * f->quantity ) - f->fees_in_currency ;

        write_fill_to_string( fill_str, 256, f );
        fprintf( fills_file, "%s\n", fill_str );
    }
    
    // Write the positions file.

    char positions_file_name [512];
    snprintf( positions_file_name, 512, "%s/%s_positions_set%d_%d.txt",
        simulator->output_directory, simulator->country.name,
        simulator->args.set_id, simulator->idate);
    FILE * positions_file = fopen( positions_file_name, "w" );

    if ( positions_file != NULL )
    { log_info( "Writing positions to %s", positions_file_name ); }
    else
    {
        log_error( "simulator::agent_fini() unable to open positions file %s", positions_file_name );
        return ERROR_FILE_ERROR;
    }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    fprintf( positions_file, "%d CASH 1 %.2f\n",
            CASH_POSITION_INDEX, simulator->global_trading_data.cash );

    for ( size_t sec_id = 0; sec_id < simulator->max_symbols; sec_id++ )
    {
        sidb_byid( simulator->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL || !sym_info->enable )
        { continue; }

        double close_price = mid_bbo_double( simulator->nbbos[sec_id] );

        const struct SymbolData * symbol_data = &simulator->symbol_datas[sec_id];
        const struct SymbolTradingData *  std = symbol_data->trading_data ;

        fprintf( positions_file, "%lu %s %d %.2f\n",
            sec_id,
            sym_info->ticker,
            std->share_position,
            (std->share_position*close_price) );
    }

    fflush( NULL );  // flush ALL open streams to disk.

    fclose( fills_file );
    fclose( positions_file );
    fclose( simulator->orders_file );
    fclose( simulator->intraday_file );

    cfgdb_destroy( &simulator->cfgdb );
    task_manager_fini( &simulator->task_manager );

    global_trading_data_fini( &simulator->global_trading_data );

    for ( size_t i = 0; i < simulator->max_symbols; ++i )
    { symbol_trading_data_fini( &simulator->symbol_trading_datas[i] ); }

    for ( size_t i = 0; i < MAX_MODEL_COUNT; i++ )
    { mart_fini( &simulator->models[i] ); }

    free( simulator->orders );
    free( simulator->fills );
    free( simulator->nbbos );
    free( simulator->symbol_trading_datas );
    free( simulator->strategy_config );
    free( simulator->symbol_strategy_datas );
    free( simulator->symbol_datas );
    free( simulator );

    dlclose( stratlib );

    return 0;
}


static int simulator_send_order (
    struct Simulator * simulator,
    struct Order * o
)
{
    if ( simulator->order_count >= simulator->max_orders )
    { return ERROR_TOO_MANY; }

    struct Order * so = &simulator->orders[simulator->order_count];

    memcpy( so, o, sizeof(struct Order) );

    so->id = simulator->order_count++;

    char order_string [256];
    write_order_to_string( order_string, 256, so );
    fprintf( simulator->orders_file, "%s\n", order_string );

    task_manager_add_task( &simulator->task_manager, so->nsecs,
        (so->nsecs+simulator->exchange_latency), simulator_match, so );

    return 0;
}


static int simulator_match (
    void * simulator_void_ptr,
    uint64_t nsecs,
    void * order_void_ptr
)
{
    struct Simulator * simulator = simulator_void_ptr;
    struct Order * o = order_void_ptr;

    struct mbp_level ask[simulator->args.levels], bid[simulator->args.levels];
    struct mbp_book book = { .ask=ask, .bid=bid };

    const struct mkt * m = mktdb_byid( simulator->cfgdb.mktdb, o->mkt_id );
    if ( m == NULL )
    { return ERROR_BAD_MARKET_ID; }
    else if ( !m->enable )
    { return ERROR_MARKET_DISABLED; }

    int res;
    if ( ( res = simulator->args.getbook(o->sec_id, o->mkt_id, &book) ) != 0 )
    {
        log_notice( "Unable to get book %u for symbol id %u, res = %d",
            o->mkt_id, o->sec_id, res );
        return ERROR_CANNOT_GET_BOOK;
    }

    struct SymbolData * symbol_data = &simulator->symbol_datas[o->sec_id];
    struct SymbolTradingData *  std = symbol_data->trading_data;

    uint32_t remaining_quantity = o->quantity;

    if ( o->side == BUY )
    {
        for ( size_t l = 0; l < simulator->args.levels; l++ )
        {
            if ( o->price >= book.ask[l].px && book.ask[l].sz > 0 )
            {
                struct Fill * f = &simulator->fills[simulator->fill_count++];
                f->order_id = o->id;
                f->nsecs = nsecs;
                f->side = o->side;
                f->price = book.ask[l].px;
                f->quantity = MIN(remaining_quantity,book.ask[l].sz);
                f->mkt_id = o->mkt_id;
                f->sec_id = o->sec_id;
                strncpy(f->ticker, o->ticker, TICKERLEN);

                double notional_in_currency = fix2dbl(f->price) * f->quantity;
                f->fees_in_currency = notional_in_currency * ( 1. / BPS );  // 1bps fees

                symbol_trading_data_on_fill( std, o, f );
                global_trading_data_on_fill( &simulator->global_trading_data, o, f );

                remaining_quantity -= f->quantity;
            }

            if ( o->price < book.ask[l].px || remaining_quantity == 0 )
            { break; }
        }
    }
    else if ( o->side == SELL )
    {
        for ( size_t l = 0; l < simulator->args.levels; l++ )
        {
            if ( o->price <= book.bid[l].px && book.bid[l].sz > 0 )
            {
                struct Fill * f = &simulator->fills[simulator->fill_count++];
                f->order_id = o->id;
                f->nsecs = nsecs;
                f->side = o->side;
                f->price = book.bid[l].px;
                f->quantity = MIN(remaining_quantity,book.bid[l].sz);
                f->mkt_id = o->mkt_id;
                f->sec_id = o->sec_id;
                strncpy(f->ticker, o->ticker, TICKERLEN);

                double notional_in_currency = fix2dbl(f->price) * f->quantity;
                f->fees_in_currency = notional_in_currency * ( 1. / BPS );  // 1bps fees

                symbol_trading_data_on_fill( std, o, f );
                global_trading_data_on_fill( &simulator->global_trading_data, o, f );

                remaining_quantity -= f->quantity;
            }

            if ( o->price > book.bid[l].px || remaining_quantity == 0 )
            { break; }
        }
    }

    if ( remaining_quantity > 0 )
    {
        symbol_trading_data_on_cancel( std, o, remaining_quantity );
    }

    return 0;
}


static int simulator_write_trading_state (
    void * simulator_void_ptr,
    uint64_t nsecs,
    void * file_ptr )
{
    struct Simulator * simulator = simulator_void_ptr;

    global_trading_data_update(
        &simulator->global_trading_data,
        simulator->symbol_trading_datas,
        simulator->nbbos,
        simulator->max_symbols,
        nsecs );

    FILE * f = file_ptr;

    double total_pnl = simulator->global_trading_data.net_notional_position +
        simulator->global_trading_data.cash;

    double intraday_pnl = 0.;

    for ( size_t fi = 0; fi < simulator->fill_count; fi++ )
    {
        struct Fill * f = &simulator->fills[fi];

        double current_price = mid_bbo_double( simulator->nbbos[f->sec_id] );

        double fill_pnl_in_currency = ( f->side == BUY ) ?
            (( current_price - fix2dbl(f->price) ) * f->quantity ) - f->fees_in_currency :
            (( fix2dbl(f->price) - current_price ) * f->quantity ) - f->fees_in_currency ;

        intraday_pnl += fill_pnl_in_currency;
    }

    fprintf( f, "%llu %lu %lu %.2f %.2f %.2f %.2f %.2f\n",
            nsecs/SECONDS,
            nsecs,
            simulator->global_trading_data.last_update_nsecs,
            simulator->global_trading_data.notional_volume,
            simulator->global_trading_data.net_notional_position,
            simulator->global_trading_data.cash,
            intraday_pnl,
            total_pnl );

    fflush( NULL );  // flush ALL open streams to disk.

    return 0;
}


void write_fill_to_string ( char * dst, size_t dst_len, const struct Fill * f )
{
    snprintf( dst, dst_len, "%lu %lu %u %s %d %.4f %u %u %.4f %.4f",
        f->order_id,
        f->nsecs,
        f->sec_id,
        f->ticker,
        f->side,
        fix2dbl(f->price),
        f->quantity,
        f->mkt_id,
        f->fees_in_currency,
        f->pnl_in_currency );
}

