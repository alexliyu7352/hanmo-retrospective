# 城域总览 UI 补全提示词

使用内置 ImageGen 编辑原有城景；以下保留初次补全、路线样式修订与最终行动标签修订。交付图片为最终修订版。

输入底图：`../v0.1/daily-overview.png`。界面及头像画风参考：`../../scene-layout/v4/browse.png`。

## 初次补全

Use case: precise-object-edit / ui-mockup.
Asset type: COMPLETE Chinese historical RPG CITY OVERVIEW interface target, one widescreen game screen.

INPUT 1 is the city background to EDIT. Preserve its city architecture, exact layout, camera, colors, daylight, wall, four gates, market, outside camp, training yard, workshop, dock and river. Do not redesign or relocate buildings. Add a carefully designed usable interface over this existing image.
INPUT 2 is the approved interface and portrait STYLE REFERENCE. Reuse its refined warm-paper labels, dark ink typography, tiny bronze frame details and existing companion portrait identities. Do not copy its tavern scene or its bottom crowd roster. Only borrow the young female guard portrait at upper right, the male attendant below her, and the male traveler portrait near the lower right for the player identity. These are UI portrait samples, not new character designs.

GOAL
The previous image omitted all gameplay UI. This version must look like an actual complete in-game UI-08 city exploration screen, with a visible selected destination and usable commands. Use the FIRST image as the full-bleed backdrop, SAME 16:9 view. High-quality 2D Chinese historical RPG interface, not a web dashboard, not a promotional poster. The city remains the central visual subject. Large readable, sharply formed Simplified Chinese text with no gibberish. Short labels must match exactly. Use an elegant readable CJK text face, restrained Song-style short headings. Warm opaque parchment under text; dark ink; very thin dark/old-bronze edges; dark blue-green with pale lettering for selected state and main button. No bright red selected state. Rectangular subtly clipped corners. No huge enclosing wood frame, no glassmorphism, no rounded dashboard cards, no neon.

LAYOUT / CONTENT (all regions required):
A. TOP LEFT, compact current player group, about x2%-23%, y3%-12%:
Small existing traveler portrait from input 2, next to one compact parchment status plaque.
Exact text: "陆行舟" (main), "旅人" (small), "体力 82/100" (small with a modest thin bar). Keep this compact, not a huge character panel.
B. TOP CENTER, location and game time:
A short title plaque "示范郡城 · 南门", with a smaller adjacent or below plaque "中平元年 四月初八 · 午后".
Below them, two compact mode buttons "游览" and "经营规划". "游览" is selected dark blue-green. "经营规划" is an available entry to a separate planning view, NOT an enabled build command.
C. TOP RIGHT, one modest notification slip:
Exact text "明日午前：递交书信". No extra quest stack. Do not hide the north gate with a large panel.
D. LEFT SIDE, an OPEN destination drawer about 15% of image width, y19%-69%, along the far-left edge. It may partly overlap the far-west margin but must leave the main city unobscured.
Header tabs: "地点" (selected), "找人" (unselected), a small "收起" action.
Under tabs a simple search field with text "查找地点".
Nine clear rows in this exact order, one per row:
"郡府" with small status "需通报"
"市" with small status "开放" — SELECTED dark blue-green
"东里"
"西里"
"学舍"
"官仓" with small status "受限"
"西郊作坊"
"东郊码头"
"南郊军营" with small status "门禁"
Maintain generous line spacing and no truncated labels. No resource counters or unrelated metrics here.
E. ON THE MAP:
A small restrained lozenge position marker at the SOUTH GATE, with the short label "当前位置".
A thin blue-green selection outline around the existing SOUTHEAST market compound, with only one compact parchment marker "市". No large painted overlay obscuring stalls.
A subtle dotted walking route from the current position north THROUGH the south gate, then east ALONG THE STREET INSIDE the south wall, then into the market's south entrance. Keep the route on visible streets; never cut across roofs, wall or river. This is a selected route, not a changed building layout. Do not label every building permanently.
F. RIGHT SIDE, at the far right margin below the notification, about x91%-98%, y20%-52%:
Heading "同行 2". Exactly two small stacked portrait tiles using the SAME young female guard and male attendant from input 2. Caption first "护卫", second "随从". Individually framed portraits and narrow name tabs only; NO common background panel behind the column. Leave the dock readable.
G. BOTTOM CENTER, compact selected-destination command group over foreground fields, x31%-74%, y82%-96%, leaving the camp and south gate visible.
One short readable plaque: "市 · 开放" as heading, then "酒肆、布帛、器物" and "步行约一刻 · 无费用".
Directly below or beside, two complete rectangular buttons:
"前往市门" — clear DARK BLUE-GREEN primary
"查看场所" — secondary parchment.
All of these text lines and both buttons must be visible, not stacked over one another. Keep this group limited in height; do not fill the whole bottom with a panel.
H. BOTTOM LEFT, one standalone parchment button "出城".
I. BOTTOM RIGHT, three compact plainly labeled utility buttons in a single row: "地图", "手记 2", "系统". No full-width bottom bar.
No bottom resident portrait roster. The only portraits are the one current player at top left and the two current companions at right.

COMPOSITION / CHECK:
Keep comfortable safe margins, no clipped borders or text. Hierarchy: city view first, selected market and action second, navigation and state third. UI must be integrated with the approved game's visual language, visibly legible at normal viewing size. No invented production/developer labels such as UI-08, mockup, version, prototype, placeholder, testing. No explanatory arrows, numbered design callouts, logos or watermark. All specified Chinese UI labels rendered accurately. Keep the whole background and spatial topology intact; only add the interface.

## 路线样式修订

Use case: precise-object-edit. Edit ONLY the route overlay in this existing complete Chinese RPG city interface. Preserve EVERY UI panel, portrait, Chinese text, button, layout, city building, camera and background exactly. Do not recompose the screenshot.
Fix the selected walking route so the preview agrees with the button "前往市门":
- Remove the bright cyan glow, white-blue sparkling dots, and neon building outline.
- Use a fine, subdued dark blue-green dotted path with a thin warm pale keyline for readability. No bloom or glow. The selected market boundary is a quiet thin blue-green outline along the SAME entire market perimeter, not luminous.
- Put the CURRENT POSITION lozenge and its "当前位置" plaque on the GROUND immediately SOUTH of the south gate, below the gate building, not on its roof. Remove all path continuing farther south below that marker.
- The path starts at this current-position ground marker, goes north through the south gate, then turns east on the street just INSIDE the southern city wall, and STOPS at the SOUTH-FACING ENTRANCE to the southeast market, at its boundary next to the small roofed market gate. It must NOT continue north up the market's internal aisle to the center.
- Keep the small "市" label identifying the selected market. Move its destination lozenge to the actual southern market entrance so the route end and destination marker coincide.
- Path stays entirely on visible roads and through openings, never on a roof, wall, garden or stall.
Keep all text EXACTLY as it is, especially "前往市门", "查看场所", "经营规划", "地点", "找人", "同行 2". Maintain the existing image dimensions and all other visual content.

## 最终标签修订

Edit only ONE Chinese button label in this supplied complete game UI screenshot. In the dark blue-green main action button at the BOTTOM CENTER-LEFT, replace the existing four-character label “前往市门” with the exact four-character label “前往市场”. Preserve the button size, color, typography and position. Do not change any other text, UI, portrait, selected market outline, route line, building, background, lighting, camera, dimensions or composition. The destination is the market compound, so the action must read exactly “前往市场”.

