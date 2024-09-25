#ifndef _MRTL_COMMON_COUNTRY_H_
#define _MRTL_COMMON_COUNTRY_H_

#include <iniparser.h>
#include <stdint.h>
#include <stdbool.h>


struct Country
{
    uint64_t open_time;
    uint64_t lunch_start;
    uint64_t lunch_end;
    uint64_t close_time;
    char dbhost [16];
    char dbname [16];
    char name [4];
    uint8_t exchange_count;
    char market_letter;
};


int country_init ( struct Country *, dictionary * );

bool country_has_lunch ( const struct Country * );

bool country_during_lunch ( const struct Country *, uint64_t nsecs );

bool country_during_continuous_session ( const struct Country *, uint64_t nsecs );

int country_date_ok ( const struct Country *, int idate );

int country_previous_trading_day ( const struct Country *, int idate );

#endif  // _MRTL_COMMON_COUNTRY_H_

