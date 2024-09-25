#ifndef _MRTL_AGENT_LIVE_H_
#define _MRTL_AGENT_LIVE_H_

#include <mrtl/model/mart/mart.h>
#include <mrtl/common/country.h>
#include <mrtl/common/types.h>
#include <mrtl/common/task.h>
#include <mrtl/common/constants.h>
#include <mbpsig.h>


struct LiveClient
{
    struct mbp_sigproc_args args;
    struct cfgdb_db         cfgdb;

    void * strategy_config;

    struct Mart models [MAX_MODEL_COUNT];

    struct GlobalTradingData global_trading_data;

    struct BBO * nbbos;

    struct SymbolData * symbol_datas;

    void * symbol_strategy_datas;
    size_t max_symbols;

    struct SymbolTradingData * symbol_trading_datas;

    double max_notional_position;

    struct Order * orders;
    size_t order_count;
    size_t max_orders;

    uint64_t open_wait;

    int32_t idate;

    int8_t do_cycle_trading;
    int8_t do_trade_trading;
    int8_t do_nbbo_trading;

    struct Country country;

    char output_directory [256];
};


static int live_client_on_cycle ( struct LiveClient *, uint64_t );

static int live_client_send_order ( struct LiveClient *, struct Order * );



#endif  // _MRTL_AGENT_LIVE_H_

