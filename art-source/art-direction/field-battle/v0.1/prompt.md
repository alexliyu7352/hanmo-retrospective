# 军队野战完整界面 v0.1 生成提示词

日期：2026-10-04。使用 imagegen 新建一张不透明背景图片，无参考图。本文件保留实际提示词；生成结果仍是待评审的静态视觉目标，具体限制见同目录 README。

```text
Use case: ui-mockup.
Create ONE polished wide 16:9 COMPLETE gameplay screen for the historical Chinese life/strategy RPG《汉末行记》, a FIELD ARMY BATTLE planning phase named “青川河谷”. This must feel like a refined strategy RPG with hand-painted scenery and small troop formations, not a web dashboard, poster, cinematic melee or a personal duel.

Style: fine 2D semi-realistic hand painting of late-Han countryside and modest soldiers, creamy parchment panels, fine bronze/gold frames, deep teal selection, restrained rust-red enemies, readable elegant BLACK Chinese type. Rich painted ground and warm daylight, quiet UI texture. Near-overhead orthographic terrain, small upright painted troops, no steep 3D/isometric view. Distinct from a personal combat image: each unit is a COMPACT GROUP of several soldiers plus banner, not one oversized hero. Representative figures do not have to match headcounts; visible numerical labels are actual example unit sizes.

COMPOSITION: battlefield takes left ~73%; ONE coherent command panel on right ~27%; slim top header and slim bottom roster/feedback area. All essential controls and preview fit onscreen. Background is an open grassy valley floor with worn paths, isolated woods and one low hill. SIX units only, three friendly and three hostile, lots of readable space between them. No castle, siege equipment, moat, magical effects or tactical freehand circles. No massive portraits, no mobile app cards filling the battlefield.

HEX FIELD: use a subtle contiguous POINT-TOP hex lattice over the hand-painted ground, approximately 10 columns by 7 rows, NOT squares and NOT stacked 3D hex pedestals. Think axial q=0..9,r=0..6, with each row shifted half a hex to the right. Hex lines are thin warm grey/brown, unobtrusive outside selection. The arena is mostly plain. Four small woods patches are around cells (3,1),(6,1),(3,5),(6,5); a single low grassy hill is around (5,3). No forest or hill blocking the firing lane along row r=2 between q=3 and q=6. More decorative vegetation may frame the OUTSIDE of the arena. A small own-exit marker “己方退路” is at the upper-left edge, enemy exit “敌方退路” at lower-right edge. No central guard-point objective: this battle objective is to repel the opponent.

SIX TROOP GROUPS and their spatial relations:
Friendly teal banners face broadly right.
- “行旅人部 8” infantry with spears and simple shields, around (4,3), forward of archers and slightly below; its planned order is defense. Small shield badge beside its label.
- SELECTED “曹勇部 8” archers, around (3,2), behind that infantry, with a teal/gold outline on its occupied hex. Several bowmen in a compact formation. Small arrow on hex edge shows east facing.
- “韩平部 8” cavalry around (3,4), lower-left of the infantry, distinct horse silhouettes. This is a friendly unit outside the player's command set: show a tiny neutral lock/eye badge and “友军自主”, not a planned route.
Hostile muted rust banners face broadly left.
- TARGET “石猛部 6” infantry around (6,2), THREE hexes horizontally to the right of 曹勇部; a rust angular target border on its hex, with the exact label “石猛部 6”.
- “鲁达部 6” archers around (7,1), behind the enemy infantry.
- “陈虎部 6” cavalry around (7,5), lower right.
Small sober morale bars and short name/count labels stay close to each troop. Do not draw duplicates or extra formations.

PLANNING OVERLAY: selected action is a stationary VOLLEY “齐射”, so the selected archers stay on their current hex; do NOT show a movement path or ghost troops for them. Show only a thin teal dashed AIM line from their occupied hex through the unobstructed plain to the target hex. This is a targeting preview, not a projectile flight, not guaranteed damage. Selected and target hexes are clear; add a small label beside line “当前射线”. Only lightly outline a handful of relevant attackable cells, do not flood the map with color or draw a circular personal-weapon sector. All enemy and uncommanded friendly FUTURE plans stay hidden. No enemy intent arrows, action queue or initiative strip.

TOP HEADER:
small game title “汉末行记”
main battlefield name “青川河谷”
phase text “野战 · 第 3 / 24 轮 · 拟令”
small scope line “行旅人 · 指挥本军”
one unobtrusive gear icon.
Avoid a decorative real-time timer; half-day is explained at confirmation.

RIGHT PANEL, readable and compact:
1. “本战目标”
   “击退对方全部作战部队”
   Scope chip “可指挥 2 部”
   Small folded disclosure “军粮详情 ▸” (no invented stock numbers).
2. Selected unit header “曹勇部 · 弓手”, small banner/bow emblem, NO giant portrait.
   “可战 8  ·  伤员 0  ·  阵亡 0”
   “被俘 0  ·  逃散 0”
   a slender morale bar “士气 80/100”
   resource line “战法点 8/8”
3. “本轮军令”
   Compact five ordinary buttons, “移动”, “攻击”, “防御”, “等待”, “退兵”.
   Directly below, the current tactic selector is selected teal and reads “战法 · 齐射 ▾”.
   One concise tactic description “原地发动 · 消耗 4 战法点”.
   A smaller line “齐射射程 2—5 格”. (This is the volley range; ordinary archers have shorter range.)
   A compact facing row “朝向 · 东”.
4. A clear preview box:
   title “当前预览”
   “曹勇部 → 石猛部”
   “预计减员 0—1 人”
   “当前射程内 · 射线畅通”
   in muted amber, separated from facts: “共同移动后复核”
   Do NOT print a 'chance to hit' percentage; this game uses collective casualty calculation, not one all-or-nothing hit roll.
5. Pin a full-width primary teal button “确认军令 · 推进半日”.
   Under it a small note “本军各部同步结算”.
   No “next unit”, “end this unit's turn”, reaction prompts, or fast-time controls.

BOTTOM of battlefield:
A compact single-row unit selector, THREE entries only, with small troop insignias and clear status, not portrait trading cards:
“行旅人部 · 防御草案”
“曹勇部 · 齐射草案” (selected teal)
“韩平部 · 友军自主” (neutral, subtle eye/lock icon, inspectable)
This is a roster, NOT action order. Keep it within left battlefield width.
Below or just above roster, a thin collapsible report “上轮战报 ▸” and concise example “攻击未生效：目标移出射程”. Treat it as a past-result report, do not animate live losses.
A tiny input hint may read “点选部队或目标 · 右键设朝向”.

Visual priorities: coherent field with readable ground and coordinated infantry/archer/cavalry silhouettes; player command scope; correctly grounded stationary volley selection; costs and conditional outcome; half-day commitment. Depict a historical military encounter with small representative formations, no fantasy uniforms, no gunpowder, no oversized generals. Chinese text should be crisp, correctly spelled, and well-spaced. This is a STATIC VISUAL TARGET, not an exact engine screenshot or a technical diagram. No production annotations, no model/version jargon or watermarks in the image.
```
