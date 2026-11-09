# 08｜程序架构、数据、存档与跨平台详细设计

**基线：总体设计第21—23章。决策：I04、I47—I52。**

## 8.1 已核对的技术约束与选择

2026-09-09核对Godot官方文档：Godot 4网页导出使用WebAssembly、WebGL 2.0及Compatibility路径，C#项目不能按该官方路线导出Web；单线程导出可避免把跨源隔离作为本项目初始部署前提。[R1] 因而沿用GDScript和单线程网页基线，不引入第二套Phaser规则实现。

具体Godot小版本不在未执行工程探针前冒定。工程首个验收要求一人物、一次旅行、一次交易、一次保存，Linux与Web分别运行；通过后构建清单记录准确引擎版本、提交标识、模板标识和导出设置。这里不宣称已找到“最好最新版本”，也不把未锁定构建混同于未定义游戏规则。

规则对象可使用RefCounted等GDScript对象但不依赖SceneTree节点；不声称因此完全不依赖Godot。原生文件与网页文件分别实现适配层，核心不写浏览器API。

## 8.2 模块依赖与职责

| 模块 | 写入职责 | 对外契约 | 不允许依赖 |
|---|---|---|---|
| ContentCatalog | 已校验静态定义索引 | 按ID读、列资格、内容版本 | UI节点、玩家存档副作用 |
| WorldKernel | tick/revision、提交顺序、不变量 | preview/submit/advance、稳定状态 | 文件选择器、音乐动画 |
| PersonRules | 属性、经验、关系、任职 | 资格/请求/成长结果 | 全军越权调度 |
| EconomyRules | 报价、所有权、账单、店务 | quote/trade/transfer/day_close | UI按钮、绘图 |
| TaskRules | 目标事实、期限、清算 | accept/report/settle | 自行推进时钟 |
| EventRules | 候选、绑定、选项效果 | enqueue/present/choose | 任意脚本求值 |
| DuelRules/ArmyRules | 回合、命令、伤害/损失 | plan/resolve/result | 物理动画决定胜负 |
| WorldAI | 根据已知信息选合法行动 | proposals、决策理由 | 绕过WorldKernel写资源 |
| SaveCodec | 状态与版本编解码、校验 | encode/decode/migrate | 存储成功假设 |
| StorageAdapter | 实际持久化和导入导出 | put/get/list/export/import | 游戏经济/战斗规则 |
| Presentation | 输入、场景、文本、音画 | 订阅结果，提交草案 | 直接修改权威状态 |

规则模块返回效果，由Kernel按固定阶段合并提交。没有微服务、HTTP游戏后端、消息队列、ECS总框架、数据库服务器或运行时大模型。浏览器IndexedDB是本地存储适配，不是引入联网游戏后台。

### 请求、结果与幂等

`Command`含kind、actor_id、参数、expected_revision、request_seq。每个操作主体的已接受request_seq单调增加；相同序号已执行则返回缓存结果，缓存淘汰后返回AlreadyApplied，不再执行。序号更旧同样不重新执行；新序号但版本过时返回StalePreview。实例ID由世界计数器产生，存档恢复计数器。

`ActionResult`含result_id、from_revision/to_revision、start/end_tick、实体变化、领域事实、显示消息键及参数、需交互事项。结果发生一次；不把它做成无限历史重放系统。保留最近64个请求结果便于重复点击反馈，长期事实由任务/卡片/账单自身的终态保存。普通明细账保留最近360游戏日，重要人生记录最多2000条；更早非关键记录汇总到累计统计。未清任务关联、一次性标记、解锁条件计数与现有资源不随日志清理删除，不能靠剪日志重新领取成就。

读旧手动档后另走一条人生是允许的单机行为。不同周目不能合并局内财物，因而不需要防止玩家主动读档的服务器审计。

## 8.3 数据表示与校验

静态内容采用UTF-8 JSON，以稳定ID关联；显示文本通过text_key引用；资产清单记录asset_id、路径、角色关联、来源/许可和正式/临时状态。实际制作可用表格辅助，再转成同一份规范内容，不让运行时依赖Excel或外部服务器。

| 记录 | 必填核心字段 | 参照完整性 |
|---|---|---|
| ActorDef | id、origin_tag、attributes、skills、initial_place、text_keys | 地点/组织/资源存在；属性范围合法 |
| SettlementDef | id、type、facilities、market、owner、map_xy | 8城2乡1渡1营数量；合法可达 |
| RouteDef | id、端点、长度、模式、风险、费用 | 端点存在，长度正整数 |
| ItemDef | id、price、weight、stack_limit、category | 重量>0，价格>0 |
| CardDef | id、type、condition、effect、hint、icon | 效果类型合法，跨战斗引用不允许 |
| TaskDef | id、模板、目标、期限、资金、清算、文本 | 10类目标字段齐全，预算不为负 |
| EventDef | id、条件、人物槽、选择、效果、失效支路 | 专属/替代人物条件清晰，循环可终止 |
| ScenarioDef | id、epoch、人物/组织/据点初始状态、enabled_features | 不把P2系统放进P1正常入口 |
| CareerDef | id、阶段条件、权限、事业/结局 | 晋升不是无效果的名称切换 |

正式加载前批量检查所有引用、重复ID、负数、无出口事件、无法完成的必需任务、无人可教的必需技能。报告指明文件和记录ID。缺少必需内容则停止开启该剧本，不用空白头像或无作用卡片默默顶替正式交付；内部测试夹具可明确标测试资源。

核心金额、数量、tick、基点均为有界整数。P1单个数字绝对值最大2^31−1，乘法中间值须控制在有符号64位内；批量价格逐件累加前检查溢出。超过上限报数据错误/交易过大，不能环绕成负数。

## 8.4 随机流和可复现边界

使用封装的Godot RandomNumberGenerator实例，world/event/duel/battle/minigame五个独立流；UI、音效和装饰随机不得调用这五个实例。官方文档说明其底层算法是实现细节，支持保存恢复state，但不应假设以后引擎版本永远保持同一序列。[R2]

因此保证目标仅为**相同已锁定引擎构建＋相同ruleset/content＋相同快照＋相同动作序列**跨平台规则一致。跨引擎升级必须跑固定序列回归和存档迁移，不能只改版本号就称兼容。

种子生成只在开局：对UTF-8的`用户种子文本+冒号+固定流名`求SHA-256，取前8字节按大端无符号整数解释，再与0x7fffffffffffffff相与，得到该流seed；保存seed和原始state的十进制字符串以及rng_backend构建标识。加载先设seed再恢复曾保存的state，不再次randomize。不会任意伪造state值。所有随机判断只用整数区间；遍历字典先按稳定ID排序，不让容器迭代顺序改变取随机数顺序。

需要比较跨平台结果时使用规范化状态摘要，排除真实保存时间、存档路径、动画时长和UI设置；保留全部玩法字段。只比较随机seed不够，还要比较已经消费后的state。

## 8.5 存档封装与安全边界

文件扩展名`.hmsave`，UTF-8 JSON封装：magic=HM-SAVE、format_version、ruleset_id、content_version、engine_build、rng_backend、save_id、generation、payload_length、payload_sha256、payload_json。`payload_json`是核心生成的确定字节文本，再作为字符串装进外层；避免各浏览器对重新格式化JSON产生不同摘要。

payload含WorldState、正在进行的行动/事件/战斗、已完成清算、重要历史及本局图鉴。外层保存时间只用于显示。64位seed/state都为字符串；游戏数值在安全整数上限内，JSON解析后必须校验整数性，不能把3.5个士卒或非数值塞回规则层。

全局档案独立保存设置、图鉴和人物解锁。导入局内档不清空现有全局档案；仅在明确“合并图鉴”操作时合并发现集合，不能并入上一周目的财物。本局取得的新发现携带待投递记录，即使全局档案写失败，重新加载本局也能按集合补记，而不会重新发一次物质奖励。

导入上限64MiB、嵌套深度32、单文本字段16384字符；检查未知必要版本、重复键/ID、非法引用、资源上界、校验值。校验摘要用于识别损坏与版本混淆，**不是签名、加密或防单机修改系统**。不把文件内容交给eval、ResourceLoader脚本或任意构造器。

未来内容版本迁移采用显式映射：旧ID→新ID、字段默认与规则变更。原始文件保留，迁移到新槽。不支持的更新版本拒绝并说明需要哪个版本，不能随机猜字段；已删除关键人物/任务不能静默丢弃后称迁移成功。

## 8.6 稳定保存流程

保存状态：`Idle → SnapshotReady → Writing → ReadbackVerify → Saved`；任一步失败进入Failed，保留当前内存世界和旧有效档。**规则提交成功**与**文件保存成功**是两件事，UI分别提示。存储失败不回滚已经玩的行动，也不禁用文件导出。

3个轮换自动槽、10个手动槽；每槽保存当前和前一代有效快照。创建/读取只在安全边界，战斗输入期、长行程路段进度都可保存。写盘异步期间可继续读信息；新的自动保存请求合并为最新安全快照，但不能让较旧写操作在较新之后覆盖它。每槽同一时刻一条写流水线。

### 原生适配

为每槽保留A/B两代。将下一代写入非当前代的临时文件，flush并关闭，读回检查长度/摘要/格式，再替换该备用代文件；前一代有效文件保留。读取时检查两代，选择最高有效generation；不依赖一份容易损坏的“当前指针”才能找到存档。目录元数据可重建。

手动覆盖先说明将替换哪个槽；坏文件不要就地修复，允许原样导出故障副本。此策略提高异常恢复能力，不承诺所有文件系统在突然断电时绝对零丢失。桌面P1不支持多个原生进程同时写同一用户档案目录。

### 网页适配

选择一层小型JavaScriptBridge接入IndexedDB，核心传递同样的完整快照文本。Godot提供JavaScript对象调用和回调机制，回调引用需保留到操作结束。[R3] 不把引擎虚拟文件系统的写入返回直接当成浏览器持久化已确认。

存储使用snapshots（各代快照）、slots（当前/上一代引用与generation）、profile三个object store。摘要和编码在事务前计算；一个readwrite事务读取预期generation、写入新快照并更新槽引用。事务内不穿插无关await，避免事务因事件循环而失活。[R4]

以事务complete而非单次put成功回调判定提交，再用新只读事务读回并校验，才显示“已保存”。事务abort、容量不足、权限拒绝、意外关闭分别返回可读错误。浏览器事务完成也不是对断电永久性或系统永不清理的承诺；MDN明确讨论了事务完成与真实磁盘刷写的边界。[R4]

同一槽多标签页采用事务内generation比较：标签A已从g写到g+1后，仍持g的标签B写入被拒；B可另存独立槽/导出，不能覆盖A。没有引入协同编辑协议。数据库升级阻塞时提示关闭旧标签或导出当前局，不删库重建。[R4]

浏览器配额、隐私浏览和用户清理可能影响本地存储，因此保留导入/导出与风险提示；`persist()`申请不能描述为永久保险。[R5] 退出页面时仅提示未完成保存，不把最后一次保存寄托于unload。

## 8.7 文件导入导出与往返

原生由文件选择对话框操作，网页由用户点击触发浏览器文件选择/下载，移动端测试实际文件行为。Godot JavaScriptBridge支持导出生成缓冲区；这不等于每个平台的下载体验已测试。[R3]

标准往返测试：网页快照A→Linux加载→执行固定10条命令→导出B→网页加载B→与Linux的规范化玩法状态比较；再继续相同命令序列比较关键结果与随机状态。跨局图鉴另测，不能用“标题显示同一人物名”代替互通。

## 8.8 发布与缓存

HTTPS静态部署；`.wasm`为`application/wasm`，脚本使用适当JavaScript类型，资源包为二进制；压缩声明与实际文件一致。版本目录固定一组引擎、代码、数据、美术，不在运行时从latest路径随机取新素材。

首屏提供用户点击进入、资源加载进度、能力不支持/加载失败信息。声音、全屏等在允许的用户操作流程中发起；网页后台暂停表现但不以真实时间追赶世界。[R1]

PWA作为增强能力：新版本先完整缓存和验证清单，等待当前游玩安全保存/退出后再切换；不在活跃游戏中强制skipWaiting换脚本。保留可启动旧版本，缓存缺失就显示需联网补齐。只有经断网启动测试通过才显示“可离线使用”。资源缓存与存档分开，删除旧资源不能清掉IndexedDB快照。

域名、仓库、部署账号由用户指定，本设计不部署到tvfan.app或其他已有站点，不复用其他项目的Android最低版本或TV遥控器要求。

## 8.9 平台与性能验证口径

P1必须：Linux真实运行、Chrome/Chromium及Firefox桌面网页核心路径、触摸输入路径。Windows尽早构建/启动，但未实测不称正式通过。macOS、Android原生、iOS、移动浏览器分别测试。iOS导出有macOS和Xcode环境要求，需在对应环境验证。[R6]

沿用总体预算：完整P1压缩首传≤80MB，含字体音频；桌面半日模拟p95≤100ms、移动≤300ms；桌面界面目标60fps、受限移动30fps。新增验收方法：预热50步，连续模拟1000步，记录全部耗时和最大值，不只选一个快速截图。报告设备、OS、浏览器、引擎模板、内容数量和内存峰值，不能在无设备时宣称满足。

地图/立绘按当前场景加载，图集以2048×2048为初始上限；字号放大需重新布局。长模拟每完成安全tick可让出一帧，显示进度并允许停止；不能在一个逻辑事务中途让用户发第二条游戏命令。先优化资源和查询，再考虑改规模；不以删掉卡片或NPC假装性能达标。

## 8.10 技术验收

T73：同版本跨平台固定命令得到相同玩法状态及随机state。T74：预览不取玩法随机流，动画速度不改变结果。T75：存档载入seed/state顺序正确，版本不相容明确拒绝。T76：原生写临时文件中断后旧代仍可加载。T77：网页put成功而事务失败不能显示已保存。T78：多标签页旧generation写入失败且可另存。T79：全局图鉴失败后从本局补记不重复物质奖励。T80：损坏/超限/未知版本导入不覆盖原槽。T81：网页→Linux→网页往返后继续运行一致。T82：更新不混用新旧资源，也不删用户存档。T83：离线标记只有真实缓存完整且测试通过才出现。T84：性能报告含样本、设备与版本，不从导出成功推断实测通过。

## 8.11 外部依据

以下只支持当前技术约束，规则结构和所有参数为本项目设计。核对日期2026-09-09。

[R1] Godot，Exporting for the Web。`https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_web.html`

[R2] Godot，RandomNumberGenerator。`https://docs.godotengine.org/en/stable/classes/class_randomnumbergenerator.html`

[R3] Godot，The JavaScriptBridge singleton。`https://docs.godotengine.org/en/stable/tutorials/platform/web/javascript_bridge.html`

[R4] MDN，Using IndexedDB。`https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API/Using_IndexedDB`

[R5] MDN，Storage quotas and eviction criteria。`https://developer.mozilla.org/en-US/docs/Web/API/Storage_API/Storage_quotas_and_eviction_criteria`

[R6] Godot，Exporting for iOS。`https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_ios.html`
