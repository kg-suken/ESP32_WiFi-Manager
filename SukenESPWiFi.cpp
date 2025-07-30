#include "SukenESPWiFi.h"

SukenESPWiFi::SukenESPWiFi(const String& deviceName) : 
    DeviceName(deviceName),
    WiFiName(deviceName),
    server(80),
    apIP(192, 168, 1, 100),
    apip("192.168.1.100"),
    WiFiList(512),
    a(0),
    useStaticIP(false),
    staticIP(192, 168, 1, 200),
    gateway(192, 168, 1, 1),
    subnet(255, 255, 255, 0),
    primaryDNS(8, 8, 8, 8),
    secondaryDNS(8, 8, 4, 4) {
}

void SukenESPWiFi::init() {
    client.setInsecure();
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
    ScanWiFi();
    Serial.println("WiFiコンフィグ探知");
    
    // ネットワーク設定を読み込み
    readNetworkSettings();
    
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        Serial.println("wifi_credentials.txtが存在します");
        connectToWiFi();
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi接続失敗");
        APStart();
    }
}

void SukenESPWiFi::APStart() {
    Serial.println("APスタート");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WiFiName.c_str());
    delay(200);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    xTaskCreatePinnedToCore(WebServerTask, "WebServer", 8192, this, 2, &thp[2], 1);
}

void SukenESPWiFi::ScanWiFi() {
    int numNetworks = WiFi.scanNetworks();
    Serial.println("Scan done");

    if (numNetworks == 0) {
        Serial.println("No networks found");
    } else {
        Serial.print(numNetworks);
        Serial.println(" networks found");

        JsonArray networkList = WiFiList.createNestedArray("networks");
        for (int i = 0; i < numNetworks; ++i) {
            networkList.add(WiFi.SSID(i));
        }
    }

    serializeJsonPretty(WiFiList, Serial);
}

void SukenESPWiFi::WiFiSettingPage() {
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
    <p>デバイスをネットワークに接続するために、ご自宅のWiFiを設定してください。</p>
    <p>設定した情報は内部データベースにのみ保存されます。</p>
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

<div id="loadingIndicator">
    <p>設定を保存中...</p>
    <p>「<span id="deviceNameSpan"></span>」は再起動されます</p>
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

        document.getElementById('loadingIndicator').style.display = 'block';

        xhr.onload = function () {
            if (xhr.status == 200) {
                fetchDeviceInfo();
            } else {
                console.error('Error:', xhr.statusText);
            }
            document.getElementById('loadingIndicator').style.display = 'none';
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
                document.getElementById('deviceNameHeader').textContent = deviceName + ' WiFi設定';
                var deviceNameSpan = document.getElementById('deviceNameSpan');
                if (deviceNameSpan) deviceNameSpan.textContent = deviceName;
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
    server.send(200, "text/html", html);
}

void SukenESPWiFi::infoapi() {
    StaticJsonDocument<200> doc;
    doc["MAC"] = getMAC();
    doc["DeviceName"] = DeviceName;
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    server.send(200, "application/json", jsonPayload);
}

void SukenESPWiFi::WiFiSettingAPI() {
    delay(250);
    String body = server.arg("plain");

    Serial.println("=== Received JSON Data ===");
    Serial.println(body);
    Serial.println("==========================");

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        server.send(400, "application/json", "Failed to parse JSON");
        return;
    }

    Serial.println("JSON parsing successful");

    String ssid = doc["ssid"];
    String password = doc["password"];
    
    Serial.println("Parsed SSID: " + ssid);
    Serial.println("Parsed Password: " + password);
    
    // StaticIP設定の処理
    Serial.println("Checking for useStaticIP key...");
    if (doc.containsKey("useStaticIP")) {
        Serial.println("useStaticIP key found");
        useStaticIP = doc["useStaticIP"].as<bool>();
        Serial.println("Parsed useStaticIP: " + String(useStaticIP ? "true" : "false"));
    } else {
        Serial.println("useStaticIP key not found in JSON");
        useStaticIP = false;
    }
    
    Serial.println("Current useStaticIP value: " + String(useStaticIP ? "true" : "false"));
    
    if (useStaticIP) {
        Serial.println("Processing static IP settings...");
        
        if (doc.containsKey("staticIP")) {
            String staticIPStr = doc["staticIP"].as<String>();
            Serial.println("Raw staticIP string: " + staticIPStr);
            staticIP.fromString(staticIPStr);
            Serial.println("Parsed Static IP: " + staticIP.toString());
        } else {
            Serial.println("staticIP key not found in JSON");
        }
        
        if (doc.containsKey("gateway")) {
            String gatewayStr = doc["gateway"].as<String>();
            Serial.println("Raw gateway string: " + gatewayStr);
            gateway.fromString(gatewayStr);
            Serial.println("Parsed Gateway: " + gateway.toString());
        } else {
            Serial.println("gateway key not found in JSON");
        }
        
        if (doc.containsKey("subnet")) {
            String subnetStr = doc["subnet"].as<String>();
            Serial.println("Raw subnet string: " + subnetStr);
            subnet.fromString(subnetStr);
            Serial.println("Parsed Subnet: " + subnet.toString());
        } else {
            Serial.println("subnet key not found in JSON");
        }
        
        if (doc.containsKey("primaryDNS")) {
            String primaryDNSStr = doc["primaryDNS"].as<String>();
            Serial.println("Raw primaryDNS string: " + primaryDNSStr);
            primaryDNS.fromString(primaryDNSStr);
            Serial.println("Parsed Primary DNS: " + primaryDNS.toString());
        } else {
            Serial.println("primaryDNS key not found in JSON");
        }
        
        if (doc.containsKey("secondaryDNS")) {
            String secondaryDNSStr = doc["secondaryDNS"].as<String>();
            Serial.println("Raw secondaryDNS string: " + secondaryDNSStr);
            secondaryDNS.fromString(secondaryDNSStr);
            Serial.println("Parsed Secondary DNS: " + secondaryDNS.toString());
        } else {
            Serial.println("secondaryDNS key not found in JSON");
        }
    } else {
        // StaticIPが無効な場合、デフォルト値にリセット
        Serial.println("StaticIP is false, resetting to defaults");
        useStaticIP = false;
        staticIP = IPAddress(192, 168, 1, 200);
        gateway = IPAddress(192, 168, 1, 1);
        subnet = IPAddress(255, 255, 255, 0);
        primaryDNS = IPAddress(8, 8, 8, 8);
        secondaryDNS = IPAddress(8, 8, 4, 4);
        Serial.println("Static IP disabled, using DHCP");
    }
    
    Serial.println("=== Final Settings ===");
    Serial.println("useStaticIP: " + String(useStaticIP ? "true" : "false"));
    Serial.println("staticIP: " + staticIP.toString());
    Serial.println("gateway: " + gateway.toString());
    Serial.println("subnet: " + subnet.toString());
    Serial.println("primaryDNS: " + primaryDNS.toString());
    Serial.println("secondaryDNS: " + secondaryDNS.toString());
    Serial.println("=====================");
    
    Serial.println("About to save settings...");
    saveWiFiCredentials(ssid, password);
    saveNetworkSettings();
    Serial.println("Settings saved, sending response...");

    server.send(200, "application/json", "{\"message\":\"WiFi settings updated\"}");
    Serial.println("Response sent");
    
    // レスポンス送信後に少し待ってから再起動
    delay(1000);
    Serial.println("Restarting ESP32...");
    ESP.restart();
}

void SukenESPWiFi::WiFiListAPI() {
    String json;
    serializeJsonPretty(WiFiList, json);
    server.send(200, "application/json", json);
}

void WebServerTask(void* args) {
    SukenESPWiFi* instance = (SukenESPWiFi*)args;
    instance->WebServer(args);
}

void SukenESPWiFi::WebServer(void* args) {
    SukenESPWiFi* instance = (SukenESPWiFi*)args;
    
    Serial.print("mDNS server instancing");
    if (!MDNS.begin(instance->WiFiName)) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(100);
        }
    }
    Serial.println("mDNSを開始しました");
    MDNS.addService("http", "tcp", 80);
    instance->dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    instance->dnsServer.setTTL(300);
    instance->dnsServer.start(53, "*", instance->apIP);
    Serial.println("DNSサーバーを開始しました");
    
    instance->server.on("/", [instance]() { instance->WiFiSettingPage(); });
    instance->server.on("/api/info", [instance]() { instance->infoapi(); });
    instance->server.onNotFound([instance]() { instance->WiFiSettingPage(); });
    instance->server.on("/WiFiSetting", [instance]() { instance->WiFiSettingPage(); });
    instance->server.sendHeader("Access-Control-Allow-Origin", "*");
    instance->server.sendHeader("Access-Control-Max-Age", "10000");
    instance->server.sendHeader("Content-Length", "0");
    instance->server.on("/api/WiFiSetting", HTTP_POST, [instance]() { instance->WiFiSettingAPI(); });
    instance->server.on("/api/WiFiList", [instance]() { instance->WiFiListAPI(); });
    instance->server.begin();
    Serial.println("Webサーバー開始");
    
    while (1) {
        instance->dnsServer.processNextRequest();
        instance->server.handleClient();
        instance->a++;
        delay(1);
    }
}

void SukenESPWiFi::connectToWiFi() {
    String ssid;
    String password;
    WiFi.mode(WIFI_STA);
    readWiFiCredentials(ssid, password);
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + password);
    Serial.println("SSID Length: " + String(ssid.length()));
    Serial.println("Password Length: " + String(password.length()));
    
    // 静的IP設定を適用（WiFi.begin()の前に実行）
    if (useStaticIP) {
        Serial.println("Applying static IP configuration...");
        Serial.println("Static IP: " + staticIP.toString());
        Serial.println("Gateway: " + gateway.toString());
        Serial.println("Subnet: " + subnet.toString());
        Serial.println("Primary DNS: " + primaryDNS.toString());
        Serial.println("Secondary DNS: " + secondaryDNS.toString());
        
        if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
            Serial.println("Static IP configuration failed");
        } else {
            Serial.println("Static IP configuration applied successfully");
        }
    } else {
        Serial.println("Using DHCP configuration");
    }
    
    WiFi.begin(ssid, password);
    WiFi.setHostname(WiFiName.c_str());
    
    Serial.println("Connecting to WiFi...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.println("Gateway: " + WiFi.gatewayIP().toString());
        Serial.println("Subnet Mask: " + WiFi.subnetMask().toString());
        Serial.println("DNS: " + WiFi.dnsIP().toString());
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

void SukenESPWiFi::readWiFiCredentials(String& ssid, String& password) {
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
                    ssid = value;
                } else if (key.equals("Password")) {
                    password = value;
                }
            }
        }
        file.close();
        Serial.println("WiFi credentials read successfully.");
    } else {
        Serial.println("Error reading WiFi credentials from SPIFFS");
    }
}

void SukenESPWiFi::saveWiFiCredentials(const String& ssid, const String& password) {
    Serial.println("Saving WiFi credentials...");
    
    // ファイルが存在する場合は削除
    if (SPIFFS.exists("/wifi_credentials.txt")) {
        SPIFFS.remove("/wifi_credentials.txt");
        Serial.println("Removed existing wifi_credentials.txt");
    }
    
    File file = SPIFFS.open("/wifi_credentials.txt", "w");
    if (file) {
        file.println("SSID=" + ssid);
        file.println("Password=" + password);
        file.close();
        Serial.println("WiFi credentials saved successfully.");
        Serial.println("File size: " + String(SPIFFS.open("/wifi_credentials.txt", "r").size()) + " bytes");
        // ESP.restart()を削除 - WiFiSettingAPI()の最後で呼び出す
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
                        useStaticIP = (value == "true");
                        Serial.println("useStaticIP: " + String(useStaticIP ? "true" : "false"));
                    } else if (key.equals("staticIP")) {
                        staticIP.fromString(value);
                        Serial.println("staticIP: " + staticIP.toString());
                    } else if (key.equals("gateway")) {
                        gateway.fromString(value);
                        Serial.println("gateway: " + gateway.toString());
                    } else if (key.equals("subnet")) {
                        subnet.fromString(value);
                        Serial.println("subnet: " + subnet.toString());
                    } else if (key.equals("primaryDNS")) {
                        primaryDNS.fromString(value);
                        Serial.println("primaryDNS: " + primaryDNS.toString());
                    } else if (key.equals("secondaryDNS")) {
                        secondaryDNS.fromString(value);
                        Serial.println("secondaryDNS: " + secondaryDNS.toString());
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

void SukenESPWiFi::saveNetworkSettings() {
    Serial.println("Saving network settings...");
    
    // ファイルが存在する場合は削除
    if (SPIFFS.exists("/network_settings.txt")) {
        SPIFFS.remove("/network_settings.txt");
        Serial.println("Removed existing network_settings.txt");
    }
    
    File file = SPIFFS.open("/network_settings.txt", "w");
    if (file) {
        Serial.println("Saving network settings to SPIFFS...");
        
        String useStaticIPStr = String(useStaticIP ? "true" : "false");
        file.println("useStaticIP=" + useStaticIPStr);
        Serial.println("Saving useStaticIP: " + useStaticIPStr);
        
        String staticIPStr = staticIP.toString();
        file.println("staticIP=" + staticIPStr);
        Serial.println("Saving staticIP: " + staticIPStr);
        
        String gatewayStr = gateway.toString();
        file.println("gateway=" + gatewayStr);
        Serial.println("Saving gateway: " + gatewayStr);
        
        String subnetStr = subnet.toString();
        file.println("subnet=" + subnetStr);
        Serial.println("Saving subnet: " + subnetStr);
        
        String primaryDNSStr = primaryDNS.toString();
        file.println("primaryDNS=" + primaryDNSStr);
        Serial.println("Saving primaryDNS: " + primaryDNSStr);
        
        String secondaryDNSStr = secondaryDNS.toString();
        file.println("secondaryDNS=" + secondaryDNSStr);
        Serial.println("Saving secondaryDNS: " + secondaryDNSStr);
        
        file.close();
        Serial.println("Network settings saved successfully.");
        Serial.println("File size: " + String(SPIFFS.open("/network_settings.txt", "r").size()) + " bytes");
    } else {
        Serial.println("Error saving network settings to SPIFFS");
    }
}

char* SukenESPWiFi::getMAC() {
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

bool SukenESPWiFi::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String SukenESPWiFi::getLocalIP() {
    return WiFi.localIP().toString();
}

String SukenESPWiFi::getMACAddress() {
    return String(getMAC());
}

String SukenESPWiFi::getConnectedSSID() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.SSID();
    } else {
        return "未接続";
    }
}

String SukenESPWiFi::getDeviceMAC() {
    return String(getMAC());
}

// 設定管理関数
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
    useStaticIP = false;
    staticIP = IPAddress(192, 168, 1, 200);
    gateway = IPAddress(192, 168, 1, 1);
    subnet = IPAddress(255, 255, 255, 0);
    primaryDNS = IPAddress(8, 8, 8, 8);
    secondaryDNS = IPAddress(8, 8, 4, 4);
}

// 設定参照関数
String SukenESPWiFi::getStoredSSID() {
    String ssid, password;
    readWiFiCredentials(ssid, password);
    return ssid;
}

String SukenESPWiFi::getStoredPassword() {
    String ssid, password;
    readWiFiCredentials(ssid, password);
    return password;
}

bool SukenESPWiFi::getUseStaticIP() {
    return useStaticIP;
}

String SukenESPWiFi::getStaticIP() {
    return staticIP.toString();
}

String SukenESPWiFi::getGateway() {
    return gateway.toString();
}

String SukenESPWiFi::getSubnet() {
    return subnet.toString();
}

String SukenESPWiFi::getPrimaryDNS() {
    return primaryDNS.toString();
}

String SukenESPWiFi::getSecondaryDNS() {
    return secondaryDNS.toString();
}

String SukenESPWiFi::getCurrentDNS() {
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress dns1 = WiFi.dnsIP(0);
        IPAddress dns2 = WiFi.dnsIP(1);
        return dns1.toString() + ", " + dns2.toString();
    } else {
        return "Not connected";
    }
}

String SukenESPWiFi::getNetworkInfo() {
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
    info += "Stored SSID: " + getStoredSSID() + "\n";
    info += "Use Static IP: " + String(useStaticIP ? "Yes" : "No") + "\n";
    if (useStaticIP) {
        info += "Static IP: " + getStaticIP() + "\n";
        info += "Gateway: " + getGateway() + "\n";
        info += "Subnet: " + getSubnet() + "\n";
        info += "Primary DNS: " + getPrimaryDNS() + "\n";
        info += "Secondary DNS: " + getSecondaryDNS() + "\n";
    }
    
    // デバイス情報
    info += "\n=== Device Information ===\n";
    info += "Device Name: " + DeviceName + "\n";
    info += "MAC Address: " + getMACAddress() + "\n";
    
    return info;
} 