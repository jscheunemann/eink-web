#pragma once
// eink-web bridge between the TRMNL firmware build and the page.
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Panel: show the controller's two RAM planes (1 bit per pixel each,
// row-major, MSB first). gray4 selects the 4-gray interpretation.
void eink_web_present(const uint8_t *plane0, const uint8_t *plane1, int width, int height, int gray4);

// HTTP through the page (fetch, or the page's mock server).
int eink_fetch_start(const char *method, const char *url, const char *headersJson, const uint8_t *body, int bodyLen);
int eink_fetch_state(int id);          // 0 pending, 1 done, -1 network error
int eink_fetch_status(int id);
int eink_fetch_body_len(int id);
void eink_fetch_body_copy(int id, uint8_t *dst);
char *eink_fetch_header(int id, const char *name);  // malloc'd, or NULL
void eink_fetch_free(int id);

// Sleep: deep sleep saves RTC memory and asks the page to wake the device
// after sleepMs (or on a button press).
void eink_web_deep_sleep(uint64_t sleepUs);
int eink_web_wake_cause(void);         // an esp_sleep_wakeup_cause_t value

// Battery voltage the page reports, in millivolts.
int eink_web_battery_mv(void);

#ifdef __cplusplus
}
#endif
