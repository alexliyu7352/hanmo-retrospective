# 汉末人物工坊

独立 Godot 人物捏脸与换装工作台。三个成人比例族、每族四名实际取样代表已完成本阶段验收；眉、眼、鼻、口和衣发可真正独立组合。不连接主游戏、世界或存档，不替换历史默认素材。

## 直接运行

使用项目固定的 Godot **4.7.2.stable.official.ed1daf0bf**，在仓库根目录执行：

```sh
/path/to/Godot_v4.7.2-stable_linux.x86_64 --path game/character_studio --editor --import --quit
/path/to/Godot_v4.7.2-stable_linux.x86_64 --path game/character_studio
```

也可在该版本编辑器打开本目录 `project.godot` 并运行。`run/main_scene` 已直接指向 `studio.tscn`，无需经过主游戏、原生扩展或测试脚本。初始画面就是可操作工作台；默认适中男像保留原像素。

## 一次连贯操作

1. 可从右上“代表样本”载入三族各四名已审代表，或直接在“底稿”选择比例族。样本是显式整组替换，可一步撤销；原锁保留，锁只约束随机。
2. 切换四种脸型，再分别选择眉、眼、鼻、唇；换脸型会保留这些选项的语意。
3. 向下滚动，切发冠与衣身，试受控肤色、发须色、衣色。衣身不会改脸。
4. 左下切半身/头像、浅底/深底/棋盘；旁边88/96px头像来自同一已合成人物。
5. 锁定要保留的项后“随机未锁定项”；可撤销、重做、重置本组或“恢复进入时”。

灰项和底部原因说明的是不兼容候选，不代表当前人物非法。例如短髭须仅适配两男性族各自基础脸型+平唇；带须切女性族会明确拒绝，当前人物及历史保持不动。先手动选无须即可切族，不会偷偷换脸或剃须。

## 已做与边界

- 三个成人适配族分别有自己的完整头颈肩躯、领口、发冠遮挡与裁框。
- 每族四骨相，眉/眼/鼻/口各两款；衣身两款；肤色、发须色、衣色各三组受控颜色。
- 各骨相用自己的 `face_profiles` 源及固定作者区域。1024×1536完成合成后整体裁切/等比缩图，没有自由拖拽或局部拉伸。
- 跨族保留稳定 `choice_semantics`，真实缺件或兼容冲突原子拒绝；撤销/恢复可还原实际像素。
- **12代表限实际取样**：三族各4，非16格换件=16人，也未逐一认可所有理论组合。详见 `evidence/twelve-representatives-96.png` 与同名JSON选值。
- 未做儿童/老年比例、额外身形、持物/动态姿态、大衣装库、战斗装备外观、主世界/存档整合。灰发不等于老年比例完成。
- 肩袖按半身取景裁切，头像按各族自己的正方形安全框等比取样，不是完整全身立绘。

右上“操作说明”随时可查操作和阶段边界；返回或Escape不改变人物。关闭工坊后本次选择不会保存。只读 `representatives.json` 是样本按钮与12人证据导出的共同选值来源，version1严格限定三族各4种不同face，缺项或非法条目整批拒绝，不能同脸换鼻凑数。

正式入口清单是 `fittings.json`，对应 `catalog_medium_v3.json`、`catalog_female_v2.json`、`catalog_long_v2.json`。历史catalog、失败研究源、提示词和旧证据均保留。

## 实际证据

- `twelve-representatives-96.png`：三行各四名代表；detail版同源放大。每行锁同衣发肤色及本族平静表情。
- `medium-profiles-*`、`female-profiles-*`、`long-profiles-*`：每结构独立五官和衣冠组合，不以格数计算身份。
- `female-identities-switched-96.png`、`long-identities-switched-96.png`：保持同表情，仅切眼鼻后的原生96px对照。
- `*-brow-eye-cross-combinations.png`：眉眼四种交叉，检查残眉与边界。
- 适中族真实GUI：8帧/24次鼠标，`medium-gui-verification.json`。
- 女性族真实GUI：9帧/28次鼠标，`female-four-gui-verification.json`。
- 偏长族最终真实GUI：9帧/32次鼠标，`long-four-gui-verification.json`。包括第三结构平睑+挺鼻、衣冠头像、跨三族face3语意/撤销、短须缺件拒绝、1280×900窄窗和入口恢复。官方终端exit0/新prompt/无Godot已核实。

旧 `identity-candidates-96.png` 和 `long-fitting-tests.json` 的候选矩阵是早期两脸组合检查，不能作为本次12身份验收或否定本次新版样本。原始阶段记录见 `docs/presentation/character-studio/` 下 `IDENTITY_DIVERSITY.md`、`FEMALE_IDENTITY_DIVERSITY.md`、`LONG_IDENTITY_DIVERSITY.md`。

## 对应检查

从仓库根目录给同一Godot加 `--headless --path game/character_studio --script tests/<脚本>.gd`：

- `test_studio`：196项会话/控件/锁定随机/缓存/兼容检查（已有相邻验证，当前基础模块未改）。
- `test_face_profiles`：568项逐脸源和区域合法性、失败原子性、旧默认像素兼容（已核证据，当前渲染API未改）。
- `test_medium_identity`：131项；`test_female_identity`：128项；`test_long_identity`：128项。数值通过不能代替看实际图。
- `test_medium_workbench`：67项；`test_female_workbench`：75项；`test_long_workbench`：50项。三族四脸已合法，真实缺须拒绝仍覆盖。
- `test_fitting_switch`：204项；`test_long_fitting`：122项。最终第三族接入后已重跑。
- `test_representative_catalog`：484项固定样本/非法数据原子拒绝/稳定族ID重排/深拷贝隔离。
- `test_studio_entry`：173项全部12样本整组载入、保锁、后续独立编辑和单步撤销、未知样本拒绝、遮挡底层随机、窄窗与Escape。
- `export_twelve_representatives`：从同一代表清单重绘12名取样代表与准确选值JSON，接入口后两张PNG的SHA保持完全相同。

`capture_long_workbench.gd` 必须在真实图形显示运行，自动鼠标操作/截图后退出，headless明确失败。`capture_medium_workbench.gd`、`capture_female_workbench.gd`已更新三族完整四脸边界，扩展输出采用 `-extended`，本轮未重跑它们的GUI，不覆盖此前真实回执。

`build_*`脚本仅是作者源层导出工具，不是普通用户入口。修改源后须重新导出/导入并查看真实组合；不把数学分区或测试数量当作画面验收。

## 独立入口收口

`tests/capture_studio_entry.gd` 在真实X11/Godot4.7.2完成9帧、24次鼠标：从工作台查看说明，锁定底稿/眼形后显式载入女性代表，独立修改眼鼻/衣发，再撤销编辑和样本，载入偏长代表头像，检查1280窄窗样本与说明并恢复进入时。所有帧实际查看，官方终端PORTRAIT_ENTRY_EXIT=0、新prompt与无Godot核实。证据为 `evidence/gui-entry-*.png` 与 `studio-entry-gui-verification.json`。

没有新增启动菜单或美术，初始入口仍是同一真实工作台。此次相邻 `test_studio` 196项重新通过；只读样本清单未改任何核心会话/渲染接口。
