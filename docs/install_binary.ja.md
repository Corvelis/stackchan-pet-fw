# バイナリ版インストール手順

この文書は、GitHub Releases で配布されている M5Stack CoreS3 用バイナリのインストール手順です。

## 配布物

Release から用途に合うファイルをダウンロードしてください。

```text
stackchan_cores3_firmware.bin
stackchan_cores3_factory.bin
```

| ファイル | 用途 |
| --- | --- |
| `stackchan_cores3_firmware.bin` | 既に導入済みの人向けの更新用 firmware です。Wi-Fi 設定、サーボ原点、LittleFS 画像を保持します。 |
| `stackchan_cores3_factory.bin` | 新しく入れる人向けの factory image です。bootloader、partition table、firmware、LittleFS 画像データを結合しています。 |

## 注意事項

- このバイナリは M5Stack CoreS3 用です。他の機種には書き込まないでください。
- ファームウェア単体では会話機能は動作しません。
- 会話、音声認識、TTS、応答生成には、対応するスマホアプリ、WebSocket クライアント、または USB Serial クライアントが必要です。
- 既にこのファームウェアを使っている人は、通常 `stackchan_cores3_firmware.bin` を使って更新してください。
- factory image を書き込むと、本体内に保存済みの Wi-Fi 設定やサーボ原点設定は初期化されます。
- バイナリに含まれる画像を、素材として取り出して使うことは許可しません。
- スクリーンショット、キャプチャ動画、紹介動画を投稿する場合は、このファームウェア/アプリを使用していることが分かる形で記載してください。

表記例:

```text
Stack-chan CoreS3 Controller 使用
```

## つくよみちゃん関連のクレジット

このファームウェアでは、フリー素材キャラクター「つくよみちゃん」（© Rei Yumesaki）の立ち絵素材を使用させていただいております。

```text
フリー素材キャラクター「つくよみちゃん」（© Rei Yumesaki）
https://tyc.rei-yumesaki.net/

つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

利用時は、つくよみちゃん公式サイトの利用規約とクレジット表記ルールを確認してください。

- 利用規約: https://tyc.rei-yumesaki.net/about/terms/
- クレジット表記: https://tyc.rei-yumesaki.net/about/terms/credit/

## Wi-Fi 設定

初回起動時、または Wi-Fi が未設定の場合は、本体が設定用アクセスポイントを起動します。

```text
SSID: StackChan-Direct
Password: stackchan123
URL: http://192.168.4.1/wifi
```

本体の Network 画面では、`Wi-Fi QR` で `StackChan-Direct` に接続し、続けて
`Setup QR` で `http://192.168.4.1/wifi` を開けます。QR を使わない場合は、
スマホまたは PC から `StackChan-Direct` に接続し、ブラウザで `http://192.168.4.1/wifi` を開いてください。
設定ページでは、SSID スキャン、SSID/password の保存、保存済み Wi-Fi の編集、削除、優先度変更ができます。

## 既に導入済みの人の更新手順

Wi-Fi 設定、サーボ原点、画像データを残したまま更新する場合は、firmware bin だけを書き込みます。

### 1. 必要なもの

- M5Stack CoreS3
- データ通信できる USB-C ケーブル
- PC または Mac
- Release からダウンロードした `stackchan_cores3_firmware.bin`

### 2. esptool.py とポートを準備する

初回インストール手順の「esptool.py を準備する」と「CoreS3 を USB で接続する」を確認してください。

### 3. firmware を書き込む

`stackchan_cores3_firmware.bin` は flash offset `0x10000` に書き込みます。

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

macOS の例:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

Windows の例:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

## 新しく入れる人のインストール手順

初めて使う人は、次の順番で書き込んでください。

### 1. 必要なもの

- M5Stack CoreS3
- データ通信できる USB-C ケーブル
- PC または Mac
- Release からダウンロードした `stackchan_cores3_factory.bin`

### 2. esptool.py を準備する

Python が使える環境で、`esptool.py` をインストールします。

```sh
python3 -m pip install esptool
```

インストールできたか確認します。

```sh
esptool.py version
```

### 3. CoreS3 を USB で接続する

CoreS3 を PC/Mac に USB で接続します。

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

`esptool.py` を使って、factory image を flash offset `0x0` に書き込みます。
新規インストールでは `stackchan_cores3_factory.bin` を使ってください。

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

macOS の例:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

Windows の例:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

書き込み後、本体は自動的に再起動します。書き込みに失敗する場合は、USB ケーブルがデータ通信対応か、
ほかのシリアルモニタやアプリがポートを使用していないか確認してください。

### 5. Wi-Fi を設定する

再起動後、Wi-Fi 未設定の場合は `StackChan-Direct` が表示されます。
本体の Network 画面で `Wi-Fi QR` を読み取って接続し、続けて `Setup QR` を読み取ると設定ページを開けます。
QR を使わない場合は、スマホまたは PC から `StackChan-Direct` に接続し、ブラウザで次を開いてください。

```text
http://192.168.4.1/wifi
```

画面に従って、普段接続したい Wi-Fi の SSID/password を保存します。

### 6. 会話クライアントを接続する

このファームウェア単体では会話機能は動作しません。
対応スマホアプリは今後アップロード予定です。
ご自身の WebSocket または USB Serial クライアントを接続したい場合は、API 仕様を
[README.md](../README.md#接続口) と
[好感度 API 詳細](device_affection_api.ja.md) で確認してください。USB Serial の frame 仕様は
[USB Serial Protocol](usb_serial_protocol.ja.md) を参照してください。

## 基本の使い方

- タッチ画面の端からフリックすると、設定画面を開閉できます。
- 設定画面の下部タブで `Network`、`Display`、`Audio`、`Servo`、`Pwr` を切り替えます。
- `Network` では、現在の Wi-Fi モード、IP アドレス、WebSocket 接続先、Wi-Fi 設定用 QR を確認できます。
- Wi-Fi 未設定時は、本体が `StackChan-Direct` という設定用 Wi-Fi を出します。スマホや PC をそこへ接続し、`http://192.168.4.1/wifi` を開くと Wi-Fi 設定ができます。
- `Network` 画面が SoftAP モードのときは、`Wi-Fi QR` で `StackChan-Direct` に接続し、続けて `Setup QR` で設定ページを開けます。
- `Network` 画面を長押しすると、通常接続用の STA モードと設定用の SoftAP モードを切り替えます。
- `Display` では、画面の明るさと画面オン/オフを変更できます。
- `Audio` では、スピーカー音量を変更できます。
- `Servo` では、顔の基準位置を保存したり、保存済み位置へ戻したりできます。
- `Pwr` では、バッテリー、温度、低電力モードを確認/変更できます。
- 低電力モードをオンにすると、画面の最大輝度を抑え、待機中の表情更新と首振り動作を減らします。
  発話中の音声再生と口パクは継続します。
- 電源ボタンを短押しすると、画面のオン/オフを切り替えます。
- 画面オフ中は、なでなでやフリフリの反応は発生しません。画面をオンに戻すと通常通り反応します。
- WebSocket または USB Serial クライアント接続中は、表情画面右側のマイク表示をタップすると、マイク送信のミュート/解除を切り替えます。
- WebSocket または USB Serial クライアント接続中は、マイク表示の上にあるカメラ表示をタップすると `camera_button` イベントを送ります。
  ネットワーク接続のクライアントは HTTP `POST /capture`、USB Serial クライアントは `capture.request` で JPEG を取得します。

### 7. サーボ原点を調整する

顔の向きが正面からずれている場合は、本体の `Servo` 画面で調整できます。

1. 手で顔を正面の少し上向きに合わせます。
2. `Save Direction` を押して、その向きを顔の基準位置として保存します。
3. `Go to Saved` を押すと、保存済みの基準位置に戻ります。
