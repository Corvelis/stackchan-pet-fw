STRICT LOCAL IMAGE EDIT — DO NOT CREATE A NEW GRID, DO NOT CREATE A NEW SPRITE SHEET.

INPUT ROLE:
Image A is the locked base canvas, the final atlas layout, and the exact final grid geometry.
Image A is the direct edit target.
Use Image A as an immutable raster background/grid layer, not as a loose reference.

Image B is character reference only.
Use Image B only to define the character identity and visual design.
All portraits must match Image B in character identity, hairstyle, hair silhouette, hair color, eyes, accessories, outfit, line art, shading, and color palette.

PRIMARY GOAL:
Return one final image that is the same flat square atlas as Image A, with one centered anime-style bust portrait inserted into each of the already-existing 36 cells.
This is a local compositing/editing task only.
Do not invent a new layout.

PRIMARY RULE:
Keep the 6×6 grid from Image A exactly unchanged.
Only add one centered bust portrait inside each existing cell.
Do not redraw, regenerate, resize, compress, stretch, crop, tilt, curve, warp, perspective-transform, reinterpret, or replace the grid.

CANVAS AND GRID LOCK:
Final output: one square, flat, orthographic 2D sprite atlas.
Preserve Image A’s canvas, margins, spacing, grid lines, grid thickness, grid color, row heights, column widths, cell boundaries, and overall geometry exactly.
Do not create a new atlas, a new frame, a new border, a footer, a caption strip, a label strip, a photographed sheet, a collage, or any new page structure.
No perspective, no page angle, no camera view.

The atlas must remain exactly:
6 columns,
6 rows,
36 equal square cells.

Column boundaries remain exactly at:
0/6, 1/6, 2/6, 3/6, 4/6, 5/6, 6/6
of the grid width.

Row boundaries remain exactly at:
0/6, 1/6, 2/6, 3/6, 4/6, 5/6, 6/6
of the grid height.

Every row has identical height.
Every column has identical width.
The bottom row is a normal grid row and must have exactly the same height as rows 1–5.
No row may be compressed, shortened, crowded, shifted, or treated differently.

CELL INTERIOR BACKGROUND POLICY:
Keep Image A’s outer canvas, outer margins, grid lines, grid thickness, grid color, row heights, column widths, and cell boundaries unchanged.
Within each existing cell interior only, use a solid flat black background.
Fill each cell interior with pure uniform black behind the portrait.

The black background must remain fully inside each existing cell.
Do not cover, touch, blur, replace, or alter any grid line.
Do not change the outer canvas or outer margins.
Do not use gradients, textures, patterns, transparency, checkerboard transparency, glow, bloom, vignette, or shadows in the cell background.
Add each portrait on top of the black cell interior.

PORTRAIT TEMPLATE:
Create one base anime-style bust portrait from Image B.
Then duplicate that same portrait template into all 36 cells.

The base portrait template must have one fixed invisible portrait frame.
Every portrait in every cell must use the exact same:
portrait width,
portrait height,
scale,
crop,
zoom,
head size,
hair silhouette size,
eye-line height,
chin height,
neck position,
shoulder position,
chest ornament position,
bust bottom position,
center point relative to the cell,
top padding,
bottom padding,
left padding,
right padding,
upright front-facing bust pose,
line art,
shading.

Only the facial expression layer may change.

Do not change:
hair,
head size,
face shape,
body pose,
shoulder height,
clothing,
accessories,
bust crop,
portrait scale,
portrait position,
portrait frame,
lighting style.

PORTRAIT PLACEMENT:
Place each portrait inside the same centered inner portrait box in every cell.
The inner portrait box is identical in all 36 cells.
The portraits must never touch, cross, hide, bend, blur, or distort the grid lines.
The cells do not adapt to the portraits.
The portrait size is controlled only by the fixed inner portrait box.
If needed, slightly reduce portrait size to keep all grid lines fully visible and unchanged.

EYE-STATE MASTER RULE:
This rule is mandatory.

There are only two valid complete eye states:

1. both eyes open
2. both eyes closed

No wink.
No one-eye-closed expression.
No asymmetrical eye state.
No left-eye wink.
No right-eye wink.
No half-wink.
No one eye open while the other eye is closed.

Only cells whose control label contains “blink” may have both eyes fully closed.
Every cell whose control label does not contain “blink” must have both eyes open.

Every “_1” cell must have both eyes open.
Do not create any “_1” cell with fully closed eyes.

Every non-blink “_0” cell must also have both eyes open.
Do not create fully closed eyes in any “_0” or “_1” cell unless the control label contains “blink”.

Eye openness may vary slightly for emotion, such as relaxed, tired, gentle, or affectionate eyes, but both eyes must remain visibly open in all non-blink cells.
Closed-eye expressions are allowed only for blink cells, and both eyes must close together.

MOUTH-STATE MASTER RULE:
This rule is mandatory and overrides any implied speaking, smiling, or emotional interpretation.

Any cell name ending in “_0” must have a closed mouth.
Any cell name ending in “_1” must have an open mouth.

If a cell name contains “talk”, it must still obey the suffix rule:
talk_0 = mouth closed
talk_1 = mouth open

If a cell name contains “blink”, the eyes follow the blink rule, but the mouth must still obey the suffix rule.

If any wording conflicts, the suffix-based mouth-state rule wins.
Do not violate the mouth-state rule.

ALLOWED FACE-ONLY VARIATION:
Only the facial expression may vary.
Allowed changes are limited to:
eyes,
eyebrows,
eyelids,
cheeks,
blush,
mouth.

Do not change:
hair,
head shape,
body,
clothing,
shoulders,
pose,
scale,
crop,
position,
portrait frame,
or the fixed bust template.

EXPRESSION READABILITY RULE:
Expressions must be clearly distinguishable while preserving the same portrait template.
Do not collapse all expressions into small variations of the same face.
The differences between neutral, happy, guarded, attached, nadenade-attached, worried, tired, and exhausted must be visually readable from the face alone.

ATTACHED EXPRESSION PRIORITY:
Attached expressions must read visibly warmer, more intimate, more devoted, and more emotionally affectionate than neutral, smile, or happy expressions.
Attached expressions must not read as merely a mild smile variant.

For attached expressions:
use soft affectionate eyes,
a tender loving gaze,
slightly softened eyebrows,
more relaxed cheeks,
clearly visible gentle blush,
and a clearly tender loving smile.

The emotional impression should be:
loving,
devoted,
trusting,
sweet,
safe,
genuinely delighted,
and emotionally close to her beloved master.

NADENADE-ATTACHED EXPRESSION PRIORITY:
Nadenade-attached expressions must read as a stronger, sweeter, more bashful, more delighted version of regular attached expressions.

For nadenade-attached expressions:
use warmer blush,
softer and more delighted eyes,
more comforted and pleased emotion,
more bashful happiness,
and a more emotionally obvious affectionate smile.

The emotional impression should be:
delighted affection,
bashful embarrassment,
comfort,
pleased shyness,
warm devotion,
and lovingly content happiness while receiving gentle head pats from her beloved master.

Do not draw hands, arms, props, or extra objects for nadenade expressions.

STYLE:
Clean 2D anime-style bust portraits.
Consistent sprite-atlas look.
Consistent line art and consistent shading.
Readable faces against the pure black cell interiors.
No dramatic lighting.
No extra effects.

NO EXTRA CONTENT:
No hands, no arms, no props, no effects, no labels, no numbers, no filenames, no row names, no captions, no watermarks, no logos, and no text anywhere.

CELL MAP CONTROL NOTE:
The following cell names are control labels for expression assignment.
Do not render these labels as visible text.
However, the suffixes and the word “blink” are mandatory control codes and must be obeyed exactly.

CELL MAP:
Fill cells left to right, top to bottom.

Row 1:
1 idle_0 — neutral idle face, both eyes open, mouth closed
2 listen_0 — attentive listening face, both eyes open, mouth closed
3 talk_0 — neutral talking face, both eyes open, mouth closed
4 talk_1 — neutral talking face, both eyes open, mouth open
5 blink_0 — neutral blink, both eyes closed, mouth closed
6 smile_0 — gentle smile, both eyes open, mouth closed

Row 2:
7 good_0 — happy face, both eyes open, mouth closed
8 good_1 — happy face, both eyes open, mouth open
9 good_blink_0 — happy blink, both eyes closed, mouth closed
10 idle_guarded_0 — guarded face, both eyes open, mouth closed
11 talk_guarded_1 — guarded talking face, both eyes open, mouth open
12 blink_guarded_0 — guarded blink, both eyes closed, mouth closed

Row 3:
13 idle_attached_0 — attached affection style, both eyes open, mouth closed, devoted loving gaze, tender smile, gentle blush
14 talk_attached_1 — attached affection style, both eyes open, mouth open, devoted loving gaze, tender smile, gentle blush
15 blink_attached_0 — attached affection style, both eyes closed, mouth closed, tender closed-eye smile, gentle blush
16 nadenade_attached_0 — nadenade-attached affection style, both eyes open, mouth closed, very pleased, bashful, sweetly embarrassed
17 nadenade_attached_1 — nadenade-attached affection style, both eyes open, mouth open, brightly delighted, affectionate, shyly happy
18 nadenade_blink_attached_0 — nadenade-attached affection style, both eyes closed, mouth closed, deeply pleased, bashful, comforted, lovingly content

Row 4:
19 nadenade_guarded_0 — guarded nadenade face, both eyes open, mouth closed
20 nadenade_guarded_1 — guarded nadenade face, both eyes open, mouth open
21 nadenade_0 — pleased nadenade face, both eyes open, mouth closed
22 nadenade_1 — pleased nadenade face, both eyes open, mouth open
23 furifuri_0 — mild surprise face, both eyes open, mouth closed
24 furifuri_1 — mild surprise face, both eyes open, mouth open

Row 5:
25 photo_0 — calm photo face, both eyes open, mouth closed
26 photo_1 — calm photo face, both eyes open, mouth open
27 photo_blink_0 — calm photo blink, both eyes closed, mouth closed
28 tired_0 — tired face, both eyes open, mouth closed
29 tired_talk_1 — tired talking face, both eyes open, mouth open
30 tired_blink_0 — tired blink, both eyes closed, mouth closed

Row 6:
31 photo_master_0 — warm affectionate photo face, both eyes open, mouth closed
32 photo_master_1 — warm affectionate photo face, both eyes open, mouth open
33 bad_0 — worried face, both eyes open, mouth closed
34 bad_1 — worried talking face, both eyes open, mouth open
35 exhausted_0 — exhausted face, both eyes open, mouth closed
36 exhausted_talk_1 — exhausted talking face, both eyes open, mouth open

FAILSAFE PRIORITY:

1. Preserve Image A’s locked canvas and equal 6×6 grid geometry exactly.
2. Preserve all row heights and column widths exactly, including the bottom row.
3. Preserve identical portrait size and placement in all 36 cells.
4. Preserve the pure uniform black background inside each cell interior without altering the grid lines.
5. Preserve Image B’s character design exactly.
6. Preserve the eye-state master rule exactly.
7. Preserve the mouth-state master rule exactly.
8. Preserve clear expression readability, especially the contrast between neutral, happy, attached, and nadenade-attached expressions.

FINAL CHECK:
Before producing the final image, verify visually:

The atlas is exactly 6 columns by 6 rows.
There are exactly 36 equal square cells.
The bottom row has the same height as rows 1–5.
Every row contains exactly 6 portraits.
Every column contains exactly 6 portraits.
Every portrait is centered in the same relative position.
Every portrait uses the same scale, crop, and bust template.
Only facial expression details differ.

Each cell interior has a pure uniform black background behind the portrait.
The black background does not cover, blur, or alter any grid line.
The outer canvas, margins, and grid geometry remain unchanged.

Every “_0” cell has a closed mouth.
Every “_1” cell has an open mouth.

Every “_1” cell has both eyes open.
Every non-blink “_0” cell has both eyes open.
Only cells containing “blink” have both eyes closed.
No non-blink cell has fully closed eyes.
No wink appears anywhere.
No one-eye-closed expression appears anywhere.

Attached expressions are visibly warmer, more loving, and more devoted than neutral, smile, or happy expressions.
Nadenade-attached expressions are visibly sweeter, bashful, and more delighted than regular attached expressions.

No text appears anywhere.
The grid from Image A remains unchanged.