#pragma once
// eink-web: the device's Wi-Fi setup portal, replaced by a browser stand-in.
// The page is always online, so a network counts as saved and connecting
// succeeds at once. The firmware's own wifi-types.h supplies the types.
#include <functional>
#include <vector>

#include "WString.h"
#include "WiFi.h"
#include "wifi-types.h"

struct ExternalNetwork {
  String ssid;
  int32_t rssi;
  bool open;
  bool is5GHz;
  bool enterprise;
};

class WifiCaptive {
 public:
  using ModemConnectCallback = std::function<bool(const String &ssid, const String &pass)>;
  using ModemScanCallback = std::function<std::vector<ExternalNetwork>()>;

  bool startPortal() { return autoConnect(); }
  String getAPSSID() { return String("TRMNL"); }
  bool isSaved() { return true; }
  void resetSettings() {
    if (onReset_) onReset_();
  }
  void setResetSettingsCallback(std::function<void()> func) { onReset_ = std::move(func); }
  void setHostname(const String &hostname) { hostname_ = hostname; }
  String getHostname() { return hostname_; }
  bool autoConnect() { return WiFi.begin("eink-web", "") == WL_CONNECTED; }
  void setNetworks(const std::vector<ExternalNetwork> &) {}
  WifiCredentials getLastCredentials() {
    WifiCredentials c;
    c.ssid = "eink-web";
    c.pswd = "";
    c.is5GHz = false;
    return c;
  }
  void setModemConnectCallback(ModemConnectCallback) {}
  void setModemMac(const String &) {}
  void setModemScanCallback(ModemScanCallback) {}

 private:
  std::function<void()> onReset_;
  String hostname_ = "trmnl";
};

inline WifiCaptive WifiCaptivePortal;
