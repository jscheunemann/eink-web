// eink-web: stand-ins for firmware pieces that need factory or lab hardware.
#include <Arduino.h>

// Factory QA (src/qa.cpp) checks sensors and the panel on a test jig. A
// browser device has passed it.
bool checkIfAlreadyPassed() { return true; }
bool startQA() { return true; }

// Serial output goes to the browser console (HardwareSerial.h writes stderr).
HWCDC Serial;

// The Wi-Fi interface (shim/WiFi.h); it reports connected once begin() runs.
#include <WiFi.h>
WiFiClass WiFi;

// From the excluded captive-portal library (lib/wificaptive): a readable
// name for a Wi-Fi status, used in log lines.
const char *wifiStatusStr(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    default: return "WL_UNKNOWN";
  }
}

// The optional environment sensor (src/misc/sensor/environment_sensor.cpp)
// talks I2C to an SCD41 or temperature chip; a browser device has neither,
// so it finds nothing and sends no SENSORS header.
#include <misc/sensor.h>
void EnvironmentSensor::init(TRMNL_DEVICE *) {}
bool EnvironmentSensor::buildSensorsHeader(char **) { return false; }

// TLS session resumption (src/resumable_wifi_client_secure.cpp) has no
// meaning when the browser's fetch does the HTTPS; connect as usual.
#include "resumable_wifi_client_secure.h"
int ResumableWiFiClientSecure::connect(IPAddress ip, uint16_t port) { return WiFiClientSecure::connect(ip, port); }
int ResumableWiFiClientSecure::connect(const char *host, uint16_t port) { return WiFiClientSecure::connect(host, port); }
