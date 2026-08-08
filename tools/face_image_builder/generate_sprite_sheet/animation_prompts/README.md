# アニメーション生成プロンプト

顔画像v2用の正規プロンプトです。利用者自身が用意した参考画像、または`../references/`のサンプルを使用してください。

[English](README.en.md)

| プロンプト | グリッド | 出力名 | サンプル |
| --- | --- | --- | --- |
| `base_animation_4x4_prompt.txt` | 4×4 | `base_m0_e0` ... `base_m3_e3` | `samples/base_animation_4x4/` |
| `petting_4x4_prompt.txt` | 4×4 | `pet_anim_0` ... `pet_anim_15` | `samples/petting_4x4/` |
| `guruguru_5x5_prompt.txt` | 5×5 | `dir0` ... `dir16` | `samples/guruguru_dir_5x5/` |
| `guruguru_blink_5x5_prompt.txt` | 5×5 | `blink0` ... `blink16` | `samples/guruguru_blink_5x5/` |
| `dizzy_4x4_prompt.txt` | 4×4 | `dizzy_01` ... `dizzy_15` | `samples/dizzy_4x4/` |
| `travel_3x3_prompt.txt` | 3×3 | `travel_wink` ... `travel_peace` | — |

分割方法は[`../../build_faces_from_sprite_sheet/README.md`](../../build_faces_from_sprite_sheet/README.md)を参照してください。基本顔サンプルにだけ必要な列補正値など、生成画像固有の補正を別の画像へ無条件に流用しないでください。
