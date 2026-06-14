# Stack-chan 顔スプライトシート生成用素材

[English](README.en.md)

このフォルダは、ChatGPT Image や Gemini などの画像生成AIで、Stack-chan pet firmware 用の 6x6 顔スプライトシートを作るための素材置き場です。

分割してファームウェア用の48枚のPNGを作る処理は、隣の `tools/face_image_builder/build_faces_from_sprite_sheet/` に分けています。

## ファイル構成

```text
tools/face_image_builder/generate_sprite_sheet/
  README.md
  README.en.md
  sprite_sheet_prompt.md
  grid_template_6x6.png
  references/
    face_reference_01.png
    face_reference_02.png
    face_reference_03.png
    face_reference_04.png
```

## 使うもの

画像生成AIには、基本的に次の3つを渡します。

```text
Image A:
  grid_template_6x6.png

Image B:
  好きな顔画像を1枚

Prompt:
  sprite_sheet_prompt.md の全文
```

`grid_template_6x6.png` は、プロンプト内の Image A です。6列x6行のグリッドそのものを固定するためのテンプレート画像です。

Image B には、作りたいキャラクターの好きな顔画像を1枚使います。`references/` には、このリポジトリで試したサンプル用の参考画像を置いているだけです。

自分のキャラクターで作る場合は、`references/` の画像にこだわる必要はありません。正面寄りで、髪型・目・服・アクセサリが分かりやすい顔画像を1枚選んでください。

`sprite_sheet_prompt.md` は完成済みプロンプトです。内容は編集せず、そのまま画像生成AIに貼り付けて使います。

## 画像生成の手順

ChatGPT Image 2.0 / gpt-image-2.0 や Gemini + nano banana 系の画像生成UIで、Image A、Image B、プロンプトを使ってスプライトシートを作ります。

スマホアプリのチャットUIからでも作れます。専用のAPIやスクリプトは不要です。

手順:

1. ChatGPT や Gemini のチャットを開き、画像を作れる状態にします。
2. `grid_template_6x6.png` を添付します。
3. 好きな顔画像を1枚添付します。
4. `sprite_sheet_prompt.md` の全文をメッセージ欄に貼り付けます。
5. 生成を開始します。
6. できあがった正方形の 6x6 スプライトシート画像を保存します。
7. 保存した画像を `build_faces.py` に渡して、ファームウェア用の48枚に分割します。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

## サンプル画像の生成環境

`tools/face_image_builder/build_faces_from_sprite_sheet/samples/` にあるサンプルは、次の環境で試したものです。

| サンプル | 生成環境 |
| --- | --- |
| `sprite_sheet_sample_01.png` | Gemini Pro + nano banana 2 |
| `sprite_sheet_sample_02.png` | Gemini Pro + nano banana 2 |
| `sprite_sheet_sample_03.png` | GPT-5.5 知能最高 + ChatGPT Image 2.0 |
| `sprite_sheet_sample_04.png` | GPT-5.5 知能最高 + ChatGPT Image 2.0 |

Gemini側は無料ユーザー、ChatGPT側は課金ユーザーで試しています。どちらもスマホアプリのチャットUIから生成しました。

## 生成時の注意

このプロンプトは、Image A のグリッドを作り直さず、既存のグリッドに顔を入れることを強く指定しています。

生成結果で次の問題が出た場合は、同じ素材とプロンプトで再生成してください。

```text
- 6x6 になっていない
- グリッド線が消えている、太さが変わっている
- 行や列のサイズがずれている
- 顔の大きさや位置がセルごとに大きく違う
- 片目だけ閉じている表情が混ざっている
- 背景やグリッドに余計な装飾が入っている
```

多少のグリッド線のゆらぎや顔位置のずれは、`build_faces.py --detect-grid` 側で補正します。ただし、セルの構造が大きく崩れている画像は分割前に作り直した方が安定します。
