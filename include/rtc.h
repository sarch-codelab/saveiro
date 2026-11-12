#ifndef SAVEIRO_RTC_H
#define SAVEIRO_RTC_H

#include <types.h>

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

void rtc_read(rtc_time_t *out);

#endif
