# Stack-chan Multi-Device Controller

[日本語](README.md) | [English](README.en.md)

M5Stack CoreS3 ベースのｽﾀｯｸﾁｬﾝ、M5Stack StopWatch、M5Stack AtomS3R Chatbot 向けの
ファームウェアです。表情表示、マイク/スピーカー音声ストリーミング、なでなで/ふりふり反応、
StreetPass、外部制御用の HTTP / WebSocket / USB Serial インターフェースを扱います。
CoreS3 ではサーボ動作とカメラ撮影も扱います。

このリポジトリに含めているのはデバイス本体側のファームウェアだけです。
README では、本体が公開する HTTP / WebSocket / USB Serial の口を記載しています。
外部クライアント側の実装方法は対象外です。

## できること

- PlatformIO / Arduino でビルドする CoreS3 / StopWatch / AtomS3R Chatbot ファームウェア
- LittleFS 上の JPG 表情画像を各デバイスの画面サイズで表示
- WebSocket JSON による状態制御
- WebSocket binary による PCM 音声再生とマイク音声送信
- Android 直結向けの USB CDC / USB Serial 制御チャンネル
- HTTP によるステータス取得、CoreS3 でのカメラ撮影
- CoreS3 の表情画面上のカメラボタンからクライアントへ撮影要求イベントを送信
- BLE による Stack-chan 同士のすれ違い通信
- すれ違いプロフィール、直近 30 件の履歴、未読通知、外部アプリ同期 API
- Wi-Fi STA 接続、SoftAP 直接接続、USB Serial 接続
- ブラウザからの Wi-Fi 設定、SSID スキャン、複数 Wi-Fi 保存
- 本体側のなでなで/ふりふり反応。反応方法はデバイスごとに異なります
- WebSocket イベントによる好感度管理
- なでなで、ふりふり、カメラボタン、接続開始、好感度レベル変化の本体側 interaction event
- Network / Display / Audio / Power / StreetPass を切り替えられる本体設定画面
- バッテリー、マイク、好感度、熱状態、低電力状態の画面オーバーレイ

## 必要なもの

- PlatformIO 開発環境
- 対象デバイス:

| デバイス | 追加で必要なもの |
| --- | --- |
| M5Stack CoreS3 + ｽﾀｯｸﾁｬﾝ | Stack-chan 互換のサーボ構成 |
| M5Stack StopWatch | なし |
| M5Stack AtomS3R Chatbot | Atomic Echo Base |

PlatformIO の環境名:

| デバイス | env | LittleFS 画像ディレクトリ |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `m5stack-cores3` | `data/` |
| StopWatch | `m5stack-stopwatch` | `data_stopwatch/` |
| AtomS3R Chatbot | `m5stack-atoms3r-chatbot` | `data_atoms3r/` |

機種別のビルド方法、操作方法、非対応機能は
[対応デバイス別ガイド](docs/devices.ja.md) を参照してください。

## PlatformIO の準備

VS Code の PlatformIO 拡張、または PlatformIO Core CLI を使います。

`pio` コマンドが使えることを確認してください。

```sh
pio --version
```

macOS で PlatformIO は入っているのに `pio` が見つからない場合は、次の場所にあることがあります。

```sh
~/.platformio/penv/bin/pio --version
```

## リポジトリ構成

必須:

- `platformio.ini`: PlatformIO のビルド設定
- `boards/`: PlatformIO に未収録のボード定義
- `src/`: ファームウェア本体
- `data/`: CoreS3 用 LittleFS 表情画像。デフォルトの JPG を同梱しています
- `data_stopwatch/`: StopWatch 用 LittleFS 表情画像
- `data_atoms3r/`: AtomS3R Chatbot 用 LittleFS 表情画像
- `docs/devices.ja.md`: デバイス別ビルド・操作ガイド
- `docs/device_affection_api.ja.md`: 本体側の好感度 API 詳細
- `docs/usb_serial_protocol.ja.md`: アプリ向け USB Serial frame protocol 詳細
- `docs/streetpass_protocol.ja.md`: すれ違い通信 BLE / JSON API 詳細

任意:

- `.vscode/extensions.json`: VS Code の推奨拡張
- `tools/face_image_builder/`: 画像生成AIのスプライトシートから表情画像一式を作るツール

GitHub に上げないもの:

- `.pio/`: ビルド生成物とダウンロードされた依存ライブラリ
- `.DS_Store`: macOS のメタデータ
- `src/config_private.h`: ローカル Wi-Fi 認証情報
- `assets/`: ローカルの画像素材作業フォルダ

## セットアップ手順

1. PlatformIO をインストールし、`pio --version` が動くことを確認します。
2. ローカル設定ファイルを作ります。

```sh
cp src/config_private.example.h src/config_private.h
```

3. 必要に応じて `src/config_private.h` に Wi-Fi 情報を書きます。
   未設定のままでも、起動後にブラウザの Wi-Fi 設定画面から登録できます。

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
```

4. 後述の表情画像を `.jpg` で対象デバイスの画像ディレクトリに配置します。
   CoreS3 は `data/`、StopWatch は `data_stopwatch/`、AtomS3R Chatbot は `data_atoms3r/` を使います。
5. 対象デバイスを USB で接続します。
6. 必要に応じて、ビルドだけ確認します。`<env>` は対象デバイスの環境名に置き換えてください。

```sh
pio run -e <env>
```

7. ファームウェアを書き込みます。

```sh
pio run -e <env> -t upload
```

8. LittleFS の画像データを書き込みます。

```sh
pio run -e <env> -t uploadfs
```

9. シリアルモニタを開きます。

```sh
pio device monitor -e <env>
```

起動ログに Wi-Fi モードと IP アドレスが表示されます。
初回書き込み時は `upload` のあとに `uploadfs` を実行してください。コードだけ変更した場合は
`upload`、表情画像だけ変更した場合は `uploadfs` だけで足ります。

配布用 `.bin` のインストール手順は、[バイナリ版インストール手順](docs/install_binary.ja.md) を参照してください。
配布用ビルドでは `STACKCHAN_RELEASE_BUILD` を指定し、`src/config_private.h` の個人 Wi-Fi 情報を含めないでください。
GitHub Releases 用のデバイス別 asset は次のコマンドで `dist/` に生成します。
配布物には通常フォルダの画像を含めるため、`STACKCHAN_FACE_DATA_DIR` は指定しません。

```sh
bash scripts/build_release_bins.sh all
```

生成されるファイル名は [バイナリ版インストール手順](docs/install_binary.ja.md) の配布物一覧と一致します。

## 表情画像の配置

ファームウェアは `src/config.h` に定義されたパスで LittleFS から画像を読みます。
読み込むファイル名は全デバイス共通ですが、アップロード元ディレクトリと推奨サイズは
デバイスごとに異なります。

| デバイス | アップロード元 | 推奨サイズ |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `data/` | 240 x 240 |
| StopWatch | `data_stopwatch/` | 386 x 386 |
| AtomS3R Chatbot | `data_atoms3r/` | 128 x 128 |

通常の `pio run -e <env> -t uploadfs` は上表の通常フォルダを使います。
`data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/` など別の画像フォルダを使う場合は、
ビルド時に明示してください。

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_atoms3r_local pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

実機に置く画像ファイルは `.jpg` に統一しています。`src/config.h` の表情パスは後方互換で
`.png` 名を使っていますが、ファームウェア側で同名の `.jpg` に解決します。
完成ディレクトリは、基本顔48枚に加えて `dir0..dir16`、`blink0..blink16`、
`pet_anim_0..pet_anim_8`、`dizzy_01..dizzy_15` を含む106 JPG構成です。

自分の画像に差し替える場合は、次のファイル名で対象デバイスの画像ディレクトリに置いてください。

画像生成AIで 6x6 のスプライトシートを作って表情画像を生成する場合は、
[`tools/face_image_builder/`](tools/face_image_builder/) を使えます。プロンプト、グリッドテンプレート、
スプライトシート分割CLI、対象デバイスの画像ディレクトリへの配置手順をまとめています。

`assets/` 配下のフォルダは素材作業用で、ファームウェアからは直接読みません。
実機で使う画像は、下表のファイル名へリネームして対象デバイスの画像ディレクトリ直下に置き、
`pio run -e <env> -t uploadfs` で LittleFS に書き込んでください。
画像セットの整理方針は `docs/face_image_inventory.ja.md` にまとめています。

| ファイル名 | 置いてほしい画像 | つくよみちゃん万能立ち絵素材を使う場合の例 |
| --- | --- | --- |
| `idle.png` | 通常待機顔。口閉じ。 | `01-01a 基本-目ふんわり-口閉じ.png` |
| `listen.png` | 聞き取り中/待ち受け顔。口閉じ。 | `01-02a 基本-目ぱっちり-口閉じ.png` |
| `talk_0.png` | 通常発話の口閉じフレーム。 | `01-01a 基本-目ふんわり-口閉じ.png` |
| `talk_1.png` | 通常発話の口開けフレーム。 | `01-01b 基本-目ふんわり-口開け.png` |
| `blink.png` | 通常まばたきフレーム。 | `01-01c 基本-目ふんわり-目閉じ.png` |
| `smile.png` | 通常好感度 tier の待機中にだけ出るにっこり顔。 | `01-03ac 基本-にっこり-口閉じ.png` |
| `good_0.png` | ポジティブ/マスター認識時の口閉じ顔。 | `02-04a 喜び・称賛-口閉じ.png` |
| `good_1.png` | ポジティブ/マスター認識時の口開け顔。 | `02-04b 喜び・称賛-口開け.png` |
| `good_blink.png` | ポジティブ/マスター認識時のまばたき顔。 | `02-04c 喜び・称賛-目閉じ.png` |
| `bad_0.png` | ネガティブ/not_master 時の口閉じ顔。 | `10-03ac 覚悟-口閉じ.png` |
| `bad_1.png` | ネガティブ/not_master 時の口開け顔。 | `10-03b 覚悟-口開け.png` |
| `photo_0.png` | 撮影/photo モードの口閉じ顔。 | `09-01a 祈り-口閉じ.png` |
| `photo_1.png` | 撮影/photo モードの口開け顔。 | `09-01b 祈り-口開け.png` |
| `photo_blink.png` | 撮影/photo モードの目閉じ・口閉じ顔。 | `09-01c 祈り-目閉じ・口閉じ.png` |
| `photo_blink_talk.png` | 撮影/photo モードの目閉じ・口開け顔。 | `09-01d 祈り-目閉じ・口開け.png` |
| `photo_master_0.png` | マスター撮影/photo_master モードの口閉じ顔。 | `02-02ac ほんわか-手を組む-口閉じ.png` |
| `photo_master_1.png` | マスター撮影/photo_master モードの口開け顔。 | `02-02b ほんわか-手を組む-口開け.png` |
| `nadenade_0.png` | なでなで/pet モードの口閉じ顔。 | `02-01ac ほんわか-基本ポーズ-口閉じ.png` |
| `nadenade_1.png` | なでなで/pet モードの口開け顔。 | `02-01b ほんわか-基本ポーズ-口開け.png` |
| `furifuri_0.png` | ふりふり/shake モードの口閉じ顔。 | `07-01ac 大変です！-口閉じ.png` |
| `furifuri_1.png` | ふりふり/shake モードの口開け顔。 | `07-01b 大変です！-口開け.png` |

`_1.png` で終わる口開け画像は発話アニメーションで使います。対応する `_0.png` は
口閉じフレームで、そのモードの基本顔としても使います。

より細かい状態表示を使う場合は、次の追加画像も配置できます。存在しない場合、
ファームウェアは上記の基本表情へフォールバックします。
`talk_guarded_0.png` と `talk_attached_0.png` は、口閉じ発話フレームを
idle 顔と分けたい場合だけ用意してください。存在しない場合は、それぞれ
`idle_guarded_0.png` / `idle_attached_0.png` を口閉じフレームとして使います。

| グループ | ファイル名 |
| --- | --- |
| 警戒寄りの好感度 tier | `idle_guarded_0.png`, `blink_guarded_0.png`, `talk_guarded_0.png`, `talk_guarded_1.png` |
| 親密寄りの好感度 tier | `idle_attached_0.png`, `blink_attached_0.png`, `talk_attached_0.png`, `talk_attached_1.png` |
| 警戒寄りのなでなで | `pet_guarded_0.png`, `pet_guarded_1.png`, `pet_blink_guarded_0.png` |
| 親密寄りのなでなで | `pet_attached_0.png`, `pet_attached_1.png`, `pet_blink_attached_0.png` |
| 警戒寄りのふりふり | `shake_guarded_0.png`, `shake_guarded_1.png` |
| 親密寄りのふりふり | `shake_attached_0.png`, `shake_attached_1.png` |
| 熱状態と低電力 | `tired_0.png`, `tired_talk.png`, `tired_blink.png`, `exhausted_0.png`, `exhausted_talk.png`, `exhausted_blink.png`, `low_power_0.png`, `low_power_talk.png`, `low_power_blink.png` |

このリポジトリには、`tools/face_image_builder/` のワークフローで生成したデフォルトJPGを同梱しています。
上表の「つくよみちゃん万能立ち絵素材を使う場合の例」は、手動で画像を用意する場合の参考です。
つくよみちゃん素材そのものはこのリポジトリには含めていません。利用する場合は公式配布ページから素材を入手し、
利用規約に従って対象デバイスの推奨サイズで JPG を書き出し、対象デバイスの画像ディレクトリに配置してください。

`data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/` はローカル差し替え用で、
GitHub Releases の配布物には含めません。ローカル画像セットでつくよみちゃん素材を使う場合のクレジット:

```text
フリー素材キャラクター「つくよみちゃん」（© Rei Yumesaki）
https://tyc.rei-yumesaki.net/
つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

## ネットワークモード

ファームウェアには 2 つの接続モードがあります。

- STA モード: 保存済み Wi-Fi 設定、または `src/config_private.h` の Wi-Fi に接続します。
- SoftAP モード: CoreS3 自身がアクセスポイントになります。

SoftAP の初期値:

| デバイス | SSID | Password | IP |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `192.168.4.1` |
| StopWatch | `StopWatch` | `stopwatch123` | `192.168.4.1` |
| AtomS3R | `AtomS3R` | `atoms3r123` | `192.168.4.1` |

Network 画面で、CoreS3 / StopWatch は画面長押し、AtomS3R Chatbot は BtnA 長押しをすると
STA と SoftAP を切り替えます。CoreS3 / StopWatch は切り替え後に再起動し、AtomS3R Chatbot は
その場で接続を切り替えます。選択したモードは本体に保存されます。

## Wi-Fi 設定ページ

Wi-Fi 未設定の状態で STA 接続を開始すると、本体は自動で SoftAP モードに切り替わります。
スマホまたは PC を対象デバイスの SoftAP SSID に接続し、ブラウザで次の URL を開いてください。

```text
http://192.168.4.1/wifi
```

設定ページでは、周辺 SSID のスキャン、SSID/password の保存、保存済み Wi-Fi の編集、
削除、優先度変更ができます。保存済み Wi-Fi は上から順に接続を試します。
STA 接続済みの場合は、本体の Network 画面に表示される IP アドレスの `/wifi` からも
同じ設定ページを開けます。

本体の Network 画面では、QR コードから Wi-Fi 設定ページを開けます。

- SoftAP モード: `Wi-Fi QR` で対象デバイスの SoftAP に接続し、続けて `Setup QR` で
  `http://192.168.4.1/wifi` を開きます。
- STA 接続済み: `Setup QR` で `http://<本体のIP>/wifi` を開きます。スマホや PC は
  本体と同じ Wi-Fi に接続している必要があります。
- QR 表示画面をタップすると Network 画面に戻ります。

## 本体操作

- 本体操作はデバイスごとに異なります。詳細は [対応デバイス別ガイド](docs/devices.ja.md) を参照してください。
- CoreS3 ではタッチ画面の端からフリックすると、設定画面の表示/非表示を切り替えます。
- 設定画面には Network / Display / Audio / Servo / Pwr のタブがあります。
- Servo タブはサーボ搭載の CoreS3 で使います。StopWatch / AtomS3R Chatbot では表示されません。
- Wi-Fi 未設定時は、本体がデバイスごとの設定用 Wi-Fi を出します。
  スマホや PC をそこへ接続し、`http://192.168.4.1/wifi` を開くと Wi-Fi 設定ができます。
- Network 設定ページでは、CoreS3 / StopWatch は画面長押し、AtomS3R Chatbot は BtnA 長押しで STA と SoftAP を切り替えます。
- Display 設定では、明るさと画面オフを操作できます。AtomS3R Chatbot には Display ページはありません。
- Audio 設定では、スピーカー音量を操作できます。AtomS3R Chatbot は Audio 画面で BtnA ダブルクリックすると音量調整モードに入り、短押しで音量アップ、長押しで音量ダウンします。
- Servo 設定では、CoreS3 の顔の基準位置を保存したり、保存済み位置へ戻したりできます。
- Power 設定では、熱状態、バッテリー状態、低電力モードを確認/変更できます。
- 低電力モードをオンにすると、画面の最大輝度を抑え、待機中の表情更新と首振り動作を減らします。
  発話中の音声再生と口パクは継続します。
- CoreS3 / StopWatch では電源ボタンを押すと、画面のオン/オフを切り替えます。
- 画面オフ中は、なでなでやフリフリの反応は発生しません。
- CoreS3 では、WebSocket または USB Serial クライアント接続中に表情画面右側のマイク表示をタップすると
  マイク送信のミュート/解除を切り替えます。
- AtomS3R Chatbot では、通常画面で BtnA ダブルクリックするとマイク送信のミュート/解除を切り替えます。
- CoreS3 では、WebSocket または USB Serial クライアント接続中にマイク表示の上にあるカメラ表示をタップすると
  `camera_button` イベントを送ります。本体は WebSocket では画像を送らないため、
  ネットワーク接続のクライアントは HTTP `POST /capture`、USB Serial クライアントは
  `capture.request` メッセージで画像を取得します。
  StopWatch / AtomS3R Chatbot はカメラ非搭載のため、この操作と画像取得は使えません。

## 接続口

### HTTP

ベース URL:

```text
http://<stack-chan-ip>
```

エンドポイント:

| Method | Path | 説明 |
| --- | --- | --- |
| `GET` | `/status` | 本体状態を JSON で返します。`charging` は充電中または USB/VBUS 給電中なら `true` です。 |
| `POST` | `/capture` | CoreS3 のカメラで JPEG を撮影して返します。カメラ非搭載機種では失敗します。 |
| `GET`, `POST` | `/speaker-test` | StopWatch / AtomS3R Chatbot のスピーカー診断用に短いトーンを再生します。 |
| `GET`, `POST` | `/mic-test` | マイク診断結果を JSON で返します。 |
| `OPTIONS` | `/status`, `/capture`, `/speaker-test`, `/mic-test` | CORS preflight 用です。 |

`/status` のレスポンス例:

```json
{
  "cameraReady": false,
  "networkMode": "STA",
  "wsClientConnected": true,
  "usbSerialClientConnected": false,
  "affection": 500,
  "mood": 0,
  "confusion": 0,
  "affectionSeq": 1,
  "affectionLevel": "normal",
  "levelIndex": 3,
  "visualTier": "normal",
  "styleId": "normal",
  "timestampMs": 345678,
  "freeHeap": 123456,
  "freePsram": 1234567,
  "displayOn": true,
  "brightness": 160,
  "volume": 180,
  "micMuted": false,
  "micStreaming": true,
  "lowPowerMode": false,
  "thermalLevel": "Normal",
  "chipTempC": 48.5,
  "pmicTempC": 42.0,
  "batteryLevel": 87,
  "charging": false,
  "wifiConnected": true,
  "ip": "192.168.1.10"
}
```

### WebSocket

エンドポイント:

```text
ws://<stack-chan-ip>:8080/
```

text frame は UTF-8 JSON コマンドです。クライアントから本体へ送る binary frame は
スピーカー再生用の signed 16-bit PCM 音声です。本体からクライアントへ送るマイク
binary frame は 16 byte header の後ろに signed 16-bit PCM を入れ、listening 状態
かつマイク送信がミュートされていない間に送ります。remote VAD が inactive の間は、
本体側で無音と判定したときにマイク frame 送信を休止することがあります。WebSocket
接続は維持し、音量が戻ると短い pre-roll 付きで送信を再開します。マイクのミュートは
送信だけを止め、WebSocket 接続は切断しません。

音声設定:

```text
Sample rate: 16000 Hz
Channels: 1
Format: signed 16-bit PCM
Recommended chunk: mic input 40 ms, playback 60 ms
Microphone packet: "MIC1" magic, uint32 little-endian seq, uint32 little-endian timestampMs, uint16 little-endian sampleCount, uint16 flags, PCM payload
Microphone flags: bit 0 = stream segment start
```

### USB Serial

ファームウェアは、USB CDC / USB Serial でも同じ command/event モデルを受け付けます。
Android 側で `usb-serial-for-android` などの USB Host API からデバイスの USB port を
開き、Wi-Fi に依存せずに制御する用途を想定しています。

USB Serial 入力は 2 形式に対応しています。

- 診断向けの改行区切り raw JSON。例: `{"type":"ping","id":"phone_001"}\n`
- JSON、TTS PCM、マイク PCM、撮影画像を扱う binary SCU1 frame

SCU1 frame layout:

```text
magic     4 bytes  "SCU1"
version   1 byte   0x01
type      1 byte
flags     1 byte
reserved  1 byte   0x00
seq       4 bytes  little-endian
length    4 bytes  little-endian
payload   length bytes
crc32     4 bytes  little-endian
```

CRC32 は標準 IEEE polynomial を使い、`version` から `payload` までを対象にします。
`magic` と最後の CRC field は含めません。

Frame type:

| Type | Direction | Payload |
| --- | --- | --- |
| `0x01` JSON | 双方向 | UTF-8 JSON command/event |
| `0x02` TTS PCM | client to device | raw signed 16-bit little-endian PCM, 16 kHz mono |
| `0x03` MIC PCM | device to client | 既存の `MIC1` microphone packet |
| `0x04` capture request | client to device | JSON request payload |
| `0x05` capture image chunk | device to client | JPEG bytes |
| `0x08` ping | client to device | optional JSON payload |
| `0x09` pong | device to client | JSON payload |

下記の WebSocket JSON コマンドと同じ JSON を、SCU1 type `0x01` の payload として
送れます。TTS 再生では、まず `{"type":"state","value":"speaking"}` を送り、
続けて type `0x02` の PCM frame を送り、最後に `{"type":"state","value":"idle"}`
を送ります。再生はプリバッファ閾値に到達した時点、または `idle` による残りバッファの
drain 時に開始します。

USB Serial クライアントは、USB stream を binary として扱ってください。開発ビルドでは
同じ CDC port に診断ログが混ざることがあるため、受信側は常に `SCU1` magic をスキャンして
再同期し、frame 外のテキストを読み飛ばす必要があります。完全な仕様と Android 側の注意点は
`docs/usb_serial_protocol.ja.md` を参照してください。

## StreetPass / すれ違い通信

StreetPass は BLE を使って、近くの Stack-chan と名前・メッセージを交換する機能です。
本体は直近 30 件だけを保存し、スマホなどの外部アプリが WebSocket / USB Serial JSON API で
履歴を取得して長期保存します。

本体 UI:

- 通常画面では未読があると封筒アイコンを表示します。
- 設定画面の `Pass` タブで StreetPass の On/Off、プロフィール、履歴を確認できます。
- 履歴は新しい順に表示し、個別削除できます。
- 日本語名・メッセージを表示し、半角カタカナは表示時に全角へ正規化します。

音声対話中の扱い:

- `Listening` でマイクストリーミング中、`Speaking` 中、再生 drain 中は BLE scan / GATT / advertisement を停止します。
- マイク OFF で `micStreaming=false` になった場合は、`Listening` 状態でも StreetPass を再開します。
- 画面 OFF 時は、画面更新などを抑えつつ StreetPass と最低限の通信処理を継続します。

外部アプリ向け API:

- `streetpass.time.set`: UTC 時刻同期
- `streetpass.profile.get` / `streetpass.profile.set`: 自分のプロフィール確認・編集
- `streetpass.encounters.get`: 本体履歴取得
- `streetpass.encounters.ack`: アプリ保存済み・既読の反映
- `streetpass.encounters.delete`: 本体履歴の個別削除

詳細は `docs/streetpass_protocol.ja.md` を参照してください。

## WebSocket JSON コマンド

状態を切り替える:

```json
{ "type": "state", "value": "listening" }
```

`value`: `idle`, `listening`, `speaking`

認識結果を送る:

```json
{ "type": "auth", "result": "master" }
```

`result`: `master`, `not_master`, `unknown`, `none`

発話検出状態を送る:

```json
{ "type": "vad", "active": true }
```

表情モードを切り替える:

```json
{ "type": "face_mode", "value": "photo" }
```

`value`: `normal`, `photo`, `photo_master`, `nadenade`, `pet`, `furifuri`, `shake`

特定の表情画像を表示する:

```json
{ "type": "face", "value": "idle" }
```

主な `value`: `idle`, `listen`, `talk_0`, `talk_1`, `bad_0`, `bad_1`,
`good_0`, `good_1`, `good_blink`, `photo_0`, `photo_1`, `photo_blink`,
`photo_blink_talk`, `photo_master_0`, `photo_master_1`, `nadenade_0`,
`nadenade_1`, `furifuri_0`, `furifuri_1`, `blink`, `smile`、および
「表情画像の配置」にある guarded / attached / thermal / low-power 系の追加表情名

モーションを指定する:

```json
{ "type": "motion", "name": "center" }
```

`name`: `center`, `look_left`, `look_right`, `look_away`, `not_master`, `nod`,
`small_nod`, `small_bounce`, `lean_forward`, `wobble`, `shy_nod`, `thinking`

サーボ非搭載の StopWatch / AtomS3R Chatbot でもコマンドは受け付けますが、物理的な首振りは発生しません。

サーボ角を直接指定する:

```json
{ "type": "pose", "pan": 90, "tilt": 82 }
```

初期設定の範囲:

```text
pan: 45..135
tilt: 60..120
```

なでなで状態を送る:

```json
{ "type": "pet", "active": true }
```

`nadenade` も `pet` の別名として受け付けます。

## 好感度コマンド

好感度の正本は本体側が持ちます。本体はイベントメッセージを受け取り、
重み、クールダウン、重複抑制、永続化、表示更新をファームウェア側で処理します。

本体が受け付けるイベント例:

```json
{
  "type": "affection.event",
  "id": "client-000001",
  "event": "praise",
  "confidence": 0.92,
  "intensity": 0.8,
  "source": "phone",
  "requestId": "req-000001"
}
```

対応イベント名:

```text
talk
positive_talk
negative_talk
praise
thanks
apology
master_seen
session_start
petting
shake
```

状態取得リクエスト:

```json
{ "type": "affection.get", "requestId": "req-000002" }
```

本体からのレスポンス:

```json
{
  "type": "affection.state",
  "requestId": "req-000002",
  "seq": 128,
  "timestampMs": 345678,
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "level": "nakayoshi",
  "levelIndex": 4,
  "visualTier": "attached",
  "styleId": "friendly"
}
```

デバイス識別:

```json
{ "type": "device.info.get", "requestId": "req-device-001" }
```

本体はNVSに保存した安定 `deviceId` を `device.info` で返します。

キャラクター単位の同期:

```json
{ "type": "affection.sync.state", "requestId": "req-sync-001", "characterId": "char_shared_001" }
{ "type": "affection.sync.apply", "requestId": "req-sync-002", "characterId": "char_shared_001", "affection": 680 }
```

本体は `characterId` ごとに `syncedBaseAffection` を保存し、`affection.sync.state`
で `unsyncedDelta` を返します。

本体は `petting`, `shake`, `camera_button`, `session_start`, `level_up`,
`level_down` などの物理/本体側イベントを `interaction.event` として broadcast
します。`camera_button` は CoreS3 で WebSocket または USB Serial クライアント接続中だけ送信し、
phase は `pressed`、クライアントから次の text/binary 応答を受けるか 30 秒
タイムアウトするまで再押下をロックします。

短期状態の扱い:

- WebSocket 接続直後は `mood` と `confusion` を `0` に戻した `affection.state` を送り、
  その後に `session_start` の `interaction.event` を送ります。
- `mood` は 10 秒ごとに `2` ずつ、`confusion` は 10 秒ごとに `10` ずつ `0` へ戻ります。
- TTS 再生中と再生バッファのドレイン中は、音声再生を優先して自然減衰と
  減衰由来の `affection.state` broadcast を止めます。
- 会話由来の好感度変化は内部状態と WebSocket 応答にはすぐ反映し、画面のハートメーターと
  差分表示は TTS 再生後にまとめて更新します。
- ふりふり検知時は `confusion` を `100` まで上げます。

リセットリクエスト:

```json
{ "type": "affection.reset", "value": 500 }
```

デバッグ用の増減リクエスト:

```json
{ "type": "affection.debug_adjust", "delta": 10 }
```

デバッグ用の直接設定リクエスト:

```json
{ "type": "affection.debug_set", "levelIndex": 5, "mood": 40, "persist": false }
```

本体側の好感度 API 詳細は `docs/device_affection_api.ja.md` を参照してください。

## トラブルシュート

- Wi-Fi が未設定の場合は、対象デバイスの SoftAP SSID に接続して
  `http://192.168.4.1/wifi` から SSID/password を登録してください。
  ソースビルド時は `src/config_private.h` に `WIFI_SSID` / `WIFI_PASSWORD` を
  設定することもできます。
- 表情が表示されない場合は、必要な JPG が対象デバイスの画像ディレクトリに揃っていることを確認し、
  `pio run -e <env> -t uploadfs` を実行してください。
- HTTP は通るが WebSocket が通らない場合は、接続先が port `8080` になっているか確認してください。
- SoftAP モードの場合は、スマホ、PC、その他のクライアント端末を対象デバイスの SoftAP SSID に接続し、
  IP に `192.168.4.1` を指定してください。Wi-Fi 設定ページは `/wifi` です。
- StopWatch / AtomS3R Chatbot で `/capture` が失敗するのは正常です。カメラ非搭載のためです。
- USB Serial の ping/pong が timeout する場合は、アプリ側が `SCU1` magic へ再同期できているか、
  frame の前後に混ざる診断テキストを読み飛ばせているか確認してください。

## ライセンスに関する注意

このリポジトリのファームウェアソースコードは MIT License で公開します。詳細は
`LICENSE` を参照してください。

直接利用している外部ライブラリのライセンス概要は `THIRD_PARTY_NOTICES.md` にまとめています。

このリポジトリには第三者のキャラクター画像ファイルを含めていません。実行時用画像は
ローカルで準備し、使用する素材の利用規約に従ってください。
