# 关中—河洛区域图 v0.1：提示词

日期：2026-10-04。内置 imagegen；以全域图作为编辑输入生成区域视野，再进行一次关口道路连接的局部编辑。最终输出 1672×941 RGB PNG。提示词的地理布局与约束不等于已验证的输出精度。

## 区域视野

参考文件：`../v0.1/world-map-ui.png`（只延续视觉与 UI，不把其大陆轮廓当作区域底图）。

```text
Use case: ui-mockup, historical-scene. Edit the supplied world-map screenshot into the NEXT ZOOM LEVEL of the same Chinese late-Han life-and-travel RPG. Preserve its visual identity, palette, fine parchment-and-bronze interface, refined 2D painterly terrain, slim border, and player strip. Completely replace the continental viewport with a regional map of the Chang'an–Luoyang corridor. ONE landscape 16:9 full-screen game screenshot. No external mockup frame, no poster, no explanatory design notes. This is a regional travel map, NOT a city district, city-builder or battle deployment screen. No scene card carousel or big character portraits.

CAMERA AND STYLE: Near-vertical top-down cartographic view with gently painted terrain relief, no horizon, no tilt-shift or low-angle diorama. Sage-green valleys, warm ochre loess, blue-green rivers, dark wooded ridges, pale earthen roads, fine Han architecture. Continuous terrain, readable playable ground, not densely piled fantasy mountains. Keep forests and relief subordinate to roads and selectable objects. Scale up map symbols moderately from the reference: period towns now have visible low rammed-earth walls, gray tiled roofs and small gate silhouettes; traveling parties use discrete miniature sprites clearly resting on their road. Buildings and people are map symbols, not true-scale aerial photography.

GEOGRAPHIC ORGANIZATION, NORTH UP: Chang'an in the WEST / LEFT, Luoyang in the EAST / RIGHT. The broad Wei valley occupies the western half; the Wei River flows east from the left into the Yellow River at the great bend in the central area. The Yellow River enters from the north near the center, turns east, and continues across the upper half toward the upper-right. Keep the main east-west travel corridor on the south side of the Yellow River, running through the Wei valley, constrained valleys between rugged hills, and the Luoyang basin. A continuous Qinling foothill belt lies south of the western valley, rugged Xiao mountains south of the central corridor. The Luo River flows east across the eastern basin, south of the Luoyang city. Do NOT confuse the Luo River with the Wei River. Rivers remain physically continuous, roads cross water only at a visibly built bridge or ferry, and no track cuts straight through a mountain wall.

Useful composition anchors as percentages of the entire screen, not printable coordinates: city “长安” near x16%, y53%; the Wei River to its north across x6–39%, y43%; the Yellow River north-south bend near x41%, y25–44%, then east toward x60–94%, y27–36%; a small county town “陕县” near x50%, y45%; “函谷关” near x64%,y49%, much closer to Luoyang than to Chang'an; city “雒阳” near x71%,y58%, north of the Luo River at y67%. “函谷关” here explicitly means the HAN Hangu Pass in Xin'an, NOT the Qin pass at Lingbao, not Tongguan. Render a low historic rammed-earth checkpoint across a narrow valley road, with the actual road through its gate. Do not add a second Hangu Pass or a Tongguan label. Main road goes Chang'an → central valley and 陕县 → Han Hangu Pass → 雒阳, bends naturally with terrain, and visually continues beyond each map edge. Show only a few clearly secondary dirt branches leading to settlements and independent sites. No modern highways, railway, modern dams or modern city labels. Major river labels: “渭水”, “河水”, “洛水”; understated small mountain labels “秦岭”, “崤山”. Use the period form “雒阳” everywhere instead of “洛阳”, carefully render 雒 (各 + 隹).

GAME OBJECTS, EACH VISUALLY DISTINCT AND INDEPENDENT:
1. Player party of three modest travelers on the road around x59%,y50%, encircled by a small deep-teal selection/position ring and a narrow label “陆行舟”. No giant portrait. A subtle pale-gold current itinerary follows the real road westward toward 长安, staying on the visible travel surface, with small directional ticks.
2. A selected merchant caravan a short road segment EAST of the player near x65%,y55%, moving west. Two pack animals, one wagon and a few escorts, a distinct thin teal highlight and nameplate “商队”. Keep it small; never fill the whole foreground with people. Its selection connects visually to the right detail panel by matching border color, not a giant arrow.
3. A few ordinary pedestrians on a separate road segment, tiny label “行人”; one modest marching infantry column with a crimson standard farther along the central road, tiny label “军队”. Do not label a historical faction, commander, number, strength or intentions.
4. One compact privately fortified estate off a branch near x28%,y66%, labeled “坞堡”, with an unobtrusive household/seal emblem rather than a government military flag.
5. One KNOWN mountain stockade on a small side trail near x47%,y67%, labeled “山寨”, marked by a separate camp emblem. No revealed hidden enemies or omniscient statistics.
6. A northern river ferry near x79%,y29%, labeled “渡口”: paired landings and a small period river ferry, NOT a bridge. A small cargo vessel on a different river segment; do not imply all rapids are navigable. One roadside relay/post labeled “驿舍” near the route if there is clear space. Site types are illustrative, no fake county names or invented faction names.
Clear separations between traveling icons, independent settlements and official places. Do not color private estates or bandit sites as territory belonging to a city.

COMPLETE GAME UI, exact concise Chinese text:
- Reuse top-left compact player strip: “陆行舟” “旅人” “体力 82/100” with slender teal bar; no added portrait.
- Small centered parchment heading “关中 · 河洛”, smaller “区域”.
- Top-right small “初平元年 · 春” and pause control “暂停”, matching reference. Date is an illustrative screen state, not a reconstructed specific historical event.
- Bottom-left layer buttons “地形” “势力” “贸易” “人物”; “人物” selected in teal. Adjacent “−” “＋”, “全域”, recenter icon. Small checked “仅显示已知”. Keep the controls compact and in the same visual family.
- Small bottom-center activity strip only because a journey exists: “前往长安” · “已暂停”, button “继续行程”. Do not invent ETA, coins, energy cost, danger percentage or quest.
- Very small nearby legend with distinct emblems: “城池” “关渡” “独立据点” “行人” “商旅” “军队”. No bottom venue cards.
- Right-side selected caravan detail panel, ~23% screen width, from y40% to y80%, avoiding city labels and the northern ferry. Light parchment, fine bronze edge, restrained teal selection accent. Heading “商队” and close ×; subtitle “官道西行”; body “目视可见 · 尚未交谈”, “货物与人数未知”. Two clear buttons “查看队伍” and “接近交谈”. This button means approaching before speaking; do not offer instantaneous remote trading or attack. A small top band silhouette of a wagon is sufficient; no full-screen dialogue and no invented character face.
- Tiny system/menu icon bottom-right as in source.

Legibility: clear compact Chinese characters, no collisions, modest transparent label backgrounds, main map remains the subject. Land route, impassable mountains, river crossing, private/organization sites, player party, independent traffic and current selection must be understandable at a glance. Preserve the reference's professional painted-game appearance and parchment UI while creating this regional level; do not keep Asia silhouettes, huge sea, region labels or ocean ships from the continental reference.
```

## 关口连接局部修订

```text
Edit ONLY the Hangu Pass road connection in this supplied regional game-map screenshot. Preserve the whole screenshot, map extent, all other geography, city and unit positions, player and caravan, buildings, palette, painting style, all UI and readable text exactly. The highlighted main road currently skirts below the Hangu Pass gate. Correct this: locally bend the pale earth road and its gold route highlight up into the visible gate opening of 函谷关, pass underneath the gate structure, and continue out on the eastern side toward the caravan. Make the gate orientation and the short road segment physically coherent so the road clearly enters and exits the checkpoint. There must be no parallel bypass path on the immediate southern apron of this checkpoint; replace only that tiny old bypass segment with impassable hillside/rocks matching the neighboring ridges. The current gold route should be unbroken through the gate. Keep all units and their selection rings where they are. The gate's label must read exactly “函谷关”, first character 函, not 画. This is one targeted local connectivity correction; do not redraw or rearrange the rest of the screen, do not add more labels, routes or structures.
```
