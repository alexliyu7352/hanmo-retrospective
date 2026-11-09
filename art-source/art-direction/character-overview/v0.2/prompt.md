# 人物总览 v0.2 编辑提示词

使用内置 ImageGen，对已检查的 v0.1 图片进行一次局部编辑。输入为 [v0.1 人物总览](../v0.1/character-overview-ui.png)，实际输出 1672×941 RGB PNG；未进行后续像素编辑或放大。

```text
Use case: ui-mockup, targeted edit.
Edit the provided character overview screenshot, preserving its refined hand-painted Chinese late-Han art direction, exact portrait identity and pose, panel textures, dark teal/bronze palette, and landscape dimensions. This is a focused UI correction, NOT a new character painting. Keep the entire left portrait, five attribute rows with all values 60, ten skills and their XP, and the right selected 悟性 · 二档 details intact except the exact text changes below.

Problem to fix: the three huge talent cards look like exactly three fixed slots or the complete talent catalogue. Replace ONLY that central middle section with a scalable compact row list and clear per-person scope, plus separate categories. Do not invent additional talent names, effects, scores or catalogue totals. The three shown talents are illustrative entries known for this sample character, not the total game collection.

1. TOP TABS:
Replace the five tabs with six coherent tabs: “总览” (selected), “禀赋与特质”, “技艺”, “武学”, “关系”, “经历”. Fit them neatly in the existing full-width tab row without reducing readable text size. Keep the large header “人物总览”, game title and top-right “返回”.

2. CENTRAL MIDDLE SECTION (between 五维属性 above and 技艺摘要 below):
Remove all three large illustrated card rectangles. Build a clean, data-driven compact list on the same warm paper, no empty item slots.
Section header “禀赋与特质” on left, real-looking clear clickable “查看全部 ›” on right.
A smaller subheading line “先天禀赋” on left and “此人已知 3 项” aligned right. The count belongs ONLY to the person and this subgroup.
Below exactly three FULL-WIDTH horizontal rows with subtle separators, generous readable spacing, aligned icon / name / tier / disclosure-chevron columns:
first row “悟性” and “二档”, small ink book icon, two filled teal diamonds and one outlined diamond. SELECTED row has pale teal paper tint and teal left-edge selection mark, plus small checkmark at far right.
second row “筋骨” and “一档”, small root icon, one filled and two outlined diamonds.
third row “协调” and “一档”, small flowing cord icon, one filled and two outlined diamonds.
These are list entries, not collectible cards or upgrade slots. Each row runs the entire middle column width. No fixed three-column grid, no blank fourth slot, no “3/3”, no total-catalogue counter. Because all 3 rows fit, no fake scrollbar in this particular view.
Below the list, TWO clearly separate compact category summary lines on paper:
“性格　未有记录　›”
“后天特质　未有记录　›”
They must be visually distinct from the innate talent rows. Keep the bottom “技艺摘要” list in its own separate section, with all 10 existing skill names and XP numbers unchanged. Maintain comfortably readable type, no tiny packed text. This whole replacement may use the existing middle section's height efficiently by deleting the oversized illustrations.

3. LEFT COLUMN: remove only the duplicate bottom line “性格 未有记录”, since this information has moved into the central category summary. Keep 陆楷, 二十八岁, 在野 · 退伍戍卒, location, affiliation, HP100/100, fatigue0/100, injury无 unchanged.

4. RIGHT DETAIL PANEL: change only its top heading from “禀赋详情” to “条目详情”; keep the selected 悟性 · 二档 content, +10%, 15% cap, parent-record unknown, illustration, and “查看技艺进度” control. Selection must clearly match the first list row.

Retain the five attribute labels 统率、武力、智谋、政务、魅力, all 60; retain skill rows 步战1级100/300,骑战0级0/100,弓弩0级0/100,武艺0级0/100,兵法1级100/300,辩说0级0/100,交涉0级0/100,礼学0级0/100,商术0级0/100,运输0级0/100. Keep all Chinese text clean and correct. Do not add new background characters, editorial annotations, developer notes, watermarks, price tags, rarity colors, star ratings, talent upgrade or point-spending buttons. The rest of the screen must remain as close to the input as possible.
```
