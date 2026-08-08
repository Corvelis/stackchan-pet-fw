# 顔レンダラー v2 設計

[English](face_renderer_v2.md) | [日本語](face_renderer_v2.ja.md)

顔画像v2は、旧静止画表情を使わず、通常会話とリアクションをアニメーション画像で表示します。通常配布用画像はv2へ移行済みです。

## 方針

- 通常表示、口パク、まばたきは`base_m*_e*`だけで構成します。
- なでなで、方向、混乱はそれぞれ完結したアニメーションセットを使います。
- 好感度、認証、熱、低電力、撮影状態は旧静止顔の選択条件にしません。
- 熱保護、低電力動作、好感度計算、撮影処理そのものは継続します。
- キャッシュは高速化のためだけに使い、確保できない場合も同じv2画像をLittleFSから直接描画します。
- 起動時に一つのレンダラーを選び、新旧画像をフレーム単位で混ぜません。

## 起動時レンダラー選択

1. 正しい`face_assets.json`と全必須画像: `animated`
2. manifestなしで移行用`voice_m*_e*` 16枚が完全: `transition`
3. manifestなしで`idle`、`listen`、`talk_0`、`talk_1`、`blink`が完全: `legacy`
4. その他: 画像に依存しない`emergency`

manifestが存在するのに壊れている場合は`emergency`とし、欠けたv2画像だけを旧画像で補いません。classicビルドは画像構成にかかわらず`classic`です。

## v2画像セット

| グループ | CoreS3 | StopWatch | AtomS3R | ファイル名 |
| --- | ---: | ---: | ---: | --- |
| 基本 | 16 | 16 | 16 | `base_m0_e0.jpg` ... `base_m3_e3.jpg` |
| なでなで | 16 | 16 | 16 | `pet_anim_0.jpg` ... `pet_anim_15.jpg` |
| 方向 | 17 | 9 | 17 | `dir0.jpg` ... |
| 中央blink | 1 | 1 | 1 | CoreS3/AtomS3R=`blink16.jpg`、StopWatch=`blink8.jpg` |
| 混乱 | 15 | 15 | 15 | `dizzy_01.jpg` ... `dizzy_15.jpg` |
| 必須合計 | 65 | 57 | 65 | |
| 旅モード静止表情 | 9 | 9 | なし | `travel_wink.jpg` ... `travel_peace.jpg` |
| 旅モードピッカー | 2 | 2 | なし | `travel_picker_page_0.jpg`, `travel_picker_page_1.jpg` |
| 0.5.0配布合計 | 76 | 68 | 65 | |

`base_m{mouth}_e{eye}`は口4段階×目4段階です。なでなでは左上から行優先で16コマを並べます。StopWatchは正規17方向から8方向＋中央へ変換します。
旅モード11ファイルはCoreS3／StopWatchの追加セットで、起動時manifestの必須65／57枚には含めません。これによりfirmware-only更新で旧LittleFSを維持した端末も起動でき、存在する旅画像だけを利用できます。0.5.0の配布用`data*`は11ファイルをすべて含みます。

## manifestと検証

manifestは`schemaVersion=2`、`renderer=animated`、対象機種、キャンバスサイズ、各グループのパターンと枚数を持ちます。機械可読schemaは`tools/face_image_builder/face_assets_v2.schema.json`です。

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
```

## 自分の顔画像を作る

[顔画像ビルダー](../tools/face_image_builder/README.md)には、参考画像から基本顔・なでなで・方向・
中央blink・混乱・旅モード表情のスプライトシートを作るプロンプト、3×3／4×4／5×5分割CLI、
旅モードピッカー生成、公開サンプル、3機種向けのリサイズ・命名変換・manifest生成手順があります。

生成途中のファイルはGit管理外の`face_assets_v2_work/`、実機確認用の完成セットは
`data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/`を使ってください。
一部の画像だけを既存セットへ混ぜず、検証を通過した完全なv2セット単位で差し替えます。

## 状態とリアクション

- 基本状態: idle、listening、speaking
- アニメーション: petting、shake recovery、guruguru direction、dizzy
- 旅モード: 選択した`travel_*`または再利用表情を静止表示し、通常の口パクとまばたきを停止
- タイムキーパー結果: `pet_anim_0,8,9,10,9,8,0`を専用笑顔として再利用
- 状態overlay: バッテリー、マイク、カメラ、熱、低電力
- その他overlay: 吹き出し、時刻、歩数、好感度変化、タイムキーパーUI

shake recoveryは閉じ目から通常目へ`e3 → e2 → e1 → e0`で戻ります。マイク接続中でも旧`idle`/`blink`へ切り替えません。

なでなでは、入力開始から3秒未満で終了すると`pet_anim_12..15`の不満リアクション、
3秒以上続けて終了すると`pet_anim_8..11`の喜びリアクションへ進みます。

タイムキーパーと旅モードでは、画面領域を競合させないためマイク／カメラ表示と吹き出しを抑止します。タイムキーパーUIはフル画面canvasのframe overlayとして顔と一度に転送し、overlayだけの更新でも黒い中間frameを露出しないよう合成します。StopWatchではタイムキーパー中の好感度差分表示を下へずらします。

## 互換期間

`face`と`face_mode`は1〜2マイナーリリースの間、意味ベースの互換アダプタとして残します。legacy rendererは旧基本5枚だけを対象とし、`good_*`、`bad_*`、`photo_*`、tier、thermal、low-power画像を現行配布へ戻しません。
