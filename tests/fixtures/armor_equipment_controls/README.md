# 成长页护具配置短合同与局部门

来源：正式 GDD09 的 UI-27 / VIS08，以及 `docs/presentation/art-design.md` 的 UI-27。复用现有成长页墨绿侧栏、同一 `OptionButton` 与命令按钮风格；只加本人护具选择、显式卸下、已选引用和实际防御，不新增装备规则、美术资源、存档迁移或 native 代码。

CharacterSummary 输出本人正数量护具候选（预留项仍列出）和 `Armor.effective` 原结果。GrowthScreen 的选择只预览，确认经 Main `_perform` / Kernel `submit`。陈旧选择可保留，防御仅依实际所有权和预留规则；非本人公物不变成私人候选。新按钮绑定当前报价的人物/版本，并拒绝失活页面，避免旧按钮借新版本重放。

## 验证入口

运行 `python tests/fixtures/armor_equipment_controls/prepare_fixture.py --destination <新的隔离目录>`。准备器薄复用 `skill_study_kernel/prepare_fixture.py` 的 `armor=True` 正式作者、compose/stage 和两张既有 tiny 图，只加入当前 UI/storage 与测试脚本。用固定 Godot 4.7.2 在准备出的 `game` 下先 `--headless --editor --import`，再分别 `--headless --script res://tests/test_armor_equipment_controls.gd`、`test_character_summary.gd`、`test_armor_kernel.gd`。XDG 数据/配置/缓存应显式指向本次隔离目录。

2026-10-04 局部结果：新护具控件 37/0，CharacterSummary 相邻门 54/0，原护具共享 Kernel 门 12/0；中文注释增量与 `git diff --check` 均通过。真实穿卸和最后一件售出由正式 Main/Kernel 完成；预留、组织和代管边界是新局副本，只用于原规则报价和 UI 投影验证。未执行全存档矩阵、全 Linux 聚合或真实美术 GUI 截图验收。

`evidence/red.log` 是未改生产源码 f734576 上的真实新缺口，2/1，仅缺正常成长页护具控件。`summary-baseline.log` 是同一 f734576 上既有 54/2，原 oracle 通过，只有已过期的外层 snapshot=2 期望失败（当前实际0）。中央批准将其改为0，未归因于护具改动。

旧 CharacterSummary oracle SHA 保留原值，未重录。精确兼容剥离名单：新增 `GrowthArmorControls` 子树、`equip_armor` 预览、新 equipment 字段 `armor_id` / `armor_options` / `armor_effective`。所有旧控件文案、画像身份、武器/心法/修炼/传授预览次序与报价、输入/RNG纯性、两页字段、深隔离和正常 Main 路由继续比较；新专测覆盖完整护具路径。

中央集成仍需登记唯一新增真实源码边：`game/core/views/character_summary.gd` → `game/core/domains/personal/personal_armor.gd`，并将本专门准备入口纳入适用验证调度。本分支不修改共享登记或发布入口。
