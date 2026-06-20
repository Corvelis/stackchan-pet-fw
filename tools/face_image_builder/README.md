# Stack-chan 顔画像ビルダー

[English](README.en.md)

このフォルダは、Stack-chan pet firmware 用の顔画像群を作るためのツール一式です。

画像生成AIで 6x6 の顔スプライトシートを作り、そこからファームウェアで使う48枚のPNGを生成して
デバイス別の画像ディレクトリへ配置する流れを扱います。

## 全体の流れ

1. `generate_sprite_sheet/` に入っているプロンプト等を使用し、画像生成AIで 6x6 スプライトシートを作ります。
2. `build_faces_from_sprite_sheet/` のCLIで、スプライトシートを48枚のPNGへ分割します。
3. 生成したPNGをファームウェアの対象デバイス用画像ディレクトリに配置します。
4. PlatformIOでLittleFSへアップロードします。

## 最短手順

依存ライブラリを入れます。

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

顔位置合わせ用モデルは、初回実行時に自動でダウンロードされます。

画像生成AIで作ったスプライトシートから、CoreS3 用の `data/` に48枚を直接生成します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

StopWatch / AtomS3R Chatbot 用に直接出力する場合は `--output-size` と出力先を変えます。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data_stopwatch --backup-existing --detect-grid --clean --output-size 386
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data_atoms3r --backup-existing --detect-grid --clean --output-size 128
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

LittleFSへアップロードします。`<env>` は対象デバイスの PlatformIO env に置き換えてください。

```bash
pio run -e <env> -t uploadfs
```

## フォルダ構成

```text
tools/face_image_builder/
  README.md
  README.en.md
  generate_sprite_sheet/
    README.md
    README.en.md
    sprite_sheet_prompt.md
    grid_template_6x6.png
    references/
  build_faces_from_sprite_sheet/
    README.md
    README.en.md
    build_faces.py
    download_model.py
    requirements.txt
    face_mapping.json
    face_copy_rules.json
    models/
    samples/
```

## 詳細

画像生成AIでスプライトシートを作る方法は、`generate_sprite_sheet/README.md` をご参照ください。

スプライトシートを分割して対象デバイス用画像ディレクトリに入れる方法は、`build_faces_from_sprite_sheet/README.md` をご参照ください。
