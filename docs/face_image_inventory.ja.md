# 顔画像ファイル棚卸し

## 結論

- 通常配布は顔画像v2のみです。
- CoreS3とAtomS3Rは65 JPG、StopWatchは57 JPGです。
- 各ディレクトリに`face_assets.json`を置き、起動時に全体を検証します。
- 旧48静止顔、旧方向blink一式、旧6×6／3×3生成素材は現行配布へ含めません。
- 旧画像だけの端末はfirmware-only更新時の`legacy` fallbackで起動できます。旧画像をGitへ残す必要はありません。

## デバイス別ディレクトリ

| 用途 | CoreS3 + ｽﾀｯｸﾁｬﾝ | StopWatch | AtomS3R Chatbot |
| --- | --- | --- | --- |
| 配布画像 | `data/` | `data_stopwatch/` | `data_atoms3r/` |
| 個人差し替え | `data_local/` | `data_stopwatch_local/` | `data_atoms3r_local/` |
| 解像度 | 240×240 | 386×386 | 128×128 |
| 画像数 | 65 | 57 | 65 |
| レンダラー | `animated` | `animated` | `animated` |

`data_local*`、`legacy_face_assets_local/`、`face_assets_v2_work/`はGit管理外です。

## v2ファイル構成

| グループ | CoreS3 | StopWatch | AtomS3R |
| --- | ---: | ---: | ---: |
| `base_m*_e*` | 16 | 16 | 16 |
| `pet_anim_*` | 16 | 16 | 16 |
| `dir*` | 17 | 9 | 17 |
| 中央`blink*` | 1 | 1 | 1 |
| `dizzy_*` | 15 | 15 | 15 |

StopWatchは`dir0..8`と`blink8`を使います。正規ソースの`dir0,2,4,6,8,10,12,14,16`を`dir0..8`へ変換します。CoreS3とAtomS3Rは`dir0..16`と`blink16`です。

## ビルド時の扱い

各PlatformIO envは対象ディレクトリを`.pio/generated_data_*`へコピーし、LittleFS imageを作ります。別の画像セットを使う場合だけ`STACKCHAN_FACE_DATA_DIR`を指定します。

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
```

Release生成は環境変数を指定せず、通常の`data*`を使用します。

## 作業素材

| 場所 | 用途 | Git管理 |
| --- | --- | --- |
| `data*` | 配布用完成v2 | する |
| `data_local*` | 個人用完成v2 | しない |
| `face_assets_v2_work/` | 生成・検証中 | しない |
| `build_faces_from_sprite_sheet/samples/` | 公開サンプル入力 | する |
| `animation_prompts/` | 画像生成プロンプト | する |

## 再チェック

```sh
python3 scripts/validate_face_assets.py
```

- 3機種すべて`mode=v2`であること。
- `.jpg`と`face_assets.json`以外の実機不要ファイルがないこと。
- 対象解像度が正しいこと。
- 画像を変更した場合は3機種で全辺とアニメーション順を確認すること。
