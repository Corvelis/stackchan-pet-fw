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
