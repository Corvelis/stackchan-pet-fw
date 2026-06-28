# アニメーション用スプライトシートプロンプト

このフォルダには、Stack-chan pet firmware の追加アニメーション画像を画像生成AIで作るためのベースプロンプトを置いています。

生成済み画像やキャラクター参照画像はGitHubに入れず、ローカルの `data_local/`、`data_stopwatch_local/`、`data_atoms3r_local/`、または外部バックアップに置いてください。このフォルダに置くのはプロンプト本文だけです。

## ファイル

```text
animation_prompts/
  README.md
  guruguru_5x5_prompt.txt
  guruguru_blink_5x5_prompt.txt
  petting_3x3_prompt.txt
  dizzy_4x4_prompt.txt
```

## 使い分け

- `guruguru_5x5_prompt.txt`: ぐるぐる追従用の `dir0..dir16` を作るための5x5シート。
- `guruguru_blink_5x5_prompt.txt`: ぐるぐる追従のblink用 `blink0..blink16` を作るための5x5シート。
- `petting_3x3_prompt.txt`: なでなで用の `pet_anim_0..pet_anim_8` を作るための3x3シート。
- `dizzy_4x4_prompt.txt`: ぐるぐる混乱用の `dizzy_01..dizzy_15` を作るための4x4シート。

| プロンプト | 作る画像 | ファームウェア上の用途 | 分割後のファイル |
| --- | --- | --- | --- |
| `guruguru_5x5_prompt.txt` | 5x5 の顔向き差分。目は開いた通常顔。 | ぐるぐるモードで、端末の傾き/タッチ位置に合わせて顔の向きを変える。 | `dir0.jpg` ... `dir16.jpg` |
| `guruguru_blink_5x5_prompt.txt` | 5x5 の顔向き差分。目を閉じたblink顔。 | ぐるぐるモード中のまばたき。`dir` と同じ向き順で作る。 | `blink0.jpg` ... `blink16.jpg` |
| `petting_3x3_prompt.txt` | 3x3 のなでなで反応アニメーション。 | なでなで入力時の表情変化。 | `pet_anim_0.jpg` ... `pet_anim_8.jpg` |
| `dizzy_4x4_prompt.txt` | 4x4 の混乱アニメーション。 | ぐるぐる後に混乱状態へ入った時のループ/復帰表情。 | `dizzy_01.jpg` ... `dizzy_15.jpg` |

## サンプル画像

このプロンプトで作った公開用サンプルは、分割CLIの `samples/` 配下に置いています。
サンプルは `split_firmware_sheet.py` の入力確認用です。生成した画像が同じ構成になっているか、分割前に見比べてください。

| プロンプト | サンプル画像 | 確認するポイント |
| --- | --- | --- |
| `guruguru_5x5_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/guruguru_dir_5x5/sprite_sheet_sample_01.png` | 5x5、目が開いた方向差分、中央が正面 |
| `guruguru_blink_5x5_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/guruguru_blink_5x5/sprite_sheet_sample_01.png` | 5x5、`guruguru` と同じ向き順、目を閉じたblink差分 |
| `petting_3x3_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/petting_3x3/sprite_sheet_sample_01.png` | 3x3、なでなで反応が左上から右下へ並ぶ |
| `dizzy_4x4_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/dizzy_4x4/sprite_sheet_sample_01.png` | 4x4、混乱ループと復帰フレームが左上から順に並ぶ |

## 作り方

画像生成AIには、作りたいキャラクターの正面寄りの顔画像を1枚添付し、目的に合う `.txt` の全文を貼ります。
生成結果は、文字、番号、グリッド線、枠線が入っていない黒背景の1枚画像として保存してください。

保存先は、プロジェクト内に置く場合は `tools/face_image_builder/sprite_sheets/` を使います。
このフォルダ配下の実画像は `.gitignore` されるので、公開できないキャラクター画像を置いてもGitHubには入りません。

推奨ファイル名:

```text
tools/face_image_builder/sprite_sheets/
  guruguru/
    sheet.png
    blink.png
  petting/
    petting.png
  dizzy/
    dizzy_contact.png
```

## 分割

生成したシートは `tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py` でファーム用JPEGへ分割します。

5x5 ぐるぐる:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

5x5 ぐるぐる blink:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

3x3 なでなで:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

4x4 混乱:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

`--target` は出力サイズです。`cores3` は240x240、`stopwatch` は386x386、`atoms3r` は128x128です。
`--out-dir` は通常配布用なら `data/`, `data_stopwatch/`, `data_atoms3r/`、ローカル差し替え用なら
`data_local/`, `data_stopwatch_local/`, `data_atoms3r_local/` を指定します。

周囲の画像の破片が入る場合は、`--layout even --cell-inset 4` を付けて分割してください。
それでも残る場合は `--cell-inset 8` のように少し増やします。
