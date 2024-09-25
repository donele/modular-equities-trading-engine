#ifndef _MRTL_AGENT_SAMPLER_H_
#define _MRTL_AGENT_SAMPLER_H_

#include <mrtl/common/task.h>
#include <mrtl/common/country.h>
#include <mbpsig.h>


struct Sampler
{
    struct mbp_sigproc_args args;
    struct cfgdb_db         cfgdb;

    void * strategy_config;

    struct Country country;

    struct BBO * nbbos;

    struct SymbolData * symbol_datas;

    void * symbol_strategy_datas;
    size_t max_symbols;

    struct TaskManager task_manager;

    uint64_t * midprice_times;
    double * midprices;
    size_t midprice_count;

    struct Sample * samples;
    size_t max_samples;
    size_t sample_count;

    uint64_t * first_trade_times;

    uint64_t transient_cutoff;
    uint64_t first_trade_time;

    int32_t idate;
    int16_t set_id;

    int8_t do_cycle_sampling;
    int8_t do_trade_sampling;
    int8_t do_nbbo_sampling;

    char output_directory [256];
};


static struct Sample * sampler_get_next_available_sample ( struct Sampler * );

static int check_sample_for_transient ( void *, uint64_t, void * );

static int save_midprice_universe ( void *, uint64_t, void * );

static int agent_on_cycle ( void *, uint64_t, void * );

static int sampler_generate_sample ( struct Sampler *, uint16_t, uint64_t, const struct mbp_book * );

static int sampler_check_sample_for_problems ( struct Sampler *, struct Sample * );

#endif  // _MRTL_AGENT_SAMPLER_H_

