#pragma once
// eink-web: variables marked RTC_DATA_ATTR survive deep sleep on the device.
// They are gathered into one data section, which firmware/trmnl/web_platform.cpp
// saves to the page on deep sleep and restores before setup() on the next boot.
#define RTC_DATA_ATTR __attribute__((section("rtc_data"), used))
#define RTC_NOINIT_ATTR __attribute__((section("rtc_data"), used))
#define RTC_FAST_ATTR
#define RTC_RODATA_ATTR
#define IRAM_ATTR
#define DRAM_ATTR
#define EXT_RAM_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))
