# SukenESPWiFi ライブラリ

ESP32用の簡単WiFi設定ライブラリです。1行のインクルードと1行の初期化で、美しいWebインターフェースによるWiFi設定機能を提供します。


![IMG_20250730_191956](https://github.com/user-attachments/assets/79ce0459-1ba0-4d7c-90df-20e2aa88c774)




## 特徴

- **簡単使用**: 1行のインクルードと1行の初期化
- **美しいUI**: レスポンシブデザインのWebインターフェース
- **自動WiFi検出**: 周囲のWiFiネットワークを自動スキャン
- **詳細設定対応**: 静的IP、ゲートウェイ、DNS設定が可能
- **設定管理機能**: Serialコマンドによる設定のクリア・参照
- **セキュア**: 設定は内部メモリにのみ保存
- **キャプティブポータル**: WiFi接続時に自動的に設定ページを表示
- **動的デバイス名**: カスタマイズ可能なデバイス名



## インストール
### 通常の方法
ArduinoIDEの上部メニュー`スケッチ>ライブラリのインクルード>zip形式のライブラリをインストール`からインストールできます。

### または以下の方法
1. `SukenESPWiFi.h` と `SukenESPWiFi.cpp` をArduinoプロジェクトフォルダにコピー
2. 必要なライブラリをインストール：
   - ArduinoJson
   - ESP32開発ボード

## 基本的な使用方法
書き込み時にFS用の領域を確保することを忘れないでください。
### 通常の使用方法
```cpp
#include <SukenESPWiFi.h>

// デバイス名を指定してライブラリを初期化
SukenESPWiFi wifiManager("MyDevice");

void setup() {
  Serial.begin(115200);
  
  // 1行でWiFi設定を開始
  wifiManager.init();
}

void loop() {
  // 接続状態を確認
  if (wifiManager.isConnected()) {
    Serial.println("WiFi接続済み");
    Serial.println("SSID: " + wifiManager.getConnectedSSID());
    Serial.println("IP: " + wifiManager.getLocalIP());
    Serial.println("MAC: " + wifiManager.getDeviceMAC());
  } else {
    Serial.println("WiFi未接続 - APモードで動作中");
    Serial.println("デバイスMAC: " + wifiManager.getDeviceMAC());
  }
  delay(5000);
}
```

## 詳細設定機能

### Webインターフェースでの詳細設定

1. WiFi設定ページにアクセス
2. 基本的なSSIDとパスワードを入力
3. 「詳細設定」リンクをクリック
4. 「静的IPアドレスを使用」にチェック
5. 以下の項目を設定：
   - **IPアドレス**: デバイスに割り当てる静的IP
   - **ゲートウェイ**: ルーターのIPアドレス
   - **サブネットマスク**: ネットワークのサブネットマスク
   - **優先DNS**: プライマリDNSサーバー
   - **代替DNS**: セカンダリDNSサーバー

### 詳細設定の使用例
```cpp
// 保存された設定を参照
Serial.println("Stored SSID: " + wifiManager.getStoredSSID());
Serial.println("Use Static IP: " + String(wifiManager.getUseStaticIP() ? "Yes" : "No"));
Serial.println("Static IP: " + wifiManager.getStaticIP());
Serial.println("Gateway: " + wifiManager.getGateway());
Serial.println("Primary DNS: " + wifiManager.getPrimaryDNS());
```

## API リファレンス

### コンストラクタ
```cpp
SukenESPWiFi(const String& deviceName = "ESP-WiFi")
```
- `deviceName`: デバイス名（AP名として使用）

### 基本メソッド

#### `void init()`
WiFi設定システムを初期化します。

#### `bool isConnected()`
WiFi接続状態を返します。
- `true`: 接続済み
- `false`: 未接続（APモード）

#### `String getLocalIP()`
接続時のIPアドレスを返します。

#### `String getMACAddress()`
デバイスのMACアドレスを返します。

#### `String getConnectedSSID()`
接続中のWiFiのSSIDを返します。
- 接続済みの場合: 接続中のSSID名
- 未接続の場合: "未接続"

#### `String getDeviceMAC()`
デバイスのMACアドレスを返します（getMACAddress()と同じ）。

### 設定管理メソッド

#### `void clearAllSettings()`
すべての設定ファイルを削除します。

#### `void clearWiFiSettings()`
WiFi設定ファイルのみを削除します。

#### `void clearNetworkSettings()`
ネットワーク設定ファイルのみを削除します。

### 設定参照メソッド

#### `String getStoredSSID()`
保存されたSSIDを取得します。

#### `String getStoredPassword()`
保存されたパスワードを取得します。

#### `bool getUseStaticIP()`
静的IP使用フラグを取得します。

#### `String getStaticIP()`
設定された静的IPアドレスを取得します。

#### `String getGateway()`
設定されたゲートウェイを取得します。

#### `String getSubnet()`
設定されたサブネットマスクを取得します。

#### `String getPrimaryDNS()`
設定された優先DNSを取得します。

#### `String getSecondaryDNS()`
設定された代替DNSを取得します。

#### `String getCurrentDNS()`
現在使用中のDNSサーバーを取得します。

#### `String getNetworkInfo()`
包括的なネットワーク情報を取得します。




## 動作フロー

1. **起動時**:
   - SPIFFSを初期化
   - ネットワーク設定を読み込み
   - 周囲のWiFiをスキャン
   - 保存済み設定があればWiFi接続を試行

2. **WiFi接続失敗時**:
   - APモードを開始
   - Webサーバーを起動
   - DNSサーバーを起動（キャプティブポータル）

3. **ユーザーアクセス時**:
   - ブラウザでAPに接続
   - 自動的に設定ページが表示
   - 周囲のWiFi一覧が表示される

4. **設定保存時**:
   - ユーザーがSSIDとパスワードを入力
   - 詳細設定も含めて内部メモリに保存
   - ESP32が再起動
   - 保存された設定でWiFiに接続

## カスタマイズ

### デバイス名の変更
```cpp
SukenESPWiFi wifiManager("MyCustomDevice");
```

### APのIPアドレス変更
`SukenESPWiFi.cpp`の以下の行を編集：
```cpp
IPAddress apIP(192, 168, 1, 100); // デフォルトIP
```

### デフォルトネットワーク設定の変更
コンストラクタでデフォルト値を変更：
```cpp
// コンストラクタ内でデフォルト値を設定
staticIP(192, 168, 1, 200),
gateway(192, 168, 1, 1),
subnet(255, 255, 255, 0),
primaryDNS(8, 8, 8, 8),
secondaryDNS(8, 8, 4, 4)
```

## トラブルシューティング

### よくある問題

1. **コンパイルエラー**
   - 必要ライブラリがインストールされているか確認
   - ESP32開発ボードが選択されているか確認

2. **APモードで接続できない**
   - デバイス名に特殊文字が含まれていないか確認
   - 他のWiFiネットワークとの干渉がないか確認

3. **設定が保存されない**
   - SPIFFSの初期化に失敗していないか確認
   - 内部メモリの容量が不足していないか確認

4. **静的IP設定が適用されない**
   - ネットワーク設定ファイルが正しく保存されているか確認
   - IPアドレスがネットワーク範囲内か確認

5. **Serialコマンドが反応しない**
   - Serial Monitorのボーレートが115200に設定されているか確認
   - コマンドの後に改行（Enter）を送信しているか確認



## 制作
**sskrc**

---

今後の改良に関する提案やバグ報告は、お気軽にIssueを通してご連絡ください。
