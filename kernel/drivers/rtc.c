#include <rtc.h>
#include <io.h>

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static bool update_in_progress(void)
{
    return (cmos_read(0x0A) & 0x80) != 0;
}

void rtc_read(rtc_time_t *out)
{
    uint32_t spin = 0;

    while (update_in_progress() && spin++ < 1000000)
        ;

    out->second = cmos_read(0x00);
    out->minute = cmos_read(0x02);
    out->hour = cmos_read(0x04);
    out->day = cmos_read(0x07);
    out->month = cmos_read(0x08);
    out->year = cmos_read(0x09);

    if (!(cmos_read(0x0B) & 0x04)) {
        out->second = bcd_to_bin(out->second);
        out->minute = bcd_to_bin(out->minute);
        out->hour = bcd_to_bin((uint8_t)(out->hour & 0x7F));
        out->day = bcd_to_bin(out->day);
        out->month = bcd_to_bin(out->month);
        out->year = bcd_to_bin(out->year);
    }
}
