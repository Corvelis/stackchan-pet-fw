# Stack-chan 顔画像分割CLI

[English](README.en.md)

`build_faces.py` は、画像生成AIなどで作った正方形の 6x6 顔スプライトシートから、
Stack-chan pet firmware の実機用画像ディレクトリに置く PNG 顔画像を生成するCLIです。
デフォルト出力は CoreS3 用の 240x240 です。

36セルを切り出したあと、ファームウェアが参照する追加画像をコピー補完し、合計48枚のPNGを作ります。

スプライトシートの作成方法は `tools/face_image_builder/generate_sprite_sheet/` に置いてあります。

このフォルダは、完成したスプライトシートをファームウェア用画像へ変換するためのものです。

## まず使うコマンド

実行環境はPython 3 が動く macOS / Windows / Ubuntu を想定しています。

初回だけ、依存ライブラリを入れます。

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

画像生成AIで作ったスプライトシートを CoreS3 用の `data/` に入れる場合は、下記のコマンドを使用します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

上記のコマンドで、36セルの切り出し、顔位置合わせ、顔サイズ正規化、コピー補完まで行い、
ファームウェアが参照する48枚のPNGを `data/` に入れます。

`--backup-existing` オプションを付けると、既存の `data/*.png` は `backups/data_faces_YYYYMMDD_HHMMSS/` に退避されます。

StopWatch / AtomS3R Chatbot 用に直接出力する場合は、出力先と `--output-size` を変えます。

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

問題なければ、上の `--install-to <画像ディレクトリ>` のコマンドで対象デバイス用ディレクトリに反映します。

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

`samples/` には、このCLIの入力として使えるスプライトシート例を置きます。公開用サンプルは01〜04です。

```text
samples/
  sprite_sheet_sample_01.png
  sprite_sheet_sample_02.png
  sprite_sheet_sample_03.png
  sprite_sheet_sample_04.png
```

サンプルを使った確認:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/build_faces_from_sprite_sheet/samples/sprite_sheet_sample_01.png --out output_faces --detect-grid --clean
```

## 主なオプション

| オプション | 既定値 | 説明 |
| --- | --- | --- |
| `--out` | `data` | 作業用の出力先ディレクトリ |
| `--install-to` | なし | 48枚を指定したファームウェア用ディレクトリへ直接配置 |
| `--backup-existing` | なし | 出力先の既存PNGをバックアップ |
| `--backup-dir` | `backups` | バックアップ作成先 |
| `--detect-grid` | なし | 画像内のグリッド線を検出して切り出す |
| `--grid-line-padding` | `12` | 検出したグリッド線の内側からさらに削るpx |
| `--cell-scale` | `0.92` | 固定cropサイズの倍率 |
| `--output-size` | `240` | 出力PNGの一辺のpx |
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
