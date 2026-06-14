# Stack-chan 顔画像ビルダー

[English](README.en.md)

このフォルダは、Stack-chan pet firmware 用の顔画像群を作るためのツール一式です。

画像生成AIで 6x6 の顔スプライトシートを作り、そこからファームウェアで使う48枚のPNGを生成して `data/` に配置する流れを扱います。

## 全体の流れ

1. `generate_sprite_sheet/` の素材を使って、画像生成AIで 6x6 スプライトシートを作ります。
2. `build_faces_from_sprite_sheet/` のCLIで、スプライトシートを48枚のPNGへ分割します。
3. 生成したPNGをファームウェアの `data/` に配置します。
4. PlatformIOでLittleFSへアップロードします。

## 最短手順

依存ライブラリを入れます。

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

顔位置合わせ用モデルは、初回実行時に自動でダウンロードされます。

画像生成AIで作ったスプライトシートから、`data/` に48枚を直接生成します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

LittleFSへアップロードします。

```bash
pio run -e m5stack-cores3 -t uploadfs
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

画像生成AIでスプライトシートを作る方法は、`generate_sprite_sheet/README.md` を読んでください。

スプライトシートを分割して `data/` に入れる方法は、`build_faces_from_sprite_sheet/README.md` を読んでください。
