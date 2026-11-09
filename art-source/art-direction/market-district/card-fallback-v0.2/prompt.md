# 场所卡片备选 v0.2 生成提示词

日期：2026-10-03。使用内置 ImageGen，编辑[六卡片 v0.1](../card-fallback-v0.1/market-cards-ui.png)，保留既有街景与 UI 风格。

输出：`market-cards-ui.png`，实际 1672×941 RGB。图用于评审单页容量、分类搜索与分页，未实现交互。提示词中的理想尺寸和布局比例不等于实际交付规格。

```text
Use case: ui-mockup.
Edit the supplied market-district game screenshot. The user says SIX bottom venue cards are too few. Keep the SAME approved Chinese painted market art style, player identity, companion portraits, parchment/brass UI, warm sunlight, and static scenic illustration concept. Replace the oversized single-row venue browser with a COMPACT TWO-ROW browser displaying EXACTLY TWELVE DISTINCT VENUE CARDS, plus category tabs, a name-search field and real pagination controls.

Deliver one complete widescreen 16:9 historical RPG screen. Prefer 2560x1440 if supported; retain readable Chinese text. This is a refinement of the supplied screen, not a modern web dashboard and not a character collection screen.

Keep unchanged:
- Upper-left player portrait and text “陆行舟”, “旅人”, “体力 82/100”.
- Buttons “地点”, “找人” beneath the player.
- Top-center “示范郡城 · 市”.
- Upper-right “同行 2”, the same two individual portraits “护卫” and “随从”.
- The market-street illustration filling roughly the upper 55–58%: aged timber, gray tiled roofs, ochre plaster, stalls, carts, cloth awnings, leafy trees and ordinary shoppers. Preserve its visual identity and composition as much as possible. No floating building markers.
- Footer left “返回城域”; footer right “产业”, “地图”, “手记”, “系统”.
- No clock, calendar, navigation route, current-location pin, movement time or destination confirmation.

NEW venue browser, occupying roughly the lower middle 30%:
1. A thin paper-and-brass control bar immediately above the cards, around y=54–58%.
   Left: “场所 · 共36处”.
   Middle: five compact tabs “全部”, “食宿”, “商贸”, “工艺”, “医药”. Only “全部” has muted deep-teal selected fill.
   Right: a simple parchment search field with a small search icon and placeholder “搜索场所名称”.
   Do not overlap controls or turn this into several tall panels.

2. EXACTLY 12 cards in a 6-COLUMN by 2-ROW grid, NOT one row of six, NOT 10 or 11 cards.
   Use compact landscape cards. Each card has a small painted venue thumbnail on the LEFT and a legible location name plus one short service/status line on the RIGHT. Card borders are fine aged brass, cream paper. Consistent dimensions, tight but clear spacing. Approximate per-card composition: thumbnail 35%, text 65%. Two rows together take about the same height as the previous six huge cards. Each card remains a distinct illustrated place, not a plain table row.
   All names and both rows must be fully visible with no truncation.

EXACT row 1, left to right:
1 “南市酒肆” / “饮食·消息”, wine jars and tavern doorway thumbnail; this card alone has a restrained deep-teal focus border and suitable high-contrast text.
2 “陈记粮铺” / “粮食买卖”, grain sacks thumbnail.
3 “许氏布庄” / “布帛买卖”, cloth bolts thumbnail.
4 “周氏铁铺” / “修缮中”, tools and scaffold thumbnail; ONLY this card is desaturated/gray disabled with a small muted cinnabar status.
5 “李氏药铺” / “求医买药”, herbs and drawers thumbnail.
6 “东来客舍” / “住宿休息”, lodging courtyard thumbnail.

EXACT row 2, left to right:
7 “陶器铺” / “陶器买卖”, pottery jars thumbnail.
8 “木器铺” / “家具器物”, woodworking and wooden furniture thumbnail.
9 “皮革铺” / “皮革买卖”, folded hides and leather goods thumbnail.
10 “肉铺” / “肉食买卖”, tasteful butcher stall thumbnail, no gore.
11 “菜蔬摊” / “菜蔬买卖”, vegetable baskets thumbnail.
12 “北巷酒肆” / “饮食·消息”, a visually different modest tavern doorway thumbnail.
The two taverns are DIFFERENT concrete locations and intentionally share a service type. Do not merge them.

3. A pagination group below the two card rows, centered between the footer return button and utility buttons:
“上一页”   “1 / 3”   “下一页”
The previous-page button is disabled because page 1 is shown; next-page button is active. The total of 36 places and 12 cards per page must remain consistent. This is visibly a browser for more than 12 locations, not 12 permanent service slots.

No extra “进入” or “前往” button on each card: the card is the visit action. No extra portraits, tooltips, tutorial overlays, design annotations, “方案”, “备选”, watermarks or implementation terminology. Keep the original refined hand-painted game aesthetic. Prioritize readable names, exact 12-card count, both complete rows, and coherent toolbar/footer spacing.
```
