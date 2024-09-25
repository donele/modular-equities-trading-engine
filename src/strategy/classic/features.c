#include <mrtl/strategy/classic/symbol_data.h>
#include <mrtl/strategy/classic/misc.h>
#include <mrtl/common/functions.h>
#include <mrtl/common/country.h>
#include <mrtl/common/constants.h>
#include <math.h>
#include <mlog.h>

int book_offMedVol(
        const struct mbp_book * agg,
        float * bookOffMedVol,
        const float * volumeFrac,
        float median_volume,
        uint8_t nVolFrac)
{
    memset(bookOffMedVol, 0, nVolFrac*sizeof(float));

    if(nVolFrac == 0)
        return 0;
    if(agg->bid[0].sz == 0 || agg->ask[0].sz == 0)
        return 0;

    float minSprdOff = 0.0001f;
    int maxLevel = 10; // The value should match the command line option -l to qhmblparse

    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);
    float nbbo_sprd = nbbo_ask - nbbo_bid;
    float nbbo_sprdOff = (nbbo_sprd > minSprdOff) ? nbbo_sprd : minSprdOff;

    int totBidSize = 0, totAskSize = 0, bidLevel = 0, askLevel = 0;
    for(uint8_t i = 0; i < nVolFrac; i++)
    {
        int volume_thres = (int)(volumeFrac[i] * median_volume);
        for(; bidLevel < maxLevel && totBidSize < volume_thres; ++bidLevel)
        {
            if( agg->bid[bidLevel].px > 0 )
                totBidSize += agg->bid[bidLevel].sz;
        }
        for(; askLevel < maxLevel && totAskSize < volume_thres; ++askLevel)
        {
            if(agg->ask[askLevel].px > 0)
                totAskSize += agg->ask[askLevel].sz;
        }

        if(bidLevel >= maxLevel || askLevel >= maxLevel)
            break;
        if(agg->bid[bidLevel].px <= 0 || agg->ask[askLevel].px <= 0)
            break;

        double b = fix2dbl( agg->bid[bidLevel].px );
        double a = fix2dbl( agg->ask[askLevel].px );
        double sprd = a - b > minSprdOff ? a - b : minSprdOff;
        double midi = mid( b, a );

        bookOffMedVol[i] = (midi / nbbo_mid - 1.) * (nbbo_sprdOff / sprd);
    }
    return 0;
}

int book_medSprdqI(
        const struct mbp_book * agg,
        float * bookMedSprdqI,
        const float * sprdBins,
        float median_spread,
        float median_volume,
        uint8_t nSprdBin)
{
    memset(bookMedSprdqI, 0, nSprdBin*sizeof(float));

    if(nSprdBin == 0 || median_volume <= 0.f)
        return 0;

    int maxLevel = 10; // The value should match the command line option -l to qhmblparse

    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);

    int bidLevel = 0;
    int askLevel = 0;
    float totBidSize = 0.f;
    float totAskSize = 0.f;
    for(int i = 0; i < nSprdBin; ++i)
    {
        double bidLimit = nbbo_mid - sprdBins[i] * median_spread * nbbo_mid;
        double askLimit = nbbo_mid + sprdBins[i] * median_spread * nbbo_mid;
        for(; bidLevel < maxLevel && fix2dbl(agg->bid[bidLevel].px) >= bidLimit; ++bidLevel)
        {
            if(agg->bid[bidLevel].px > 0)
                totBidSize += agg->bid[bidLevel].sz;
        }
        for(; askLevel < maxLevel && fix2dbl(agg->ask[askLevel].px) <= askLimit; ++askLevel)
        {
            if(agg->ask[askLevel].px > 0)
                totAskSize += agg->ask[askLevel].sz;
        }
        if(totBidSize + totAskSize > 0)
            bookMedSprdqI[i] = (totBidSize - totAskSize) / (float)(totBidSize + totAskSize);
    }

    return 0;
}

float book_quoteRat(
        const struct mbp_book * agg,
        int iLevel)
{
    if(!book_side_is_good(agg->bid[iLevel]) || !book_side_is_good( agg->ask[iLevel]))
        return 0.f;

    float minSprdOff = 0.0001f;
    float min_price = .01f;
    float sizeRatMax = 20.f;
    double nbbo_bid = fix2dbl( agg->bid[0].px );
    double nbbo_ask = fix2dbl( agg->ask[0].px );
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);
    double nbbo_sprd = nbbo_ask - nbbo_bid;

    float quote_rat = 0.f;
    float nbbo_tot_size = agg->bid[0].sz + agg->ask[0].sz;
    int bs = agg->bid[iLevel].sz;
    int as = agg->ask[iLevel].sz;
    double sprd = fix2dbl(agg->ask[iLevel].px) - fix2dbl(agg->bid[iLevel].px);
    double sprdOff = (sprd > minSprdOff) ? sprd : minSprdOff;
    double sprdOffRel = BPS * sprdOff / nbbo_mid;
    if(sprdOffRel < 500.f && nbbo_mid > min_price && bs > 0 && as > 0 && nbbo_tot_size > 0)
    {
        float tot_size = bs + as;
        quote_rat = tot_size / nbbo_tot_size;
        if(quote_rat > sizeRatMax)
            quote_rat = sizeRatMax;
    }

    return quote_rat;
}

float book_sprdRat(
        const struct mbp_book * agg,
        int iLevel)
{
    if(!book_side_is_good(agg->bid[iLevel]) || !book_side_is_good( agg->ask[iLevel]))
        return 0.f;

    float minSprdOff = 0.0001f;
    float min_price = .01f;
    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);
    double nbbo_sprd = nbbo_ask - nbbo_bid;
    double nbbo_sprdOff = (nbbo_sprd > minSprdOff) ? nbbo_sprd : minSprdOff;

    int bs = agg->bid[iLevel].sz;
    int as = agg->ask[iLevel].sz;
    float sprdRat = 0.f;
    double sprd = fix2dbl(agg->ask[iLevel].px) - fix2dbl(agg->bid[iLevel].px);
    double sprdOff = (sprd > minSprdOff) ? sprd : minSprdOff;
    double sprdOffRel = BPS * sprdOff / nbbo_mid;
    if(sprdOffRel < 500.f && nbbo_mid > min_price && bs > 0 && as > 0)
    {
        double sprd = fix2dbl(agg->ask[iLevel].px) - fix2dbl(agg->bid[iLevel].px);
        double sprdOff = (sprd > minSprdOff) ? sprd : minSprdOff;
        sprdRat = nbbo_sprdOff / sprdOff;
    }

    return sprdRat;
}

float book_qutImb(
        const struct mbp_book * agg,
        int iLevel)
{
    if(!book_side_is_good(agg->bid[iLevel]) || !book_side_is_good( agg->ask[iLevel]))
        return 0.f;

    float minSprdOff = 0.0001f;
    float min_price = .01f;
    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);
    double nbbo_sprd = nbbo_ask - nbbo_bid;
    double nbbo_sprdOff = (nbbo_sprd > minSprdOff) ? nbbo_sprd : minSprdOff;

    int bs = agg->bid[iLevel].sz;
    int as = agg->ask[iLevel].sz;
    float qutImb = 0.f;
    double sprd = fix2dbl(agg->ask[iLevel].px) - fix2dbl(agg->bid[iLevel].px);
    double sprdOff = (sprd > minSprdOff) ? sprd : minSprdOff;
    double sprdOffRel = BPS * sprdOff / nbbo_mid;
    if(sprdOffRel < 500.f && nbbo_mid > min_price && bs > 0 && as > 0)
    {
        qutImb = (float)(bs - as) / (float)(bs + as);
    }

    return qutImb;
}

float book_offset(
        const struct mbp_book * agg,
        int iLevel)
{
    if(!book_side_is_good(agg->bid[iLevel]) || !book_side_is_good( agg->ask[iLevel]))
        return 0.f;

    float minSprdOff = 0.0001f;
    float min_price = .01f;
    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);
    double nbbo_sprd = nbbo_ask - nbbo_bid;
    double nbbo_sprdOff = (nbbo_sprd > minSprdOff) ? nbbo_sprd : minSprdOff;

    int bs = agg->bid[iLevel].sz;
    int as = agg->ask[iLevel].sz;
    float offset = 0.f;
    double sprd = fix2dbl(agg->ask[iLevel].px) - fix2dbl(agg->bid[iLevel].px);
    double sprdOff = (sprd > minSprdOff) ? sprd : minSprdOff;
    double sprdOffRel = BPS * sprdOff / nbbo_mid;
    if(sprdOffRel < 500.f && nbbo_mid > min_price && bs > 0 && as > 0)
    {
        double mid_ = mid(fix2dbl(agg->bid[iLevel].px), fix2dbl(agg->ask[iLevel].px));
        offset = BPS * (mid_ / nbbo_mid - 1.f) * (nbbo_sprdOff / sprdOff);
    }

    return offset;
}

float get_qi_max(double maxbs, double maxas)
{
    float qi_max = (maxbs + maxas > 0.) ? (maxbs - maxas) / (maxbs + maxas) : 0;
    return qi_max;
}

int strategy_generate_features(
        float * xs,
        const struct SymbolData * symbol_data,
        uint64_t nsecs,
        const struct Country * country,
        const struct mbp_book * agg,
        void * strategy_config)
{
    memset(xs, 0, MAX_FEATURE_COUNT*sizeof(float));

    const struct SymbolStrategyData * s = symbol_data->strategy_data;

    double nbbo_bid = fix2dbl(agg->bid[0].px);
    double nbbo_ask = fix2dbl(agg->ask[0].px);
    double nbbo_mid = mid(nbbo_bid, nbbo_ask);

    if(!s->stock_characteristics_ok)
        return ERROR_BAD_SYMBOL;

    if(spread_in_bps(nbbo_bid, nbbo_ask) > SAMPLE_SPREAD_LIMIT)
        return ERROR_SPREAD_TOO_WIDE;

    if(nsecs >= country->lunch_start && nsecs <= country->lunch_end)
        return ERROR_BAD_SAMPLE;
    if(nbbo_bid > nbbo_ask)
        return ERROR_SPREAD_NEGATIVE;
    if(s->last_trade_time <= 0.)
        return ERROR_BAD_SAMPLE;
    if(s->first_quote_time >= s->last_trade_time)
        return ERROR_BAD_SAMPLE;
    if(s->med_med_sprd < MIN_MED_MED_SPRD || s->med_med_sprd > MAX_MED_MED_SPRD)
        return ERROR_BAD_SYMBOL;
    if(s->tick_valid < 1)
        return ERROR_BAD_SYMBOL;

    double minHiloD = 5.;

    const int nVolFrac = 6;
    float bookOffMedVol[nVolFrac];
    {
        const float volumeFrac[6] = {0.01, 0.02, 0.04, 0.08, 0.16, 0.32};
        book_offMedVol(agg, bookOffMedVol, volumeFrac, s->median_volume, nVolFrac);
    }

    const int nSprdBin = 7;
    float bookMedSprdqI[nSprdBin];
    {
        const float sprdBins[7] = {0.25, 0.5, 1., 2., 4., 8., 16.};
        book_medSprdqI(agg, bookMedSprdqI, sprdBins, s->med_med_sprd, s->median_volume, nSprdBin);
    }

    float hlspr = (s->last_trade_time > 0) ? spread_in_bps(s->low_trade_price, s->high_trade_price) : 0.f;
    float fraction_of_day = ((double)(nsecs - country->open_time)) / ((double)(country->close_time - country->open_time));
    float n1sec = (double)(country->close_time - country->open_time) / SECONDS;
    double vwap = (s->intraday_share_volume > 0) ? s->intraday_notional_volume / s->intraday_share_volume : 0.f;

    // -----------------------------------------------
    // Features calculation begins
    // -----------------------------------------------

    float time = (nsecs - country->open_time) / SECONDS;
    float sprd = spread_in_bps(nbbo_bid, nbbo_ask);
    float logVolu = log10(s->median_volume);
    float logPrice = log10(s->p_close);
    float logMedSprd = log10(s->med_med_sprd);
    float prevDayVolu = s->volume / s->median_volume;
    float medVolat = s->median_volatility;
    float relVolat = (fraction_of_day > 0.f && s->low_trade_price > 0. && s->median_volatility > 0.) ? BPS * sqrt(1. / fraction_of_day) * pow(log(s->high_trade_price / s->low_trade_price), 2.) / s->median_volatility : 0.f;
    float relSprd = (s->med_med_sprd > 0.) ? sprd / BPS / s->med_med_sprd : 0.;
    float volSclBsz = BPS * agg->bid[0].sz / s->median_volume;
    float volSclAsz = BPS * agg->ask[0].sz / s->median_volume;

    float fillImb = s->fill_imb;
    float rtrd = (s->last_trade_price > .0001) ? nbbo_mid / s->last_trade_price - 1. : 0.f;
    float mrtrd = (s->mid_before_lastTradeBeforeNbbo > .0001) ? nbbo_mid / s->mid_before_lastTradeBeforeNbbo - 1. : 0.f;

    float qI = quote_imbalance(agg->bid[0].sz, agg->ask[0].sz);
    float qIMax = get_qi_max(lacc_calculate(&s->lacc_max_bid_size_200s, nsecs, 0.), lacc_calculate( &s->lacc_max_ask_size_200s, nsecs, 0.));
    float qIMax2 = get_qi_max(lacc_calculate(&s->lacc_max_bid_size_1200s, nsecs, 0.), lacc_calculate( &s->lacc_max_ask_size_1200s, nsecs, 0.));
    float qIWt = (double)(agg->bid[0].sz - agg->ask[0].sz) / (double)(agg->bid[0].sz + agg->ask[0].sz) * sqrt(fmax(agg->bid[0].sz, agg->ask[0].sz) / s->median_volume) / 10.; // factor 10 is for backward compatibility

    float mret5 = return_in_bps(twma_calculate(&s->midprice_moving_average_5s, nsecs, nbbo_mid), nbbo_mid);
    float mret15 = return_in_bps(twma_calculate(&s->midprice_moving_average_15s, nsecs, nbbo_mid), nbbo_mid);
    float mret30 = return_in_bps(twma_calculate(&s->midprice_moving_average_30s, nsecs, nbbo_mid), nbbo_mid);
    float mret60 = return_in_bps(twma_calculate(&s->midprice_moving_average_60s, nsecs, nbbo_mid), nbbo_mid);
    float mret120 = return_in_bps(twma_calculate(&s->midprice_moving_average_120s, nsecs, nbbo_mid), nbbo_mid);
    float mret300 = return_in_bps(twma_calculate(&s->midprice_moving_average_300s, nsecs, nbbo_mid), nbbo_mid);
    float mret600 = return_in_bps(twma_calculate(&s->midprice_moving_average_600s, nsecs, nbbo_mid), nbbo_mid);
    float mret1200 = return_in_bps(twma_calculate(&s->midprice_moving_average_1200s, nsecs, nbbo_mid), nbbo_mid);
    float mret2400 = return_in_bps(twma_calculate(&s->midprice_moving_average_2400s, nsecs, nbbo_mid), nbbo_mid);
    float mret4800 = return_in_bps(twma_calculate(&s->midprice_moving_average_4800s, nsecs, nbbo_mid), nbbo_mid);
    float mret9600 = return_in_bps(twma_calculate(&s->midprice_moving_average_9600s, nsecs, nbbo_mid), nbbo_mid);
    float mret300L = mret600 - mret300;
    float mret600L = mret1200 - mret600;
    float mret1200L = mret2400 - mret1200;
    float mret2400L = mret4800 - mret2400;
    float mret4800L = mret9600 - mret4800;

    float cwret30 = mret30 * s->logCap;
    float cwret60 = mret60 * s->logCap;
    float cwret120 = mret120 * s->logCap;
    float cwret300 = mret300 * s->logCap;
    float cwret300L = mret300L * s->logCap;

    float mretOpen = clip(return_in_bps(s->first_quote_mid, nbbo_mid), 100000., 10000000.);
    float mretONLag0 = clip(s->overnight_return_bps, 100000., 10000000.);
    float mretONLag1 = (s->pp_close > 0. && s->p_open > 0.) ? clip(return_in_bps(s->pp_close, s->p_open), 100000., 10000000.) : 0.;
    float mretIntraLag1 = (s->p_open > 0. && s->p_close > 0.) ? clip(return_in_bps(s->p_open, s->p_close), 100000., 10000000.) : 0.;
    float mretIntraLag2 = (s->pp_open > 0. && s->pp_close > 0.) ? clip(return_in_bps(s->pp_open, s->pp_close), 100000., 10000000.) : 0.;

    float TOBoff1 = 0.; // need tob
    float TOBoff2 = 0.; // need tob
    float TOBqI2 = 0.; // need tob

    float TOBqI3 = 0.; // need tob
    float BsRat1 = book_sprdRat(agg, 1);
    float BsRat2 = book_sprdRat(agg, 2);
    float BqRat1 = book_quoteRat(agg, 1);
    float BqRat2 = book_quoteRat(agg, 2);
    float BqI2 = book_qutImb(agg, 1); // BqI2 = book_qutImb(agg, 1)
    float BqI3 = book_qutImb(agg, 2); // BqI3 = book_qutImb(agg, 2)
    float Boff1 = book_offset(agg, 1);
    float Boff2 = book_offset(agg, 2);
    float BOffmedVol_01 = bookOffMedVol[0];
    float BOffmedVol_02 = bookOffMedVol[1];
    float BOffmedVol_04 = bookOffMedVol[2];
    float BOffmedVol_08 = bookOffMedVol[3];
    float BOffmedVol_16 = bookOffMedVol[4];
    float BOffmedVol_32 = bookOffMedVol[5];
    float BmedSprdqI_5 = bookMedSprdqI[1];
    float BmedSprdqI1 = bookMedSprdqI[2];
    float BmedSprdqI2 = bookMedSprdqI[3];
    float BmedSprdqI4 = bookMedSprdqI[4];
    float BmedSprdqI8 = bookMedSprdqI[5];
    float BmedSprdqI16 = bookMedSprdqI[6];

    float hiloD = BPS * (s->high_trade_price - s->low_trade_price) / (s->high_trade_price + s->low_trade_price);
    float hilo = (hiloD > minHiloD) ? (2. * (s->last_trade_price - mid(s->high_trade_price, s->low_trade_price)) / (s->high_trade_price-s->low_trade_price)) : 0.f;

    float hilo900D = BPS * (s->high_trade_price_900s - s->low_trade_price_900s) / (s->high_trade_price_900s + s->low_trade_price_900s);
    float hilo900 = (hilo900D > minHiloD) ? (2. * s->last_trade_price - s->high_trade_price_900s - s->low_trade_price_900s) / (s->high_trade_price_900s - s->low_trade_price_900s) : 0.f;

    float hiloLag1D = BPS * (s->p_high - s->p_low) / (s->p_high + s->p_low);
    float hiloLag1 = ( hiloLag1D > minHiloD ) ? 2. * (s->last_trade_price - mid(s->p_high, s->p_low)) / (s->p_high - s->p_low) : 0.f;

    float hiloLag1Rat = (s->p_close > 0) ? (s->p_high - s->p_low) / s->p_close : 0.f;
    float hiloLag1Open = (s->p_open > .0001f && s->p_close > .0001f && s->p_high > s->p_low + .0001f && s->p_low > .0001f) ? clip(2. * (s->p_open - s->p_low) / (s->p_high - s->p_low) - 1., 1., 2.) : 0.f;

    float hiloLag2D = BPS * (s->pp_high - s->pp_low) / (s->pp_high + s->pp_low);
    float hiloLag2 = (hiloLag2D > minHiloD) ? 2. * (s->last_trade_price - mid( s->pp_high, s->pp_low)) / (s->pp_high - s->pp_low) : 0.f;

    float hiloLag2Rat = (s->pp_close > 0) ? (s->pp_high - s->pp_low) / s->pp_close : 0.f;
    float hiloLag2Open = (s->pp_open > .0001f && s->pp_close > .0001f && s->pp_high > s->pp_low + .0001f && s->pp_low > .0001f) ? clip(2. * (s->pp_open - s->pp_low) / (s->pp_high - s->pp_low) - 1., 1., 2.) : 0.f;

    float hiloQAI = (s->p_open > .0001f && s->p_close > .0001f && s->p_high > s->p_low && s->p_low > .0001f) ? clip(2. * (s->p_close - s->p_low) / (s->p_high - s->p_low) - 1., 1., 2.) : 0.f;
    float hiloQAI2 = (s->pp_open > .0001f && s->pp_close > .0001f && s->pp_high > s->pp_low && s->pp_low > .0001f) ? clip(2. * (s->pp_close - s->pp_low) / (s->pp_high - s->pp_low) - 1., 1., 2.) : 0.f;

    float vm15 = lacc_calculate(&s->lacc_trade_qty_sum_15s, nsecs, 0.) * (n1sec / 15.) / s->median_volume;
    float vm30 = lacc_calculate(&s->lacc_trade_qty_sum_30s, nsecs, 0.) * (n1sec / 30.) / s->median_volume;
    float vm60 = lacc_calculate(&s->lacc_trade_qty_sum_60s, nsecs, 0.) * (n1sec / 60.) / s->median_volume;
    float vm120 = lacc_calculate(&s->lacc_trade_qty_sum_120s, nsecs, 0.) * (n1sec / 120.) / s->median_volume;
    float vm300 = lacc_calculate(&s->lacc_trade_qty_sum_300s, nsecs, 0.) * (n1sec / 300.) / s->median_volume;
    float vm600 = lacc_calculate(&s->lacc_trade_qty_sum_600s, nsecs, 0.) * (n1sec / 600.) / s->median_volume;
    float vm3600 = lacc_calculate(&s->lacc_trade_qty_sum_3600s, nsecs, 0.) * (n1sec / 3600.) / s->median_volume;
    float vmIntra = s->intraday_share_volume / fraction_of_day / s->median_volume;

    float bollinger300 = (lacc_calculate( &s->lacc_trade_count_300s, nsecs, 0.) > 9.) ? twma_calculate(&s->trade_price_moving_average_300s, nsecs, s->last_trade_price) / (s->intraday_price_sum / s->intraday_ntrades) - 1. : 0.;
    float bollinger900 = (lacc_calculate( &s->lacc_trade_count_900s, nsecs, 0.) > 9.) ? twma_calculate(&s->trade_price_moving_average_900s, nsecs, s->last_trade_price) / (s->intraday_price_sum / s->intraday_ntrades) - 1. : 0.;

    float dnmk = 0.; // need tob
    float snmk = 0.; // need tob

    float mI600 = 0.; // need to read orders
    float mI3600 = 0.;
    float mIIntra = 0.;

    float exchange = 0.; // only US
    float northRST = 0.; // only US
    float northTRD = 0.; // only US
    float northBP = 0.; // only US
    float sprdOpen = 0.; // will not use?

    xs[0] = time;
    xs[1] = sprd;
    xs[2] = logVolu;
    xs[3] = logPrice;
    xs[4] = logMedSprd;
    xs[5] = prevDayVolu;
    xs[6] = medVolat;
    xs[7] = relVolat;
    xs[8] = relSprd;
    xs[9] = volSclBsz;
    xs[10] = volSclAsz;
    xs[11] = fillImb;
    xs[12] = rtrd;
    xs[13] = mrtrd;
    xs[14] = qI;
    xs[15] = qIMax;
    xs[16] = qIMax2;
    xs[17] = qIWt;
    xs[18] = mret5;
    xs[19] = mret15;
    xs[20] = mret30;
    xs[21] = mret60;
    xs[22] = mret120;
    xs[23] = mret300;
    xs[24] = mret600;
    xs[25] = mret300L;
    xs[26] = mret600L;
    xs[27] = mret1200L;
    xs[28] = mret2400L;
    xs[29] = mret4800L;
    xs[30] = cwret30;
    xs[31] = cwret60;
    xs[32] = cwret120;
    xs[33] = cwret300;
    xs[34] = cwret300L;
    xs[35] = mretOpen;
    xs[36] = mretONLag0;
    xs[37] = mretONLag1;
    xs[38] = mretIntraLag1;
    xs[39] = mretIntraLag2;
    xs[40] = TOBoff1;
    xs[41] = TOBoff2;
    xs[42] = TOBqI2;
    xs[43] = TOBqI3;
    xs[44] = BsRat1;
    xs[45] = BsRat2;
    xs[46] = BqRat1;
    xs[47] = BqRat2;
    xs[48] = BqI2;
    xs[49] = BqI3;
    xs[50] = Boff1;
    xs[51] = Boff2;
    xs[52] = BOffmedVol_01;
    xs[53] = BOffmedVol_02;
    xs[54] = BOffmedVol_04;
    xs[55] = BOffmedVol_08;
    xs[56] = BOffmedVol_16;
    xs[57] = BOffmedVol_32;
    xs[58] = BmedSprdqI_5;
    xs[59] = BmedSprdqI1;
    xs[60] = BmedSprdqI2;
    xs[61] = BmedSprdqI4;
    xs[62] = BmedSprdqI8;
    xs[63] = BmedSprdqI16;
    xs[64] = hilo;
    xs[65] = hilo900;
    xs[66] = hiloLag1;
    xs[67] = hiloLag1Rat;
    xs[68] = hiloLag1Open;
    xs[69] = hiloLag2;
    xs[70] = hiloLag2Rat;
    xs[71] = hiloLag2Open;
    xs[72] = hiloQAI;
    xs[73] = hiloQAI2;
    xs[74] = vm15;
    xs[75] = vm30;
    xs[76] = vm60;
    xs[77] = vm120;
    xs[78] = vm300;
    xs[79] = vm600;
    xs[80] = vm3600;
    xs[81] = vmIntra;
    xs[82] = bollinger300;
    xs[83] = bollinger900;
    xs[84] = dnmk;
    xs[85] = snmk;
    xs[86] = mI600;
    xs[87] = mI3600;
    xs[88] = mIIntra;
    xs[89] = exchange;
    xs[90] = northRST;
    xs[91] = northTRD;
    xs[92] = northBP;
    xs[93] = sprdOpen;

    int8_t features_finite = 1;

    for(size_t i = 0; i < MAX_FEATURE_COUNT; i++)
    {
        if(!isfinite(xs[i]))
        {
            log_notice("Warning: feature %d has value %f", i, xs[i]);
            features_finite = 0;
        }
    }

    return features_finite ? 0 : ERROR_FEATURE_NOT_FINITE;
}

int strategy_write_sample_header( char * dst, size_t dst_len )
{
    const size_t feature_string_length = 16;
    const size_t minimum_dst_len = ((MAX_FEATURE_COUNT+MAX_TARGET_COUNT) * feature_string_length) + 128;

    if ( dst_len < minimum_dst_len )
    {
        log_notice( "Warning: In call to strategy_write_sample_header(), dst_len should be > %lu, set to %lu.\n",
                minimum_dst_len, dst_len);
        return -1;
    }

    sprintf(dst, "ticker,");
    strcat(dst, "nsecs,");
    strcat(dst, "bidSize,");
    strcat(dst, "bid,");
    strcat(dst, "ask,");
    strcat(dst, "askSize,");

    strcat(dst, "target0,");
    strcat(dst, "target1,");
    strcat(dst, "target2,");
    strcat(dst, "target3,");

    strcat(dst, "time,");
    strcat(dst, "sprd,");
    strcat(dst, "logVolu,");
    strcat(dst, "logPrice,");
    strcat(dst, "logMedSprd,");
    strcat(dst, "prevDayVolu,");
    strcat(dst, "medVolat,");
    strcat(dst, "relVolat,");
    strcat(dst, "relSprd,");
    strcat(dst, "volSclBsz,");
    strcat(dst, "volSclAsz,");
    strcat(dst, "fillImb,");
    strcat(dst, "rtrd,");
    strcat(dst, "mrtrd,");
    strcat(dst, "qI,");
    strcat(dst, "qIMax,");
    strcat(dst, "qIMax2,");
    strcat(dst, "qIWt,");
    strcat(dst, "mret5,");
    strcat(dst, "mret15,");
    strcat(dst, "mret30,");
    strcat(dst, "mret60,");
    strcat(dst, "mret120,");
    strcat(dst, "mret300,");
    strcat(dst, "mret600,");
    strcat(dst, "mret300L,");
    strcat(dst, "mret600L,");
    strcat(dst, "mret1200L,");
    strcat(dst, "mret2400L,");
    strcat(dst, "mret4800L,");
    strcat(dst, "cwret30,");
    strcat(dst, "cwret60,");
    strcat(dst, "cwret120,");
    strcat(dst, "cwret300,");
    strcat(dst, "cwret300L,");
    strcat(dst, "mretOpen,");
    strcat(dst, "mretONLag1,");
    strcat(dst, "mretONLag1,");
    strcat(dst, "mretIntraLag1,");
    strcat(dst, "mretIntraLag2,");
    strcat(dst, "TOBoff1,");
    strcat(dst, "TOBoff2,");
    strcat(dst, "TOBqI2,");
    strcat(dst, "TOBqI3,");
    strcat(dst, "BsRat1,");
    strcat(dst, "BsRat2,");
    strcat(dst, "BqRat1,");
    strcat(dst, "BqRat2,");
    strcat(dst, "BqI2,");
    strcat(dst, "BqI3,");
    strcat(dst, "Boff1,");
    strcat(dst, "Boff2,");
    strcat(dst, "BOffmedVol_01,");
    strcat(dst, "BOffmedVol_02,");
    strcat(dst, "BOffmedVol_04,");
    strcat(dst, "BOffmedVol_08,");
    strcat(dst, "BOffmedVol_16,");
    strcat(dst, "BOffmedVol_32,");
    strcat(dst, "BmedSprdqI_5,");
    strcat(dst, "BmedSprdqI1,");
    strcat(dst, "BmedSprdqI2,");
    strcat(dst, "BmedSprdqI4,");
    strcat(dst, "BmedSprdqI8,");
    strcat(dst, "BmedSprdqI16,");
    strcat(dst, "hilo,");
    strcat(dst, "hilo900,");
    strcat(dst, "hiloLag1,");
    strcat(dst, "hiloLag1Rat,");
    strcat(dst, "hiloLag1Open,");
    strcat(dst, "hiloLag2,");
    strcat(dst, "hiloLag2Rat,");
    strcat(dst, "hiloLag2Open,");
    strcat(dst, "hiloQAI,");
    strcat(dst, "hiloQAI2,");
    strcat(dst, "vm15,");
    strcat(dst, "vm30,");
    strcat(dst, "vm60,");
    strcat(dst, "vm120,");
    strcat(dst, "vm300,");
    strcat(dst, "vm600,");
    strcat(dst, "vm3600,");
    strcat(dst, "vmIntra,");
    strcat(dst, "bollinger300,");
    strcat(dst, "bollinger900,");
    strcat(dst, "dnmk,");
    strcat(dst, "snmk,");
    strcat(dst, "mI600,");
    strcat(dst, "mI3600,");
    strcat(dst, "mIIntra,");
    strcat(dst, "exchange,");
    strcat(dst, "northRST,");
    strcat(dst, "northTRD,");
    strcat(dst, "northBP,");
    strcat(dst, "sprdOpen,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused,");
    strcat(dst, "notused");

    return 0;
}

