# 城域界面 v0.3 生成提示词

生成日期：2026-10-03。使用内置 ImageGen 编辑图像；输出为静态目标图，未经用户评审。

首张以原城景及既有界面风格为参考；城主图沿用本轮旅人图，属官图沿用本轮城主图。提示词规定的目标不等于全部已精确实现，实际输出与边界见 README。

## 1. 旅人点击区域访问

输出：`city-visit.png`

```text
Edit the first reference image (the original painted Chinese late-Han-inspired city) into a COMPLETE, SIMPLER DAILY CITY UI. The second image is UI style/portrait reference only. Create one widescreen 16:9 screen, same city and camera. This replaces an over-designed navigation interface.

NONNEGOTIABLE:
- City areas are directly CLICKABLE NAMED MARKERS on the illustrated city. No route lines, dots, arrows, current-position pin, travel-time estimates, movement costs, destination confirmation, bottom "go to" panel, clock, calendar or date/time bar anywhere.
- No expanded left destination list. No big bottom panel. The city must remain open and readable.
- Real vacant land and a real building UNDER CONSTRUCTION must be visible IN THE WORLD, not merely mentioned in a menu.
- Keep all original city buildings and relative layout, daylight and painterly texture. The role on this screen is a TRAVELER, so no citywide construction authority buttons.

WORLD CHANGES (make only these two localized additions to first image):
1. In the clear grassland immediately EAST/right of the south road and opposite the existing southern military camp, add an unmistakable active BARRACKS EXTENSION construction site: low foundations, partly assembled timber columns and roof frame, bamboo/wood scaffolding, stacked lumber, a few small workers. Located SOUTH/outside the city wall, NOT in the market. Keep the south road and existing camp/training ground intact. This is a single modest new building, not a completed tiled-roof hall.
2. In the east outskirts on the near/west bank of the river, directly SOUTH/below the existing dock and beside its access lane, show a clearly EMPTY rectangular building parcel with earth, corner stakes, rope and a few grass tufts. NO building or tent in this parcel; don't put it in the water. Reserve it for the later construction screen.
The same two world sites will be reused in subsequent role screens.

UI STYLE:
Use warm opaque paper plaques, clear dark Chinese characters, thin dark/old-bronze borders, restrained blue-green selections from reference 2. Refined Chinese 2D RPG UI, no web dashboard, rounded cards, neon or giant wood frame. Reuse the reference player portrait at top left, not a new character design. Important Chinese text must be clean and exact.

TOP LEFT: small compact player portrait and parchment info: "陆行舟", "旅人", "体力 82/100". Same portrait style as reference 2.
TOP CENTER: modest title plaque "示范郡城". NO date, time, location pin or two-mode switch under it.
UPPER LEFT below player: ONLY two small buttons "地点" and "找人" representing collapsed indexes. No open drawer.
RIGHT EDGE: "同行 2", the same two separately framed small portrait tiles as reference 2, labelled "护卫" and "随从". No shared large panel. Don't cover dock or empty parcel.
BOTTOM LEFT: button "出城".
BOTTOM RIGHT: four compact buttons "产业", "地图", "手记", "系统". No status/command panel across the bottom.

DIRECT CLICKABLE MAP MARKERS:
Place small readable paper sign buttons directly above each appropriate area, with a tiny downward attachment pointer to that area. Each is a distinct hit target. Separate the labels clearly; never put them in a list or connect them with navigation paths. Exact labels:
"郡府" — northwest official compound INSIDE walls.
"东里" — northeast residential neighborhood.
"官仓武库" — the two storage buildings directly north of the east-west street below the eastern homes.
"西里" — southwest residential neighborhood.
"学舍" — small school/shrine compound at the lower-left of the walled town.
"市" — southeast market inside walls.
"旧仓" — narrow warehouse just inside south wall WEST of south gate.
"西郊作坊" — workshops OUTSIDE west gate.
"东郊码头" — dock OUTSIDE east gate.
"南郊军营" — barracks OUTSIDE south wall WEST of main south road.
"练兵场" — open training yard immediately west of that camp.
For the two new world sites, add TWO distinct slightly accentuated status labels:
"营舍扩建 · 施工中" above the actual scaffolded building south-east of the south road.
"河岸空地" above the EMPTY staked parcel south of dock.
Avoid overlaps and keep construction and empty ground physically visible under the labels. Markers occupy little space and must not cover whole compounds.

No mission deadline slip needed in this version. No resident portrait strip at bottom. No invented role-switch control. No route preview, "前往", "一刻", "无费用", clock or ETA. No explanatory callouts or developer/version text. One complete daily city-access screen whose areas can clearly be clicked.
```

## 2. 城主选址与新建

编辑基底：`city-visit.png`。输出：`city-ruler-construction.png`

```text
Edit this supplied city UI into the CITY RULER'S CONSTRUCTION PLANNING screen. Preserve the SAME city geometry, camera, lighting, paint style, buildings, river, roads, and especially the actual scaffolded barracks construction site in the lower-right foreground and the empty staked parcel just below the east dock. This is a different UI state over the same living city, not a new town.

Role: the same portrait and player name "陆行舟"; replace "旅人" with "城主". Do not redraw the portrait or costume.
Keep top center title "示范郡城". Beneath it add two small mode tabs "日常" and "城建", with "城建" selected dark blue-green.
No world clock, calendar, travel route, player position marker, arrival time or movement costs. Construction duration is allowed only as an actual construction-project field.

MAIN STATE: select the EMPTY RIVERSIDE PLOT south of the dock, to build a NEW warehouse for cargo. This must actually be visible on the map:
- Highlight the existing vacant parcel with a fine restrained blue-green perimeter and corner stakes.
- Inside this otherwise empty parcel, show a pale translucent proposed building footprint/timber outline, explicitly an UNBUILT preview, not a finished opaque building.
- Label this exact parcel "河岸空地 · 官有" with pointer attached to the LAND, not the water. A smaller tag "货栈 · 拟建" identifies the preview. Keep the dock access lane free.
- Do not move the river or invent an extra island.
- The SEPARATE barracks extension south of the city remains physically UNDER CONSTRUCTION with scaffolding, roof framework, stacked wood and workers. Its label reads "营舍扩建 · 施工中 45%". This is an existing project, distinct from the proposed dock cargo warehouse.
- Keep the existing completed barracks and training yard in the southwest intact.

LEFT construction panel, about leftmost 21% wide, from below portrait to near bottom, on warm opaque parchment with refined thin bronze/dark outline. It may cover the western peripheral workshop but leave all central city, southern construction and east empty parcel clearly visible.
Header "城建规划".
Three tab buttons "新建", "改建", "工程". "新建" selected.
Four build type choices in a compact 2 by 2 arrangement: "货栈" selected, "粮仓", "营舍", "商铺".
Then a clearly separated selected-project section with EXACT readable lines:
"河岸空地 · 官有"
"拟建：货栈"
"用途：储货转运"
"资金：公帑"
"费用 600钱"
"木材 40 · 石料 20"
"劳力 8人 · 工期 10日"
At the bottom, primary DARK BLUE-GREEN button "确认开工" and secondary paper button "取消选址".
Keep all this readable and within the screen; no dense tiny text. No "upgrade vacant land" action.

A modest upper-right resource / authority plaque, outside of main scenery:
"权限：全城公产"
"公帑 2400钱"
"木材 160 · 石料 80"
This refers to public works, not the personal wallet.

Keep DIRECT CLICKABLE regional markers on the city (郡府、东里、官仓武库、西里、市、学舍、南郊军营、练兵场、东郊码头). No navigation or connecting paths. Existing-building actions are reached via their marker; don't falsely present an active upgrade for an empty parcel.
Keep the familiar right-edge companion portraits if space permits; do not cover selected riverside plot, dock, or project labels. The bottom right compact utility buttons remain "产业", "地图", "手记", "系统"; bottom left may be "返回日常" instead of "出城".

UI visual language remains refined 2D historical Chinese RPG: warm paper, dark ink, restrained bronze edging, blue-green primary/selected controls. Clear Simplified Chinese. No web dashboard, neon, full-map square grid, modern industrial construction machinery, role-switch cheat menu, logos, watermark or developer notes.
This image must visibly communicate: city ruler has public planning authority; selecting real vacant land previews a new building; confirm opens a project; another building is already under construction.
```

## 3. 属官管理与升级

编辑基底：`city-ruler-construction.png`。输出：`city-official-upgrade.png`

```text
Create the AUTHORIZED CITY OFFICIAL'S BUILDING MANAGEMENT / UPGRADE state by editing this city UI. Use the SAME city, camera, construction site, empty riverside plot, buildings, sunlight, illustrated style, named area markers, player portrait and companion portraits. Do not invent another city. No navigation route, movement countdown, clock or travel costs.

Role and focus:
- Keep name "陆行舟", change role "城主" to "属官".
- The player is delegated to manage the EXISTING public granary/armory INSIDE the city, directly below the eastern residential area and north of the main east-west street.
- Top center title stays "示范郡城"; tabs under it are "日常" and "设施管理", with "设施管理" selected.
- This official has authority ONLY over "官仓武库", with an 800钱 project budget. Do NOT give this role global construction authority.
- Replace top-right authority/resources plaque with exact text "权限：官仓武库", "授权：维护与扩建", "工程额度 800钱". No "全城公产".
- The southeast foreground scaffolded barracks remains a separate actual ongoing project, label "营舍扩建 · 施工中 45%". It is visible but not selected.
- Restore the riverside parcel south of dock to EMPTY staked ground. REMOVE the translucent proposed warehouse and its "货栈 · 拟建" label from the source image. Label the ground "河岸空地". This official has not started a riverside project.

SELECTED BUILDING ON MAP:
Subtle thin blue-green boundary around ONLY the existing two granary/armory buildings and their own yard. The "官仓武库" marker is clearly selected. Do not outline the whole city or neighboring houses. Buildings remain intact and operational; the proposal is an upgrade, not a completed change.

LEFT PANEL, same warm paper style and approximate width as source construction panel. REPLACE its entire content, no building catalogue:
Header: "官仓武库".
Subheading: "官产 · 受托管理".
Two tabs: "管理" and "升级"; select "升级" dark blue-green.
One compact row of three clearly readable ordinary action buttons: "调拨", "盘点", "修缮". These are existing-building operations.
Then a section for the selected upgrade with EXACT text:
"仓房加固"
"防潮加固 · 扩充储位"
"资金：公帑"
"费用 400钱"
"木材 30 · 石料 10"
"工期 8日"
"授权额度 800钱"
At bottom: primary blue-green button "确认升级" and secondary paper button "返回管理".
No free construction palette, no citywide planning, no level-1/level-2 labels, no role-switch cheat menu. All text large, clear, Simplified Chinese, well spaced with no clipped controls.

Bottom left "返回日常". Bottom right retain "产业", "地图", "手记", "系统". Keep two right-edge companion portraits in individual frames with heading "同行 2".
Keep the same direct clickable map area labels. They remain visitable; do not make all other areas physically inaccessible just because management permission is limited.
Preserve the elegant painterly Chinese RPG look: warm parchment, dark ink, fine bronze edges, restrained blue-green active state. No modern dashboard or glowing sci-fi outlines, logos, watermark, explanatory annotations or developer labels. This must visibly show an officer operating and upgrading a real existing building under a specific delegated scope and budget.
```

