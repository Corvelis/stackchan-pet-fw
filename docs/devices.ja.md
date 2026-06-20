# 対応デバイス別ガイド

この文書は、同じファームウェアソースを CoreS3 / StopWatch / AtomS3R Chatbot 向けに
ビルド・書き込み・操作するための機種別メモです。共通の HTTP / WebSocket /
USB Serial API は README と各 API 仕様を参照してください。

## 対応デバイス

| デバイス | PlatformIO env | 画像ディレクトリ | 画像サイズ | SoftAP SSID | 主な特徴 |
| --- | --- | --- | --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `m5stack-cores3` | `data/` | 240 x 240 | `StackChan-Direct` | サーボ、カメラ、背面タッチ |
| M5Stack StopWatch | `m5stack-stopwatch` | `data_stopwatch/` | 386 x 386 | `StopWatch` | 丸画面、画面タッチなでなで、バイブ、時計表示 |
| M5Stack AtomS3R Chatbot | `m5stack-atoms3r-chatbot` | `data_atoms3r/` | 128 x 128 | `AtomS3R` | 小画面、BtnA 操作、Atomic Echo Base 音声 |

共通の設定用 Wi-Fi パスワードは `stackchan123`、設定ページは
`http://192.168.4.1/wifi` です。

## 機能差分

| 機能 | CoreS3 | StopWatch | AtomS3R Chatbot |
| --- | --- | --- | --- |
| 表情表示 | あり | あり | あり |
| WebSocket / USB Serial | あり | あり | あり |
| マイク / スピーカー | あり | あり | Atomic Echo Base 使用 |
| カメラ撮影 `/capture` | あり | なし | なし |
| `camera_button` UI | あり | なし | なし |
| サーボモーション | あり | なし | なし |
| 背面タッチなでなで | あり | なし | なし |
| 画面タッチなでなで | なし | あり | なし |
| ボタン長押しなでなで | なし | なし | あり |
| バイブ | なし | あり | なし |
| 時計表示 | なし | あり | なし |
| StreetPass | あり | あり | あり |

`motion` / `pose` コマンドはサーボ非搭載機種でも受け付けますが、物理的な首振りは発生しません。
`capture.request` と HTTP `POST /capture` はカメラ非搭載機種では失敗します。

## 共通セットアップ

PlatformIO CLI を使います。

```sh
pio --version
```

macOS で `pio` が見つからない場合は、次のパスにあることがあります。

```sh
~/.platformio/penv/bin/pio --version
```

任意でローカル Wi-Fi 設定を作成できます。未設定でも起動後に Wi-Fi 設定ページから登録できます。

```sh
cp src/config_private.example.h src/config_private.h
```

PlatformIO コマンドの使い分け:

| 用途 | コマンド |
| --- | --- |
| ビルドだけ確認 | `pio run -e <env>` |
| ファームウェアを書き込み | `pio run -e <env> -t upload` |
| 表情画像など LittleFS データを書き込み | `pio run -e <env> -t uploadfs` |
| シリアルログを確認 | `pio device monitor -e <env>` |

初回書き込み時は `upload` のあとに `uploadfs` を実行してください。
コードだけ変更した場合は `upload`、表情画像だけ変更した場合は `uploadfs` だけで足ります。

## CoreS3 + ｽﾀｯｸﾁｬﾝ

### 必要なもの

- M5Stack CoreS3
- Stack-chan 互換のサーボ構成
- データ通信できる USB-C ケーブル

### ビルドと書き込み

```sh
pio run -e m5stack-cores3
pio run -e m5stack-cores3 -t upload
pio run -e m5stack-cores3 -t uploadfs
pio device monitor -e m5stack-cores3
```

配布用ビルドは `m5stack-cores3-release` を使います。配布用では
`STACKCHAN_RELEASE_BUILD` が有効になり、`src/config_private.h` の個人 Wi-Fi 情報を含めません。

```sh
pio run -e m5stack-cores3-release
pio run -e m5stack-cores3-release -t buildfs
```

GitHub Releases 用に全デバイス分の asset を作る場合は、リポジトリ直下で次を実行します。

```sh
bash scripts/build_release_bins.sh all
```

個別に作る場合は `cores3`、`stopwatch`、`atoms3r` のいずれかを指定できます。

### 操作

- 画面端からフリック: 設定画面の表示/非表示。
- Network 画面を長押し: STA / SoftAP の切り替え。
- 電源ボタン短押し: 画面オン/オフ。
- 背面タッチ: なでなで反応。
- WebSocket または USB Serial クライアント接続中のマイク表示タップ: マイク送信ミュート/解除。
- カメラ表示タップ: `camera_button` イベント送信。
- Servo 画面: サーボ原点の保存と復帰。

## M5Stack StopWatch

### 必要なもの

- M5Stack StopWatch
- データ通信できる USB-C ケーブル

### ビルドと書き込み

```sh
pio run -e m5stack-stopwatch
pio run -e m5stack-stopwatch -t upload
pio run -e m5stack-stopwatch -t uploadfs
pio device monitor -e m5stack-stopwatch
```

StopWatch は `data_stopwatch/` を LittleFS に書き込みます。`platformio.ini` の
`extra_scripts` で自動的に選択されます。
配布用 asset 生成では `m5stack-stopwatch-release` env が使われます。

### 操作

- BtnA 短押し: 設定画面の表示/非表示。
- BtnB 短押し、または電源ボタン短押し: 画面オン/オフ。
- 通常画面中央付近を短く押し続ける、または中央付近からドラッグ: なでなで反応。画面オフ中と設定画面表示中は無効です。
- 設定画面上部の `<` / `>` ボタンをタップ: 前/次の設定ページへ移動。
- 設定画面を左右フリック: 前/次の設定ページへ移動。左向きフリックで次ページ、右向きフリックで前ページです。
- 設定ページの順序: Network -> Display -> Audio -> Power -> StreetPass -> Network。StopWatch には Servo ページはありません。
- QR 表示中に画面をタップ: Network 画面へ戻る。
- Network 画面を長押し: STA / SoftAP の切り替え。SoftAP では `Wi-Fi QR` と `Setup QR`、STA 接続済みでは `Setup QR` をタップして QR を表示できます。
- Display 画面: `-` / `+` をタップして明るさを 20 単位で調整。`Screen Off` / `Screen On` をタップして画面オン/オフ。
- Audio 画面: `-` / `+` をタップしてスピーカー音量を 20 単位で調整。
- Power 画面: `Low Power On` / `Low Power Off` をタップして低電力モードを切り替え。
- StreetPass 画面: `Turn On` / `Turn Off` で StreetPass On/Off、`Profile` / `History` で自分のプロフィール表示と最新履歴表示を切り替え。

StopWatch にはカメラとサーボがありません。HTTP `/capture`、USB Serial `capture.request`、
サーボ原点調整、カメラ表示操作は使えません。丸画面 UI ではマイク表示タップによる
ミュート/解除操作もありません。

## M5Stack AtomS3R Chatbot

### 必要なもの

- M5Stack AtomS3R
- Atomic Echo Base
- データ通信できる USB-C ケーブル

### ビルドと書き込み

```sh
pio run -e m5stack-atoms3r-chatbot
pio run -e m5stack-atoms3r-chatbot -t upload
pio run -e m5stack-atoms3r-chatbot -t uploadfs
pio device monitor -e m5stack-atoms3r-chatbot
```

AtomS3R は `boards/m5stack-atoms3r.json` のボード定義と `data_atoms3r/` の画像を使います。
`platformio.ini` の `extra_scripts` で LittleFS 用ディレクトリが自動的に選択されます。
配布用 asset 生成では `m5stack-atoms3r-chatbot-release` env が使われます。

### 操作

AtomS3R Chatbot はタッチ操作がなく、BtnA の短押し、ダブルクリック、長押しで操作します。
画面オフ中は BtnA 短押しまたは長押しで画面をオンにできます。
設定ページの順序は Network -> StreetPass -> Audio -> Power -> 通常画面です。

| 画面 | BtnA 短押し | BtnA ダブルクリック | BtnA 長押し |
| --- | --- | --- | --- |
| 通常画面 | Network 画面を開く | マイク送信のミュート/解除 | なでなで反応。離したあと少しだけ反応が継続 |
| Network | StreetPass 画面へ移動。QR 表示中は Network 画面へ戻る | `Setup QR` の表示/非表示 | STA / SoftAP の切り替え |
| StreetPass | Audio 画面へ移動 | Status / Profile / Latest の表示切り替え | StreetPass On/Off |
| Audio | 通常表示中は Power 画面へ移動。音量調整モード中は音量を 20 上げる | 音量調整モードの On/Off | 音量調整モード中だけ音量を 20 下げる。押し続けると約 1 秒ごとに下がる |
| Power | 通常画面へ戻る | 操作なし | 低電力モード On/Off |

AtomS3R Chatbot にはカメラとサーボがありません。HTTP `/capture`、USB Serial
`capture.request`、サーボ原点調整、カメラ表示操作は使えません。タッチ操作、Display ページ、
Servo ページはありません。

## Wi-Fi 設定

Wi-Fi 未設定で STA 接続できない場合、各デバイスは設定用 SoftAP を起動します。

| デバイス | SSID | Password | URL |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `http://192.168.4.1/wifi` |
| StopWatch | `StopWatch` | `stackchan123` | `http://192.168.4.1/wifi` |
| AtomS3R | `AtomS3R` | `stackchan123` | `http://192.168.4.1/wifi` |

STA 接続済みの場合は、本体の Network 画面に表示される IP アドレスの `/wifi` でも
同じ設定ページを開けます。

## 表情画像

各デバイスは LittleFS 直下の同じファイル名を読みますが、アップロード元ディレクトリと
画像サイズが異なります。

| デバイス | アップロード元 | サイズ |
| --- | --- | --- |
| CoreS3 | `data/` | 240 x 240 |
| StopWatch | `data_stopwatch/` | 386 x 386 |
| AtomS3R | `data_atoms3r/` | 128 x 128 |

`pio run -e <env> -t uploadfs` を実行すると、対象 env に対応するディレクトリが LittleFS へ
書き込まれます。

## トラブルシュート

- 表情が表示されない場合は、対象 env の画像ディレクトリに PNG が揃っていることを確認し、
  `pio run -e <env> -t uploadfs` を実行してください。
- Wi-Fi 設定ページを開けない場合は、接続している SoftAP SSID が対象デバイスのものか確認してください。
- WebSocket は port `8080`、HTTP は port `80` です。
- StopWatch / AtomS3R で `/capture` が失敗するのは正常です。カメラ非搭載のためです。
- AtomS3R の音声が動かない場合は、Atomic Echo Base の接続と `m5stack-atoms3r-chatbot` env で
  ビルドしていることを確認してください。
