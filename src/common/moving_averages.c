#include <mrtl/common/moving_averages.h>
#include <mrtl/common/functions.h>
#include <assert.h>


double twma_calculate(
        const struct TimeWeightedMovingAverage * twma,
        double t,
        double v_current )
{
    assert( twma->K > 0 );

    double dt = MIN( twma->K, t - twma->t_last );
    return (v_current - twma->ma_last) * dt / twma->K + twma->ma_last;
}


void twma_close_previous_value(
        struct TimeWeightedMovingAverage * twma,
        double t,
        double v_prev )
{
    assert( twma->K > 0 );

    twma->ma_last = ( twma->t_last == 0 ) ?
        v_prev :  // If we have no data yet, just take the value given.
        twma_calculate( twma, t, v_prev );

    twma->t_last = t;
}


double lacc_calculate(
		const struct LeakyAccumulator * lacc,
		double t,
		double v_input )
{
	assert( lacc->K > 0. );
	double dt = MIN( lacc->K, t - lacc->t_last );
	return (1. - dt / lacc->K) * lacc->sum_last + v_input;
}

void lacc_close_previous_value(
		struct LeakyAccumulator * lacc,
		double t,
		double v_input )
{
	assert( lacc->K > 0. );
	lacc->sum_last = (lacc->sum_last == 0) ? v_input : lacc_calculate( lacc, t, v_input );
	lacc->t_last = t;
}

