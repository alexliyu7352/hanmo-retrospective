# 青川、松门与北驿 1.3.0

正式包 `hanmo.local_society@1.3.0`，由 `content/packs/selections/local-society-1.3.0.json` 精确选择。1.2 原目录、选择集及素材字节保留；新代使用原人物与原素材，只有两个新的正常标题情景。独立 compose/install 后，新旧安装代各用自己的锁和存档身份，不能拿 1.3 替换旧 1.2 存档内容。

## 两个正常入口

- `scenario.society_daily_routes`，青川往来 · 做工备粮：粮行有 40 粟。
- `scenario.society_market_day`，集上余粮 · 松门与北驿：粮行只有 3 粟。

正常组合后的情景 ID 分别为 `society::scenario.society_daily_routes`、`society::scenario.society_market_day`。除标题/情景身份及粮行初始存货 40/3 外，两配置相同。两局的程禾都从青川庭院开始，携带真实自有 2 粟和 120 钱。

谷诚和沈蘅沿用《青川习业》的 `actor.study_caravanner`、`actor.study_swordswoman`。1.2 把他们显示为“县仓运粮人”“松门行脚弟子”；这里恢复既有姓名，不生成第二个人。谷诚到杜衡的粮行做工，再买自用备粮，沿原路去北驿探访沈简。沈蘅在孟修处做工，再去青川买粮，沿原路回松门。

每人目标是本人行囊中有 2 粟，留存现金策略为 8 钱，工作疲劳策略阈值为 80。这是有限的私人物资准备目标，不是新增饥饿/死亡规则。既有合同和受托货物优先；工资尚在预留时不能花未来收入，买来的自用粮不转成组织公粮。目标与目的地都满足后不反复刷工钱或买货，之后真实物品变化才可能重新考虑。

玩家可以做自己的工作、购物、学习、住宿或往返。每一个真实世界半日，其他人物的行动、群体生产/工资/口粮和北驿军事供给仍按原规则推进。NPC 做工会占用实际雇主；杜衡或孟修忙碌时，玩家看到真实占用，稍后才能使用其服务，没有另生替班人。

## 存货竞争与玩家介入

充足局中，两人可从同一有限粮行取得所需备粮。集日局的三粟无法同时满足两人各两粟，至少一人会因实际缺货等待。玩家可以到粮行正常“出售”自己带来的 1 粟：杜衡的真实商店账付款，粮进入同一个商店库存，等待的人根据新观察重新报价并购买。没有脚本补货、隐藏入账或“看完对白奖励”。玩家也可以自己购买，让存货竞争产生不同结果。

县仓当前初始储备是 120 粟，公开储备目标是 124 粟，所以开局会产生真实的储备补足需求；这是缓冲储备公托，并非饥荒。实际可领份额继续随生产、消费、承接和交付变化。玩家可在县仓书佐的当面事务对白查看并承接原公共货单，也可暂不接。两位私人备粮 NPC 没有公共采购模板，不会偷偷承接这份需求。玩家选择公共采购会真实占用同一市场存货，与单纯卖粮帮助个人是不同的自愿选择。

## 明确初态与责任

以下是 provisional 开发参数，不是最终平衡承诺：

- 粮行原 900 钱、短工原工资 30 钱、1 半日、疲劳 +8；市场成交价由原库存/数量公式计算，粟的基价 12 不是固定成交价。
- 松门工资账与县务工资账各显式初有 240 钱；松门原 1.2 的 7 钱不足一次短工工资，因此本代新情景明确给出启动预算。运行时不补钱。
- 县仓初始 120 粟、松门公库 60 粟；原 city/sect 群体、劳动分配、配方、家户私账和周期供养保持，私人短工不再产一份粮。
- 北驿沿原沈简和一名静态士卒、公库 80 粟、120 军钱、2 矛。沈简的军事供养仍由原军事领域负责；谷诚和沈蘅没有凭空新增的个人口粮义务。
- 青川↔松门、青川↔北驿均用原双向道路，各 1 半日。先通过本地通道到城口，再真实出行；没有直接松门↔北驿瞬移。
- 原固定角色、家户关系、钱包和库存 owner 均保留；本人 bag 由 factory 生成并从 actor 的 inventory_id 读取，作者不另造行囊账户。

## 当面近况与同源公托

`dynamic_world.regional_life` 的 `actor_topics` 只声明谷诚/沈蘅愿在同地分享的 work/trade 近况；正文不假定工钱已付、交易数量或当前库存。具体事实、日期与结果由原生真实来源投影，默认私人账目不向远方公开。

- `topic.caravanner.work` → `dialog.life_caravanner_work`
- `topic.caravanner.trade` → `dialog.life_caravanner_trade`
- `topic.swordswoman.work` → `dialog.life_swordswoman_work`
- `topic.swordswoman.trade` → `dialog.life_swordswoman_trade`

这些 actor 对白 `context_role` 为空，只能 say/choice/end，不能借普通交谈取得营业权限。`need_topics` 则绑定实际岗位，确认时使用会话冻结的原 typed OfferRef/Need revision；作者不填另一张静态 offer：

- `topic.city.grain` → `role.city_supply` / `dialog.life_city_need` / `supply.city` / `policy.city_need` / `template.city_grain`
- `topic.sect.grain` → `role.sect_supply` / `dialog.life_sect_need` / `supply.sect` / `policy.sect_need` / `template.sect_grain`

所有 topic ID 是此情景内部 ID，不加 namespace；actor/role/dialogue/item/place 等实体引用由 compose 正常限定。每个主题冷却 2 半日，按观察者、主题和真实来源轮次区分。读页面不消耗机会；关对白不取消需求、NPC 计划或已承接合同。

沈简新增的 `role.city_outpost_host` / `dialog.city_outpost_host` 只把原守备放入正常在场人物与对白入口，不改变其军籍、工资或货权。

## 正式准入与联合节点输入

显式能力 `regional_life_v1` 与 `native_dynamic_world` 使用同源原生库；Schema 接受不代表旧库已支持新行为。作者检查须核 private policy/模板配对、纯私人模板才可省组织、有限已知服务、可达商店与终点、来源身份、演员/岗位/对白权限、同源共享需求和原单一供养责任。

组合入口：`python3 tools/content_pipeline.py compose --packs content/packs/authored/local_society_1_3 --selection content/packs/selections/local-society-1.3.0.json --output <独立输出目录>`。随后由中央正常 content stage 安装，再从标题 picker 进入。

中央 Main helper 接收精确 scenario_id、catalog_path、40 位 source_commit、catalog/pack_lock/schema/capabilities/native_library/binding 的实际路径及 SHA256、参与 helper/Main/人物/对白/商店/行旅脚本摘要，以及独立 profile 与存档路径。不能猜相邻文件名、借旧库报告或由 helper 改 NPC、库存、知识、时钟。

主要输入触点为原 `role.merchant`/`shop.grain`、人物 `actor.study_caravanner`/`actor.study_swordswoman`、县仓 `role.city_supply`、北驿 `place.city_outpost`。保存时选真实尚未完工的工作或私人计划，再从新标题进程读取继续；不约定一个未经实跑的固定 tick。

本 README 不宣称 Main、GUI、冷热继续或整章体验已通过。作者 Schema/compose 与最终同源 native/Main/存读证据分别记录，只有中央最终产物的实际回执能证明可玩结果。
