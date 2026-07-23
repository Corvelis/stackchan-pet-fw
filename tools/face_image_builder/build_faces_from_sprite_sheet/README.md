# スプライトシート分割CLI

`split_firmware_sheet.py`は、4×4または5×5のスプライトシートを、顔画像v2の正規フレームへ分割します。旧6×6静止顔ツールは現行ツリーから削除しており、必要な場合は`v0.3.1`タグを参照してください。

[English](README.en.md)

## セットアップ

```sh
python3 -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

## 基本顔4×4

行を口`m0..m3`、列を目`e0..e3`として出力します。

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/base_animation_4x4.png:base_ \
  --grid 4x4 --directions 16 --layout even --crop-size auto \
  --row-top-mask 6 --column-side-mask 6 \
  --size 512 --format png --output-naming base-mouth-eye \
  --out-dir face_assets_v2_work/canonical \
  --preview-dir face_assets_v2_work/previews
```

現在の同梱基本顔サンプルは、列補正なしで顔位置が揃います。`--column-x-offsets`は、別の生成画像をプレビューして列ずれが確認できた場合だけ指定してください。

## なでなで4×4

左上から右下へ行優先で`pet_anim_0..15`を出力します。

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/petting_4x4.png:pet_anim_ \
  --grid 4x4 --directions 16 --layout even --crop-size auto \
  --row-top-mask 6 --column-side-mask 6 \
  --size 512 --format png \
  --out-dir face_assets_v2_work/canonical \
  --preview-dir face_assets_v2_work/previews
```

## ぐるぐる方向・blink

5×5から外周16方向と中央を`dir0..16`または`blink0..16`へ出力します。

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/guruguru_5x5.png:dir \
  --grid 5x5 --directions 17 --target cores3 --format jpg \
  --out-dir face_assets_v2_work/canonical
```

blinkも同じコマ順で、prefixを`blink`に変更します。

## 混乱4×4

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/dizzy_4x4.png:dizzy_ \
  --grid 4x4 --directions 15 --layout even \
  --start-index 1 --pad 2 --size 512 --format png \
  --out-dir face_assets_v2_work/canonical
```

## 境界片の補正

- `--layout even`: 等間隔のセルとして分割します。
- `--cell-inset`: 全セルの切り出し範囲を内側へ寄せます。
- `--row-top-mask`: 2行目以降の上端を黒で覆います。
- `--column-side-mask`: 内部列境界の左右を黒で覆います。
- `--column-x-offsets`: 列単位で画像を水平移動します。
- `--preview-dir`: 分割結果のプレビューを保存します。

顔の位置・周囲のコマの混入・下端をプレビューで確認してから採用してください。出力先は既定で同prefixの既存画像を置き換えます。保持したい場合は`--no-clean`を指定します。

## サンプル

| フォルダ | 用途 |
| --- | --- |
| `samples/base_animation_4x4/` | 基本顔4×4 |
| `samples/petting_4x4/` | なでなで4×4 |
| `samples/guruguru_dir_5x5/` | 方向顔5×5 |
| `samples/guruguru_blink_5x5/` | 閉じ目方向顔5×5 |
| `samples/dizzy_4x4/` | 混乱4×4 |

サンプル画像の利用条件は[`ASSET_LICENSE.md`](../../../ASSET_LICENSE.md)を参照してください。
