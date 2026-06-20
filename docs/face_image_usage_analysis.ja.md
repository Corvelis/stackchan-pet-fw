# 顔画像グループの使用重要度

このメモは、実機アップロード用の顔画像 PNG がコード上でどれだけ重要かを、
単体ファイルではなく表情グループ単位で整理したものです。CoreS3 は `data/`、
StopWatch は `data_stopwatch/`、AtomS3R Chatbot は `data_atoms3r/` を使います。

## 表情選択の優先順位

通常の自動表示では、`FaceController` が状態とモードから画像を選びます。発話中・待機中・聞き取り中で多少違いますが、発話中の優先順位はおおむね次の通りです。

```text
shake > pet > photo_master > photo > auth > thermal > affection visual tier > normal
```

ただし、`src/main.cpp` の `AUTH_FACE_BASE_SWITCH_ENABLED` が `false` なので、現状では `auth` による `good/bad` への自動切替は実質無効です。

## 最小構成

体験を大きく壊さずに最小寄りへ落とすなら、まず残すべきグループはこれです。

| グループ | ファイル | 重要度 | 理由 |
| --- | --- | --- | --- |
| 通常会話 | `idle.png`, `listen.png`, `talk_0.png`, `talk_1.png`, `blink.png` | 必須 | 起動後の基本顔、聞き取り中、発話口パク、通常まばたきに使う |
| 通常スマイル | `smile.png` | 低 | normal tier のIdle中だけ、8-20秒間隔で1.5秒出る演出 |
| 低電力 | `low_power_0.png`, `low_power_talk.png`, `low_power_blink.png` | 中 | 低電力モードで常時寄りに使う。状態が分かりやすくなる |

`idle.png` と `talk_0.png` が同じ絵でも、役割名として分かれているのは妥当です。発話の口閉じフレームを後から変えられるためです。

## グループ別評価

| グループ | ファイル | 自動使用 | 重要度 | 整理判断 |
| --- | --- | --- | --- | --- |
| 通常会話 | `idle.png`, `listen.png`, `talk_0.png`, `talk_1.png`, `blink.png` | あり | 高 | 残す。アプリの基本表情 |
| 通常スマイル | `smile.png` | あり | 低 | 消しても会話は壊れないが、Idleの表情変化は減る |
| 好感度: guarded | `idle_guarded_0.png`, `blink_guarded_0.png`, `talk_guarded_0.png`, `talk_guarded_1.png` | あり | 中 | 好感度が低い時の個性。`talk_guarded_0.png` は `idle_guarded_0.png` にフォールバックできるので省略可 |
| 好感度: attached | `idle_attached_0.png`, `blink_attached_0.png`, `talk_attached_0.png`, `talk_attached_1.png` | あり | 中 | 好感度が高い時の個性。`talk_attached_0.png` は `idle_attached_0.png` と同じなら省略可 |
| なでなで: normal | `nadenade_0.png`, `nadenade_1.png` | あり | 中 | CoreS3 の背面タッチ、StopWatch の画面タッチ、AtomS3R の BtnA 長押し、または WS コマンドで使う。触る体験を残すなら重要 |
| なでなで: guarded | `pet_guarded_0.png`, `pet_guarded_1.png`, `pet_blink_guarded_0.png` | あり | 低-中 | 低好感度時だけの差分。無くても `nadenade_*` にフォールバックする |
| なでなで: attached | `pet_attached_0.png`, `pet_attached_1.png`, `pet_blink_attached_0.png` | あり | 低-中 | 高好感度時だけの差分。無くても `nadenade_*` にフォールバックする |
| ふりふり: normal | `furifuri_0.png`, `furifuri_1.png` | あり | 中 | IMU shake またはWSコマンドで1.5秒程度使う。リアクションを残すなら重要 |
| ふりふり: guarded/attached | `shake_guarded_0.png`, `shake_guarded_1.png`, `shake_attached_0.png`, `shake_attached_1.png` | ありだが未配置 | 低 | 無ければ `furifuri_*` にフォールバック。現状未配置でも問題は小さい |
| 写真撮影 | `photo_0.png`, `photo_1.png`, `photo_blink.png` | CoreS3 であり | 中 | CoreS3 の `/capture` や `face_mode=photo` で使う。StopWatch / AtomS3R ではカメラ非搭載だが、直接 face 指定や将来拡張用として残せる |
| 写真撮影の目閉じ発話 | `photo_blink_talk.png` | 直接指定のみ | 低 | 自動の口パク・まばたき経路では使われていない。削減候補 |
| マスター撮影 | `photo_master_0.png`, `photo_master_1.png` | あり | 低-中 | `face_mode=photo_master` 専用。使っていなければ削減候補 |
| 認証: master | `good_0.png`, `good_1.png`, `good_blink.png` | 現状ほぼなし | 低 | `AUTH_FACE_BASE_SWITCH_ENABLED=false` のため通常のauthでは出ない。直接 `face` 指定では出せる |
| 認証: not_master | `bad_0.png`, `bad_1.png` | 現状ほぼなし | 低 | 同上。非マスター時のモーションには効くが、顔画像切替は無効 |
| 熱状態: warm/hot | `tired_0.png`, `tired_talk.png`, `tired_blink.png`, `exhausted_0.png`, `exhausted_talk.png`, `exhausted_blink.png` | あり | 中 | 温度状態の可視化。通常会話には不要だが、デバイス状態表示として意味がある |
| 低電力 | `low_power_0.png`, `low_power_talk.png`, `low_power_blink.png` | あり | 中 | 低電力モード中の顔。設定で明示的に使われる |

## 省略しやすい候補

コード上の安全度だけで見ると、次の順に削りやすいです。

1. `photo_blink_talk.png`: 自動経路では使われず、`face` コマンドで直接指定した時だけ表示されます。
2. `shake_guarded_*.png`, `shake_attached_*.png`: 現状未配置でも `furifuri_*` にフォールバックします。
3. `talk_guarded_0.png`, `talk_attached_0.png`: 口閉じ発話をidle顔と同じにするなら省略できます。
4. `good_*`, `bad_*`: 現状の設定ではauthによる自動顔切替がオフなので、直接表示テストや将来ONにする予定がなければ優先度は低いです。
5. `smile.png`: なくすならコード側のスマイル演出も無効化した方がログ上のmissingを避けられます。

## 残した方がよい単位

削る場合も、口パク用のペアはなるべくペア単位で扱うのが安全です。

- `_0` / `_1` のペアは発話中に300ms間隔で交互に使われます。
- `_blink` 系は非発話時の120ms程度の短い演出です。印象には効きますが、ペア画像ほど重要ではありません。
- `*_guarded_*` と `*_attached_*` は好感度tierの見た目差分です。好感度機能を見せたいなら残す価値があります。

## まとめ

今の48枠は、実質的には次の10前後の表情グループです。

```text
normal conversation
normal smile
affection guarded
affection attached
pet normal
pet guarded
pet attached
shake normal
shake tier variants
photo
photo master
auth good/bad
thermal
low power
```

この中で、アプリ体験の中心は `normal conversation`、次点が `affection guarded/attached`、`pet normal`、`shake normal`、`thermal/low power` です。CoreS3 では `photo` も重要です。`auth good/bad` と `photo_blink_talk` は、現状コードでは優先度がかなり低いです。
