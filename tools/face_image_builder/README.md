# Stack-chan 顔画像ビルダー

[English](README.en.md)

このフォルダは、Stack-chan pet firmware 用の顔画像群を作るためのツール一式です。

画像生成AIで作ったスプライトシートから、基本顔48枚と追加アニメーションを生成し、
ファームウェアで使う106枚のJPGをデバイス別の画像ディレクトリへ配置する流れを扱います。

## 全体の流れ

1. `generate_sprite_sheet/` に入っているプロンプト等を使用し、画像生成AIで 6x6 スプライトシートを作ります。
2. `animation_prompts/` のプロンプトを使用し、ぐるぐる、ぐるぐるblink、なでなで、混乱用の追加シートを作ります。
3. `build_faces_from_sprite_sheet/` のCLIで、基本顔と追加シートを分割します。
4. 生成した画像を `.jpg` にそろえて、ファームウェアの対象デバイス用画像ディレクトリに配置します。
5. PlatformIOでLittleFSへアップロードします。

完成ディレクトリは次のどちらかに分けます。

| 用途 | CoreS3 | StopWatch | AtomS3R |
| --- | --- | --- | --- |
| 通常配布用 | `data/` | `data_stopwatch/` | `data_atoms3r/` |
| ローカル差し替え用 | `data_local/` | `data_stopwatch_local/` | `data_atoms3r_local/` |

GitHub Releases 用の配布物は通常配布用フォルダを使います。ローカル差し替え用フォルダは GitHub に上げません。

## 最短手順

依存ライブラリを入れます。

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

顔位置合わせ用モデルは、初回実行時に自動でダウンロードされます。

画像生成AIで作ったスプライトシートから、CoreS3 用の作業フォルダへ基本顔48枚を切り出し、
ファーム投入用の `data/` へJPGとして配置します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces data --target cores3 --format jpg --clean
```

StopWatch / AtomS3R Chatbot 用に出力する場合は `--output-size` と `prepare_firmware_assets.py` の
`--target` / 出力先を変えます。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_stopwatch --detect-grid --clean --output-size 386
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_stopwatch data_stopwatch --target stopwatch --format jpg --clean

python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_atoms3r --detect-grid --clean --output-size 128
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_atoms3r data_atoms3r --target atoms3r --format jpg --clean
```

既存の CoreS3 用 `data/` から StopWatch / AtomS3R Chatbot 用の画像ディレクトリをまとめて生成することもできます。

```bash
python3 scripts/generate_device_assets.py all
```

個別に生成する場合:

```bash
python3 scripts/generate_device_assets.py stopwatch
python3 scripts/generate_device_assets.py atoms3r
```

別の CoreS3 用画像ディレクトリを元にする場合は `--source` を指定します。

```bash
python3 scripts/generate_device_assets.py all --source path/to/core-data
```

ローカル差し替え用の3ディレクトリをバックアップ元から作り直す場合は `generate_local_face_assets.py` を使います。

```bash
python3 scripts/generate_local_face_assets.py path/to/backup-data all
```

既存フォルダ内の画像サイズと実ファイル形式をそろえる場合も `prepare_firmware_assets.py` を使います。
トップレベルの顔画像、なでなで、ぐるぐる用フレームだけを変換し、WebPなどの元素材は残します。

```bash
python3 tools/face_image_builder/prepare_firmware_assets.py data_local --target cores3 --format jpg --in-place
python3 tools/face_image_builder/prepare_firmware_assets.py data_stopwatch_local --target stopwatch --format jpg --in-place
python3 tools/face_image_builder/prepare_firmware_assets.py data_atoms3r_local --target atoms3r --format jpg --in-place
```

`M5-guruguru` のセパレーターと同じ考え方で、スプライトシートやコンタクトシートをファームウェア用画像へ分割する場合は `build_faces_from_sprite_sheet/split_firmware_sheet.py` を使います。ファームウェア投入用は `--format jpg` を使います。QOIは使いません。

5x5 のぐるぐる用 `dir` / `blink` シート:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet sheet.png:dir --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

3x3 のなでなでアニメーションシート:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

4x4 のぐるぐる混乱フレーム用コンタクトシートから、このファームで使う先頭15枚を出力:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

LittleFSへアップロードします。`<env>` は対象デバイスの PlatformIO env に置き換えてください。

```bash
pio run -e <env> -t uploadfs
```

別フォルダを焼く場合は、PlatformIO実行時に `STACKCHAN_FACE_DATA_DIR` を指定します。

```bash
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
```

## フォルダ構成

```text
tools/face_image_builder/
  README.md
  README.en.md
  prepare_firmware_assets.py
  sprite_sheets/
    README.md
    README.en.md
    base_faces/
    guruguru/
    petting/
    dizzy/
  generate_sprite_sheet/
    README.md
    README.en.md
    sprite_sheet_prompt.txt
    grid_template_6x6.png
    animation_prompts/
      README.md
      README.en.md
      guruguru_5x5_prompt.txt
      guruguru_blink_5x5_prompt.txt
      petting_3x3_prompt.txt
      dizzy_4x4_prompt.txt
    references/
  build_faces_from_sprite_sheet/
    README.md
    README.en.md
    build_faces.py
    split_firmware_sheet.py
    download_model.py
    requirements.txt
    face_mapping.json
    face_copy_rules.json
    models/
    samples/
      base_faces_6x6/
      guruguru_dir_5x5/
      guruguru_blink_5x5/
      petting_3x3/
      dizzy_4x4/
```

## 詳細

画像生成AIでスプライトシートを作る方法は、`generate_sprite_sheet/README.md` をご参照ください。

スプライトシートを分割して対象デバイス用画像ディレクトリに入れる方法は、`build_faces_from_sprite_sheet/README.md` をご参照ください。
