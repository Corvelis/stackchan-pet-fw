# Stack-chan 顔画像分割CLI

[English](README.en.md)

`build_faces.py` は、画像生成AIなどで作った正方形の 6x6 顔スプライトシートから、
Stack-chan pet firmware の基本顔48枚を切り出すCLIです。
デフォルト出力は CoreS3 用の 240x240 です。

36セルを切り出したあと、ファームウェアが参照する追加画像をコピー補完し、合計48枚を作ります。
`build_faces.py` の出力は PNG です。ファームウェア投入用ディレクトリへ入れる時は
`prepare_firmware_assets.py` で JPG に統一します。

スプライトシートの作成方法は `tools/face_image_builder/generate_sprite_sheet/` に置いてあります。

このフォルダは、完成したスプライトシートをファームウェア用画像へ変換するためのものです。

6x6 の基本顔シートは `build_faces.py`、ぐるぐる・なでなで・混乱などの追加アニメーションシートは `split_firmware_sheet.py` を使います。

## まず使うコマンド

実行環境はPython 3 が動く macOS / Windows / Ubuntu を想定しています。

初回だけ、依存ライブラリを入れます。

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

画像生成AIで作ったスプライトシートを CoreS3 用の `data/` に入れる場合は、まず作業用フォルダへ出力し、
次にファーム投入用JPGへ変換します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces data --target cores3 --format jpg --clean
```

上記のコマンドで、36セルの切り出し、顔位置合わせ、顔サイズ正規化、コピー補完まで行い、
ファームウェアが参照する基本顔48枚を `data/` にJPGとして入れます。

`--backup-existing` オプションを `--install-to` と一緒に使うと、既存の `*.png` は
`backups/data_faces_YYYYMMDD_HHMMSS/` に退避されます。最終JPG運用では、作業用フォルダへ
出してから `prepare_firmware_assets.py` で反映する方が安全です。

StopWatch / AtomS3R Chatbot 用に出力する場合は、`--output-size` と変換先を変えます。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_stopwatch --detect-grid --clean --output-size 386
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_stopwatch data_stopwatch --target stopwatch --format jpg --clean

python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_atoms3r --detect-grid --clean --output-size 128
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_atoms3r data_atoms3r --target atoms3r --format jpg --clean
```

追加アニメーション用のシートを分割する場合は `split_firmware_sheet.py` を使います。

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/petting/petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/dizzy/dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
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

※顔位置合わせ用にAIモデルを使用していますが、モデルファイルは、`build_faces.py` の初回実行時に自動でダウンロードします。

生成後、LittleFSへアップロードします。

```bash
pio run -e <env> -t uploadfs
```

ファームウェア本体も更新する場合:

```bash
pio run -e <env> -t upload
```

## 出力を確認してから使う場合

いきなり実機用の画像ディレクトリに入れず、作業用フォルダで確認したい場合は `--out` を使います。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
```

問題なければ、`prepare_firmware_assets.py` で対象デバイス用ディレクトリにJPGとして反映します。

## 仮想環境を使う場合

macOS / Ubuntu:

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

Windows PowerShell:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

## よく使う調整

グリッド線やセル端のにじみが残る場合:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --grid-line-padding 14 --clean
```

顔の周辺が切れすぎる、または余白が多すぎる場合:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --cell-scale 0.94 --clean
```

補完色を変える場合:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --pad-color 0,0,0 --clean
```

## デバッグ

ファイルを書かず、crop座標と出力予定ファイル名だけ確認します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --dry-run
```

顔検出の矩形を確認する画像を出します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --debug-detections --clean
```

`debug_face_detections.png` が出力先に生成されます。

## サンプル

`samples/` には、このCLIの入力として使えるスプライトシート例を置きます。
用途ごとに下記の5種類へ分ける構成を推奨します。

```text
samples/
  base_faces_6x6/
    sprite_sheet_sample_01.png
    sprite_sheet_sample_02.png
    sprite_sheet_sample_03.png
    sprite_sheet_sample_04.png
  guruguru_dir_5x5/
  guruguru_blink_5x5/
  petting_3x3/
  dizzy_4x4/
```

既存の公開用サンプル01〜04は、基本顔 6x6 シートなので `base_faces_6x6/` に置く想定です。

各フォルダの用途:

| フォルダ | 用途 |
| --- | --- |
| `base_faces_6x6/` | `build_faces.py` で基本顔48枚へ分割する6x6シート |
| `guruguru_dir_5x5/` | `split_firmware_sheet.py` で `dir0..dir16` へ分割する5x5シート |
| `guruguru_blink_5x5/` | `split_firmware_sheet.py` で `blink0..blink16` へ分割する5x5シート |
| `petting_3x3/` | `split_firmware_sheet.py` で `pet_anim_0..pet_anim_8` へ分割する3x3シート |
| `dizzy_4x4/` | `split_firmware_sheet.py` で `dizzy_01..dizzy_15` へ分割する4x4シート |

サンプルを使った確認:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/build_faces_from_sprite_sheet/samples/base_faces_6x6/sprite_sheet_sample_01.png --out output_faces --detect-grid --clean
```

## 主なオプション

| オプション | 既定値 | 説明 |
| --- | --- | --- |
| `--out` | `data` | 作業用の出力先ディレクトリ |
| `--install-to` | なし | 48枚のPNGを指定ディレクトリへ直接配置 |
| `--backup-existing` | なし | 出力先の既存PNGをバックアップ |
| `--backup-dir` | `backups` | バックアップ作成先 |
| `--detect-grid` | なし | 画像内のグリッド線を検出して切り出す |
| `--grid-line-padding` | `12` | 検出したグリッド線の内側からさらに削るpx |
| `--cell-scale` | `0.92` | 固定cropサイズの倍率 |
| `--output-size` | `240` | 出力画像の一辺のpx |
| `--pad-color` | `0,0,0` | 顔サイズ補正時の補完色 |
| `--clean` | なし | 出力先の既存PNGを削除してから生成 |
| `--dry-run` | なし | ファイルを書かずにcrop計画を表示 |
| `--debug-detections` | なし | 顔検出デバッグ画像を生成 |
| `--mapping-json` | `face_mapping.json` | 36セルのファイル名マッピングをJSONで上書き |
| `--copy-json` | `face_copy_rules.json` | コピー補完ルールをJSONで上書き |

通常は `--detect-grid --clean` を使います。画像によってズレがある場合だけ `--grid-line-padding` と `--cell-scale` を調整してください。

## 36セルの読み取り順

読み取り順は左上から右へ、行末まで行ったら次の行へ進む row-major です。

| # | ファイル名 |
| --- | --- |
| 1 | `idle.png` |
| 2 | `listen.png` |
| 3 | `talk_0.png` |
| 4 | `talk_1.png` |
| 5 | `blink.png` |
| 6 | `smile.png` |
| 7 | `good_0.png` |
| 8 | `good_1.png` |
| 9 | `good_blink.png` |
| 10 | `idle_guarded_0.png` |
| 11 | `talk_guarded_1.png` |
| 12 | `blink_guarded_0.png` |
| 13 | `idle_attached_0.png` |
| 14 | `talk_attached_1.png` |
| 15 | `blink_attached_0.png` |
| 16 | `pet_attached_0.png` |
| 17 | `pet_attached_1.png` |
| 18 | `pet_blink_attached_0.png` |
| 19 | `pet_guarded_0.png` |
| 20 | `pet_guarded_1.png` |
| 21 | `nadenade_0.png` |
| 22 | `nadenade_1.png` |
| 23 | `furifuri_0.png` |
| 24 | `furifuri_1.png` |
| 25 | `photo_0.png` |
| 26 | `photo_1.png` |
| 27 | `photo_blink.png` |
| 28 | `tired_0.png` |
| 29 | `tired_talk.png` |
| 30 | `tired_blink.png` |
| 31 | `photo_master_0.png` |
| 32 | `photo_master_1.png` |
| 33 | `bad_0.png` |
| 34 | `bad_1.png` |
| 35 | `exhausted_0.png` |
| 36 | `exhausted_talk.png` |

## コピー補完ルール

36セルから直接生成しないが、ファームウェアが参照する顔画像は既存PNGのコピーで補完します。

| 生成先 | コピー元 |
| --- | --- |
| `photo_blink_talk.png` | `photo_blink.png` |
| `talk_guarded_0.png` | `idle_guarded_0.png` |
| `talk_attached_0.png` | `idle_attached_0.png` |
| `pet_blink_guarded_0.png` | `pet_guarded_0.png` |
| `shake_guarded_0.png` | `furifuri_0.png` |
| `shake_guarded_1.png` | `furifuri_1.png` |
| `shake_attached_0.png` | `furifuri_0.png` |
| `shake_attached_1.png` | `furifuri_1.png` |
| `exhausted_blink.png` | `tired_blink.png` |
| `low_power_0.png` | `exhausted_0.png` |
| `low_power_talk.png` | `exhausted_talk.png` |
| `low_power_blink.png` | `tired_blink.png` |

## モデルとライセンス

顔位置合わせには `deepghs/anime_face_detection` の `face_detect_v1.4_n` ONNXモデルを使います。

- Source: https://huggingface.co/deepghs/anime_face_detection
- License: MIT

モデルは通常、`build_faces.py` の初回実行時に自動取得されます。事前に取得したい場合は `download_model.py` を実行してください。
