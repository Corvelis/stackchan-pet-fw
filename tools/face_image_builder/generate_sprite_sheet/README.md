# アニメーション用スプライトシートの生成

このディレクトリには、顔画像v2で使うスプライトシートの生成プロンプトがあります。固定の旧6×6テンプレートは使いません。生成したいキャラクターの画像、または`references/`のサンプル参考画像を1枚添付し、対象アニメーションのプロンプトを使用してください。

[English](README.en.md)

## 作成できるシート

| プロンプト | グリッド | 用途 |
| --- | --- | --- |
| `animation_prompts/base_animation_4x4_prompt.txt` | 4×4 | 口4段階×目4段階 |
| `animation_prompts/petting_4x4_prompt.txt` | 4×4 | なでなで開始・ループ・喜び・不満 |
| `animation_prompts/guruguru_5x5_prompt.txt` | 5×5 | 16方向＋中央 |
| `animation_prompts/guruguru_blink_5x5_prompt.txt` | 5×5 | 方向顔と同じ向きの閉じ目 |
| `animation_prompts/dizzy_4x4_prompt.txt` | 4×4 | 混乱アニメーション15コマ |

## サンプル参考画像

`references/face_reference_01.png`から`face_reference_04.png`は、画像生成手順を試すための任意のサンプルです。自分で用意したキャラクター画像を使用する場合は置き換えてください。

## 手順

1. 使用するキャラクター画像、または`references/`のサンプルを1枚添付します。
2. 対象のpromptファイル全文を画像生成AIへ送ります。
3. 出力が指定グリッド、黒背景、同じ構図であることを確認します。
4. 画像を保存し、`build_faces_from_sprite_sheet/split_firmware_sheet.py`で分割します。
5. プレビューで顔の位置、全辺、隣接セルの混入を確認します。

AI生成では構図やセル境界がずれることがあります。基本顔は目と口以外、なでなではプロンプトで許可した差分以外が動いていないことも確認してください。崩れが大きい場合は分割側で無理に補正せず、シートを生成し直します。

同梱サンプルと分割コマンドは[`../build_faces_from_sprite_sheet/README.md`](../build_faces_from_sprite_sheet/README.md)を参照してください。
