# Contributing

[日本語](#日本語) | [English](#english)

## 日本語

### 開発前の準備

1. `main` から作業ブランチを作成します。
2. 必要なら `src/config_private.example.h` を `src/config_private.h` にコピーします。
3. Wi-Fi 認証情報などの個人設定は `src/config_private.h` だけに置きます。このファイルは Git の管理対象外です。

### 変更時の確認

- 変更した機種の通常 env と、影響する場合は `-classic` env をビルドしてください。
- 共通コード、`config.h`、`platformio.ini` を変更した場合は、次の6 envを確認してください。

```sh
pio run -e m5stack-cores3
pio run -e m5stack-cores3-classic
pio run -e m5stack-stopwatch
pio run -e m5stack-stopwatch-classic
pio run -e m5stack-atoms3r-chatbot
pio run -e m5stack-atoms3r-chatbot-classic
git diff --check
```

- 配布用表情画像を変更した場合は、次の検証を実行してください。v2では
  `face_assets.json` と必須画像の一式が揃っている場合だけ成功します。

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
python3 scripts/check_no_local_paths.py
python3 scripts/check_bilingual_docs.py
```

- 通常配布用の`data*`はmanifest付きv2だけを受け付けます。manifestなしの129枚構成は移行ツールと互換テスト専用です。新旧画像を部分的に混ぜないでください。
- `.pio/`、`dist/`、バックアップ、ローカル画像素材、秘密情報はコミットしないでください。
- 開発PCのユーザー名を含む絶対パスを、文書、コメント、設定、サンプルへ記載しないでください。
- Release環境はコンパイル時のホームパスを匿名化し、`build_release_bins.sh`は生成バイナリに実ホームパスが残っていないことを検査します。
- 公開文書は必ず日本語版と英語版を同時に作成・更新してください。`CHANGELOG.md`と
  `CHANGELOG.ja.md`、`docs/*.md`と`docs/*.ja.md`、各toolの`README.md`と
  `README.en.md`は対です。CIは片方だけの追加・更新を失敗として扱います。

### コミットの分け方

レビューしやすいよう、ファームウェア本体、画像アセット、文書、CI/ビルド設定は可能な範囲で別コミットに分けてください。生成物だけのコミットには、生成元や使用したスクリプトをコミットメッセージまたはPR本文へ記載してください。

## English

### Before development

1. Create a working branch from `main`.
2. If needed, copy `src/config_private.example.h` to `src/config_private.h`.
3. Keep Wi-Fi credentials and other personal settings only in `src/config_private.h`. Git ignores this file.

### Validation

- Build the normal environment for each changed target and its `-classic` environment when relevant.
- When shared code, `config.h`, or `platformio.ini` changes, build all six environments listed in the Japanese section above.
- Run `python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v` and
  `python3 scripts/validate_face_assets.py` when runtime face assets change.
- Run `python3 scripts/check_no_local_paths.py` and do not commit absolute paths
  containing a developer machine's username.
- Run `python3 scripts/check_bilingual_docs.py`. Public documentation must
  always be created and updated as a Japanese/English pair. CI rejects changes
  that update only one side.
- Release environments anonymize the build home path, and
  `build_release_bins.sh` rejects binaries that still contain the real home
  directory.
- Release `data*` directories must be complete manifest-backed v2 sets. The manifest-free 129-image layout is only for migration tooling and compatibility tests. Never mix partial new and legacy sets.
- Do not commit `.pio/`, `dist/`, backups, local source artwork, or secrets.
- Paired documents include `CHANGELOG.md`／`CHANGELOG.ja.md`,
  `docs/*.md`／`docs/*.ja.md`, and each tool's
  `README.md`／`README.en.md`.

Keep firmware code, runtime assets, documentation, and CI/build configuration in separate commits where practical. For generated assets, record the source and generation command in the commit message or pull request description.
