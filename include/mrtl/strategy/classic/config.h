#ifndef _MRTL_CLASSIC_MODEL_PARAMS_H_
#define _MRTL_CLASSIC_MODEL_PARAMS_H_
#include <stdint.h>

struct StrategyConfig
{
    uint64_t horizon1_from;
    uint64_t horizon1_to;
    uint64_t horizon2_from;
    uint64_t horizon2_to;
    uint64_t horizon3_from;
    uint64_t horizon3_to;
    uint64_t horizon4_from;
    uint64_t horizon4_to;

    float restoring_force_strength_factor;
    float restoring_force_position_factor;
    uint64_t order_cooldown;
};

#endif

