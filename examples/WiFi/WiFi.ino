#include <SukenESPWiFi.h>

// Callback function to be called when a client connects to the AP
void clientConnected() {
    Serial.println("Client has connected to the AP!");
}

// Callback function to be called when entering setup mode
void enterSetupMode() {
    Serial.println("Entering setup mode.");
    // You can add code here to indicate setup mode, e.g., by blinking an LED.
}

// ライブラリがグローバルオブジェクト "SukenWiFi" を提供します。
// 自分でインスタンスを作成する必要はありません。

void setup() {
    Serial.begin(115200);

    // Register callbacks
    SukenWiFi.onClientConnect(clientConnected); 
    SukenWiFi.onEnterSetupMode(enterSetupMode);

    SukenWiFi.init("MyDevice"); // デバイス名を指定して初期化
    
    // 現在の設定を確認
    Serial.println("Stored SSID: " + SukenWiFi.getStoredSSID());
    Serial.println("Use Static IP: " + String(SukenWiFi.getUseStaticIP() ? "Yes" : "No"));
    Serial.println("Static IP: " + SukenWiFi.getStaticIP());
    Serial.println("Gateway: " + SukenWiFi.getGateway());
    Serial.println("Primary DNS: " + SukenWiFi.getPrimaryDNS());
    
    // 包括的なネットワーク情報を表示
    Serial.println(SukenWiFi.getNetworkInfo());
}

void loop() {
}

// Serialコマンドで設定をクリアする例
void serialEvent() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "clear all") {
            SukenWiFi.clearAllSettings();
            Serial.println("All settings cleared. Please restart the device.");
        } else if (command == "clear wifi") {
            SukenWiFi.clearWiFiSettings();
            Serial.println("WiFi settings cleared. Please restart the device.");
        } else if (command == "clear network") {
            SukenWiFi.clearNetworkSettings();
            Serial.println("Network settings cleared. Please restart the device.");
        }
    }
}
