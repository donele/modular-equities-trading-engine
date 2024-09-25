#include <mrtl/agent/live.h>
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
        log_error( "live::agent_init() had error from load_strategy_library(): %d", res );
        return res;
    }

    log_trace( "Loaded strategy library: %s", strategy_library_file );

    struct LiveClient * live_client = (struct LiveClient *) calloc( 1, sizeof(struct LiveClient) );
    live_client->args = *args;

    if ( (res = cfgdb_get(live_client->args.cfgdbt, &live_client->cfgdb)) != 0 )
    {
        free( live_client );
        return res;
    }

    if ( country_init( &live_client->country, cfg ) < 0 )
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

    live_client->idate = atoi( date_string );

    if ( live_client->idate < 20050000 || 20500000 < live_client->idate )
    {
        log_error( "Unexpected date in environment variable DT: %s", date_string );
        return -1;
    }

    res = country_date_ok( &live_client->country, live_client->idate );
    if ( res < 0 )
    { return res; }
    else if ( !res )
    {
        log_error( "agent_init(): Tick data is not okay for %s on %d",
                live_client->country.name, live_client->idate );
        return -1;
    }

    global_trading_data_init( &live_client->global_trading_data );

    const char * output_directory = iniparser_getstring( cfg, "agent:output_directory", "." );
    strncpy( live_client->output_directory, output_directory, 255 );
    log_info( "Writing output to %s", live_client->output_directory );

    live_client->do_cycle_trading = iniparser_getint( cfg, "agent:do_cycle_trading", 0 );
    live_client->do_trade_trading = iniparser_getint( cfg, "agent:do_trade_trading", 0 );
    live_client->do_nbbo_trading  = iniparser_getint( cfg, "agent:do_nbbo_trading", 0 );

    log_info( "Trading order schedule types:  cycle:%d  trade:%d  nbbo:%d",
            live_client->do_cycle_trading, live_client->do_trade_trading, live_client->do_nbbo_trading );

    live_client->open_wait = iniparser_getint( cfg, "agent:open_wait_seconds", 30 ) * SECONDS;
    log_info( "Using open wait: %lu sec", (live_client->open_wait/SECONDS) );

    live_client->max_notional_position = iniparser_getint( cfg, "agent:max_notional_position", 10000 );
    log_info( "Using maximum notional position: %f", live_client->max_notional_position );

    if ( (res = strategy_init( &live_client->strategy_config, live_client->models, cfg )) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init()" );
        return res;
    }

    // TODO: Load stockcharacteristics data, pass it to
    // strategy_init_symbol_datas(), and use the previous close
    // prices to calculate max share positions.

    struct StockCharacteristics * scs;
    res = load_stock_characteristics( &scs, MAX_SYMBOLS, live_client->cfgdb.sidb,
            live_client->args.set_id, live_client->idate, &live_client->country );

    struct OrderParams* op;
    res = load_order_params( &op, MAX_SYMBOLS, live_client->cfgdb.sidb,
            live_client->args.set_id, live_client->idate, &live_client->country );

    struct MarketMakingParams* mmp;
    res = load_market_making_params( &mmp, MAX_SYMBOLS, live_client->cfgdb.sidb,
            live_client->args.set_id, live_client->idate, &live_client->country );

    live_client->symbol_datas = calloc( MAX_SYMBOLS, sizeof(struct SymbolData) );

    for ( size_t i = 0; i < MAX_SYMBOLS; i++ )
    { symbol_data_init( &live_client->symbol_datas[i] ); }

    if ( (res = strategy_init_symbol_datas( &live_client->symbol_strategy_datas, &live_client->max_symbols,
                    live_client->symbol_datas, scs, op, mmp, MAX_SYMBOLS, live_client->strategy_config ) ) < 0 )
    {
        log_error( "agent_init(): error in call to strategy_init_symbol_datas()" );
        return res;
    }

    log_notice( "Loaded symbol data for %lu symbols.", live_client->max_symbols );

    live_client->max_orders = 1e6;
    live_client->orders = calloc( live_client->max_orders, sizeof(struct Order) );
    live_client->nbbos = calloc( live_client->max_symbols, sizeof(struct BBO) );
    live_client->symbol_trading_datas = calloc( live_client->max_symbols, sizeof(struct SymbolTradingData) );

    if ( live_client->symbol_trading_datas == NULL )
    {
        log_error( "live_client::agent_init() unable to alloc symbol_trading_datas" );
        return -1;
    }

    for ( size_t i = 0; i < live_client->max_symbols; ++i )
    { symbol_trading_data_init( &live_client->symbol_trading_datas[i] ); }

    link_symbol_trading_datas( live_client->symbol_datas, live_client->symbol_trading_datas, live_client->max_symbols );

    int32_t previous_trading_idate = 20200213;
    //int32_t previous_trading_idate = country_previous_trading_day(
    //        &live_client->country, live_client->idate );

    //if ( previous_trading_idate < 0 )
    //{
    //    log_error( "live_client::agent_init() cannot find previous trading day for %d, function call returned %d",
    //            live_client->idate, previous_trading_idate );
    //    return -1;
    //}

    double total_cash_position;
    size_t position_file_count;

    if ( (res = read_positions_from_files( live_client->symbol_datas, &total_cash_position, &position_file_count,
                    &live_client->country, previous_trading_idate, live_client->args.set_id,
                    live_client->cfgdb.sidb, live_client->output_directory )) < 0 )
    {
        log_error( "live_client::agent_init() unable to read positions from files" );
        return -1;
    }

    for ( size_t sec_id = 0; sec_id < live_client->max_symbols; ++sec_id )
    {
        struct SymbolTradingData * std = live_client->symbol_datas[sec_id].trading_data;

        std->max_share_position =
            floor( live_client->max_notional_position / scs[sec_id].prev_close ); // Adjust by 'adjust' factor?
    }

    live_client->global_trading_data.cash = total_cash_position / position_file_count;

    free( scs );
    free( op );
    free( mmp );

    *proct = live_client;

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
    struct LiveClient * live_client = proct;

    const struct Country * c = &live_client->country;

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    struct SymbolData * symbol_data = &live_client->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( live_client->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL || !sym_info->enable )
    { return 0; }

    struct SymbolTradingData *  std = symbol_data->trading_data;

    symbol_trading_data_on_market_trade( std, nsecs, sz );

    int err = strategy_on_trade( symbol_data, nsecs, mkt_id,
            px, sz, book, agg, live_client->strategy_config, c );

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

    // This could be split into a series of if-elseif's so we can log the
    // impact of these conditions.
    if ( live_client->do_trade_trading                 &&
         bbo_is_good( agg )                            &&
         market_trade_since_last_order                 &&
         book_update_since_last_order                  &&
         c->open_time + live_client->open_wait < nsecs &&
         ( !country_has_lunch( c )                     ||
           nsecs < c->lunch_start                      ||
           c->lunch_end + live_client->open_wait < nsecs )
       )
    {
        float features [MAX_FEATURE_COUNT] = { 0.f };

        err = strategy_generate_features( features, symbol_data,
                nsecs, &live_client->country, agg, live_client->strategy_config );

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
        strategy_predict( &predictions, live_client->models,
                features, ORDER_SCHEDULE_TYPE_TRADE_EVENT, live_client->strategy_config );

        err = strategy_adjust_predictions( &predictions, symbol_data,
                &live_client->global_trading_data, live_client->strategy_config );

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
            &live_client->args,
            &live_client->cfgdb,
            nsecs,
            symbol_data,
            agg,
            live_client->strategy_config );

        pthread_rwlock_wrlock( &std->lock );

        for ( uint8_t i = 0; i < order_count; i++ )
        {
            struct Order * o = &orders[i];
            strncpy( o->ticker, sym_info->ticker, TICKERLEN );
            o->order_schedule_type = ORDER_SCHEDULE_TYPE_TRADE_EVENT;
            live_client_send_order( live_client, o );

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
    struct LiveClient * live_client = proct;
    const struct Country * c = &live_client->country;

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    struct SymbolData * symbol_data = &live_client->symbol_datas[sec_id];

    if ( symbol_data->in_universe < 0 )
    { return 0; }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    sidb_byid( live_client->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
    if ( sym_info == NULL || !sym_info->enable )
    { return 0; }

    // TODO: Move into a function like: live_client_on_book_update()
    struct BBO * nbbo = &live_client->nbbos[sec_id];

    int err = strategy_on_book_update( symbol_data, nsecs, mkt_id,
            book, agg, nbbo, live_client->strategy_config, &live_client->country );

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

    // Note: Do not check continuation (u->nxt) flag or u->rdy flag here, as they may be
    // set (or may not be set) on some exchanges when the trade arrives.

    // TODO: call the strategy?  Need several conditions met, like not during
    // lunch, not before the open_wait, !u->nxt, etc.  See
    // agent_on_trade().

    return 0;
}


static int live_client_on_cycle (
        struct LiveClient * live_client,
        uint64_t nsecs )
{
    // TODO: This needs to be done regularly, not just in this function.
    // When?  Anytime there is a fill?  And regularly, e.g. each 5s?
    global_trading_data_update(
        &live_client->global_trading_data,
        live_client->symbol_trading_datas,
        live_client->nbbos,
        live_client->max_symbols,
        nsecs );

    const struct Country * c = &live_client->country;

    if ( !country_during_continuous_session( c, nsecs ) )
    { return 0; }

    const struct si * sym_info = NULL;
    size_t sym_info_count;
    struct mbp_level ask[live_client->args.levels], bid[live_client->args.levels];
    struct mbp_book agg = { .ask = ask, .bid = bid };

    for ( size_t sec_id = 0; sec_id < live_client->max_symbols; sec_id++ )
    {
        sidb_byid( live_client->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL || !sym_info->enable )
        { continue; }

        struct SymbolData * symbol_data = &live_client->symbol_datas[sec_id];

        if ( symbol_data->in_universe < 0 )
        { continue; }

        memset( ask, 0, sizeof(ask) );
        memset( bid, 0, sizeof(bid) );

        int err = live_client->args.getbook( sec_id, UINT16_MAX, &agg );

        if ( err != 0 )
        { log_notice( "Unable to get MBP agg book for sec_id: %d", sec_id ); }

        struct SymbolTradingData * std = symbol_data->trading_data;

        pthread_rwlock_rdlock( &std->lock );

        bool market_trade_since_last_order =
            std->last_buy_order_nsecs < std->last_market_trade_nsecs &&
            std->last_sell_order_nsecs < std->last_market_trade_nsecs;

        bool book_update_since_last_order =
            std->last_buy_price != agg.ask[0].px &&
            std->last_sell_price != agg.bid[0].px;

        pthread_rwlock_unlock( &std->lock );

        if ( bbo_is_good( &agg )           &&
            market_trade_since_last_order  &&
            book_update_since_last_order )
        {
            float features [MAX_FEATURE_COUNT] = { 0.f };

            err = strategy_generate_features( features, symbol_data, nsecs, &live_client->country, &agg, live_client->strategy_config );

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
            strategy_predict( &predictions, live_client->models,
                    features, ORDER_SCHEDULE_TYPE_TRADE_EVENT, live_client->strategy_config );

            err = strategy_adjust_predictions( &predictions, symbol_data,
                    &live_client->global_trading_data, live_client->strategy_config );

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
                &live_client->args,
                &live_client->cfgdb,
                nsecs,
                symbol_data,
                &agg,
                live_client->strategy_config );

            pthread_rwlock_wrlock( &std->lock );

            for ( uint8_t i = 0; i < order_count; i++ )
            {
                struct Order * o = &orders[i];
                strncpy( o->ticker, sym_info->ticker, TICKERLEN );
                o->order_schedule_type = ORDER_SCHEDULE_TYPE_TRADE_EVENT;
                live_client_send_order( live_client, o );

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


int agent_fini ( mbp_sigproc_t proct )
{
    struct LiveClient * live_client = proct;

    // Write the positions file.

    char positions_file_name [512];
    snprintf( positions_file_name, 512, "%s/%s_positions_set%d_%d.txt",
        live_client->output_directory, live_client->country.name,
        live_client->args.set_id, live_client->idate);
    FILE * positions_file = fopen( positions_file_name, "w" );

    if ( positions_file != NULL )
    { log_info( "Writing positions to %s", positions_file_name ); }
    else
    {
        log_error( "live_client::agent_fini() unable to open positions file %s", positions_file_name );
        return ERROR_FILE_ERROR;
    }

    const struct si * sym_info = NULL;
    size_t sym_info_count;

    fprintf( positions_file, "%d CASH 1 %.2f\n",
            CASH_POSITION_INDEX, live_client->global_trading_data.cash );

    for ( size_t sec_id = 0; sec_id < live_client->max_symbols; sec_id++ )
    {
        sidb_byid( live_client->cfgdb.sidb, sec_id, &sym_info, &sym_info_count );
        if ( sym_info == NULL || !sym_info->enable )
        { continue; }

        double close_price = mid_bbo_double( live_client->nbbos[sec_id] );

        const struct SymbolData * symbol_data = &live_client->symbol_datas[sec_id];
        const struct SymbolTradingData *  std = symbol_data->trading_data;

        fprintf( positions_file, "%lu %s %d %.2f\n",
            sec_id,
            sym_info->ticker,
            std->share_position,
            (std->share_position*close_price) );
    }

    fflush( NULL );  // flush ALL open streams to disk.

    fclose( positions_file );

    cfgdb_destroy( &live_client->cfgdb );

    global_trading_data_fini( &live_client->global_trading_data );

    for ( size_t i = 0; i < live_client->max_symbols; ++i )
    { symbol_trading_data_fini( &live_client->symbol_trading_datas[i] ); }

    for ( size_t i = 0; i < MAX_MODEL_COUNT; i++ )
    { mart_fini( &live_client->models[i] ); }

    free( live_client->orders );
    free( live_client->nbbos );
    free( live_client->symbol_trading_datas );
    free( live_client->strategy_config );
    free( live_client->symbol_strategy_datas );
    free( live_client->symbol_datas );
    free( live_client );

    dlclose( stratlib );

    return 0;
}


static int live_client_send_order (
    struct LiveClient * live_client,
    struct Order * o
)
{
    // NOTE: Need to set the order id here!!!
    return 0;
}

