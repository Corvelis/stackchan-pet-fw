# Anime Face Detection Model

[Japanese](README.md)

`anime_face_detect_v1.4_n.onnx` is the lightweight `face_detect_v1.4_n` ONNX model from `deepghs/anime_face_detection`.

- Source: https://huggingface.co/deepghs/anime_face_detection
- Model folder: https://huggingface.co/deepghs/anime_face_detection/tree/main/face_detect_v1.4_n
- License: MIT

The model is used by `build_faces.py` to estimate anime face rectangles, align face positions, and normalize face sizes before resizing generated face assets.

The model is usually downloaded automatically by `build_faces.py` on the first run. To download it ahead of time, install Python dependencies and run:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/download_model.py
```
