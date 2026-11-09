# 标准Alpha送信与护送真实Main节点

本节点从正常 `main.tscn` 标题点击启程，在当前正式安装目录中选择 `scenario.alpha_life`。未传 `--content`、`--scenario` 或 `--native-resident`；实际采用标准legacy backend，不代表青川society专用原生路线。

`prepare_stage.py --repo <仓库> --source-commit <完整SHA> --output <全新目录> --engine <固定Godot4.7.2> [--resource-root <精确恢复原件根>]` 只物化该提交的必要运行源、编译工具、foundation和表现清单资源，然后调用现有 `tools.build.stage_content`。恢复根中的 `game/assets/...` 文件必须匹配该提交的Git blob OID。缺文件即失败，不修改全局manifest、旧安装代或历史锁。每套新stage有来源绑定和独占准备XDG。

准备完成后，对绑定中的 `stage_game` 使用固定Godot的 `--headless --audio-driver Dummy --editor --import`，保留完整导入日志，并将三个XDG目录指向此stage独占目录。之后执行：

`run_node.py --binding <binding.json> --output <全新case目录> --case <letter-first|accepted-letter|escort-first> [--expect-blocked]`

- `letter-first`：先正常完成家书并回访，再接护送。
- `accepted-letter`：先接护送，暂未和老邱同行，完成家书后回访查询；未送达不能重接或提前回报，然后正常同行履约。
- `escort-first`：先正常护送、清算与重复回访，再完成家书。
- `--expect-blocked` 只用于前两条原红：必须已真正送信成功，并在回访后观察到旧对白结束和护送入口缺失。程序零退出代表阻断成功复现，不代表原产品可用。

标题、任务对白、同行确认、地图选择、半日、真实送达、当面回报和重复回访均走生产控件信号。若实际同行期间出现可战路遇，战术由既有 `Round.make_ai` 在独立战局副本上计算，然后提交 `Main._perform("battle_plan")`；这是规则命令证据，不是战术控件或鼠标验收。没有路遇则记录 `fought=false`，不强制生成战斗或修改随机流。

每次运行有独立XDG、启动来源、命令事件、完整检查结果和关键世界原状态。`.world.json` 是只读证据，不是物理存档银行，也不会被恢复回内核。三条修后路径必须各自证实玩家增加180、沈安减少180，完成回访无重付。任何提前退出、未到终点或引擎错误都不能判为成功。失败尝试保留在自己的目录，不覆盖或伪装为通过。

本节点只验证headless生产控件和实际规则流程；不声称GUI像素、真实鼠标、原生银行或新正式发行包验收。
