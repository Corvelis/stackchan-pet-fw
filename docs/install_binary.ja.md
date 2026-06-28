# バイナリ版インストール手順

この文書は、GitHub Releases で配布されるビルド済みバイナリのインストール手順です。
必ず手元のデバイスに対応するファイルを選んでください。

## 配布物

GitHub Releases には、デバイスごとに次の firmware bin と factory image が添付されています。
インストールする人は、手元のデバイスに対応するファイルを Release からダウンロードしてください。

| デバイス | 更新用 firmware | 初回用 factory image |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

| 種類 | 用途 |
| --- | --- |
| firmware bin | 既に導入済みの人向けの更新用です。Wi-Fi 設定、LittleFS 画像、CoreS3 のサーボ原点などを保持します。 |
| factory image | 新しく入れる人向けです。bootloader、partition table、firmware、LittleFS 画像データを結合しています。 |

リリース作成時は、リポジトリ直下で次を実行すると同じファイル名の asset を `dist/` に生成できます。
このコマンドは通常の画像フォルダを使って配布物を作るため、`STACKCHAN_FACE_DATA_DIR` は指定しません。

```sh
bash scripts/build_release_bins.sh all
```

個別に生成する場合は `cores3`、`stopwatch`、`atoms3r` のいずれかを指定します。

## 注意事項

- 対象デバイスと違うバイナリを書き込まないでください。
- factory image を書き込むと、本体内に保存済みの Wi-Fi 設定、StreetPass 設定、CoreS3 のサーボ原点設定などは初期化されます。
- ファームウェア単体では会話機能は動作しません。
- 会話、音声認識、TTS、応答生成には、対応するスマホアプリ、WebSocket クライアント、または USB Serial クライアントが必要です。
- StopWatch / AtomS3R Chatbot にはカメラとサーボがありません。`/capture`、`capture.request`、サーボ原点調整は使えません。
- バイナリに含まれる画像を、素材として取り出して使うことは許可しません。
- スクリーンショット、キャプチャ動画、紹介動画を投稿する場合は、このファームウェア/アプリを使用していることが分かる形で記載してください。

表記例:

```text
Stack-chan Multi-Device Controller 使用
```

## 画像とクレジット

GitHub Releases で配布する factory image には、リポジトリ通常フォルダの画像を含めます。
通常フォルダは次の3つです。

```text
data/
data_stopwatch/
data_atoms3r/
```

`data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/` はローカル差し替え用です。
これらは Git 管理や GitHub Releases の配布物には含めません。

ローカル差し替えでつくよみちゃん素材を使う場合は、フリー素材キャラクター「つくよみちゃん」
（© Rei Yumesaki）と、使用する立ち絵素材の利用規約・クレジット表記ルールを確認してください。
このリポジトリと通常配布バイナリには、つくよみちゃん素材そのものは含めません。

- 利用規約: https://tyc.rei-yumesaki.net/about/terms/
- クレジット表記: https://tyc.rei-yumesaki.net/about/terms/credit/

## Wi-Fi 設定

初回起動時、または Wi-Fi が未設定の場合は、本体が設定用アクセスポイントを起動します。

| デバイス | SSID | Password | URL |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `http://192.168.4.1/wifi` |
| StopWatch | `StopWatch` | `stopwatch123` | `http://192.168.4.1/wifi` |
| AtomS3R | `AtomS3R` | `atoms3r123` | `http://192.168.4.1/wifi` |

本体の Network 画面では、`Wi-Fi QR` で設定用 Wi-Fi に接続し、続けて `Setup QR` で
設定ページを開けます。QR を使わない場合は、スマホまたは PC から対象デバイスの
SSID に接続し、ブラウザで `http://192.168.4.1/wifi` を開いてください。
設定ページでは、SSID スキャン、SSID/password の保存、保存済み Wi-Fi の編集、削除、
優先度変更ができます。

## 既に導入済みの人の更新手順

Wi-Fi 設定、画像データ、CoreS3 のサーボ原点などを残したまま更新する場合は、
対象デバイスの firmware bin だけを書き込みます。

### 1. 必要なもの

- 対象デバイス
- データ通信できる USB-C ケーブル
- PC または Mac
- Release からダウンロードした対象デバイス用 firmware bin

### 2. esptool.py とポートを準備する

初回インストール手順の「esptool.py を準備する」と「デバイスを USB で接続する」を確認してください。

### 3. firmware を書き込む

firmware bin は flash offset `0x10000` に書き込みます。

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x10000 <FIRMWARE_BIN>
```

macOS の例:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

Windows の例:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

`stackchan_cores3_firmware.bin` の部分は、StopWatch では `stackchan_stopwatch_firmware.bin`、
AtomS3R では `stackchan_atoms3r_firmware.bin` に置き換えてください。

## 新しく入れる人のインストール手順

初めて使う人は、対象デバイスの factory image を書き込んでください。

### 1. 必要なもの

- 対象デバイス
- データ通信できる USB-C ケーブル
- PC または Mac
- Release からダウンロードした対象デバイス用 factory image

### 2. esptool.py を準備する

Python が使える環境で、`esptool.py` をインストールします。

```sh
python3 -m pip install esptool
```

インストールできたか確認します。

```sh
esptool.py version
```

### 3. デバイスを USB で接続する

対象デバイスを PC/Mac に USB で接続します。

macOS では、ポート名は次のような形式になります。

```text
/dev/cu.usbmodem101
```

ポート一覧は次のコマンドで確認できます。

```sh
ls /dev/cu.*
```

Windows では、ポート名は `COM3` や `COM4` のような形式になります。
デバイスマネージャーで確認してください。

### 4. バイナリを書き込む

factory image は flash offset `0x0` に書き込みます。

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x0 <FACTORY_BIN>
```

macOS の例:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

Windows の例:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

`stackchan_cores3_factory.bin` の部分は、StopWatch では `stackchan_stopwatch_factory.bin`、
AtomS3R では `stackchan_atoms3r_factory.bin` に置き換えてください。

書き込み後、本体は自動的に再起動します。書き込みに失敗する場合は、USB ケーブルが
データ通信対応か、ほかのシリアルモニタやアプリがポートを使用していないか確認してください。

### 5. Wi-Fi を設定する

再起動後、Wi-Fi 未設定の場合は対象デバイスの設定用 Wi-Fi が表示されます。
本体の Network 画面で `Wi-Fi QR` を読み取って接続し、続けて `Setup QR` を読み取ると
設定ページを開けます。QR を使わない場合は、スマホまたは PC から対象デバイスの
設定用 Wi-Fi に接続し、ブラウザで次を開いてください。

```text
http://192.168.4.1/wifi
```

画面に従って、普段接続したい Wi-Fi の SSID/password を保存します。

### 6. 会話クライアントを接続する

このファームウェア単体では会話機能は動作しません。
対応スマホアプリを使う場合は、その配布ページの手順に従って接続してください。
ご自身の WebSocket または USB Serial クライアントを接続する場合は、API 仕様を
[README.md](../README.md#接続口) と
[好感度 API 詳細](device_affection_api.ja.md) で確認してください。USB Serial の frame 仕様は
[USB Serial Protocol](usb_serial_protocol.ja.md) を参照してください。

## 基本の使い方

機種別の詳しい操作は [対応デバイス別ガイド](devices.ja.md) を参照してください。

共通:

- `Network` では、現在の Wi-Fi モード、IP アドレス、WebSocket 接続先、Wi-Fi 設定用 QR を確認できます。
- Wi-Fi 未設定時は、対象デバイスの設定用 Wi-Fi を出します。スマホや PC をそこへ接続し、`http://192.168.4.1/wifi` を開くと Wi-Fi 設定ができます。
- `Display` では、画面の明るさと画面オン/オフを変更できます。AtomS3R Chatbot には `Display` 画面はありません。
- `Audio` では、スピーカー音量を変更できます。
- `Pwr` では、バッテリー、温度、低電力モードを確認/変更できます。
- 低電力モードをオンにすると、画面の最大輝度を抑え、待機中の表情更新を減らします。
  発話中の音声再生と口パクは継続します。
- CoreS3 はマイク表示タップ、AtomS3R Chatbot は通常画面の BtnA ダブルクリックで、マイク送信のミュート/解除を操作できます。StopWatch には本体側のマイクミュート操作はありません。

通常/ボイス画面とぐるぐるモード:

ここでの通常/ボイス画面は、設定画面ではない顔表示画面です。ぐるぐる顔モードは、
WebSocket / USB Serial の会話クライアント接続中、設定画面表示中、画面オフ中は有効表示になりません。
ぐるぐるを試す時は、会話クライアントを切断して通常の顔表示画面に戻してください。

| デバイス | 通常/ボイス画面から入る/戻る | ぐるぐる中の顔向き操作 | ぐるぐる中の補助操作 |
| --- | --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | 電源ボタンダブルクリック | タッチ入力時は画面タッチ/ドラッグで16方向+中央。IMU入力時は本体の傾き | 電源ボタン3クリック以上でタッチ/IMU切替。IMU入力時は画面長押しで基準姿勢リセット |
| StopWatch | BtnA ダブルクリック | タッチ入力時は画面タッチ/ドラッグで8方向+中央。IMU入力時は本体の傾き | BtnB ダブルクリックでタッチ/IMU切替。BtnB 長押しで基準姿勢リセット |
| AtomS3R Chatbot | 通常画面で BtnA 3クリック | IMU入力固定。本体の傾きで16方向+中央 | BtnA 長押しで基準姿勢リセット。ぐるぐる中の BtnA ダブルクリックはマイクミュートには使いません |

CoreS3:

- タッチ画面の端からフリックすると、設定画面を開閉できます。
- 背面タッチでなでなで反応を発生させます。
- 電源ボタンダブルクリックでぐるぐる顔モードをON/OFFします。ぐるぐる中に電源ボタンを3クリック以上すると、タッチ入力と IMU 入力を切り替えます。
- カメラ表示をタップすると `camera_button` イベントを送ります。
- `Servo` では、顔の基準位置を保存したり、保存済み位置へ戻したりできます。

StopWatch:

- BtnA 短押しで設定画面を開閉します。
- BtnA ダブルクリックでぐるぐる顔モードをON/OFFします。
- BtnB または電源ボタンで画面オン/オフを切り替えます。
- ぐるぐる中は BtnB ダブルクリックでタッチ入力/IMU入力を切り替え、BtnB 長押しで IMU 基準姿勢をリセットします。
- 通常画面中央付近のタッチ/ドラッグでなでなで反応を発生させます。
- 設定画面は Network、Display、Audio、Power、StreetPass の順です。上部の `<` / `>` または左右フリックでページを切り替えます。
- Display / Audio では `-` / `+` をタップして明るさや音量を 20 単位で調整します。

AtomS3R Chatbot:

- BtnA 短押しでページを送ります。通常画面では Network を開き、以後 Network -> StreetPass -> Audio -> Power -> 通常画面の順で切り替わります。
- ぐるぐる顔モード中の BtnA 短押しもページ送りとして扱われ、Network 画面を開くとぐるぐる表示は止まります。
- Network 画面: 短押しで StreetPass へ移動、ダブルクリックで `Setup QR` 表示/非表示、長押しで STA / SoftAP を切り替えます。
- StreetPass 画面: 短押しで Audio へ移動、ダブルクリックで Status / Profile / Latest 表示を切り替え、長押しで StreetPass On/Off を切り替えます。
- Audio 画面: 短押しで Power へ移動します。ダブルクリックで音量調整モードに入り、音量調整モード中は短押しで音量アップ、長押しで音量ダウンします。
- Power 画面: 短押しで通常画面へ戻り、長押しで低電力モードを切り替えます。
- 通常画面では、長押しでなでなで反応、ダブルクリックでマイク送信のミュート/解除を切り替えます。
- 通常画面で BtnA を3クリックすると、IMU入力固定のぐるぐる顔モードをON/OFFします。ぐるぐる中の長押しは、なでなでではなく IMU 基準姿勢リセットです。

### CoreS3 のサーボ原点を調整する

顔の向きが正面からずれている場合は、CoreS3 の `Servo` 画面で調整できます。

1. 手で顔を正面の少し上向きに合わせます。
2. `Save Direction` を押して、その向きを顔の基準位置として保存します。
3. `Go to Saved` を押すと、保存済みの基準位置に戻ります。
