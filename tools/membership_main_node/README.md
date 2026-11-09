# 具名任职单一Main联合节点

本目录仅是一套联合驱动器。`prepare_inputs.py` 生成正式1.4 compose与独立安装代，`named_membership_main.gd` 驱动实际 `main.tscn`，`run_node.py` 按明确阶段启动进程。没有新游戏工区、测试快照安装、直接改钱/物/知识或自动重跑旧矩阵。

## 准备与最终绑定

准备命令显式提供 `--repo` 当前中央源、`--pack` 正式1.4包、`--selection` 精确选择、`--output` 新目录、`--stage-game` 已有唯一资源stage和 `--engine` 固定4.7.2。它只读stage，恢复缺席原PNG须与当前Git blob及原manifest摘要一致。输出：

- `compose/`：完整正式目录、包锁、表现、Schema与六项原资源
- `install-generation/`：仅新正式内容安装代和必要资源；由中央在最终冻结后覆盖唯一stage
- `runtime-source-binding.json`：实际输入和driver摘要，状态 `MISSING_REVIEWED_ARTIFACT`，禁止运行玩法

中央保全旧stage的源码/库/内容绑定之后再overlay。在输出另一份最终 `runtime-binding.json` 时，必须补入确切 `source_commit == reviewed_source_commit`、`native_extension`、`native_build_receipt`、`registration`、`descriptor` 和实际stage源码/安装内容/导入资源的摘要记录，状态才改为 `BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED`。库路径必须指向已有stage内实际加载文件；准备阶段不伪造库摘要。若Schema或能力文件变化，compose身份也需重新生成并保留之前准备记录。

## 一套驱动器的正式路线

`run_node.py --binding <最终绑定> --case-root <新独占目录> --case <case> --phase <phase>`。每个case独立XDG数据/配置/缓存；其消费者复用同case的原物理profile，但进程独立，不能在生产者尚未结束时恢复。没有显示需求时headless，需要实际绘制可加 `--rendered`。

case只有：

- `funded`：正常标题进充足章，同地分别邀请谷诚、沈蘅，菜单保存未决；本人接受/拒绝后，从原人物事项委派Task，真实采购、行走和交付；证明原工资钱包向本人真实周期付款、组织取得合同酬金。
- `short`：正常进入缺钱缺货章，保存未决，接受后委派原Task；真实部分交付后缺货及欠薪触发本人待释放，保存 `manual-2`；玩家至多正常卖出本来持有的四粟（2粟补旧约余缺、2粟保留真实私人竞争），原Task继续，合法离开并继续普通做工。旧债可以仍未清，原雇主、原义务和旧欠期必须保留。
- `deferred`：先等沈蘅真实承接、采购及部分交付，余货不足时当面邀请并实际暂缓；菜单保存，再正常卖原粮让旧责任继续；只等到原邀请到期30半日，实际去已知粮行找到本人并观察有来源的邀请失效，确认未入职及旧合同依法部分到期清算。
- `recipient_accept`：NPC当前实际职务只向玩家发邀请，菜单保存；玩家实际接受，经过一个有效服务期，从正常本人任职按钮合法离开；证明正余数1/2及新增补偿1/2只形成一次1钱原雇主义务。
- `recipient_decline`：同一正式NPC招揽入口，玩家实际拒绝且没有新增成员。

原始阶段为 `produce`、`consume-pending`、`consume-release`；最后一个只用于short的真实旧责银行。producer创建菜单bank，继续尾程并记录原回执；consumer从空标题“继续”按钮和实际槽确认读取原bank，执行同一尾程。对照剩余正式命令回执顺序、整个世界JSON、实际有效checkpoint原字节，并核物理bank没有被重写。

所有期限统一为作者有限120半日上界，helper只发正常半日。若某路线上真实行为与预期不同，必须留下红证据后依据具体原因修复同一节点，不能悄悄改快照或另起一个绕过该前缀的小测试。

## 当前边界

工具解析和来源准备成功不等于运行成功。尚未最终绑定的目录只可解析与检视；实际Main、原生结算、渲染观察及两个银行的独立冷恢复结论必须来自最终绑定后的真实进程回执。

## 已失败生产者的原释放银行续程

若原short生产者已经真实写出manual-2，而后因过严helper断言失败，保留原红和全部原进程证据。新增 `continue-release-reference` 与 `continue-release-compare` 两个明确阶段仅续原释放银行，不重跑邀请、入职或委派前缀。两阶段须提供 `--saved-case-root <原short目录>`、`--saved-binding <原runtime-binding.json>`、`--saved-driver-root <保全的原driver目录>`，新 `--case-root` 必须是原case之外的新独占目录。

启动器核原进程已终止、原failed状态、原完整证据摘要、原绑定、归属profile、release-state摘要及全部物理银行；还核保全driver与原绑定摘要逐项一致。当前绑定必须保留原目录及同一真实native库。参考与比较各从新空标题恢复同一manual-2，原XDG数据只用于读该银行，新配置/缓存放新case。比较参考的新尾程正式回执、整个世界和有效checkpoint；原生产者继续明确标failed，不改写为全过程通过。

short应核实际到期清算：旧合同3/4交付、6钱比例酬金、19钱未用预算及2钱未得酬金返还，settled且不再有受托货物；13个有效工资期与终态1/2补偿形成33钱应付，实付8钱，原义务仍欠25钱。funded仍须4/4真实交付和8钱合同酬金，不能把这条窄例外泛化成任意失败均通过。若同一银行尾程产生不同事实，保留新红查实。

路线失败但native世界确实空闲时，另存一次实际terminal debug export和source；不清空失败、不重试忙碌世界，也不将导出重新注入游戏。
