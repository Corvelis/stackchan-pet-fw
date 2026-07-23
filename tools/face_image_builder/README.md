# Stack-chan 顔画像ビルダー

このディレクトリは、アニメーション用スプライトシートから顔画像v2を作るためのツールとサンプルをまとめています。旧6×6静止顔、旧3×3なでなで、好感度・熱・低電力・撮影状態ごとの静止顔は現行配布では使いません。旧手順が必要な場合は`v0.3.1`タグを参照してください。

[English](README.en.md)

## 顔画像v2の構成

| グループ | 正規ソース | 出力 |
| --- | --- | --- |
| 基本アニメーション | 4×4、行=口、列=目 | `base_m0_e0.jpg` ... `base_m3_e3.jpg` |
| なでなで | 4×4、行優先 | `pet_anim_0.jpg` ... `pet_anim_15.jpg` |
| ぐるぐる方向 | 5×5から17コマ | `dir0.jpg` ... `dir16.jpg` |
| 中央blink | 5×5から中央を使用 | CoreS3/AtomS3Rは`blink16.jpg`、StopWatchは`blink8.jpg` |
| 混乱 | 4×4から15コマ | `dizzy_01.jpg` ... `dizzy_15.jpg` |

CoreS3とAtomS3Rは65画像、StopWatchは57画像と`face_assets.json`を使います。

## 推奨ワークフロー

1. [`generate_sprite_sheet/animation_prompts/`](generate_sprite_sheet/animation_prompts/) のプロンプトと自分の参考画像でスプライトシートを作ります。
2. [`split_firmware_sheet.py`](build_faces_from_sprite_sheet/split_firmware_sheet.py) でCoreS3相当の正規ソースへ分割します。
3. `generate_v2_face_assets.py`で3機種向けにリサイズ・命名変換・manifest生成を行います。
4. 検証後、`data_local*`へ配置して実機確認します。
5. 配布用画像として採用する場合だけ、`data/`、`data_stopwatch/`、`data_atoms3r/`をディレクトリ単位で置き換えます。

```sh
python3 -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt

python3 scripts/generate_v2_face_assets.py \
  face_assets_v2_work/canonical all --replace

python3 scripts/validate_face_assets.py \
  face_assets_v2_work/generated/cores3 --target cores3 --mode v2
```

既存の完全な129枚transitionセットを、JPEGを再圧縮せずv2へ移行する場合は、
`legacy_face_assets_local/`直下に`data/`、`data_stopwatch/`、`data_atoms3r/`を置いて次を使います。
通常の`data*`を直接変更せず、Git管理外の作業フォルダへ出力します。

```sh
python3 scripts/promote_transition_face_assets.py all \
  --source-root legacy_face_assets_local --replace
```

## 実機確認

```sh
STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/cores3 \
  pio run -e m5stack-cores3 -t uploadfs

STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/stopwatch \
  pio run -e m5stack-stopwatch -t uploadfs

STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/atoms3r \
  pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

起動後、`device.info.faceRendererMode`が`animated`であることを確認してください。

## フォルダ

```text
face_image_builder/
  face_assets_v2.schema.json
  prepare_firmware_assets.py
  generate_sprite_sheet/
    animation_prompts/
  build_faces_from_sprite_sheet/
    split_firmware_sheet.py
    requirements.txt
    samples/
      base_animation_4x4/
      petting_4x4/
      guruguru_dir_5x5/
      guruguru_blink_5x5/
      dizzy_4x4/
```

サンプル画像はリポジトリ直下の[`CC0 1.0`](../../ASSET_LICENSE.md)で自由に利用できます。
