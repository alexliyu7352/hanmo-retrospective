# 市场街区场所卡片备选 v0.1 生成提示词

日期：2026-10-03。使用内置 ImageGen，以[已认可市场效果参考](../v0.1/market-district-ui.png)为编辑基底制作备选构图，保留画风与既有 UI 身份。

输出：`market-cards-ui.png`。实际尺寸 1672×941 RGB。提示词中的理想尺寸及布局比例不代表全部已精确实现；状态与边界见 README。此图用于界面评审，未执行建筑程序组合。

```text
Use case: ui-mockup.
Edit the supplied late-Han-inspired Chinese market game screenshot into an ALTERNATIVE classic historical role-playing game interface: ONE BEAUTIFUL STATIC DISTRICT ILLUSTRATION ABOVE, a HORIZONTAL ROW OF SIX CONCRETE VENUE CARDS BELOW. The user approves this painted art style. Preserve its refined semi-realistic 2D hand-painted quality, warm sunlight, timber grain, ochre plaster, gray tiled roofs and lived-in Chinese marketplace atmosphere.

The design principle is like a classic Taiko-style town visit screen, but use the established Chinese visual language and the user's supplied UI. Do not copy any Japanese art, logo or castle. The BACKGROUND is an atmospheric market illustration, while the BOTTOM CARDS are the actual visit controls. It is NOT a map with clickable plaques over individual buildings.

Use one widescreen 16:9 finished game screen, preferably 2560x1440.
Composition:
- Roughly the upper 70–73% shows a spacious, inviting market street illustration. Keep the recognizable tavern timber and wine jars, gray-tile roofs, soft leafy trees, cloth awnings, sacks, modest carts, courtyard passages and townspeople. You may recompose the lower foreground to create room for the cards. Keep the main lane inviting and readable. A modest roof-repair scaffold may remain at the far right as incidental scene atmosphere. Do not center the image on architecture planning.
- REMOVE ALL floating building plaques and pointer labels from the source, including 酒肆, 粮铺, 布帛铺, 铁器铺, 药铺, 客舍, 空院, 修缮中. None of these may float over the scene. Physical cloth shop signs can remain as natural scenery.
- No layout grid, selected plot, route, current-position pin, GPS, minimap, travel timer, clock, calendar or destination confirmation panel.

Preserve the existing restrained HUD:
- Upper left same small player portrait in the same parchment/brass frame, unchanged identity and costume: “陆行舟”, “旅人”, “体力 82/100”.
- Below it two small buttons: “地点” and “找人”.
- Top center compact parchment location cartouche: “示范郡城 · 市”.
- Upper-right “同行 2” with the SAME two individual portraits and labels “护卫”, “随从”. Keep them clear of the venue row. Do not generate a crowd portrait roster.
- Bottom footer: “返回城域” at left, and “产业”, “地图”, “手记”, “系统” at right. Keep footer buttons BELOW the venue cards so nothing overlaps.

BOTTOM VENUE ROW is the crucial new element:
Place EXACTLY SIX generously sized illustrated venue cards in one straight horizontal row. The whole row should occupy roughly y=70% to 91% with a short quiet heading “场所” above or at its left. Cards need enough width for a legible title, a small finely painted location vignette and one short function/status line. Cream paper, fine aged-brass borders, dark ink typography; elegant tactile game UI, not a modern website dashboard or a collectible card game.
Left to right, use these EXACT titles and subtitles:
1. “酒肆” — “饮食·消息”; vignette of wine jars and a tavern doorway. This first card has a subtle deep-teal focus border/header and ivory title, no neon glow.
2. “粮铺” — “粮食买卖”; vignette of grain sacks and a wooden measure.
3. “布帛铺” — “布帛买卖”; vignette of cloth bolts and a shop counter.
4. “铁器铺” — “修缮中”; vignette of tools and a small scaffold. This card alone is visibly disabled with desaturated/dimmed art, still readable title and a small muted cinnabar status stamp. It must not look clickable/active.
5. “药铺” — “求医买药”; vignette of herb baskets and pharmacy drawers.
6. “客舍” — “住宿休息”; vignette of a lodging courtyard entrance.

The six cards must be INDIVIDUAL SCENE/VENUE CARDS, not character portraits or rows in a text list. No extra “进入”/“前往” buttons beneath the cards: the card itself is the visit action. No price, walking cost, duration or arbitrary gameplay stats.
Arrange all six so every title and subtitle can be read, with consistent spacing; do not crop any card. Retain around 6–8% of screen height for the navigation footer beneath them. Preserve a large scenic view above rather than surrounding it with more panels.

No design explanation, “方案B”, “备选”, “静态背景”, engine terminology, watermarks, comparison panels or callout arrows inside the image. Deliver one polished, complete gameplay UI target with a static market illustration and bottom venue cards.
```
