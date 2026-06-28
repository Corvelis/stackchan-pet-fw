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

設定用 Wi-Fi のパスワードはデバイス別です。設定ページは `http://192.168.4.1/wifi` です。

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
| なでなでアニメーション | あり | あり | あり |
| ぐるぐる顔 | あり | あり | あり |
| ぐるぐる入力 | タッチ / IMU 切替 | タッチ / IMU 切替 | IMU 固定 |
| ぐるぐる方向数 | 16方向 + 中央 | 8方向 + 中央 | 16方向 + 中央 |
| ぐるぐる混乱 | あり | あり | あり |
| バイブ | なし | あり | なし |
| 時計表示 | なし | あり | なし |
| StreetPass | あり | あり | あり |

`motion` / `pose` コマンドはサーボ非搭載機種でも受け付けますが、物理的な首振りは発生しません。
`capture.request` と HTTP `POST /capture` はカメラ非搭載機種では失敗します。

実装差分の要点:

- CoreS3 は `STACKCHAN_HAS_SERVO=1`、`STACKCHAN_HAS_CAMERA=1`、`STACKCHAN_HAS_BACK_TOUCH=1` で、サーボ、カメラ、背面タッチを使います。
- StopWatch は `STACKCHAN_ROUND_DISPLAY=1`、`STACKCHAN_HAS_SCREEN_TOUCH_PETTING=1`、`STACKCHAN_HAS_HAPTIC=1` で、丸画面、画面タッチなでなで、バイブ、時計表示を使います。
- AtomS3R Chatbot は `STACKCHAN_SMALL_DISPLAY=1`、`STACKCHAN_HAS_ATOMIC_ECHO_BASE=1` で、タッチなしの BtnA 操作と Atomic Echo Base の音声入出力を使います。
- なでなで表示は3機種とも `pet_anim_0..pet_anim_8` を使います。発火入力だけが異なり、CoreS3 は背面タッチ、StopWatch は画面中央付近のタッチ/ドラッグ、AtomS3R は BtnA 長押しです。
- ぐるぐる表示は3機種とも `dir*` / `blink*` / `dizzy_*` 画像を使います。CoreS3 と AtomS3R は16方向+中央、StopWatch は実行時は8方向+中央です。画像セットは生成フローを統一するため、StopWatch 用フォルダにも `dir0..dir16` と `blink0..blink16` を置きます。
- ぐるぐる混乱の回転判定は機種別です。CoreS3 は2秒窓で合計32ステップ/偏り24ステップ、StopWatch は合計16ステップ/偏り12ステップ、AtomS3R は合計22ステップ/偏り4ステップを使います。AtomS3R はさらに大きく不規則に振られた時の IMU shake 混乱判定として、0.40G 以上の変化が900ms続く条件も使います。
- 画像ファイルは LittleFS 上のJPGとして持ち、実行時のぐるぐる方向、blink、混乱フレームは `STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED` 配下で `M5Canvas` にキャッシュします。キャンバスは `setPsram(true)` を使い、PSRAMを優先して確保します。

## 操作モード別の割り当て

ここでの通常/ボイス画面は、設定画面ではない顔表示画面です。ぐるぐる顔モードは
会話クライアント接続中、設定画面表示中、画面オフ中は有効表示になりません。

| デバイス | 通常/ボイス画面 | ぐるぐる顔モード中 | 設定画面 |
| --- | --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | 電源短押しで画面ON/OFF。電源ダブルクリックでぐるぐるON。背面タッチでなでなで。画面端フリックで設定画面 | 電源ダブルクリックでぐるぐるOFF。電源3クリック以上でタッチ/IMU切替。タッチ入力時は画面タッチ/ドラッグで16方向+中央。IMU入力時は本体傾き、画面長押しで基準リセット | Network 長押しでSTA/SoftAP切替。Servo画面で原点保存/復帰 |
| StopWatch | BtnA短押しで設定画面。BtnAダブルクリックでぐるぐるON。BtnB/電源短押しで画面ON/OFF。画面中央タッチ/ドラッグでなでなで | BtnAダブルクリックでぐるぐるOFF。BtnBダブルクリックでタッチ/IMU切替。BtnB長押しで基準リセット。タッチ入力時は画面タッチ/ドラッグで8方向+中央。IMU入力時は本体傾き | `<` / `>` または左右フリックでページ移動。Network長押しでSTA/SoftAP切替 |
| AtomS3R Chatbot | BtnA短押しでNetworkへ。BtnAダブルクリックでマイクミュート。BtnA長押しでなでなで。BtnA 3クリックでぐるぐるON | BtnA 3クリックでぐるぐるOFF。IMU入力固定。BtnA長押しで基準リセット。BtnA短押しはページ送りとして扱い、Networkを開くとぐるぐる表示は止まる。BtnAダブルクリックは無効 | BtnA短押しでページ移動。Network長押しでSTA/SoftAP切替。Audioでダブルクリックすると音量調整モード |

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
- 電源ボタンダブルクリック: ぐるぐる顔モードのON/OFF。
- ぐるぐる顔モード中の電源ボタン3クリック以上: タッチ入力と IMU 入力を切り替え。
- ぐるぐる顔モード中の画面タッチ: タッチ入力時は顔の向きを指定。IMU 入力時は画面長押しで IMU 基準姿勢をリセット。

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

StopWatch はデフォルトで `data_stopwatch/` を LittleFS に書き込みます。
別の画像フォルダを使う場合は、ビルド時に `STACKCHAN_FACE_DATA_DIR` で明示してください。
配布用 asset 生成では `m5stack-stopwatch-release` env が使われます。

### 操作

- BtnA 短押し: 設定画面の表示/非表示。
- BtnA ダブルクリック: ぐるぐる顔モードのON/OFF。
- BtnB 短押し、または電源ボタン短押し: 画面オン/オフ。
- ぐるぐる顔モード中の BtnB ダブルクリック: タッチ入力と IMU 入力を切り替え。
- ぐるぐる顔モード中の BtnB 長押し: IMU 基準姿勢をリセット。
- 通常画面中央付近を短く押し続ける、または中央付近からドラッグ: なでなで反応。画面オフ中と設定画面表示中は無効です。
- ぐるぐる顔モード中の通常画面タッチ/ドラッグ: タッチ入力時は8方向+中央の顔向きとして扱います。IMU 入力時は本体の傾きで顔向きを動かします。
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
別の画像フォルダを使う場合は、ビルド時に `STACKCHAN_FACE_DATA_DIR` で明示してください。
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

通常画面で BtnA を3クリックすると、ぐるぐる顔モードのON/OFFを切り替えます。
AtomS3R のぐるぐる顔は IMU 入力固定です。ぐるぐる顔モード中の BtnA 長押しは
なでなでではなく IMU 基準姿勢リセットとして扱います。

AtomS3R Chatbot にはカメラとサーボがありません。HTTP `/capture`、USB Serial
`capture.request`、サーボ原点調整、カメラ表示操作は使えません。タッチ操作、Display ページ、
Servo ページはありません。

## Wi-Fi 設定

Wi-Fi 未設定で STA 接続できない場合、各デバイスは設定用 SoftAP を起動します。

| デバイス | SSID | Password | URL |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `http://192.168.4.1/wifi` |
| StopWatch | `StopWatch` | `stopwatch123` | `http://192.168.4.1/wifi` |
| AtomS3R | `AtomS3R` | `atoms3r123` | `http://192.168.4.1/wifi` |

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

通常の `pio run -e <env> -t uploadfs` は上表の通常フォルダを使います。`data_local/`、
`data_stopwatch_local/`、`data_atoms3r_local/` など別フォルダを使う時は、ビルド時に明示します。

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_atoms3r_local pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

GitHub Releases 用の `scripts/build_release_bins.sh all` は、`STACKCHAN_FACE_DATA_DIR` を指定せずに実行し、
通常フォルダの画像を含めます。`*_local` フォルダはローカル差し替え用で、Git管理や配布用assetには含めません。

ファームウェア内の定義名は後方互換のため `/idle.png` のような `.png` パスですが、実ファイルは
`.jpg` に統一しています。`FaceController` は `.png` が見つからない場合に同じstemの `.jpg` を読みます。

## トラブルシュート

- 表情が表示されない場合は、対象 env の画像ディレクトリに JPG が揃っていることを確認し、
  `pio run -e <env> -t uploadfs` を実行してください。
- Wi-Fi 設定ページを開けない場合は、接続している SoftAP SSID が対象デバイスのものか確認してください。
- WebSocket は port `8080`、HTTP は port `80` です。
- StopWatch / AtomS3R で `/capture` が失敗するのは正常です。カメラ非搭載のためです。
- AtomS3R の音声が動かない場合は、Atomic Echo Base の接続と `m5stack-atoms3r-chatbot` env で
  ビルドしていることを確認してください。
