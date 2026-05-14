# Stack-chan CoreS3 Controller

[English](README.md) | [日本語](README.ja.md)

M5Stack CoreS3 ベースのスタックチャン用ファームウェアです。表情表示、サーボ動作、
マイク/スピーカー音声ストリーミング、カメラ撮影、なでなで/ふりふり反応、
外部制御用の通信インターフェースを扱います。

このリポジトリに含めているのはスタックチャン本体側のファームウェアだけです。
README では、本体が公開する HTTP / WebSocket の口を記載しています。
外部クライアント側の実装方法は対象外です。

## できること

- PlatformIO / Arduino でビルドする M5Stack CoreS3 ファームウェア
- LittleFS 上の PNG 表情画像を表示
- WebSocket JSON による状態制御
- WebSocket binary による PCM 音声再生
- HTTP によるカメラ撮影とステータス取得
- Wi-Fi STA 接続と SoftAP 直接接続
- 本体側のなでなで/ふりふり反応
- WebSocket イベントによる好感度管理

## 必要なもの

- M5Stack CoreS3
- Stack-chan 互換のサーボ構成
- PlatformIO 開発環境

PlatformIO の環境名は `platformio.ini` の `m5stack-cores3` です。

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
- `src/`: ファームウェア本体
- `data/`: LittleFS にアップロードするローカルデータ置き場
- `docs/device_affection_api.ja.md`: 本体側の好感度 API 詳細

任意:

- `include/README`, `lib/README`, `test/README`: PlatformIO 標準の空フォルダ用ファイル
- `.vscode/extensions.json`: VS Code の推奨拡張

GitHub に上げないもの:

- `.pio/`: ビルド生成物とダウンロードされた依存ライブラリ
- `.DS_Store`: macOS のメタデータ
- `src/config_private.h`: ローカル Wi-Fi 認証情報
- `assets/`: ローカルの画像素材作業フォルダ
- `data/*.png`: 第三者素材から作成した実行時用の表情画像

## セットアップ手順

1. PlatformIO をインストールし、`pio --version` が動くことを確認します。
2. ローカル設定ファイルを作ります。

```sh
cp src/config_private.example.h src/config_private.h
```

3. `src/config_private.h` に Wi-Fi 情報を書きます。

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
```

4. 後述の PNG ファイルを `data/` に配置します。
5. CoreS3 を USB で接続します。
6. ファームウェアを書き込みます。

```sh
pio run --target upload
```

7. LittleFS の画像データを書き込みます。

```sh
pio run --target uploadfs
```

8. シリアルモニタを開きます。

```sh
pio device monitor
```

起動ログに Wi-Fi モードと IP アドレスが表示されます。

## 表情画像の配置

ファームウェアは `src/config.h` に定義されたパスで LittleFS から画像を読みます。
`data/` に 240 x 240 の PNG を次のファイル名で置いてください。

| ファイル名 | 置いてほしい画像 | つくよみちゃん万能立ち絵素材を使う場合の例 |
| --- | --- | --- |
| `idle.png` | 通常待機顔。口閉じ。 | `01-01a 基本-目ふんわり-口閉じ.png` |
| `listen.png` | 聞き取り中/待ち受け顔。口閉じ。 | `01-02a 基本-目ぱっちり-口閉じ.png` |
| `talk_0.png` | 通常発話の口閉じフレーム。 | `01-01a 基本-目ふんわり-口閉じ.png` |
| `talk_1.png` | 通常発話の口開けフレーム。 | `01-01b 基本-目ふんわり-口開け.png` |
| `blink.png` | 通常まばたきフレーム。 | `01-01c 基本-目ふんわり-目閉じ.png` |
| `smile.png` | 待機中に出るにっこり顔。 | `01-03ac 基本-にっこり-口閉じ.png` |
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

開発時の画像は「つくよみちゃん万能立ち絵素材（花兎*様）」からローカルで作成しています。
このリポジトリには画像ファイル本体を含めていません。公式配布ページから素材を入手し、
利用規約に従って 240 x 240 PNG を書き出して `data/` に配置してください。

ローカル画像セットのクレジット:

```text
フリー素材キャラクター「つくよみちゃん」
https://tyc.rei-yumesaki.net/
つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

## ネットワークモード

ファームウェアには 2 つの接続モードがあります。

- STA モード: `src/config_private.h` の Wi-Fi に接続します。
- SoftAP モード: CoreS3 自身がアクセスポイントになります。

SoftAP の初期値:

```text
SSID: StackChan-Direct
Password: stackchan123
IP: 192.168.4.1
```

情報画面が表示されている状態でタッチ画面を長押しすると、STA と SoftAP を切り替えます。
選択したモードは本体に保存され、再起動後に反映されます。

## 接続口

### HTTP

ベース URL:

```text
http://<stack-chan-ip>
```

エンドポイント:

| Method | Path | 説明 |
| --- | --- | --- |
| `GET` | `/status` | 本体状態を JSON で返します。 |
| `POST` | `/capture` | カメラで JPEG を撮影して返します。 |
| `OPTIONS` | `/status`, `/capture` | CORS preflight 用です。 |

`/status` のレスポンス例:

```json
{
  "cameraReady": false,
  "networkMode": "STA",
  "wsClientConnected": true,
  "affection": 500,
  "mood": 0,
  "confusion": 0,
  "affectionSeq": 1,
  "affectionLevel": "normal",
  "freeHeap": 123456,
  "freePsram": 1234567,
  "wifiConnected": true,
  "ip": "192.168.1.10"
}
```

### WebSocket

エンドポイント:

```text
ws://<stack-chan-ip>:8080/
```

text frame は UTF-8 JSON コマンドです。binary frame は本体スピーカーで再生する
raw PCM 音声データです。

音声設定:

```text
Sample rate: 16000 Hz
Channels: 1
Format: signed 16-bit PCM
Recommended chunk: input 20 ms, playback 60 ms
```

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
`nadenade_1`, `furifuri_0`, `furifuri_1`, `blink`, `smile`

モーションを指定する:

```json
{ "type": "motion", "name": "center" }
```

`name`: `center`, `look_left`, `look_right`, `look_away`, `not_master`, `nod`, `thinking`

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
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "seq": 128,
  "level": "nakayoshi"
}
```

リセットリクエスト:

```json
{ "type": "affection.reset", "value": 500 }
```

デバッグ用の増減リクエスト:

```json
{ "type": "affection.debug_adjust", "delta": 10 }
```

本体側の好感度 API 詳細は `docs/device_affection_api.ja.md` を参照してください。

## トラブルシュート

- 画面に Wi-Fi 未設定と出る場合は、`src/config_private.h` を作成して
  `WIFI_SSID` / `WIFI_PASSWORD` を設定してください。
- 表情が表示されない場合は、必要な PNG が `data/` に揃っていることを確認し、
  `pio run --target uploadfs` を実行してください。
- HTTP は通るが WebSocket が通らない場合は、接続先が port `8080` になっているか確認してください。
- SoftAP モードの場合は、スマホ、PC、その他のクライアント端末を `StackChan-Direct` に接続し、
  IP に `192.168.4.1` を指定してください。

## ライセンスに関する注意

このリポジトリのファームウェアソースコードは MIT License で公開します。詳細は
`LICENSE` を参照してください。

直接利用している外部ライブラリのライセンス概要は `THIRD_PARTY_NOTICES.md` にまとめています。

このリポジトリには第三者のキャラクター画像ファイルを含めていません。実行時用画像は
ローカルで準備し、使用する素材の利用規約に従ってください。
