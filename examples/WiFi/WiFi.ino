#include <SukenESPWiFi.h>

SukenESPWiFi wifiManager("MyDevice");

void setup() {
    Serial.begin(115200);
    wifiManager.init();
    // 現在の設定を確認
    Serial.println("Stored SSID: " + wifiManager.getStoredSSID());
    Serial.println("Use Static IP: " + String(wifiManager.getUseStaticIP() ? "Yes" : "No"));
    Serial.println("Static IP: " + wifiManager.getStaticIP());
    Serial.println("Gateway: " + wifiManager.getGateway());
    Serial.println("Primary DNS: " + wifiManager.getPrimaryDNS());
    
    // 包括的なネットワーク情報を表示
    Serial.println(wifiManager.getNetworkInfo());
}

void loop() {
}