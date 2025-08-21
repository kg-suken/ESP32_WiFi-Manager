#include "SukenESPWiFi.h"

// Define the global instance with a default device name (backward compatibility)
SukenWiFiLib::SukenESPWiFi SukenWiFi("ESP-WiFi-Manager");

namespace SukenWiFiLib {

// Singleton accessor
SukenESPWiFi& getInstance() {
    return SukenWiFi;
}

// Constructor
SukenESPWiFi::SukenESPWiFi(const String& deviceName)
    : server_(nullptr),
      apIP_(192, 168, 1, 100),
      apIPString_("192.168.1.100"),
      setupMode_(false),
      blockSetup_(false),
      taskHandle_(nullptr) {
    if (isValidHostname(deviceName)) {
        deviceName_ = deviceName;
        wifiName_ = deviceName;
    } else {
        Serial.println("[SukenESPWiFi] ERROR: Invalid characters in default device name. Using 'ESP-WiFi-Manager'.");
        deviceName_ = "ESP-WiFi-Manager";
        wifiName_ = "ESP-WiFi-Manager";
    }
}

void SukenESPWiFi::onClientConnect(CallbackFunction callback) {
    clientConnectedCallback_ = std::move(callback);
}

void SukenESPWiFi::onEnterSetupMode(CallbackFunction callback) {
    setupModeCallback_ = std::move(callback);
}

void SukenESPWiFi::onDisconnect(CallbackFunction callback) {
    disconnectedCallback_ = std::move(callback);
}

void SukenESPWiFi::onConnected(CallbackFunction callback) {
    connectedCallback_ = std::move(callback);
}

void SukenESPWiFi::onReconnected(CallbackFunction callback) {
    reconnectedCallback_ = std::move(callback);
}

void SukenESPWiFi::init(const String& deviceName) {
    if (deviceName != "") {
        setDeviceName(deviceName);
    }
    init();
}

void SukenESPWiFi::init() {
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info){
        if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
            Serial.println("Client connected to AP");
            if (clientConnectedCallback_) clientConnectedCallback_();
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.println("WiFi connected (GOT_IP)");
            if (!wasEverConnected_) wasEverConnected_ = true;
            if (connectedCallback_) connectedCallback_();
            if (disconnectedSinceLastConnect_) {
                disconnectedSinceLastConnect_ = false;
                if (reconnectedCallback_) reconnectedCallback_();
            }
            // 接続回復時にAPが残っていれば停止する
            if (setupMode_) {
                Serial.println("Exiting setup mode due to successful connection.");
                exitSetupMode();
                WiFi.mode(WIFI_STA);
            }
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.println("WiFi disconnected");
            if (disconnectedCallback_) disconnectedCallback_();
            if (wasEverConnected_) disconnectedSinceLastConnect_ = true;
            if (autoSetupOnDisconnect_) {
                if (reconnectTaskHandle_ == nullptr) {
                    xTaskCreatePinnedToCore(SukenESPWiFi::reconnectTask, "SukenWiFi_Reconnect", 4096, this, 1, &reconnectTaskHandle_, TASK_CORE);
                }
            }
        }
    });
    secureClient_.setInsecure();
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS mount failed, attempting to format...");
        if (SPIFFS.format() && SPIFFS.begin(false)) {
            Serial.println("SPIFFS formatted and mounted successfully");
        } else {
            Serial.println("SPIFFS format failed");
        }
    } else {
        Serial.println("SPIFFS mounted successfully");
    }
    
    // SPIFFSファイルの存在確認
    Serial.println("=== SPIFFS File Check ===");
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        Serial.println("wifi_credentials.txt exists");
        File file = SPIFFS.open("/wifi_credentials.txt", "r");
        Serial.println("File size: " + String(file.size()) + " bytes");
        file.close();
    } else {
        Serial.println("wifi_credentials.txt does not exist");
    }
    
    if (SPIFFS.exists("/network_settings.txt")) {
        Serial.println("network_settings.txt exists");
        File file = SPIFFS.open("/network_settings.txt", "r");
        Serial.println("File size: " + String(file.size()) + " bytes");
        file.close();
    } else {
        Serial.println("network_settings.txt does not exist");
    }
    Serial.println("========================");
    
    Serial.println("起動しました");
    scanWiFiNetworks();
    Serial.println("WiFiコンフィグ探知");
    
    // ネットワーク設定を読み込み
    readNetworkSettings();
    
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        Serial.println("wifi_credentials.txtが存在します");
        connectToWiFi();
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi接続失敗");
        enterSetupMode();
        if (blockSetup_) {
            // 接続完了まで無期限で待機
            waitUntilConnected(0);
        }
    }
}

void SukenESPWiFi::reconnectTask(void* parameter) {
    SukenESPWiFi* self = static_cast<SukenESPWiFi*>(parameter);
    if (self->setupMode_) {
        self->reconnectTaskHandle_ = nullptr;
        vTaskDelete(nullptr);
    }
    WiFiCredentials creds;
    self->readWiFiCredentials(creds);
    if (creds.ssid.length() == 0) {
        self->enterSetupMode();
        self->reconnectTaskHandle_ = nullptr;
        vTaskDelete(nullptr);
    }
    // まずは STA で一定回数だけ再接続を試行
    WiFi.mode(WIFI_STA);
    if (self->networkConfig_.useStaticIP) {
        WiFi.config(self->networkConfig_.staticIP, self->networkConfig_.gateway, self->networkConfig_.subnet, self->networkConfig_.primaryDNS, self->networkConfig_.secondaryDNS);
    }
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < self->disconnectRetryAttemptsBeforeAP_) {
        delay(self->disconnectRetryDelayMs_);
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        // APへ移行
        self->enterSetupMode();
    }
    self->reconnectTaskHandle_ = nullptr;
    vTaskDelete(nullptr);
}
void SukenESPWiFi::setDisconnectRetryPolicy(uint8_t attempts, uint32_t delayMs) {
    disconnectRetryAttemptsBeforeAP_ = attempts;
    disconnectRetryDelayMs_ = delayMs;
}
uint8_t SukenESPWiFi::getDisconnectRetryAttempts() const { return disconnectRetryAttemptsBeforeAP_; }
uint32_t SukenESPWiFi::getDisconnectRetryDelayMs() const { return disconnectRetryDelayMs_; }

void SukenESPWiFi::initBlocking() {
    blockSetup_ = true;
    init();
}

void SukenESPWiFi::initBlocking(const String& deviceName) {
    blockSetup_ = true;
    init(deviceName);
}

void SukenESPWiFi::enterSetupMode() {
    // 既にセットアップモードなら何もしない
    if (setupMode_) return;
    if (setupModeCallback_) setupModeCallback_();
    startAccessPoint();
}

void SukenESPWiFi::exitSetupMode() {
    if (!setupMode_) return;
    if (server_) server_->stop();
    dnsServer_.stop();
    serverPtr_.reset();
    server_ = nullptr;
    setupMode_ = false;
}

bool SukenESPWiFi::isInSetupMode() const { return setupMode_; }

bool SukenESPWiFi::waitUntilConnected(uint32_t timeoutMs) {
    uint32_t start = millis();
    // セットアップモード中はAPを維持しつつ、接続を待つ
    while (WiFi.status() != WL_CONNECTED) {
        if (timeoutMs > 0 && (millis() - start) >= timeoutMs) {
            return false;
        }
        delay(100);
    }
    // 接続できたらポータルを閉じてSTAに移行
    if (setupMode_) {
        exitSetupMode();
        WiFi.mode(WIFI_STA);
    }
    return true;
}

void SukenESPWiFi::setBlockingSetup(bool enable) { blockSetup_ = enable; }
bool SukenESPWiFi::getBlockingSetup() const { return blockSetup_; }

void SukenESPWiFi::setDeviceName(const String& name) {
    if (!isValidHostname(name)) return;
    deviceName_ = name;
    wifiName_ = name;
}

void SukenESPWiFi::setAPConfig(const IPAddress& ip, const IPAddress& gateway, const IPAddress& subnet) {
    apIP_ = ip;
    apIPString_ = ip.toString();
    if (WiFi.getMode() == WIFI_AP) {
        WiFi.softAPConfig(apIP_, apIP_, subnet);
    }
}

void SukenESPWiFi::startAccessPoint() {
    Serial.println("APスタート");
    setupMode_ = true;
    if (!server_) {
        serverPtr_.reset(new HttpServer(DEFAULT_HTTP_PORT));
        server_ = serverPtr_.get();
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiName_.c_str());
    delay(200);
    WiFi.softAPConfig(apIP_, apIP_, IPAddress(255, 255, 255, 0));
    xTaskCreatePinnedToCore(SukenESPWiFi::taskMain, "SukenESPWiFi_TaskMain", TASK_STACK_SIZE, this, TASK_PRIORITY, &taskHandle_, TASK_CORE);
}

void SukenESPWiFi::scanWiFiNetworks() {
    wifiList_.clear();
    int numNetworks = WiFi.scanNetworks();
    Serial.println("Scan done");

    if (numNetworks == 0) {
        Serial.println("No networks found");
    } else {
        Serial.print(numNetworks);
        Serial.println(" networks found");

        JsonArray networkList = wifiList_.createNestedArray("networks");
        for (int i = 0; i < numNetworks; ++i) {
            networkList.add(WiFi.SSID(i));
        }
    }

    serializeJsonPretty(wifiList_, Serial);
}

void SukenESPWiFi::handleWiFiSettingPage() {
    String html = R"=====(
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi設定</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            background-color: #f5f5f5;
        }
        h1 {
            text-align: center;
            margin-top: 20px;
        }
        .description {
            text-align: center;
            margin-bottom: 20px;
        }
        form {
            width: 80%;
            max-width: 400px;
            margin: 0 auto;
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            text-align: center;
        }
        label {
            display: block;
            margin-bottom: 5px;
            text-align: left;
        }
        input[type="text"],
        input[type="password"],
        select {
            width: calc(100% - 16px);
            padding: 8px;
            margin-bottom: 10px;
            border: 1px solid #ccc;
            border-radius: 4px;
            box-sizing: border-box;
        }
        .hidden {
            display: none;
        }
        button {
            width: calc(100% - 16px);
            padding: 10px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: all 0.3s;
            font-size: 14px;
            font-weight: bold;
        }
        .btn-primary {
            background-color: #4CAF50;
            color: white;
        }
        .btn-primary:hover {
            background-color: #45a049;
        }
        .btn-secondary {
            background-color: white;
            color: #4CAF50;
            border: 2px solid #4CAF50;
        }
        .btn-secondary:hover {
            background-color: #4CAF50;
            color: white;
        }
        #loadingIndicator {
            text-align: center;
            display: none;
            margin-top: 20px;
        }
        .warning-message {
            color: orange;
            font-weight: bold;
            margin-top: 5px;
        }
        .static-ip-form {
            margin-top: 20px;
            padding: 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            background-color: #f9f9f9;
        }
        .static-ip-form h3 {
            margin-top: 0;
            color: #333;
            text-align: center;
        }
        .ip-input-group {
            display: flex;
            gap: 5px;
            margin-bottom: 10px;
        }
        .ip-input-group input {
            flex: 1;
            text-align: center;
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        .button-group {
            margin-top: 20px;
        }
        .button-group button {
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
<h1 id="deviceNameHeader">WiFi設定</h1>
<div class="description">
    <p>WiFiを設定してください。</p>
</div>
<form id="wifiForm">
    <div>
        <label for="wifi_ssid">SSID:</label>
        <select id="wifi_ssid" name="wifi_ssid" required onchange="toggleOtherSSIDInput()">
        </select>
        <input type="text" id="other_ssid" name="other_ssid" class="hidden">
    </div>
    <div id="warningMessage" class="hidden warning-message">WiFiが5GHz帯を利用している可能性があります。2.4GHz帯を利用してください。</div>
    <div>
        <label for="wifi_password">パスワード:</label>
        <input type="password" id="wifi_password" name="wifi_password" required>
    </div>
    
    <div class="static-ip-form" id="staticIPForm" style="display: none;">
        <h3>静的IP設定</h3>
        <div>
            <label for="staticIP">IPアドレス:</label>
            <div class="ip-input-group">
                <input type="number" id="staticIP1" min="0" max="255" value="192" required>
                <input type="number" id="staticIP2" min="0" max="255" value="168" required>
                <input type="number" id="staticIP3" min="0" max="255" value="1" required>
                <input type="number" id="staticIP4" min="0" max="255" value="200" required>
            </div>
        </div>
        
        <div>
            <label for="gateway">ゲートウェイ:</label>
            <div class="ip-input-group">
                <input type="number" id="gateway1" min="0" max="255" value="192" required>
                <input type="number" id="gateway2" min="0" max="255" value="168" required>
                <input type="number" id="gateway3" min="0" max="255" value="1" required>
                <input type="number" id="gateway4" min="0" max="255" value="1" required>
            </div>
        </div>
        
        <div>
            <label for="subnet">サブネットマスク:</label>
            <div class="ip-input-group">
                <input type="number" id="subnet1" min="0" max="255" value="255" required>
                <input type="number" id="subnet2" min="0" max="255" value="255" required>
                <input type="number" id="subnet3" min="0" max="255" value="255" required>
                <input type="number" id="subnet4" min="0" max="255" value="0" required>
            </div>
        </div>
        
        <div>
            <label for="primaryDNS">優先DNS:</label>
            <div class="ip-input-group">
                <input type="number" id="primaryDNS1" min="0" max="255" value="8" required>
                <input type="number" id="primaryDNS2" min="0" max="255" value="8" required>
                <input type="number" id="primaryDNS3" min="0" max="255" value="8" required>
                <input type="number" id="primaryDNS4" min="0" max="255" value="8" required>
            </div>
        </div>
        
        <div>
            <label for="secondaryDNS">代替DNS:</label>
            <div class="ip-input-group">
                <input type="number" id="secondaryDNS1" min="0" max="255" value="8" required>
                <input type="number" id="secondaryDNS2" min="0" max="255" value="8" required>
                <input type="number" id="secondaryDNS3" min="0" max="255" value="4" required>
                <input type="number" id="secondaryDNS4" min="0" max="255" value="4" required>
            </div>
        </div>
    </div>
    
    <div class="button-group">
        <button type="button" class="btn-secondary" onclick="toggleStaticIP()">StaticIP</button>
        <button type="submit" class="btn-primary">設定を保存</button>
    </div>
</form>

<div id="loadingIndicator" style="text-align: center; display: none; margin-top: 20px;">
    <p id="statusLine" style="margin: 0; font-weight: bold;"></p>
</div>

<div id="macAddress" style="text-align: center; margin-top: 20px;"></div>

<script>
    var useStaticIP = false;
    
    document.getElementById('wifiForm').addEventListener('submit', function(event) {
        event.preventDefault();
        submitWiFiSettings();
    });

    function submitWiFiSettings() {
        var wifi_ssid = document.getElementById('wifi_ssid').value;
        if (wifi_ssid === 'その他') {
            wifi_ssid = document.getElementById('other_ssid').value;
        }
        var wifi_password = document.getElementById('wifi_password').value;

        var data = {
            ssid: wifi_ssid,
            password: wifi_password,
            useStaticIP: useStaticIP
        };

        console.log("useStaticIP value:", useStaticIP);
        console.log("useStaticIP type:", typeof useStaticIP);

        if (useStaticIP) {
            data.staticIP = document.getElementById('staticIP1').value + '.' + 
                           document.getElementById('staticIP2').value + '.' + 
                           document.getElementById('staticIP3').value + '.' + 
                           document.getElementById('staticIP4').value;
            
            data.gateway = document.getElementById('gateway1').value + '.' + 
                          document.getElementById('gateway2').value + '.' + 
                          document.getElementById('gateway3').value + '.' + 
                          document.getElementById('gateway4').value;
            
            data.subnet = document.getElementById('subnet1').value + '.' + 
                         document.getElementById('subnet2').value + '.' + 
                         document.getElementById('subnet3').value + '.' + 
                         document.getElementById('subnet4').value;
            
            data.primaryDNS = document.getElementById('primaryDNS1').value + '.' + 
                             document.getElementById('primaryDNS2').value + '.' + 
                             document.getElementById('primaryDNS3').value + '.' + 
                             document.getElementById('primaryDNS4').value;
            
            data.secondaryDNS = document.getElementById('secondaryDNS1').value + '.' + 
                               document.getElementById('secondaryDNS2').value + '.' + 
                               document.getElementById('secondaryDNS3').value + '.' + 
                               document.getElementById('secondaryDNS4').value;
            
            console.log("Static IP settings:", data);
        }

        var jsonData = JSON.stringify(data);
        console.log("Sending JSON data:", jsonData);

        var xhr = new XMLHttpRequest();
        xhr.open('POST', './api/WiFiSetting', true);
        xhr.setRequestHeader('Content-type', 'application/json');

        var indicator = document.getElementById('loadingIndicator');
        var statusEl = document.getElementById('statusLine');
        indicator.style.display = 'block';
        statusEl.textContent = '設定を保存し、WiFiに接続中...';
        statusEl.style.color = '';

        xhr.onload = function () {
            var statusEl = document.getElementById('statusLine');
            var text = '';
            var isError = false;
            try {
                var res = JSON.parse(xhr.responseText || '{}');
                if (res.message) text = res.message;
                if (res.status === 'error') isError = true;
            } catch (e) {
                text = '応答の解析に失敗しました';
                isError = true;
            }
            if (!text) {
                text = (xhr.status === 200) ? '完了しました' : 'エラーが発生しました';
            }
            statusEl.textContent = text;
            statusEl.style.color = isError ? '#d32f2f' : '';
            if (xhr.status == 200) {
                fetchDeviceInfo();
            } else {
                console.error('Error:', xhr.statusText);
            }
        };

        xhr.send(jsonData);
    }

    function fetchDeviceInfo() {
        fetch('./api/info')
            .then(response => response.json())
            .then(data => {
                var macAddress = data.MAC;
                var deviceName = data.DeviceName;
                document.getElementById('macAddress').textContent = 'MACアドレス: ' + macAddress;
                // タイトルと見出しにデバイス名を反映
                document.title = deviceName + ' WiFi設定';
                var header = document.getElementById('deviceNameHeader');
                if (header) header.textContent = deviceName + ' WiFi設定';
            })
            .catch(error => console.error('Error:', error));
    }

    function populateSSIDList() {
        fetch('./api/WiFiList')
            .then(response => response.json())
            .then(data => {
                var wifi_ssidSelect = document.getElementById('wifi_ssid');
                data.networks.forEach(network => {
                    var option = document.createElement('option');
                    option.textContent = network;
                    option.value = network;
                    wifi_ssidSelect.appendChild(option);
                });
                var otherOption = document.createElement('option');
                otherOption.textContent = 'その他';
                otherOption.value = 'その他';
                wifi_ssidSelect.appendChild(otherOption);
            })
            .catch(error => console.error('Error:', error));
    }

    function toggleOtherSSIDInput() {
        var wifi_ssid = document.getElementById('wifi_ssid').value;
        var otherSSIDInput = document.getElementById('other_ssid');
        if (wifi_ssid === 'その他') {
            otherSSIDInput.classList.remove('hidden');
        } else {
            otherSSIDInput.classList.add('hidden');
            otherSSIDInput.value = '';
        }
    }

    function toggleStaticIP() {
        var staticIPForm = document.getElementById('staticIPForm');
        var staticIPButton = event.target;
        
        console.log("toggleStaticIP called, current useStaticIP:", useStaticIP);
        
        if (useStaticIP) {
            // StaticIPを無効にする
            useStaticIP = false;
            staticIPForm.style.display = 'none';
            staticIPButton.textContent = 'StaticIP';
            staticIPButton.classList.remove('btn-primary');
            staticIPButton.classList.add('btn-secondary');
            console.log("StaticIP disabled, useStaticIP:", useStaticIP);
        } else {
            // StaticIPを有効にする
            useStaticIP = true;
            staticIPForm.style.display = 'block';
            staticIPButton.textContent = 'StaticIP (ON)';
            staticIPButton.classList.remove('btn-secondary');
            staticIPButton.classList.add('btn-primary');
            console.log("StaticIP enabled, useStaticIP:", useStaticIP);
        }
    }

    window.onload = function () {
        populateSSIDList();
        fetchDeviceInfo();
    };

    document.getElementById('wifi_ssid').addEventListener('blur', function () {
        checkSSIDInput();
    });

    document.getElementById('other_ssid').addEventListener('blur', function () {
        checkSSIDInput();
    });

    function checkSSIDInput() {
        var ssidInput = document.getElementById('wifi_ssid').value;
        var otherSSIDInput = document.getElementById('other_ssid').value;
        var warningMessage = document.getElementById('warningMessage');

        if (containsInvalidStrings(ssidInput) || containsInvalidStrings(otherSSIDInput)) {
            warningMessage.classList.remove('hidden');
        } else {
            warningMessage.classList.add('hidden');
        }
    }

    function containsInvalidStrings(ssid) {
        var invalidStrings = ['-a', '-A', 'a-', 'A-', '_a', '_A', 'a_', 'A_', '-A-', '_A_', '-a-', '_a_', '5G', '5g', '-5', '_5'];
        return invalidStrings.some(function (str) {
            return ssid.includes(str);
        });
    }
</script>
</body>
</html>
)=====";
    if (server_) server_->send(200, "text/html", html);
}

void SukenESPWiFi::handleNotFound() {
    if (server_) {
        handleWiFiSettingPage();
    }
}

void SukenESPWiFi::handleInfoAPI() {
    StaticJsonDocument<200> doc;
    doc["MAC"] = getMAC();
    doc["DeviceName"] = deviceName_;
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    if (server_) server_->send(200, "application/json", jsonPayload);
}

void SukenESPWiFi::handleWiFiSettingAPI() {
    delay(250);
    String body = server_ ? server_->arg("plain") : String();

    Serial.println("=== Received JSON Data ===");
    Serial.println(body);
    Serial.println("==========================");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        if (server_) server_->send(400, "application/json", "Failed to parse JSON");
        return;
    }

    Serial.println("JSON parsing successful");

    WiFiCredentials credentials;
    credentials.ssid = doc["ssid"].as<String>();
    credentials.password = doc["password"].as<String>();
    
    Serial.println("Parsed SSID: " + credentials.ssid);
    Serial.println("Parsed Password: " + credentials.password);
    
    // StaticIP設定の処理
    Serial.println("Checking for useStaticIP key...");
    if (doc.containsKey("useStaticIP")) {
        Serial.println("useStaticIP key found");
        networkConfig_.useStaticIP = doc["useStaticIP"].as<bool>();
        Serial.println("Parsed useStaticIP: " + String(networkConfig_.useStaticIP ? "true" : "false"));
    } else {
        Serial.println("useStaticIP key not found in JSON");
        networkConfig_.useStaticIP = false;
    }
    
    Serial.println("Current useStaticIP value: " + String(networkConfig_.useStaticIP ? "true" : "false"));
    
    if (networkConfig_.useStaticIP) {
        Serial.println("Processing static IP settings...");
        
        if (doc.containsKey("staticIP")) {
            String staticIPStr = doc["staticIP"].as<String>();
            Serial.println("Raw staticIP string: " + staticIPStr);
            networkConfig_.staticIP.fromString(staticIPStr);
            Serial.println("Parsed Static IP: " + networkConfig_.staticIP.toString());
        } else {
            Serial.println("staticIP key not found in JSON");
        }
        
        if (doc.containsKey("gateway")) {
            String gatewayStr = doc["gateway"].as<String>();
            Serial.println("Raw gateway string: " + gatewayStr);
            networkConfig_.gateway.fromString(gatewayStr);
            Serial.println("Parsed Gateway: " + networkConfig_.gateway.toString());
        } else {
            Serial.println("gateway key not found in JSON");
        }
        
        if (doc.containsKey("subnet")) {
            String subnetStr = doc["subnet"].as<String>();
            Serial.println("Raw subnet string: " + subnetStr);
            networkConfig_.subnet.fromString(subnetStr);
            Serial.println("Parsed Subnet: " + networkConfig_.subnet.toString());
        } else {
            Serial.println("subnet key not found in JSON");
        }
        
        if (doc.containsKey("primaryDNS")) {
            String primaryDNSStr = doc["primaryDNS"].as<String>();
            Serial.println("Raw primaryDNS string: " + primaryDNSStr);
            networkConfig_.primaryDNS.fromString(primaryDNSStr);
            Serial.println("Parsed Primary DNS: " + networkConfig_.primaryDNS.toString());
        } else {
            Serial.println("primaryDNS key not found in JSON");
        }
        
        if (doc.containsKey("secondaryDNS")) {
            String secondaryDNSStr = doc["secondaryDNS"].as<String>();
            Serial.println("Raw secondaryDNS string: " + secondaryDNSStr);
            networkConfig_.secondaryDNS.fromString(secondaryDNSStr);
            Serial.println("Parsed Secondary DNS: " + networkConfig_.secondaryDNS.toString());
        } else {
            Serial.println("secondaryDNS key not found in JSON");
        }
    } else {
        // StaticIPが無効な場合、デフォルト値にリセット
        Serial.println("StaticIP is false, resetting to defaults");
        networkConfig_.useStaticIP = false;
        networkConfig_.staticIP = IPAddress(192, 168, 1, 200);
        networkConfig_.gateway = IPAddress(192, 168, 1, 1);
        networkConfig_.subnet = IPAddress(255, 255, 255, 0);
        networkConfig_.primaryDNS = IPAddress(8, 8, 8, 8);
        networkConfig_.secondaryDNS = IPAddress(8, 8, 4, 4);
        Serial.println("Static IP disabled, using DHCP");
    }
    
    Serial.println("=== Final Settings ===");
    Serial.println("useStaticIP: " + String(networkConfig_.useStaticIP ? "true" : "false"));
    Serial.println("staticIP: " + networkConfig_.staticIP.toString());
    Serial.println("gateway: " + networkConfig_.gateway.toString());
    Serial.println("subnet: " + networkConfig_.subnet.toString());
    Serial.println("primaryDNS: " + networkConfig_.primaryDNS.toString());
    Serial.println("secondaryDNS: " + networkConfig_.secondaryDNS.toString());
    Serial.println("=====================");
    
    Serial.println("About to save settings...");
    saveWiFiCredentials(credentials);
    saveNetworkSettings();
    Serial.println("Settings saved. Trying live connection without reboot...");

    // ライブ接続: セットアップモード中は AP を維持したまま接続試行
    if (setupMode_) {
        WiFi.mode(WIFI_AP_STA);
    }
    connectToWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        String payload = String("{\"message\":\"接続に成功しました\",\"ip\":\"") + WiFi.localIP().toString() + "\",\"ssid\":\"" + WiFi.SSID() + "\"}";
        if (server_) server_->send(200, "application/json", payload);
        Serial.println("Connected. Shutting down AP/portal...");
        // ポータルを終了して STA のみに移行
        exitSetupMode();
        WiFi.mode(WIFI_STA);
        return;
    } else {
        if (server_) server_->send(200, "application/json", "{\"message\":\"接続に失敗しました。再試行してください\",\"status\":\"error\",\"retry\":true}" );
        Serial.println("Connection failed. Staying in setup mode.");
        return;
    }
}

void SukenESPWiFi::handleWiFiListAPI() {
    String json;
    serializeJsonPretty(wifiList_, json);
    if (server_) server_->send(200, "application/json", json);
}

void SukenESPWiFi::taskMain(void* args) {
    SukenESPWiFi* instance = static_cast<SukenESPWiFi*>(args);
    
    Serial.print("mDNS server instancing");
    if (!MDNS.begin(instance->wifiName_.c_str())) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(100);
        }
    }
    Serial.println("mDNSを開始しました");
    MDNS.addService("http", "tcp", 80);
    instance->dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
    instance->dnsServer_.setTTL(300);
    instance->dnsServer_.start(DEFAULT_DNS_PORT, "*", instance->apIP_);
    Serial.println("DNSサーバーを開始しました");
    instance->setupWebServer();
    Serial.println("Webサーバー開始");
    
    while (1) {
        if (!instance->setupMode_ || instance->server_ == nullptr) {
            Serial.println("Setup mode ended. Stopping portal task.");
            vTaskDelete(nullptr);
        }
        instance->dnsServer_.processNextRequest();
        if (instance->server_) instance->server_->handleClient();
        // 定期的に既存WiFiへの再接続を試みる（成功したらポータル終了）
        uint32_t now = millis();
        if (instance->autoReconnectDuringSetup_ && (now - instance->lastSetupReconnectMs_ >= SETUP_RECONNECT_INTERVAL_MS)) {
            instance->lastSetupReconnectMs_ = now;
            instance->attemptReconnectNonBlocking();
        }
        delay(1);
    }
}
void SukenESPWiFi::attemptReconnectNonBlocking() {
    if (!setupMode_) return;
    if (WiFi.status() == WL_CONNECTED) return;
    WiFiCredentials creds;
    readWiFiCredentials(creds);
    if (creds.ssid.length() == 0) return;
    Serial.println("[SetupMode] Trying to reconnect to stored WiFi...");
    WiFi.mode(WIFI_AP_STA);
    if (networkConfig_.useStaticIP) {
        WiFi.config(networkConfig_.staticIP, networkConfig_.gateway, networkConfig_.subnet, networkConfig_.primaryDNS, networkConfig_.secondaryDNS);
    }
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
    // 短時間だけポーリング
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 6) { // 約3秒
        delay(500);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[SetupMode] Reconnected successfully. Exiting setup mode.");
        exitSetupMode();
        WiFi.mode(WIFI_STA);
    }
}

void SukenESPWiFi::setupWebServer() {
    if (!server_) return;
    server_->on("/", [this]() { this->handleWiFiSettingPage(); });
    server_->on("/api/info", [this]() { this->handleInfoAPI(); });
    server_->onNotFound([this]() { this->handleNotFound(); });
    server_->on("/WiFiSetting", [this]() { this->handleWiFiSettingPage(); });
    server_->sendHeader("Access-Control-Allow-Origin", "*");
    server_->sendHeader("Access-Control-Max-Age", "10000");
    server_->sendHeader("Content-Length", "0");
    server_->on("/api/WiFiSetting", HTTP_POST, [this]() { this->handleWiFiSettingAPI(); });
    server_->on("/api/WiFiList", [this]() { this->handleWiFiListAPI(); });
    server_->begin();
}

bool SukenESPWiFi::isValidHostname(const String& hostname) const {
    if (hostname.length() < 1 || hostname.length() > 63) {
        return false;
    }
    for (char c : hostname) {
        if (!isAlphaNumeric(c) && c != '-') {
            return false;
        }
    }
    return true;
}

void SukenESPWiFi::connectToWiFi() {
    WiFiCredentials credentials;
    // セットアップモード中は AP を維持したまま接続を試行
    wifi_mode_t currentMode = WiFi.getMode();
    if (setupMode_) {
        if (currentMode != WIFI_AP_STA) {
            WiFi.mode(WIFI_AP_STA);
        }
    } else {
        if (currentMode != WIFI_STA) {
            WiFi.mode(WIFI_STA);
        }
    }
    readWiFiCredentials(credentials);
    
    if (networkConfig_.useStaticIP) {
        if (!WiFi.config(networkConfig_.staticIP, networkConfig_.gateway, networkConfig_.subnet, networkConfig_.primaryDNS, networkConfig_.secondaryDNS)) {
            Serial.println("Static IP configuration failed");
        }
    }
    
    WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
    WiFi.setHostname(deviceName_.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_WIFI_RETRY) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        if (!MDNS.begin(deviceName_.c_str())) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            Serial.println("mDNS responder started. You can now access the device at http://" + deviceName_ + ".local");
            MDNS.addService("http", "tcp", 80);
        }
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

void SukenESPWiFi::readWiFiCredentials(WiFiCredentials& credentials) const {
    File file = SPIFFS.open("/wifi_credentials.txt", "r");
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            int separatorIndex = line.indexOf('=');
            if (separatorIndex != -1) {
                String key = line.substring(0, separatorIndex);
                String value = line.substring(separatorIndex + 1);
                if (key.equals("SSID")) {
                    credentials.ssid = value;
                } else if (key.equals("Password")) {
                    credentials.password = value;
                }
            }
        }
        file.close();
        Serial.println("WiFi credentials read successfully.");
    } else {
        Serial.println("Error reading WiFi credentials from SPIFFS");
    }
}

void SukenESPWiFi::saveWiFiCredentials(const WiFiCredentials& credentials) {
    Serial.println("Saving WiFi credentials...");
    
    // ファイルが存在する場合は削除
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        SPIFFS.remove("/wifi_credentials.txt");
        Serial.println("Removed existing wifi_credentials.txt");
    }
    
    File file = SPIFFS.open("/wifi_credentials.txt", "w");
    if (file) {
        file.println("SSID=" + credentials.ssid);
        file.println("Password=" + credentials.password);
        file.close();
        Serial.println("WiFi credentials saved successfully.");
        Serial.println("File size: " + String(SPIFFS.open("/wifi_credentials.txt", "r").size()) + " bytes");
    } else {
        Serial.println("Error saving WiFi credentials to SPIFFS");
    }
}

void SukenESPWiFi::readNetworkSettings() {
    if (SPIFFS.exists("/network_settings.txt")) {
        File file = SPIFFS.open("/network_settings.txt", "r");
        if (file) {
            Serial.println("Reading network settings from SPIFFS...");
            while (file.available()) {
                String line = file.readStringUntil('\n');
                line.trim();
                int separatorIndex = line.indexOf('=');
                if (separatorIndex != -1) {
                    String key = line.substring(0, separatorIndex);
                    String value = line.substring(separatorIndex + 1);
                    
                    if (key.equals("useStaticIP")) {
                        networkConfig_.useStaticIP = (value == "true");
                        Serial.println("useStaticIP: " + String(networkConfig_.useStaticIP ? "true" : "false"));
                    } else if (key.equals("staticIP")) {
                        networkConfig_.staticIP.fromString(value);
                        Serial.println("staticIP: " + networkConfig_.staticIP.toString());
                    } else if (key.equals("gateway")) {
                        networkConfig_.gateway.fromString(value);
                        Serial.println("gateway: " + networkConfig_.gateway.toString());
                    } else if (key.equals("subnet")) {
                        networkConfig_.subnet.fromString(value);
                        Serial.println("subnet: " + networkConfig_.subnet.toString());
                    } else if (key.equals("primaryDNS")) {
                        networkConfig_.primaryDNS.fromString(value);
                        Serial.println("primaryDNS: " + networkConfig_.primaryDNS.toString());
                    } else if (key.equals("secondaryDNS")) {
                        networkConfig_.secondaryDNS.fromString(value);
                        Serial.println("secondaryDNS: " + networkConfig_.secondaryDNS.toString());
                    }
                }
            }
            file.close();
            Serial.println("Network settings read successfully.");
        } else {
            Serial.println("Error reading network settings from SPIFFS");
        }
    } else {
        Serial.println("Network settings file not found, using defaults.");
        Serial.println("useStaticIP: false (default)");
    }
}

void SukenESPWiFi::saveNetworkSettings() const {
    Serial.println("Saving network settings...");
    
    // ファイルが存在する場合は削除
    if (SPIFFS.exists("/network_settings.txt")) {
        SPIFFS.remove("/network_settings.txt");
        Serial.println("Removed existing network_settings.txt");
    }
    
    File file = SPIFFS.open("/network_settings.txt", "w");
    if (file) {
        Serial.println("Saving network settings to SPIFFS...");
        
        String useStaticIPStr = String(networkConfig_.useStaticIP ? "true" : "false");
        file.println("useStaticIP=" + useStaticIPStr);
        Serial.println("Saving useStaticIP: " + useStaticIPStr);
        
        String staticIPStr = networkConfig_.staticIP.toString();
        file.println("staticIP=" + staticIPStr);
        Serial.println("Saving staticIP: " + staticIPStr);
        
        String gatewayStr = networkConfig_.gateway.toString();
        file.println("gateway=" + gatewayStr);
        Serial.println("Saving gateway: " + gatewayStr);
        
        String subnetStr = networkConfig_.subnet.toString();
        file.println("subnet=" + subnetStr);
        Serial.println("Saving subnet: " + subnetStr);
        
        String primaryDNSStr = networkConfig_.primaryDNS.toString();
        file.println("primaryDNS=" + primaryDNSStr);
        Serial.println("Saving primaryDNS: " + primaryDNSStr);
        
        String secondaryDNSStr = networkConfig_.secondaryDNS.toString();
        file.println("secondaryDNS=" + secondaryDNSStr);
        Serial.println("Saving secondaryDNS: " + secondaryDNSStr);
        
        file.close();
        Serial.println("Network settings saved successfully.");
        Serial.println("File size: " + String(SPIFFS.open("/network_settings.txt", "r").size()) + " bytes");
    } else {
        Serial.println("Error saving network settings to SPIFFS");
    }
}

char* SukenESPWiFi::getMAC() const {
    static char baseMacChr[18] = {0};
    uint8_t mac_base[6] = {0};

    if (esp_efuse_mac_get_default(mac_base) == ESP_OK) {
        sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac_base[0], mac_base[1], mac_base[2],
                mac_base[3], mac_base[4], mac_base[5]);
    } else {
        strcpy(baseMacChr, "00:00:00:00:00:00");
    }

    Serial.println(baseMacChr);
    return baseMacChr;
}

bool SukenESPWiFi::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String SukenESPWiFi::getLocalIP() const {
    return WiFi.localIP().toString();
}

String SukenESPWiFi::getMACAddress() const {
    return String(getMAC());
}

String SukenESPWiFi::getConnectedSSID() const {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.SSID();
    } else {
        return "未接続";
    }
}

String SukenESPWiFi::getDeviceMAC() const {
    return String(getMAC());
}

void SukenESPWiFi::clearAllSettings() {
    clearWiFiSettings();
    clearNetworkSettings();
    Serial.println("All settings cleared.");
}

void SukenESPWiFi::clearWiFiSettings() {
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        SPIFFS.remove("/wifi_credentials.txt");
        Serial.println("WiFi settings cleared.");
    } else {
        Serial.println("No WiFi settings to clear.");
    }
}

void SukenESPWiFi::clearNetworkSettings() {
    if (SPIFFS.exists("/network_settings.txt")) {
        SPIFFS.remove("/network_settings.txt");
        Serial.println("Network settings cleared.");
    } else {
        Serial.println("No network settings to clear.");
    }
    
    // デフォルト値にリセット
    networkConfig_.useStaticIP = false;
    networkConfig_.staticIP = IPAddress(192, 168, 1, 200);
    networkConfig_.gateway = IPAddress(192, 168, 1, 1);
    networkConfig_.subnet = IPAddress(255, 255, 255, 0);
    networkConfig_.primaryDNS = IPAddress(8, 8, 8, 8);
    networkConfig_.secondaryDNS = IPAddress(8, 8, 4, 4);
}

WiFiCredentials SukenESPWiFi::getStoredCredentials() const {
    WiFiCredentials credentials;
    readWiFiCredentials(credentials);
    return credentials;
}

NetworkConfig SukenESPWiFi::getNetworkConfig() const {
    return networkConfig_;
}

String SukenESPWiFi::getCurrentDNS() const {
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress dns1 = WiFi.dnsIP(0);
        IPAddress dns2 = WiFi.dnsIP(1);
        return dns1.toString() + ", " + dns2.toString();
    } else {
        return "Not connected";
    }
}

String SukenESPWiFi::getNetworkInfo() const {
    String info = "=== Network Information ===\n";
    
    // WiFi接続情報
    if (WiFi.status() == WL_CONNECTED) {
        info += "Status: Connected\n";
        info += "SSID: " + WiFi.SSID() + "\n";
        info += "IP Address: " + WiFi.localIP().toString() + "\n";
        info += "Gateway: " + WiFi.gatewayIP().toString() + "\n";
        info += "Subnet Mask: " + WiFi.subnetMask().toString() + "\n";
        info += "DNS: " + getCurrentDNS() + "\n";
        info += "Signal Strength: " + String(WiFi.RSSI()) + " dBm\n";
    } else {
        info += "Status: Not connected\n";
    }
    
    // 保存された設定情報
    info += "\n=== Stored Settings ===\n";
    WiFiCredentials stored = getStoredCredentials();
    info += "Stored SSID: " + stored.ssid + "\n";
    info += "Use Static IP: " + String(networkConfig_.useStaticIP ? "Yes" : "No") + "\n";
    if (networkConfig_.useStaticIP) {
        info += "Static IP: " + networkConfig_.staticIP.toString() + "\n";
        info += "Gateway: " + networkConfig_.gateway.toString() + "\n";
        info += "Subnet: " + networkConfig_.subnet.toString() + "\n";
        info += "Primary DNS: " + networkConfig_.primaryDNS.toString() + "\n";
        info += "Secondary DNS: " + networkConfig_.secondaryDNS.toString() + "\n";
    }
    
    // デバイス情報
    info += "\n=== Device Information ===\n";
    info += "Device Name: " + deviceName_ + "\n";
    info += "MAC Address: " + getMACAddress() + "\n";
    
    return info;
} 

void SukenESPWiFi::enableAutoSetupOnDisconnect(bool enable) { autoSetupOnDisconnect_ = enable; }
bool SukenESPWiFi::isAutoSetupOnDisconnectEnabled() const { return autoSetupOnDisconnect_; }
void SukenESPWiFi::enableAutoReconnectDuringSetup(bool enable) { autoReconnectDuringSetup_ = enable; }
bool SukenESPWiFi::isAutoReconnectDuringSetupEnabled() const { return autoReconnectDuringSetup_; }

} // namespace SukenWiFiLib 
