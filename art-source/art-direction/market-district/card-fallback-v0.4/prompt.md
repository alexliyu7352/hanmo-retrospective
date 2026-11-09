# 单排堆叠场所卡片 v0.4 提示词

日期：2026-10-03。使用内置 ImageGen；图片为完整视觉目标，未实现运行时堆叠和点击逻辑。

第一张输入为 ../card-fallback-v0.3/market-cards-ui.png，第二张为用户提供的太阁式截图。参考重点是完整卡片在数量多时横向叠压、露出竖排名称；不是把卡片压窄后全部展开。十张只作构图示例，实际数量服从既定内容。

## 完整提示词

```text
Edit this game UI image. This is a precise layout correction, not a style redesign.
Image 1 is the current Han Chinese city UI; keep its street painting, character HUD, Chinese labels, warm painted art, fine bronze/parchment materials, upper buttons and bottom controls.
Image 2 is the user's layout reference: STUDY THE HORIZONTAL OVERLAPPING PLACE-CARD STACK at its bottom. The important mechanism is OVERLAP, not narrow independent tiles.

Replace ONLY the bottom venue row with a genuine horizontal stack of TEN overlapping full-width portrait cards. Every physical card has the SAME generous width (about 245 px) and height (about 305 px). On this 1672 px wide canvas, their left edges advance only about 153 px, so adjacent cards OVERLAP by about 92 px, around 38 percent of each card. Do NOT shrink each card to 153 px. No gaps or evenly separated narrow boxes.

Critical layering: paint rightmost card first and leftmost card last; each card on the LEFT is IN FRONT of the next card on its right, covering the next card's LEFT image portion. The RIGHT EDGE of every card remains exposed, including its tall ivory name plaque. All ten vertical title plaques must be readable. Show actual occlusion of the underlying card's painting and border, thin contact shadows and subtly offset top edges so the stacking is obvious. The first card, 南市酒肆, is selected: show its complete 245 px wide face in front, lifted 12 px with a subtle teal rim; other cards remain overlapped. The full-width selected card MUST visibly be broader than each following exposed slice. Think ten full-size illustrated cards laid over each other, spreading across the bottom like overlapping upright bookmarks, not a grid.

Suggested placement: total stack x=24 to1646; card width245, step153; ordinary card y=535 to840, selected first y=523 to828. Keep cards upright, no fan rotation, no second row. Ten exposed name plaques from left to right:
南市酒肆 / 陈记粮铺 / 许氏布庄 / 周氏铁铺 / 李氏药铺 / 东来客舍 / 陶器铺 / 木器铺 / 皮革铺 / 南市仓房
Use the same venue paintings from image1, now on generous full-width cards partially hidden by the stack. Keep the small 修缮中 red seal visible on the exposed right side of 周氏铁铺.
Preserve 返回城域 at bottom left and 产业 地图 手记 系统 at bottom right, below the stack.
No place total, no search, categories, page numbers, arrows, timers or location navigation routes. No tutorial text, annotations or explanatory arrows. No duplicate card rows.
Produce the finished cohesive 1672x941 game effect screen. The PRIMARY REQUIRED CHANGE is unmistakable shingled overlap with full card width preserved.
```
