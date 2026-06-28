# 顔画像ファイル棚卸し

このメモは、実機アップロード用の顔画像ディレクトリ、ローカル差し替え用ディレクトリ、
画像生成ツールの役割を分けて整理するための棚卸しです。コード上での表情グループごとの
使われ方は `docs/face_image_usage_analysis.ja.md` に分けてまとめています。

## 結論

- 実機が直接読む LittleFS パスは `/idle.png` のような論理名です。後方互換のため `.png` 名を使いますが、実ファイルは `.jpg` に統一しています。
- 通常の配布・通常ビルドで使う画像は `data/`, `data_stopwatch/`, `data_atoms3r/` です。
- ローカル差し替え画像は `data_local/`, `data_stopwatch_local/`, `data_atoms3r_local/` です。これらは GitHub に上げず、GitHub Releases の配布物にも含めません。
- 現在の通常3ディレクトリは、それぞれ106個の JPG を持ちます。内訳は基本顔48、ぐるぐる方向17、ぐるぐるblink17、なでなで9、混乱15です。
- `assets/` や `tools/face_image_builder/sprite_sheets/` は素材・作業用です。ファームウェアからは直接読みません。

## デバイス別ディレクトリ

| 用途 | CoreS3 + ｽﾀｯｸﾁｬﾝ | StopWatch | AtomS3R Chatbot |
| --- | --- | --- | --- |
| 通常フォルダ | `data/` | `data_stopwatch/` | `data_atoms3r/` |
| ローカル差し替え | `data_local/` | `data_stopwatch_local/` | `data_atoms3r_local/` |
| 推奨サイズ | 240 x 240 | 386 x 386 | 128 x 128 |
| GitHub Releases | 含める | 含める | 含める |
| Git管理 | 含める | 含める | 含める |

`*_local` フォルダは `.gitignore` で無視します。通常の `pio run -e <env> -t uploadfs` は
通常フォルダを使い、ローカル差し替えを使う時だけビルド時に明示します。

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_atoms3r_local pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

GitHub Releases 用の `scripts/build_release_bins.sh all` は `STACKCHAN_FACE_DATA_DIR` を指定せず、
通常フォルダの画像を含めて生成します。

## 現在のファイル構成

各通常フォルダのトップレベルはフラットに保ちます。サブフォルダや素材動画は入れません。

| グループ | 個数 | ファイル名 |
| --- | ---: | --- |
| 基本顔 | 48 | `idle.jpg`, `talk_0.jpg`, `photo_master_1.jpg`, `low_power_blink.jpg` など |
| ぐるぐる方向 | 17 | `dir0.jpg` ... `dir16.jpg` |
| ぐるぐるblink | 17 | `blink0.jpg` ... `blink16.jpg` |
| なでなでアニメーション | 9 | `pet_anim_0.jpg` ... `pet_anim_8.jpg` |
| ぐるぐる混乱ソース | 15 | `dizzy_01.jpg` ... `dizzy_15.jpg` |

合計は `48 + 17 + 17 + 9 + 15 = 106` です。

StopWatch は実行時のぐるぐる方向が8方向+中央ですが、画像生成フローを統一するため
`dir0..dir16` と `blink0..blink16` を同じように持ちます。

## ビルド時の扱い

各 env の `extra_scripts` は、選択された画像フォルダを `.pio/generated_data_*` へコピーしてから
LittleFS 用ディレクトリにします。これにより、通常フォルダと `*_local` フォルダが混ざらないようにしています。

| env | デフォルト入力 | 一時出力 |
| --- | --- | --- |
| `m5stack-cores3` | `data/` | `.pio/generated_data_cores3/` |
| `m5stack-stopwatch` | `data_stopwatch/` | `.pio/generated_data_stopwatch/` |
| `m5stack-atoms3r-chatbot` | `data_atoms3r/` | `.pio/generated_data_atoms3r/` |

`FaceController` と周辺処理は `/idle.png` のような論理パスを使います。ファイルが `.png` として
存在しない場合は、同じstemの `.jpg` を探して読みます。

## 作業用素材との分離

| 場所 | 役割 | GitHubに含めるか |
| --- | --- | --- |
| `data/`, `data_stopwatch/`, `data_atoms3r/` | 通常配布用の完成JPG | 含める |
| `data_local/`, `data_stopwatch_local/`, `data_atoms3r_local/` | 個人・ローカル用の完成JPG | 含めない |
| `assets/` | 元画像や過去作業素材 | 原則として素材管理方針に従う |
| `asset_backups/`, `backups/` | 復元・変換前バックアップ | 含めない |
| `tools/face_image_builder/sprite_sheets/` | 生成済みスプライトシートの一時置き場 | 実画像は含めない |
| `tools/face_image_builder/generate_sprite_sheet/animation_prompts/` | 追加アニメ用プロンプト | 含める |
| `tools/face_image_builder/build_faces_from_sprite_sheet/samples/` | 公開可能なサンプル入力 | 含める |

## 再チェック

画像を更新した場合は、次を確認します。

- 通常フォルダと `*_local` フォルダをビルド時に明示的に選べること。
- 通常フォルダにローカル専用キャラクター素材が混ざっていないこと。
- `data/`, `data_stopwatch/`, `data_atoms3r/` がそれぞれ106個の `.jpg` を持つこと。
- 画像サイズが対象デバイスの推奨サイズに揃っていること。
- `.png`, `.webp`, `.qoi`, `.mp4`, `.webm` など、実機投入不要なファイルが完成ディレクトリ直下に残っていないこと。
