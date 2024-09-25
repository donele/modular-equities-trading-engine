#ifndef _MRTL_AGENT_SIMULATOR_H_
#define _MRTL_AGENT_SIMULATOR_H_

#include <mrtl/model/mart/mart.h>
#include <mrtl/common/country.h>
#include <mrtl/common/types.h>
#include <mrtl/common/task.h>
#include <mrtl/common/constants.h>
#include <mbpsig.h>


struct Simulator
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

    struct Fill * fills;
    size_t fill_count;

    struct TaskManager task_manager;

    uint64_t exchange_latency;
    uint64_t open_wait;
    uint64_t first_trade_time;

    int32_t idate;

    int8_t do_cycle_trading;
    int8_t do_trade_trading;
    int8_t do_nbbo_trading;

    struct Country country;

    char output_directory [256];
    FILE * orders_file;
    FILE * intraday_file;
};


static int agent_on_cycle ( void *, uint64_t, void * );

static int simulator_send_order ( struct Simulator *, struct Order * );

static int simulator_match ( void * simulator_void_ptr, uint64_t nsecs, void * order_void_ptr );

static int simulator_write_trading_state ( void * simulator_void_ptr, uint64_t nsecs, void * file_ptr );

#endif  // _MRTL_AGENT_SIMULATOR_H_

