# ローカルスプライトシート置き場

このフォルダは、画像生成AIで作ったスプライトシートを一時的に置いて、`build_faces_from_sprite_sheet/split_firmware_sheet.py` や `build_faces.py` に渡すためのローカル入力置き場です。

生成済み画像はGitHubに上げない方針なので、このフォルダ配下の実画像や動画は `.gitignore` で無視します。管理するのはフォルダ構成とこのREADMEだけです。

## 推奨構成

```text
tools/face_image_builder/sprite_sheets/
  base_faces/
    sprite_sheet.png
  guruguru/
    sheet.png
    blink.png
  petting/
    petting.png
  dizzy/
    dizzy_contact.png
```

## 分割例

基本顔 6x6:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/sprite_sheets/base_faces/sprite_sheet.png --out output_faces_local --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_local data_local --target cores3 --format jpg --clean
```

ぐるぐる 5x5:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

ぐるぐる blink 5x5:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

なでなで 3x3:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/petting/petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

混乱 4x4:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/dizzy/dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

`--target` は `cores3`、`stopwatch`、`atoms3r` に変更できます。
`--out-dir` は通常配布用なら `data/`, `data_stopwatch/`, `data_atoms3r/`、ローカル差し替え用なら
`data_local/`, `data_stopwatch_local/`, `data_atoms3r_local/` を指定します。
