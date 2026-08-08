# Stack-chan Multi-Device Controller 0.5.0

[English](release_notes_0.5.0.md) | [日本語](release_notes_0.5.0.ja.md)

0.5.0では、CoreS3とM5Stack StopWatchに4つの体験モード、タイムキーパー、旅モードを追加します。AtomS3R Chatbotは従来の会話／ぐるぐる機能を維持します。

## 機種別の変更範囲

| 機種 | 0.5.0での変更 |
| --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | 4体験モード、タイムキーパー、旅モード、旅画像、関連APIを追加 |
| M5Stack StopWatch | 4体験モード、タイムキーパー、旅モード、旅画像、関連APIを追加 |
| AtomS3R Chatbot | 配布バイナリのversionを0.5.0へ更新し、全機種共通の`device.info.bootId`を追加。ユーザー向けの会話／ぐるぐる機能、capability、65枚の顔画像は従来どおり |

AtomS3Rには4分割モード選択、タイムキーパー、旅モード、旅画像、`experience.mode.v1`などの新しいcapabilityは追加しません。

## 主な追加

### 4つの体験モード

- 会話、ぐるぐる、タイムキーパー、旅モードを独立した最上位モードとして管理。
- CoreS3は画面左端から右フリック、StopWatchは黄色のBtnAを約0.7秒長押しして選択。
- 音声再生中のモード変更は再生終了まで遅延。
- モード変更時に`experience.mode.changed`を送信。

操作の詳細は[体験モードと本体操作](experience_modes.ja.md)を参照してください。

### タイムキーパー

- ストップウォッチ、ラップ、経過マイルストーン。
- 10秒〜120分のカウントダウンと、1／3／5／10／30／60／120分プリセット。
- 10／30／60秒の時間当てチャレンジ、3難易度、6ランク、好感度報酬。
- 作業／休憩を自動遷移するポモドーロ。初期値25分／5分／4サイクル。
- CoreS3の背面タッチ、StopWatchの黄色／青色ボタンを使った端末別操作。
- 画面OFFまたはモード変更時の安全な一時停止／中断。
- 誤差200ms以内のチャレンジ結果で専用笑顔アニメーション。

### 旅モード

- CoreS3／StopWatchへ9つの新規表情と6つの再利用表情、合計15表情を追加。
- 写真向け／気分・ネタの2ページピッカー。
- 選択した顔を口パク／まばたきなしで静止表示。
- 同じ人物の9表情を生成する3×3プロンプト、行優先分割、端末別ピッカー生成ツール。

配布画像数はCoreS3が76 JPG、StopWatchが68 JPGです。AtomS3Rは従来どおり65 JPGです。

## アプリ連携

全3機種の`device.info`へ、起動ごとに変わる`bootId`を追加します。

CoreS3とStopWatchの`device.info`には、さらに次を追加します。

- `experienceMode`／`experienceModeRevision`
- 保存中の`pomodoro`設定
- `experience.mode.v1`
- `device.communication.suspending.v1`
- `timekeeper.v1`
- `timekeeper.pomodoro.v1`

新しい主なJSONは次のとおりです。

- `experience.mode.changed`
- `timekeeper.event`
- `timekeeper.announcement.prefetch`／`timekeeper.announcement.result`
- `timekeeper.pomodoro.config.get`／`.set`／`.result`
- `device.communication.suspending`

カウントダウン完了とポモドーロ全体完了は、アプリの処理結果を受け取るまで最大120秒保持します。詳細は[タイムキーパー・体験モード通信仕様](timekeeper_protocol.ja.md)を参照してください。

## 表示と既存機能

- タイムキーパーUIを顔と同じフル画面canvasへ合成し、overlay更新時のちらつきを抑制。
- タイムキーパー／旅モードでは、専用画面を隠さないようマイク、カメラ、発話吹き出しを非表示。
- 同モードではなでなでとふりふりを抑止。
- CoreS3の電源ダブルクリックは、新しいモード管理を通して会話／ぐるぐるを切り替え。
- 従来の好感度、StreetPass、StopWatch歩数、スマホカメラ遠隔操作、CoreS3サーボ／カメラは継続。

## 更新方法とLittleFS

GitHub Releasesでは機種ごとに`firmware`と`factory`を用意します。

| 更新方法 | ファームウェア | LittleFS | 旅モード |
| --- | --- | --- | --- |
| `firmware` | 0.5.0へ更新 | 既存内容を保持 | 0.4.1からの更新では新しいピッカー画像なし。利用可能な既存表情へfallback |
| `factory` | 0.5.0へ更新 | 0.5.0画像へ置換 | 全15表情と2ページピッカーを利用可能 |
| sourceから`uploadfs` | 変更なし | 対象`data*`を書き込み | 全旅画像を追加可能 |

0.4.1の顔画像v2だけを入れた端末でも、0.5.0 firmwareは起動できます。旅モードを完全に利用したい場合はfactory更新または`uploadfs`が必要です。factory更新では保存済み設定を含むflash全体を書き換えるため、必要な設定を控えてから実行してください。

詳しい書き込み方法は[バイナリ版インストール手順](install_binary.ja.md)を参照してください。

## 開発・画像ツール

- `travel_3x3_prompt.txt`
- `split_firmware_sheet.py --output-naming travel-expressions`
- `build_travel_picker_pages.py`
- 旅モードの命名、3×3 grid、ピッカー寸法／配置テスト
- 旅モード11ファイルの全件／0件検証。部分構成は配布前検証で失敗
- ストップウォッチ、カウントダウン、チャレンジ、ポモドーロ状態機械のhost C++テスト
- 日英の体験モード操作仕様とタイムキーパー通信仕様

ローカル確認用の`data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/`はGit管理およびGitHub Releasesへ含めません。通常／releaseビルドは`data/`、`data_stopwatch/`、`data_atoms3r/`を使用します。
`build_release_bins.sh`は`STACKCHAN_FACE_DATA_DIR`が設定された状態と、古いdemo動画など規定外のファイルが`dist/`に残った状態を拒否します。

## 互換性と制約

- タイムキーパーと旅モードはCoreS3／StopWatch専用です。
- AtomS3Rは`bootId`以外の新しいフィールドやcapabilityを返さず、従来の会話／ぐるぐる動作と65枚の画像構成を維持します。
- 共通APIの`protocolVersion`は`2`を維持します。新機能の有無はversion付きcapabilityで判定してください。
- v1ではアプリから体験モードを変更できません。モードは本体で選びます。
- ポモドーロの作業／休憩時間はアプリAPI、サイクル数は本体UIで設定します。
- タイムキーパー／旅モード中の吹き出しcueは表示しません。
- 対応アプリはcapabilityを確認し、未知のJSONを無視してください。
- 旧`face`／`face_mode`コマンドは、1〜2マイナーリリースの間、意味ベースの互換adapterとして維持します。
- 旧versionへ戻す場合はfirmwareだけでなく、そのversionに対応するfactory imageまたはLittleFSも戻してください。顔画像v2だけを残すと旧firmwareが必要な画像を利用できない場合があります。

## 確認項目

- 3機種の画像顔／classic顔、合計6環境のビルド。
- CoreS3 76、StopWatch 68、AtomS3R 65画像の検証。
- タイムキーパー状態機械を含むhost自動テスト。
- 旅モードの分割、ピッカー配置、完全セット検証。
- 日英ドキュメントペアとローカル絶対パス検査。
- CoreS3／StopWatchのモード選択、タイムキーパー、旅モード実機操作。

## 関連文書

- [体験モードと本体操作](experience_modes.ja.md)
- [タイムキーパー・体験モード通信仕様](timekeeper_protocol.ja.md)
- [顔画像ビルダー](../tools/face_image_builder/README.md)
- [顔画像ファイル棚卸し](face_image_inventory.ja.md)
- [バイナリ版インストール手順](install_binary.ja.md)
