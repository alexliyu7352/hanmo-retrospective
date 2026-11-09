# 已接受行动有限继续的实际Main节点

本节点只使用同一实际 main.tscn、原作者内容、现有行动与真实物理保存菜单。准备、解析检查与原生玩法分别标记；未绑定中央受审库前启动器拒绝执行。

`prepare_inputs.py --repo <受审源码> --output <新目录> --stage-game <已有唯一stage/game> --engine <固定Godot4.7.2>` 通过原prepare/compose/staging正式生成当前基础内容及青川1.6安装代。它不修改共享stage，不创建世界。基础Alpha的锁由当前原作者、当前Schema和能力描述重新产生，旧编译目录不能冒充当前身份。

中央组合冻结后，精确合入生成代、Main源码、原生库和字节一致的资源导入，记录最终 runtime-source-binding.json。使用同一原生库运行每类的三个明确阶段：

- `--case travel --phase produce`：原Alpha人物从实际地图接受原路线，N=1有限预算停住，确认新的有限预算后在发出下一许可前保存。
- `--case rest --phase produce`：原Alpha庄主实际走入west.estate，在家事/休整页按原180半日按钮接受长休。N=2停住；再次确认有限预算后，在真实Timer的grant票据在途时点击菜单保存。原半日可完成或撤销，槽窗口必须等稳定边界，保存原计数与剩余额度。
- `--case study --phase produce`：原青川人物先报名原三半日手动礼法。未答题时自动继续禁用，原手动半日仍可用；取消后用原余款报名自动课程，保留既有首半日。N=1/M=30实授一课，M=29候选必须零世界、收费、经验和请求号效果。再选N=1/M=30并保存。
- `--phase consume-auto`：生产者已成功退出后，独立空标题从原manual-1物理银行恢复。剩余许可必须停住；等过原Timer间隔仍无推进，再点击“确认继续剩余预算”。
- `--phase consume-manual`：另一个独立空标题读取同一银行，按相同数量的原手动“半日”按钮。完成整世界JSON与自动消费者逐字节相等，所有消费者均保留原A/B银行摘要。

`run_node.py --binding <最终绑定> --case-root <本类新隔离目录> --case <travel|rest|study> --phase <produce|consume-auto|consume-manual>` 每次只运行一个明确阶段。三个case各有独立profile；手动消费者要求自动消费者已终止；自动失败仍执行独立手动尾程，并如实标记缺失的比较结果。`--rendered`才会从实际viewport保存截图；headless结果不声称GUI像素验收。

本输入不改变教师、路线、角色、钱包或原始题目，不构造已接受行动或回填导出世界，也不重跑旧修业/人事/军旅矩阵。费用毛支出中的收入、预留与失败原子性由同批原生针对性门覆盖。

`overlay_stage.py` 只在中央宣布冻结后使用，准备脚本本身不应提前执行。参数包括 `--source-commit <最终前端SHA>`、`--native-source-commit <真实库构建SHA>`、`--preparation <v2/runtime-source-binding.json>`、`--output <全新绑定/保全目录>`、`--native-extension <本次真实so>`、`--native-build-receipt <terminal完成回执>`、至少一个 `--prior-binding <原stage绑定>`，可重复 `--prior-resource-game <原导入所在game>`，以及明确的 `--execute-reviewed-freeze`。

前端与原生源可以不同，但它们的全部原生生产代码必须逐字节相同。脚本保留原生库的真实构建SHA；先核完全部写入来源、两把正式内容锁、旧安装代、现有descriptor/registration和字节相同的原图片导入，再写唯一既存stage。每个被替换文件先保存原字节，原目录完整摘要、覆盖计划和逐项写入记录均保留。最终绑定包含每一份当前core/UI/storage源码摘要、独立helper副本、实际新库与原构建回执；不会把未运行的绑定声称为玩法通过。

`run_batch.py --binding <最终绑定> --output <新的联合目录> --rendered-producers` 先顺序运行三个图形生产者，再从成功生产者的原银行运行自动与手动消费者。自动失败仍跑独立手动尾程；仅生产失败时跳过其无合法银行的消费者。收齐整组后统一报告，不在组中修复重跑。需要与前端SHA分开的工具修订时，overlay可用 `--helper-commit <精确helperSHA>`，所有helper另存副本并保留独立身份。
