# 人物总览 v0.1 生成提示词

使用内置 ImageGen，单次生成，未进行后续图像编辑。实际输出 1672×941 RGB PNG；提示中的目标尺寸不代表实际已输出该尺寸。基于真实初始属性及经验，候选禀赋另见 README。

```text
Use case: ui-mockup
Create one finished, high-quality 16:9 PC game character overview screen for the Chinese historical life RPG 《汉末行记》. It must look like a readable, polished actual game interface, with meaningful controls and real data, not a promotional poster or a web dashboard. Target 2048x1152 landscape composition. All UI copy is simplified Chinese, carefully legible.

Art direction: refined hand-painted 2D semi-realistic late Han Chinese world; warm ivory paper reading panels, ink-black text, thin aged bronze borders, dark desaturated teal lacquer header and tabs, restrained vermilion seals. Rich natural painterly portrait shading, tactile linen and wood, beautiful but restrained. Clear modern Chinese sans-serif for body text and numbers; elegant serif only for short headings. No neon, no gold fantasy armor, no anime, no ornate RPG skill-tree constellations. Flat front-facing fullscreen interface with no monitor or browser frame.

Layout: coherent three-column screen approximately 28% left /44% center /28% right, with generous inner padding. Dark teal header title “人物总览”, small game title “汉末行记” at far left, simple “返回” button at far right. A clean horizontal tab row beneath: “总览” (selected teal), “技艺”, “武学”, “关系”, “经历”. No date, clock, navigation map, power score, character level, attribute plus buttons or talent-point spending. Everything fits within frame; no cropped edge text.

LEFT COLUMN:
Large compelling waist-up portrait, taking about two thirds of this column height. Original Chinese male named 陆楷, age 28, a former border garrison soldier now out of office. Black hair tied under a simple dark cloth headwrap, lightly stubbled jaw, alert thoughtful eyes, weathered but young face, warm natural skin, understated gray-blue crossed-collar Han robe with off-white inner collar and practical cloth belt. No officer rank crown, no jewelry or displayed weapon. Relaxed dignified pose, hands naturally placed, anatomically credible. Behind him only a faint low-contrast camp timber wall and soft outdoor light, melting into paper.
Below: large name “陆楷”, “二十八岁”, “在野 · 退伍戍卒”.
Compact facts: “所在　溪口营营门” and “所屬　暂无”.
Three clear condition rows: “体力 100/100”, “疲劳 0/100”, “伤势 无”. HP slim fully filled muted teal bar, fatigue empty bar. Small final field “性格　未有记录”. No invented personality tags.

CENTER COLUMN:
Top section heading “五维属性”. Five evenly spaced compact horizontal rows, numeric value aligned at right with a small info-chevron. Exact entries:
“统率　60”
“武力　60”
“智谋　60”
“政务　60”
“魅力　60”
Each uses the same 60%-length very thin restrained ink-teal bar on a clear pale track, not a radar chart. Small section caption “当前值 · 点选查看来源”.
Middle section heading “先天禀赋”. Three beautiful small clickable seal-like rectangular plaques in ONE row, each with a modest distinct monochrome ink vignette (bamboo book / strong tree root / flowing cord), name and tier:
“悟性　二档” — SELECTED with a teal border and clear small selection tick.
“筋骨　一档”
“协调　一档”
Tier uses three tiny solid/outline lozenges: selected悟性 exactly two filled plus one empty, other two exactly one filled plus two empty. The plaques are traits, not collectible trading cards and not upgrade buttons.
Lower section heading “技艺摘要”, small link “查看全部 ›”. Compact two-column five-row list with enough text space and a column header “等级 / 累计经验”. Use exact ten distinct rows with no duplicates:
left list “步战  1级  100/300”; “骑战  0级  0/100”; “弓弩  0级  0/100”; “武艺  0级  0/100”; “兵法  1级  100/300”.
right list “辩说  0级  0/100”; “交涉  0级  0/100”; “礼学  0级  0/100”; “商术  0级  0/100”; “运输  0级  0/100”.
Skill names and numerals must be crisp and correctly paired. The fractions denote cumulative XP / next cumulative threshold.

RIGHT COLUMN: a permanent large paper detail panel, visibly attached in visual hierarchy to selected悟性 plaque. Heading “禀赋详情”. Near top, tasteful illustrated ink bamboo-book emblem, then prominent “悟性 · 二档” and category “先天禀赋”.
Below, the following short text, with generous line spacing:
“更易领会研习要领。”
A restrained highlighted effect box:
“研习效率”
“+10%”
“用于对应技艺研习”
Following small sections, each readable:
“生效条件”
“实际参与对应研习时生效”
“先天加成合计上限 15%”
“来源”
“先天禀赋”
“亲代记录：未知”
Near lower portion a quiet note “潜力仍需通过后天学习兑现”.
A modest bottom button “查看技艺进度 ›” links to the existing skill page, no “升级” or “确认加点” action. The details must not imply inherited skill levels or automatic skill mastery.

Bottom slim footer can read “点选条目查看详情” left and “Esc 返回” right. Do not put engineering, implementation, mockup, source-code, placeholder or review-status notes inside the game UI; those belong in accompanying documentation. This is a static visual target using source-backed base stats and skill XP, with deliberately illustrative candidate talents, not a claim of live gameplay. Prioritize portrait quality, clear complete Chinese UI, attractive hierarchy and playable interface proportions.
```
