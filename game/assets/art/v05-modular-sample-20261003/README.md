# V0.5 原创可组合人物最小样板（候选，2026-10-03）

目的：用真实半写实人物美术验证学龄儿童/成人独立比例、两种整头选件和两套日常衣服的自然装配。不是几何占位，不是正式发行美术，也不是完整捏脸能力验收。

## 最小素材与装配

- **8 张运行层**，均为 512×768 RGBA PNG，原生生成源为 1024×1536。统一正面静立、左上暖光、同一内领连接点。仅作高质量缩小导出，未程序绘制、修脸、抠图或合成新的美术内容。
- 每年龄族有 2 张互斥 body 选件（头、脸、发、颈、内衫作为完整复合层），以及 2 张独立 garment（蓝灰细麻、苔褐粗麻补袖）。每次仅装配 **1 body + 1 garment**，所以没有默认头与替换头叠画的问题。
- `child` 是独立生成的大头肩比、短颈/窄肩儿童，不是缩小成人；`adult` 为普通青年男性。画面只证明这两个样板比例，不证明婴幼儿、女性、老年或全部体型已覆盖。
- `anchor` 和 `support_point` 均为 `[50,100]`，无位移；body z=0，garment z=10；衣领/袖身遮盖内衫连接面。儿童与成人使用不同 `body_basis`，禁止串穿。
- `face_shape=round/angular` 只是本候选整头选件的语义入口。脸、五官、头发没有被宣称为独立可混搭槽。将来可以在原有 body/face/hair/beard 合同中增补资产，不需要另造清单接口。
- `plain` 指蓝灰日常装，`travel` 指苔褐出行装；两者都是日常衣服。无盔甲、真实装备联动或战斗美术；`battle_profile.style=none`。

## 清单与使用边界

`candidate-manifest.json` 是独立可校验的 v2 候选，引用现有 city 作为默认背景，只登记本包的 8 张人物层和 `v05.sample.person`。没有修改 `game/data/presentation.json`、人物内容或生产代码，也没有注册正式可选项。

合同域为 8–40 岁（child 8–15，adult 16–40），仅供样板解析，不代表逐岁成长绘制已完成。头像完全复用所选半身层，使用声明的儿童/成人不同 safe_crop。

## 真实验证

- `evidence/python-registry-validation.json`：现有 Python 正式资源/几何/组合校验，无错误。
- `evidence/godot-validation.log` / `.json`：固定 Godot **4.7.2.stable.official.ed1daf0bf**，生产 `PresentationRegistry`、`CharacterPresentation`、`LayeredPortrait` 实际加载并 CPU 合成；**61 检查，0 失败**，8 种组合均无降级。
- `evidence/godot-renders/`：8 张真实半身输出 + 4 张同层头像输出。证据脚本仅在本独占目录内，不改世界/生产清单。
- `previews/godot-eight-combinations.png` 和 `godot-eight-combinations-dark.png`：同一引擎结果放到浅/深纯色底上检查 alpha。上排成人、下排儿童；从左至右为 round/plain、round/travel、angular/plain、angular/travel。
- `previews/godot-avatars.png`：成人两选件、儿童两选件。已实际查看上述引擎渲染图，脸颈/衣领连接自然、无双脸/双头，换衣不替换 body。
- 透明度按实际 alpha 检查，8 张层的完全透明像素约 **41%–61%**。生成图透明区域的 RGB 仍有底色，这是未显示的 RGB，不等于不透明背景。保留生成 alpha，不阈值抠边；少量极低 alpha 边缘会进入真实包围框，清单如实记录。

未做：真实游戏窗口/字号/窄屏验证，作者选件 UI 接入，全年龄、五官独立控制、专绘与战斗扩展、正式发行资源接入。此包仅补真实素材候选和只读 CPU 装配证据。

## 原创来源与复核

全部 8 张源层由内置 `image_gen__imagegen` 新生成或基于本轮原创源层编辑，共 8 次调用，没有使用 CLI/API key、下载角色图或临摹任何特定游戏人物。只查看本工程旧 traveller/apprentice 美术以理解既有风格；没有将它们作为生成输入。

`source/prompts.json` 保存完整提示词、工具和默认输出位置回执；`source/` 留存本轮生成源图。`layers/` 为可引用的最终等比导出，`evidence/asset-metadata.json` 保存最终 PNG 的 SHA-256、尺寸、真实 alpha_bounds 和透明像素统计。发布与生产接入由主任务统一进行，本目录未独立提交或推送。
