#pragma once

#include <sys/time.h>

inline bool esp_sntp_enabled() { return true; }
inline void esp_sntp_setoperatingmode(int) {}
inline void esp_sntp_setservername(int, const char *) {}
inline void esp_sntp_init() {}
inline void esp_sntp_stop() {}
inline void sntp_set_time_sync_notification_cb(void (*)(struct timeval *)) {}
#define ESP_SNTP_OPMODE_POLL 0
#define SNTP_OPMODE_POLL 0

#define SNTP_SYNC_STATUS_COMPLETED 1
inline int sntp_get_sync_status() { return 1; }

// eink-web: the page's clock is already synchronised.
#define SNTP_MAX_SERVERS 3
typedef enum { SNTP_SYNC_STATUS_RESET = 0, SNTP_SYNC_STATUS_COMPLETED_ = 1, SNTP_SYNC_STATUS_IN_PROGRESS = 2 } sntp_sync_status_t;
inline sntp_sync_status_t esp_sntp_get_sync_status(void) { return static_cast<sntp_sync_status_t>(SNTP_SYNC_STATUS_COMPLETED); }
inline void esp_sntp_set_sync_status(sntp_sync_status_t) {}
inline const char *esp_sntp_getservername(int) { return "pool.ntp.org"; }
