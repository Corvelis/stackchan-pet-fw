# アニメ顔検出モデル

[English](README.en.md)

`anime_face_detect_v1.4_n.onnx` は、`deepghs/anime_face_detection` の軽量な `face_detect_v1.4_n` ONNXモデルです。

- Source: https://huggingface.co/deepghs/anime_face_detection
- Model folder: https://huggingface.co/deepghs/anime_face_detection/tree/main/face_detect_v1.4_n
- License: MIT

このモデルは、`build_faces.py` がアニメ顔の矩形を推定し、顔位置合わせと顔サイズ正規化を行うために使います。

通常は `build_faces.py` の初回実行時に自動で取得されます。事前に取得したい場合は、Python依存ライブラリを入れたあと、次のコマンドを実行してください。

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/download_model.py
```
