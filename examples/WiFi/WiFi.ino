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
    
    // 現在の設定を確認（構造体で取得）
    auto creds = SukenWiFi.getStoredCredentials();
    auto cfg = SukenWiFi.getNetworkConfig();
    Serial.println("Stored SSID: " + creds.ssid);
    Serial.println(String("Use Static IP: ") + (cfg.useStaticIP ? "Yes" : "No"));
    Serial.println("Static IP: " + cfg.staticIP.toString());
    Serial.println("Gateway: " + cfg.gateway.toString());
    Serial.println("Primary DNS: " + cfg.primaryDNS.toString());
    
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
            Serial.println("All settings cleared. You can reconfigure via AP portal.");
        } else if (command == "clear wifi") {
            SukenWiFi.clearWiFiSettings();
            Serial.println("WiFi settings cleared. You can reconfigure via AP portal.");
        } else if (command == "clear network") {
            SukenWiFi.clearNetworkSettings();
            Serial.println("Network settings cleared. You can reconfigure via AP portal.");
        }
    }
}
