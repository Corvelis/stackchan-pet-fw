# 顔画像 v2 移行ガイド

[English](face_asset_migration.md) | [日本語](face_asset_migration.ja.md)

## 現在の段階

通常配布用の`data/`、`data_stopwatch/`、`data_atoms3r/`は、旧静止顔を含まない顔画像v2へ移行済みです。各ディレクトリには`face_assets.json`があり、通常ビルドは`animated`レンダラーを選びます。

旧フル画像セットと旧6×6／3×3生成ツールは`v0.3.1`タグから参照できます。現行Releaseとfactory imageには同梱しません。

0.5.0では、CoreS3／StopWatchの配布用画像へ旅モードの9表情と2ページのピッカーを追加します。
起動時manifestの必須枚数は後方互換のため65／57／65枚のままです。配布用ディレクトリは
CoreS3 76枚、StopWatch 68枚、AtomS3R 65枚になり、配布前検証では旅モード11枚の全件または
0件だけを許可します。部分的な旅モードセットは不正です。

## 更新方法ごとの挙動

| 操作 | LittleFS | 顔表示 |
| --- | --- | --- |
| firmware binだけ更新 | 既存画像を保持 | v2なら`animated`、移行16枚なら`transition`、旧基本5枚なら`legacy`、その他は`emergency` |
| v2の`uploadfs` | 画像領域をv2へ更新 | `animated` |
| v2 factory image | firmwareとv2画像を同時導入 | `animated` |
| v2導入後にv0.3.1以前のfirmwareだけ書く | v2画像のまま | 旧firmwareが必要な画像を見つけられない可能性あり |

firmware-only更新時のfallbackのために、旧画像を現行リポジトリへ残す必要はありません。端末のLittleFSに残る画像はfirmware更新だけでは削除されず、起動時に画像セット全体を判定します。新旧フレームを1画面内で混ぜることはありません。

0.4.1から0.5.0へfirmware binだけで更新したCoreS3／StopWatchは、既存の顔画像v2を使って起動できます。
ただし新しい旅モード画像とピッカーはLittleFSへ追加されません。旅モードを完全に導入する場合は、
0.5.0のfactory imageを使用するか、対象環境のLittleFSを`uploadfs`してください。

## ダウングレード

v2画像へ更新した端末を`v0.3.1`以前へ戻す場合は、対象バージョンのfactory imageまたはLittleFSも書き戻してください。firmware binだけのダウングレードは保証しません。

## カスタム画像

新しく自分のキャラクター画像を作る場合は、[顔画像ビルダー](../tools/face_image_builder/README.md)の
プロンプト、分割CLI、サンプル、3機種向け生成手順を使います。生成途中は`face_assets_v2_work/`、
実機確認用の完成画像はGit管理外の`data_local/`、`data_stopwatch_local/`、
`data_atoms3r_local/`へ置いてください。旧fallback確認用の基本5枚は
`legacy_face_assets_local/`へ置きます。

完全な129枚transitionセットをv2へ変換する場合は、`legacy_face_assets_local/`直下に
`data/`、`data_stopwatch/`、`data_atoms3r/`を置き、JPEGを再圧縮しない移行コマンドを使えます。

```sh
python3 scripts/promote_transition_face_assets.py all \
  --source-root legacy_face_assets_local --replace
```

## リリース確認

1. `python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v`
2. `python3 scripts/validate_face_assets.py`で3機種すべて`mode=v2`、画像数が76／68／65
3. 通常3環境とclassic 3環境をビルド
4. `bash scripts/build_release_bins.sh all`でfirmware、factory、SHA256を生成
5. 3機種でfactory imageを起動し、`device.info.faceRendererMode=animated`を確認
6. firmware-only更新で設定とLittleFSが保持されることを確認
7. 必要に応じて旧基本5枚と画像なしの起動経路を確認
8. Release notesへダウングレード方法と旧API互換期間を記載

検証では旅モード画像を11枚すべて含めるか、1枚も含めない状態だけを許可します。0.5.0の
配布用`data/`と`data_stopwatch/`には11枚をすべて含めてください。

不正なmanifestは`emergency`を選び、`device.info.faceAssetError`と`faceAssetMissing`で診断できます。
