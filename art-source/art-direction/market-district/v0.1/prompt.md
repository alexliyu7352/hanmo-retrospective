# 市场街区展开 v0.1 生成提示词

日期：2026-10-03。使用内置 ImageGen，以同一城域为父场景绘制局部街区；结果待评审。

参考 1：[城域点击访问 v0.3](../../city-overview/v0.3/city-visit.png)，用于空间来源、建筑配色、UI 和既有头像。
参考 2：[已确认酒肆内景](../../scene-layout/v4/browse.png)，仅用于材质、绘画质感及院落气氛；不复制该图的时间、底部人物栏或交谈 UI。

输出：`market-district-ui.png`。实际尺寸 1672×941；提示词中的理想尺寸及细节约束不代表全部已经实现，边界见 README。

```text
Use case: ui-mockup / historical-scene.
Create a complete, polished illustrated GAME SCREEN showing the LOCAL MARKET QUARTER expanded from the market labelled “市” in reference image 1. This is the next spatial level of the SAME fictional late-Han-inspired Chinese city. Reference 1 is the parent city, architecture palette and EXACT UI/portrait style reference. Reference 2 is ONLY the approved tavern material/painting quality and courtyard continuity reference; do NOT copy its clock, bottom NPC roster, dialogue UI or interior composition.

The new camera is much closer and lower than the city overview, a 35–45 degree elevated oblique view across ONE compact market neighborhood. Show only 8–12 believable buildings, open fronts, individual doors, small yards and a continuous generous main lane with side passages. These are human-scale buildings, not miniature map icons. The city-wide skyline, entire city walls, government precinct, military camp and river overview are outside this frame. No split screen, no concept-board annotations. Widescreen 16:9 game composition, preferably 2560x1440.

Spatial layout:
A simple modest market entry gate is in the lower-left foreground, with the main lane opening from it into the central market crossroads and continuing upper-right. Low earth-plaster enclosure walls define the quarter; only a little city-wall masonry may appear far in the background. Keep the playable center open and readable.
The visually dominant tavern is at center-left beside the main lane: gray tiled roof, robust aged timber, ochre plaster, wide open shaded entrance, wine jars with muted red cloth seals, low wooden service furniture visible inside. Beside/behind it, an open courtyard with one leafy tree and an inn passage evokes the approved tavern image without claiming pixel-identical geometry.
Across and along the lane put a grain shop with sacks and scales, a cloth shop with restrained indigo/ochre fabric bolts, a small ironware workshop displaying agricultural tools, a herb seller with baskets and simple drawers, and a separate modest guest lodging courtyard. Their entrances must visibly meet the lane. Include merchant stalls with baskets, vegetables, pottery, a small handcart, and only about 15–20 varied life-size-in-perspective townspeople working, bargaining or passing. Keep their faces small: this is architecture and streets, not a character design sheet.
At the rear-right of the quarter, leave a small genuine empty courtyard plot with soil, a few weeds, boundary stakes and no building; nearby one small shop has roof repair timber and a scaffold, physically readable but not dominant. These are localized neighborhood details, not the city-overview riverside parcel or barracks construction.

Art direction:
Match the warm natural daylight, refined semi-realistic 2D painterly rendering, earth tones, grey roof tiles, aged timber grain and plaster of BOTH references. Detailed but readable, restrained foliage, cloth color accents, clear façade/doorway silhouettes, lived-in ordinary market. No Japanese architecture or castle, no Qing streets, no exaggerated upturned fantasy roofs, no red-lantern festival, no photorealistic render, no 3D mobile-building toy style, no modern objects.

COMPLETE but minimal UI, matching reference 1:
- Upper left compact existing player portrait and parchment frame, same person, same name “陆行舟”, role “旅人”, and small “体力 82/100”. Do not redesign this person.
- Top center location cartouche exactly “示范郡城 · 市”.
- Under the upper-left portrait: two small buttons “地点” and “找人”.
- Upper-right: “同行 2” and the SAME two separate compact portraits, labelled “护卫” and “随从”. No large shared panel.
- Bottom-left: clearly readable parchment button “返回城域”.
- Bottom-right: four modest utility buttons “产业”, “地图”, “手记”, “系统”.
- No calendar, clock, time of day, movement timer, cost-to-walk, navigation line, current-position pin, breadcrumb path, minimap, destination confirmation or bottom “前往” panel. Buildings are entered by clicking their plaque/entrance directly.
- Do NOT show a bottom strip of all market people or shop occupants. In a district view, visitors inside separate shops are not all treated as present together.

Clickable place markers are part of the UI and must be crisp, short, anchored directly above the corresponding VISIBLE doorway, not randomly on a roof or empty road. EXACTLY six principal plaques: “酒肆”, “粮铺”, “布帛铺”, “铁器铺”, “药铺”, “客舍”. Use cream paper, dark legible Chinese characters, fine old-brass borders and a very short pointer. No label may overlap another, a companion portrait, or the top HUD. Taverns plaque “酒肆” alone may use a subdued dark-teal hover/focus fill and ivory text; there is no big selection panel and no glowing path. The other five remain cream.
Two quieter secondary state labels at the rear-right: “空院” on the empty plot, “修缮中” on the scaffolded shop. The repair site is not one of the six open venues. Do not place the “空院” label on a built courtyard.
Avoid extra floating labels or decorative writing. Keep UI around the perimeter and place markers near entrances so most of the image remains the neighborhood. Show real threshold space under each marker. Return one finished local-district game screen, with readable UI and a strong sense of entering this market from the parent city.
```
