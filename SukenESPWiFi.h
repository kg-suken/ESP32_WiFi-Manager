#ifndef SUKEN_ESP_WIFI_H
#define SUKEN_ESP_WIFI_H

#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "esp_mac.h"
#include <functional>
#include <memory>

namespace SukenWiFiLib {

// 型エイリアス - 外部依存を明確化
using HttpServer = ::WebServer;
using JsonDoc = JsonDocument;

// 設定構造体 - 設定を構造化
struct NetworkConfig {
    bool useStaticIP = false;
    IPAddress staticIP = IPAddress(192, 168, 1, 200);
    IPAddress gateway = IPAddress(192, 168, 1, 1);
    IPAddress subnet = IPAddress(255, 255, 255, 0);
    IPAddress primaryDNS = IPAddress(8, 8, 8, 8);
    IPAddress secondaryDNS = IPAddress(8, 8, 4, 4);
};

struct WiFiCredentials {
    String ssid;
    String password;
};

// コールバック型定義
using CallbackFunction = std::function<void()>;
using RouteHandler = std::function<void()>;

class SukenESPWiFi {
public:
    // コンストラクタ
    explicit SukenESPWiFi(const String& deviceName = "ESP-WiFi-Manager");
    
    // 初期化
    void init();
    void init(const String& deviceName);
    // ブロッキング版初期化（接続完了まで待機）
    void initBlocking();
    void initBlocking(const String& deviceName);
    
    // WiFi状態
    bool isConnected() const;
    String getLocalIP() const;
    String getMACAddress() const;
    String getConnectedSSID() const;
    String getDeviceMAC() const;
    String getNetworkInfo() const;
    
    // コールバック設定
    void onClientConnect(CallbackFunction callback);
    void onEnterSetupMode(CallbackFunction callback);
    void onDisconnect(CallbackFunction callback);
    
    // 設定管理
    void clearAllSettings();
    void clearWiFiSettings();
    void clearNetworkSettings();
    
    // 設定取得
    WiFiCredentials getStoredCredentials() const;
    NetworkConfig getNetworkConfig() const;
    String getCurrentDNS() const;
    
    // セットアップモード制御
    void enterSetupMode();
    void exitSetupMode();
    bool isInSetupMode() const;
    // ブロッキング待機（任意）: 接続が完了するまで待機。timeoutMs=0 で無期限
    bool waitUntilConnected(uint32_t timeoutMs = 0);
    // セットアップ時にブロックするかの設定（デフォルト: false）
    void setBlockingSetup(bool enable);
    bool getBlockingSetup() const;
    
    // 詳細制御
    void setAPConfig(const IPAddress& ip, const IPAddress& gateway, const IPAddress& subnet);
    void setDeviceName(const String& name);
    
    // 自動セットアップ（切断時にAPへ）
    void enableAutoSetupOnDisconnect(bool enable);
    bool isAutoSetupOnDisconnectEnabled() const;
    // セットアップモード中の自動再接続（保存済みWiFiへ）
    void enableAutoReconnectDuringSetup(bool enable);
    bool isAutoReconnectDuringSetupEnabled() const;
    // 切断後にAPへ移行する前の再接続試行回数/間隔を設定
    void setDisconnectRetryPolicy(uint8_t attempts, uint32_t delayMs);
    uint8_t getDisconnectRetryAttempts() const;
    uint32_t getDisconnectRetryDelayMs() const;
    
private:
    // 設定
    String deviceName_;
    String wifiName_;
    NetworkConfig networkConfig_;
    
    // サーバー関連（セットアップモード時のみ）
    HttpServer* server_;  // ポインタにして動的管理
    std::unique_ptr<HttpServer> serverPtr_;
    IPAddress apIP_;
    String apIPString_;
    DNSServer dnsServer_;
    
    // 状態管理
    bool setupMode_;
    bool blockSetup_;
    TaskHandle_t taskHandle_;
    
    // 通信
    WiFiClientSecure secureClient_;
    JsonDoc wifiList_;
    
    // コールバック
    CallbackFunction clientConnectedCallback_;
    CallbackFunction setupModeCallback_;
    CallbackFunction disconnectedCallback_;
    
    // 内部メソッド
    void startAccessPoint();
    void scanWiFiNetworks();
    void setupWebServer();
    void connectToWiFi();
    void attemptReconnectNonBlocking();
    
    // ファイル操作
    void readWiFiCredentials(WiFiCredentials& credentials) const;
    void saveWiFiCredentials(const WiFiCredentials& credentials);
    void readNetworkSettings();
    void saveNetworkSettings() const;
    
    // ユーティリティ
    char* getMAC() const;
    bool isValidHostname(const String& hostname) const;
    
    // Webハンドラ
    void handleWiFiSettingPage();
    void handleInfoAPI();
    void handleWiFiSettingAPI();
    void handleWiFiListAPI();
    void handleNotFound();
    
    // タスク
    static void taskMain(void* parameter);
    static void reconnectTask(void* parameter);
    
    // 定数
    static constexpr uint16_t DEFAULT_HTTP_PORT = 80;
    static constexpr uint16_t DEFAULT_DNS_PORT = 53;
    static constexpr uint16_t TASK_STACK_SIZE = 8192;
    static constexpr uint8_t TASK_PRIORITY = 2;
    static constexpr uint8_t TASK_CORE = 1;
    static constexpr uint8_t MAX_WIFI_RETRY = 20;
    static constexpr uint32_t WIFI_RETRY_DELAY = 500;
    static constexpr uint32_t SETUP_RECONNECT_INTERVAL_MS = 5000;
    
    // 自動切断処理設定
    bool autoSetupOnDisconnect_ = true;
    TaskHandle_t reconnectTaskHandle_ = nullptr;
    uint32_t lastSetupReconnectMs_ = 0;
    bool autoReconnectDuringSetup_ = true;
    uint8_t disconnectRetryAttemptsBeforeAP_ = 6; // 約3秒（500ms * 6）
    uint32_t disconnectRetryDelayMs_ = 500;
};

// 便利なマクロ - より安全な実装
#define SUKEN_WIFI_INSTANCE() (SukenWiFiLib::getInstance())
#define SUKEN_WIFI_SSID() (SUKEN_WIFI_INSTANCE().getConnectedSSID())
#define SUKEN_WIFI_MAC() (SUKEN_WIFI_INSTANCE().getDeviceMAC())
#define SUKEN_WIFI_IP() (SUKEN_WIFI_INSTANCE().getLocalIP())
#define SUKEN_WIFI_CONNECTED() (SUKEN_WIFI_INSTANCE().isConnected())

// シングルトンアクセス関数
SukenESPWiFi& getInstance();

} // namespace SukenWiFiLib

// グローバルインスタンス（後方互換性のため）
extern SukenWiFiLib::SukenESPWiFi SukenWiFi;

#endif // SUKEN_ESP_WIFI_H