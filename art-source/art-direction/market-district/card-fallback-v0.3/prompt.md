# 单排竖向场所卡片 v0.3 提示词

日期：2026-10-03。使用内置 ImageGen；输出为整图视觉效果，不是可运行界面或独立卡片资产。

编辑底图为 ../card-fallback-v0.2/market-cards-ui.png。第二张输入是用户提供的太阁式截图，仅参考底部单排窄长卡片与竖排名称的排布；保留本项目的中国街景、人物与材质风格。

十张只用于构图示范。撤销此前36处、三页及目录控件的假设，不据此扩充既定游戏内容。

## 完整提示词

```text
Use case: ui-mockup edit.
Asset type: revised fallback city-district screen for the Chinese late-Han game 汉末行记.
Input image 1 is the edit target: preserve its approved warm detailed 2D painted Chinese street, ochre wood, gray tile, trees, lived-in market, clear parchment-and-bronze UI, player portrait/name/role/stamina top left, location title top center, and two companions top right.
Input image 2 is USER'S LAYOUT REFERENCE ONLY: borrow the single horizontal row of narrow tall illustrated venue cards with vertical title plaques. Do not copy its Japanese architecture, people, modal dialog, browser chrome or watermark.
Primary change: replace the entire bottom two-row directory with ONE continuous horizontal row of exactly TEN tall narrow illustrated venue cards. The user rejected the artificial 36-place city inventory and directory interface. Delete the count toolbar, all type filter tabs, search box, previous/next controls and page count completely. Absolutely no second row, no 36, no pagination, no new directory menu.
Composition: 1672x941 landscape. Upper scene extends down to about y=610, roughly 65% height, maintaining a wide airy street. From about y=615 to y=875 draw 10 evenly spaced portrait-oriented cards in a SINGLE row across almost the full width. Each is ~148px wide and ~255px tall, with a delicate dark bronze frame, a full-height mini illustration of its own venue, and a narrow ivory hanging name plaque at the upper right; title Chinese characters stacked vertically top-to-bottom, upright and crisp. No horizontal service description panel and no rows of tiny metadata. The images should fill the cards, like beautifully painted place picture-cards.
Exact left-to-right labels and drawings:
1 南市酒肆 — wine jar, red 酒 cloth sign and serving counter; subtle teal focus frame.
2 陈记粮铺 — large grain baskets and sacks.
3 许氏布庄 — folds and hanging rolls of rich cloth.
4 周氏铁铺 — forge, tools, timber repair scaffolding; muted colors with a small vermilion status seal 修缮中, visually unavailable.
5 李氏药铺 — herbal drawers and trays of plants.
6 东来客舍 — welcoming guesthouse doorway and warm lamps.
7 陶器铺 — rows of clay pots.
8 木器铺 — woodcraft, stools and benches.
9 皮革铺 — leather hides and finished straps.
10 南市仓房 — stacked storage bales and a large wooden storeroom door.
The TEN labels are concrete illustrative venues, not a displayed total or a newly invented number of city facilities. Do not print total counts.
Keep top text: 陆行舟, 旅人, 体力 82/100; 示范郡城·市; 同行2, 护卫, 随从. Preserve the small 地点 and 找人 buttons from the target image.
At the very bottom below the cards keep only compact 返回城域 on the left and 产业 / 地图 / 手记 / 系统 on the right, with ample margin. No full opaque background board; let the city painting show between independent framed cards.
No time, date, movement navigation, map pins, route lines, central modal, person roster along the bottom, or service purchase buttons. The bottom row consists only of PLACE cards.
High quality complete readable game UI effect image. Preserve established style, identities and Chinese setting; revise layout only.
```
