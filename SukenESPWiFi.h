#ifndef SUKEN_ESP_WIFI_H
#define SUKEN_ESP_WIFI_H

#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "esp_mac.h"

// 便利なマクロ定義
#define SUKEN_WIFI_SSID() wifiManager.getConnectedSSID()
#define SUKEN_WIFI_MAC() wifiManager.getDeviceMAC()
#define SUKEN_WIFI_IP() wifiManager.getLocalIP()
#define SUKEN_WIFI_CONNECTED() wifiManager.isConnected()

// Forward declaration for the static wrapper function
void WebServerTask(void* args);

class SukenESPWiFi {
private:
    String DeviceName;
    String WiFiName;
    WebServer server;
    IPAddress apIP;
    String apip;
    DNSServer dnsServer;
    TaskHandle_t thp[16];
    DynamicJsonDocument WiFiList;
    WiFiClientSecure client;
    int a;
    
    // 詳細設定用の変数
    bool useStaticIP;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress primaryDNS;
    IPAddress secondaryDNS;

    void APStart();
    void ScanWiFi();
    void WiFiSettingPage();
    void infoapi();
    void WiFiSettingAPI();
    void WiFiListAPI();
    void connectToWiFi();
    void readWiFiCredentials(String& ssid, String& password);
    void saveWiFiCredentials(const String& ssid, const String& password);
    void readNetworkSettings();
    void saveNetworkSettings();
    char* getMAC();

public:
    static void WebServer(void* args);
    SukenESPWiFi(const String& deviceName = "ESP-WiFi");
    void init();
    bool isConnected();
    String getLocalIP();
    String getMACAddress();
    String getConnectedSSID();
    String getDeviceMAC();
    
    // 設定管理関数
    void clearAllSettings();
    void clearWiFiSettings();
    void clearNetworkSettings();
    
    // 設定参照関数
    String getStoredSSID();
    String getStoredPassword();
    bool getUseStaticIP();
    String getStaticIP();
    String getGateway();
    String getSubnet();
    String getPrimaryDNS();
    String getSecondaryDNS();
    String getCurrentDNS();
    String getNetworkInfo();
};

#endif 