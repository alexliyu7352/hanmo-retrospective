# 北汊堡城防军议 v0.1 提示词

日期：2026-10-06。使用内置 imagegen，先新建不透明背景图片，再对同一图片做一次局部编辑。最终结果为待评审静态目标图，状态与实施边界见同目录 README。

## 初始生成

```text
Use case: ui-mockup.
Create ONE polished 16:9 complete gameplay screen for the historical Chinese RPG/strategy game《汉末行记》. This is the SIEGE COUNCIL OVERVIEW “北汊堡 · 城防军议”, based on the game's existing node-based siege rules. This is a rich hand-painted playable-looking UI target, not a cinematic illustration or webpage. It must visibly differ from a hex field-battle screen: NO HEX GRID anywhere in this overview.

STYLE: refined 2D semi-realistic painting, muted natural late-Han countryside, a modest fortified Chinese settlement, warm daylight, cream parchment UI, thin aged bronze frames, deep teal friendly selection and restrained vermilion hostile banners. Chinese earth-and-timber fortification, grey-brown roofs and wooden gates; no Japanese castle, no European towers, no fantasy weapons. Near-overhead three-quarter view, enough top-down clarity to read the courtyard and access routes. Small representative groups of soldiers and flags, no giant generals or portraits.

COMPOSITION: left 72% is one coherent illustrated fortress map; right 28% is a readable command panel. Slim top header, small unobtrusive report strip below the map. Large expressive fort painting with clear interactive NODE LABELS integrated at real places. The fort is modest, with an outer wall, front gate, side gate, a wall-top position, inner courtyard and a rear supply road leading toward small granaries outside. Sparse trees and countryside frame the map. The main gate is damaged but still closed: cracked gate timber and scuffs, no breached hole or conquering banner, no citywide fires.

THE SIX CLICKABLE NODES, exactly one label each:
“城下” outside, lower left;
“主门 47/80” at the front gate, selected with teal/bronze outline;
“侧道 50/50” at a second, smaller closed access on upper left;
“城墙 60/60” on a visible wall section toward upper middle;
“内城 · 守方控制” inside the courtyard;
“粮道 · 尚通” on the rear supply road to the right.
Do not merge “内城” with “粮道”. Keep labels off roofs and figures when possible, readable against small parchment backing.
The real connections are: 城下–主门, 主门–内城, 城下–侧道, 侧道–内城, 城墙–主门, 城墙–侧道, 城墙–内城, 内城–粮道. Represent these with coherent roads, wall walks and stairs plus restrained thin overlays. The two passages 主门–内城 and 侧道–内城 are still blocked: small vermilion bars/lock badges at the gates. Never draw a playable route straight through an intact wall. Only the SELECTED assault targeting line from 城下 to 主门 is teal dashed, and ends at the gate; it is not a movement path into the courtyard. Other routes are understated. No extra clickable nodes, no invented rear assault bypass, no hex battlefield inset.

THREE troop formations ONLY, representative figures, labels give actual sample headcounts:
Friendly “陆楷部 12” at 城下, standing in a small infantry/work-detail formation with teal 陆 banner. Beside this group a SINGLE simple mobile wooden battering ram: a stout log carried/suspended in a modest timber frame with small wheels, no giant siege tower or catapult. The ram is ready for the drafted gate assault, not spawning midair. Label the unit with a small “可指挥” badge.
Hostile “燕驰部 8” with muted red 燕 banner at the main gate, protected on the INSIDE; spears/shields.
Hostile “许砺部 4” with muted red 许 banner on the wall position; compact bowmen.
Do not depict a fourth army, reinforcement arrival, captured defenders, surrendered officers or an already occupied town.

TOP HEADER:
small “汉末行记”
main “北汊堡 · 城防军议”
“第 6 半日 · 拟令”
“陆楷 · 指挥本部”
small gear icon.
No real-time clock.

RIGHT COMMAND PANEL, clear black Chinese type with readable hierarchy:
A. “本役目标”
   “夺取内城并连续控制 2 半日”
   “内城控制 0/2”
   “破门仅开放通路”
B. “当前军情”
   “粮道尚通 · 未形成合围”
   “兵粮详情 ▸” (folded entry; do not invent stock counts)
C. Selected-unit section:
   “陆楷部 · 步卒兼工役队”
   “可战 12 · 士气 80”
   small badge “仅本部可指挥”
D. “本轮军令”
   prominent selector “攻打城防 ▾”
   “目标 · 主门”
   “城防 47/80”
   a clear item selector “简易攻门槌 ×1 ▾”
   “本军可用 1 · 本部选用 1”
   tiny secondary hint “每部每轮一件 · 使用不消耗”
E. Highlighted current preview:
   “预计破坏城防 33”
   “当前可攻打主门”
   secondary line “已计入相邻城墙防护”
   amber “仍须结算本轮军议”
Do not print predicted soldier casualties for this structural assault or a made-up hit chance. Do not show an independent construction skill bonus.
F. Small secondary row near bottom, subordinate to the attack:
   “交涉 ▾” (grey/inactive) and “循退路撤军”
   grey reason “交涉条件未满足”
G. Fixed full-width teal primary button at lower right:
   “确认军议 · 推进半日”
   small note “本部已拟令 · 其余部队自主行动”
Keep the primary button visible, not inside a long scrolled text area.

BOTTOM OF MAP: one slim report strip:
“上轮战报 ▸  陆楷部攻打主门：破坏 33，剩余 47”
A small input hint may read “点旗牌选将 · 点城防节点拟令”.
Small legend “青：本部  朱：守军  锁：通路未开” if space allows. Do not put an initiative queue, giant portrait cards, hidden enemy orders, or a war victory panel here.

VISUAL PRIORITIES: a legible Chinese fortress and its real access structure, distinction between damaged gate and captured inner court, formations tied to clickable nodes, actual one-unit command scope, selected real siege equipment, complete preview and confirmation. Keep the scenic portion attractive and dominant, and use compact refined UI rather than rectangular flowchart boxes replacing the town. This is a STATIC GAMEPLAY VISUAL TARGET, not a precise engine screenshot or a technical diagram. Do not print production notes, internal IDs, implementation versions, or watermarks.
```

## 局部修订

参考图为上述首次生成的城防军议画面，仅作为编辑目标。

```text
Edit the supplied siege game UI image with TWO tightly localized spatial corrections only. Preserve the same 1672x941 wide composition, painting style, every UI panel, every Chinese text, every number, all six node locations, ram, selected teal assault line, main gate damage and closed state. This image is the edit target, not a loose style reference.

1. Move the RED LOCK icon currently directly beneath “内城 · 守方控制” to the MAIN GATE near “主门 47/80”, indicating that the main gate passage is still closed. Keep the 内城 text and courtyard exactly as they are, with no independent lock on the inner courtyard. Keep the existing side-gate lock.
2. The red “燕驰部 8” spear/shield formation currently stands outside the main gate. Put this SAME formation and its red 燕 banner on the DEFENDER'S INSIDE of the main gate, at the inner gate-yard, visible in this elevated view. Place its “燕驰部 8” label close to it on the inner side, without covering the main gate label. Erase its old exterior figures and old exterior banner/label so there is exactly one 燕驰部. Leave 陆楷部 12 outside and 许砺部 4 on the upper wall unchanged. Do not create a new doorway, breach, open gate, new army, or extra route.

All right-hand controls and exact values 47/80, 33, 0/2, 12, 80 and ×1 stay unchanged. Everything unrelated to these two corrections stays unchanged.
```
