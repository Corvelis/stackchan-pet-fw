# Stack-chan Multi-Device Controller

[日本語の詳細](README.ja.md) | [English](README.en.md)

CoreS3 + ｽﾀｯｸﾁｬﾝ、M5Stack StopWatch、AtomS3R Chatbot向けの共通ファームウェアです。顔画像v2アニメーション、音声ストリーミング、吹き出し、なでなで／ふりふり／ぐるぐる反応、StreetPass、HTTP・WebSocket・USB Serial接続を提供します。

## クイックスタート

```sh
pio run -e m5stack-cores3 -t upload
pio run -e m5stack-cores3 -t uploadfs

pio run -e m5stack-stopwatch -t upload
pio run -e m5stack-stopwatch -t uploadfs

pio run -e m5stack-atoms3r-chatbot -t upload
pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

デバイス別の操作と設定は[README.ja.md](README.ja.md)、ビルド済みバイナリからの導入は[バイナリ版インストール手順](docs/install_binary.ja.md)を参照してください。

## 顔画像v2

配布用の`data/`、`data_stopwatch/`、`data_atoms3r/`は、旧静止顔を含まない`animated` v2構成です。旧画像だけを持つ端末はfirmware-only更新時に限定的なfallbackを利用できます。

- [顔レンダラーv2設計](docs/face_renderer_v2.ja.md)
- [顔画像v2移行ガイド](docs/face_asset_migration.ja.md)
- [顔画像生成ツール](tools/face_image_builder/README.md)
- [0.4.0リリースノート](docs/release_notes_0.4.0.ja.md)
- [StopWatch歩数同期仕様](docs/step_counter_protocol.ja.md)

## 開発と配布

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
bash scripts/build_release_bins.sh all
```

変更時の確認事項は[CONTRIBUTING.md](CONTRIBUTING.md)、変更内容は[CHANGELOG.md](CHANGELOG.md)、画像の利用条件は[ASSET_LICENSE.md](ASSET_LICENSE.md)を参照してください。ファームウェアソースはMIT Licenseです。
