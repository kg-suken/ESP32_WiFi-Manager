# SukenESPWiFi ライブラリ

> [!NOTE]
> **変更点（互換ポリシー）**
> 
> - 既存スケッチの大半はそのまま動作します。保存済み設定の参照APIのみ、構造体ベースに統合されました（`getStoredCredentials()`, `getNetworkConfig()`）。
> - 設定保存後は再起動せずにライブ適用されます（接続成功でAP停止、失敗でAP継続）。
> - 切断時の自動セットアップ移行と、セットアップ中の自動再接続はデフォルトで有効です（無効化可能）。

ESP32用の簡単WiFi設定ライブラリです。1行のインクルードと1行の初期化で、美しいWebインターフェースによるWiFi設定機能を提供します。


![IMG_20250730_191956](https://github.com/user-attachments/assets/79ce0459-1ba0-4d7c-90df-20e2aa88c774)




## 特徴

- **簡単使用**: 1行のインクルードと1行の初期化
- **美しいUI**: レスポンシブデザインのWebインターフェース
- **自動WiFi検出**: 周囲のWiFiネットワークを自動スキャン
- **詳細設定対応**: 静的IP、ゲートウェイ、DNS設定が可能
- **設定管理機能**: Serialコマンドによる設定のクリア・参照
- **セキュア**: 設定は内部メモリにのみ保存
- **キャプティブポータル**: WiFi未設定時に自動的に設定ページを表示
- **mDNS対応**: APモード、クライアントモードの両方で `デバイス名.local` のアドレスでアクセス可能
- **DHCPホスト名**: ルーターに表示されるデバイス名を `init()` で設定した名前に自動設定
- **動的デバイス名**: カスタマイズ可能なデバイス名（英数字とハイフンのみ）



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
```cpp
#include <SukenESPWiFi.h>

// ライブラリをインクルードするだけで、"SukenWiFi"という
// グローバルオブジェクトが自動的に利用可能になります。

void setup() {
  Serial.begin(115200);
  
  // init()にデバイス名を渡すことで、APモード時の名前や、
  // ルーターに表示されるホスト名、mDNS名 (MyDevice.local) を設定できます。
  // ※デバイス名に使えるのは英数字とハイフンのみです。
  // 1) 非ブロッキング（デフォルト）: 並行タスクを続けながら接続を待つ
  SukenWiFi.init("MyDevice");
  // 2) ブロッキング: 設定/接続が完了するまで待つ
  // SukenWiFi.initBlocking("MyDevice");

  // オプション: 自動動作の制御
  // 切断時に自動でセットアップへ入る（デフォルト: 有効）
  // SukenWiFi.enableAutoSetupOnDisconnect(false);
  // セットアップ中に保存済みWiFiへ自動再接続を試行（デフォルト: 有効）
  // SukenWiFi.enableAutoReconnectDuringSetup(false);
}

void loop() {
  // 接続状態を確認
  if (SukenWiFi.isConnected()) {
    Serial.println("WiFi接続済み");
    Serial.println("SSID: " + SukenWiFi.getConnectedSSID());
    Serial.println("IP: " + SukenWiFi.getLocalIP());
    Serial.println("MAC: " + SukenWiFi.getDeviceMAC());
  } else {
    Serial.println("WiFi未接続 - APモードで動作中");
    Serial.println("デバイスMAC: " + SukenWiFi.getDeviceMAC());
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
// 保存された設定を参照（構造体で取得）
auto creds = SukenWiFi.getStoredCredentials();
auto cfg = SukenWiFi.getNetworkConfig();

Serial.println("Stored SSID: " + creds.ssid);
Serial.println("Use Static IP: " + String(cfg.useStaticIP ? "Yes" : "No"));
Serial.println("Static IP: " + cfg.staticIP.toString());
Serial.println("Gateway: " + cfg.gateway.toString());
Serial.println("Primary DNS: " + cfg.primaryDNS.toString());
```

## API リファレンス

### コンストラクタ
```cpp
// ライブラリによってグローバルオブジェクト SukenWiFi が提供されるため、
// 利用者がコンストラクタを直接呼び出す必要はありません。
```

### 基本メソッド

#### `void init(const String& deviceName = "")`
WiFi設定システムを初期化します。引数でデバイス名（ホスト名、mDNS名）を指定できます。デバイス名は英数字とハイフンのみ使用可能です。

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

### コールバック
特定のイベントが発生した際に、任意の関数を呼び出すためのコールバック機能を提供します。

#### `void onEnterSetupMode(std::function<void()> callback)`
WiFiへの接続に失敗し、セットアップモード（APモード）に移行する際に呼び出されるコールバック関数を登録します。デバイスの状態をLEDなどでユーザーに通知するのに便利です。
```cpp
void setupModeCallback() {
  // 例: LEDを点滅させて設定モードであることを示す
  Serial.println("Entering setup mode.");
}

void setup() {
  SukenWiFi.onEnterSetupMode(setupModeCallback);
  SukenWiFi.init("MyDevice");
}
```

#### `void onClientConnect(std::function<void()> callback)`
デバイスがAPモードで動作しているときに、スマートフォンなどのクライアントがそのAPに接続した際に呼び出されるコールバック関数を登録します。
```cpp
void clientConnectedCallback() {
  Serial.println("Client connected to the AP!");
}

void setup() {
  SukenWiFi.onClientConnect(clientConnectedCallback);
  SukenWiFi.init("MyDevice");
}
```

#### `void onConnected(std::function<void()> callback)`
WiFi が IP を取得したタイミング（初回接続/再接続を問わず）で呼び出されます。サービス起動などの汎用処理に向いています。
```cpp
void onConnectedCb() {
  Serial.println("Connected (GOT_IP)");
}

void setup() {
  SukenWiFi.onConnected(onConnectedCb);
  SukenWiFi.init("MyDevice");
}
```

#### `void onReconnected(std::function<void()> callback)`
一度接続済みになった後に切断→再接続したときだけ呼び出されます。MQTT再購読やセッション復元などに適しています。
```cpp
void onReconnectedCb() {
  Serial.println("Reconnected to WiFi");
}

void setup() {
  SukenWiFi.onReconnected(onReconnectedCb);
  SukenWiFi.init("MyDevice");
}
```

### 設定管理メソッド

#### `void clearAllSettings()`
すべての設定ファイルを削除します。

#### `void clearWiFiSettings()`
WiFi設定ファイルのみを削除します。

#### `void clearNetworkSettings()`
ネットワーク設定ファイルのみを削除します。

### 設定参照メソッド

#### `WiFiCredentials getStoredCredentials() const`
保存されたSSID/パスワードを構造体で取得します。

#### `NetworkConfig getNetworkConfig() const`
保存されたネットワーク設定（静的IP、ゲートウェイ、DNS など）を構造体で取得します。

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
   - WebサーバーとDNSサーバーを起動（キャプティブポータル）
   - mDNSを起動 (`デバイス名.local`)
   - （デフォルト）5秒ごとに保存済みWiFiへの再接続を試行。成功するとAPを停止

3. **WiFi接続成功時**:
   - クライアントとしてネットワークに参加
   - mDNSを起動 (`デバイス名.local`)

4. **ユーザーアクセス時（APモード）**:
   - ブラウザでAPに接続
   - 自動的に設定ページが表示
   - 周囲のWiFi一覧が表示される

5. **設定保存時（再起動なしのライブ適用）**:
   - ユーザーがSSIDとパスワードを入力
   - 詳細設定も含めて内部メモリに保存
   - そのまま接続を試行（APは一時的に維持: WIFI_AP_STA）
   - 接続に成功するとAPポータルを停止して `WIFI_STA` に移行
   - 接続に失敗するとAPポータルを継続（設定をやり直し可能）

## カスタマイズ

### デバイス名の変更
`setup()`内で`init()`を呼び出す際に、引数として名前を渡します。
```cpp
void setup() {
  SukenWiFi.init("MyCustomDevice");
}
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
