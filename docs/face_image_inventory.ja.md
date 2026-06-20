# 顔画像ファイル棚卸し

このメモは、実機アップロード用の顔画像ディレクトリと `assets/` の役割を分けて、
どの画像を残すべきか判断しやすくするための棚卸しです。
コード上での表情グループごとの重要度は `docs/face_image_usage_analysis.ja.md` に分けてまとめています。

## 結論

- 実機が直接読むのは LittleFS 直下の `/idle.png` のようなパスです。アップロード元はデバイスごとに `data/`, `data_stopwatch/`, `data_atoms3r/` に分かれます。
- `assets/` は元画像・作業用素材置き場です。ファームウェアからは直接読まれません。
- 現在の `data/`, `data_stopwatch/`, `data_atoms3r/` はそれぞれ 48 個の PNG を持ちます。
- `src/config.h` に定義された顔画像 48 個は揃っています。
- `data*/` と `assets/` に完全一致する PNG がある場合、それは「実機用コピー」と「素材元」の二重管理です。

## デバイス別ディレクトリ

| デバイス | アップロード元 | サイズ |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `data/` | 240 x 240 |
| StopWatch | `data_stopwatch/` | 386 x 386 |
| AtomS3R Chatbot | `data_atoms3r/` | 128 x 128 |

PlatformIO の `extra_scripts` により、StopWatch は `data_stopwatch/`、AtomS3R Chatbot は
`data_atoms3r/` が LittleFS アップロード元になります。CoreS3 は通常の `data/` を使います。

## 重複の扱い

`data*/` と `assets/` に同一ハッシュの画像があるのは正常です。`data*/` は実機アップロード用の成果物、`assets/` は素材元として考えると整理しやすいです。

ただし、実機アップロード用ディレクトリ内だけで同一画像になっているものは、意味のある別名として残すか、ファーム側のフォールバックに寄せるかを選べます。

| 同一画像 | 現在の意味 | 整理案 |
| --- | --- | --- |
| `idle.png` / `talk_0.png` | 通常待機と通常発話の口閉じ | 口閉じ発話を待機顔と同じにしたいなら現状維持 |
| `good_0.png` / `smile.png` | ポジティブ顔と通常tierのにっこり | 別用途として分かりやすいので現状維持 |
| `good_blink.png` / `blink_attached_0.png` | ポジティブまばたきと親密tierまばたき | 親密tier専用の目閉じが不要なら現状維持 |
| `idle_attached_0.png` / `talk_attached_0.png` | 親密tier待機と発話口閉じ | 同じでよいなら `talk_attached_0.png` は省略可能 |

## 推奨する構造

当面は、実機互換を優先して `data/`, `data_stopwatch/`, `data_atoms3r/` はフラットなまま維持します。整理するなら、`assets/` 側を用途別に寄せるのが安全です。

```text
data/
  idle.png
  talk_0.png
  ...

data_stopwatch/
  idle.png
  talk_0.png
  ...

data_atoms3r/
  idle.png
  talk_0.png
  ...

assets/
  face_sources/
    tsukuyomi/
      normal/
      affection/
      interaction/
      thermal/
```

この構造にすると、`data*/` は「ビルド・アップロードする最終成果物」、`assets/face_sources/` は「元素材」という役割が明確になります。

## 判断基準

- 消してよい候補: `.DS_Store`。macOS のメタデータで、画像素材ではありません。
- まだ消さない方がよいもの: `data/*.png`, `data_stopwatch/*.png`, `data_atoms3r/*.png`。LittleFS に入る実行時ファイルなので、削除前にフォールバック挙動を確認します。
- 統合候補: `assets/image_good/` は `assets/image/` 内のにっこり画像と重複しています。素材元として一箇所に集約できます。
- 追加候補は現時点ではありません。48 個の定義済み画像は揃っています。

## 再チェック

画像を更新した場合は、`data/`, `data_stopwatch/`, `data_atoms3r/` に同じファイル名の PNG が揃っていることと、各デバイスの推奨サイズになっていることを確認します。
