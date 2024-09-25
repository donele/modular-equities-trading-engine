#ifndef _MRTL_COMMON_MOVING_AVERAGES_H_
#define _MRTL_COMMON_MOVING_AVERAGES_H_

struct TimeWeightedMovingAverage
{
    double K;
    double ma_last;
    double t_last;
};


double twma_calculate( const struct TimeWeightedMovingAverage * twma, double t, double v_current );

void twma_close_previous_value( struct TimeWeightedMovingAverage * twma, double t, double v_prev );


struct LeakyAccumulator
{
	double K;
	double sum_last;
	double t_last;
};


double lacc_calculate( const struct LeakyAccumulator * lacc, double t, double v_input);

void lacc_close_previous_value( struct LeakyAccumulator * lacc, double t, double v_input);

#endif  // _MRTL_COMMON_MOVING_AVERAGES_H_

