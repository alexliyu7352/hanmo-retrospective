# 清溪粮道 1.1：围城议编

这是既有清溪战区的正式后继包，ID `qingxi.campaign`、namespace `qingxi`、版本 `1.1.0`。通过 `content/packs/selections/qingxi-campaign-1.1.0.json` 正常组合和新局选择进入。原 `1.0.0` 作者文件及选择保持原样。

两章共用同一世界、规则与军官决定机制：

- `qingxi::scenario.grain_road_integration`「清溪粮道：围城议编」：燕驰与许砺各自只考虑溪口组织的保全部伍条款，仍须真实围城资格和本人决定。
- `qingxi::scenario.grain_road_standfast`「清溪粮道：守将不从」：许砺明示不接收任何改籍组织，真实拒绝终结本次交涉。围城、军令及旧职责继续。不得在运行中修改政策使它成功。

角色、地图、据点、军营及五张图全为原清溪输入；素材和表现清单逐字节复用，无新美术。日期推进至同年151日。所有开局办公室仍待实际受任，没有军团、围城、同意、受降或占领事实。

## 实际入口与军旅

玩家 `actor.player` 从 `place.north.gate` 走到 `place.north.camp`，由原 `role.north.officer` 办理 `office.north_leader` 营将职和 `order.north_clear`。新岗位明确使用既有 `army` 权限，不靠界面扩大单位级权限。编组 `unit.north.work_detail` 与 `unit.north.blockade`，两部各六人，建议实领粮360、钱100。第一部由玩家实领，第二部仍由真实北营官指挥，形成同一接收军团。

燕驰的 `office.south_patrol` / `order.south_patrol` 和许砺的 `office.south_guard` / `order.south_guard` 均走原公开军事计划，分别受任、受令、编组并按本城解围令留守。巡军实领粮30、钱30；弓部实领粮20、不领军钱。许砺先持实际公职办理四名预备戍卒的独立服役登记，未登记的空slot不能编组。

玩家沿原粮道实际行军到 `stronghold.south`。南堡补给口新增明确城外连边 `approach → supply`；这表示真实城外粮道支路，不改寻路、门障、AI或战斗公式。两部共同封口，原 `blockade_units=2`、`starvation_ticks=6`、`surrender_morale=25` 全部保留。粮实际耗尽、士气及全部原交涉条件满足后才能提出议编。

交接后同一 `order.north_clear` 以真实占领凭据完成目标。接收军仍须通过原 `campaign_return` 实际行军至北营，再 `army_return`、`campaign_report` 归营报告。若发生道路或节点会战，使用原战场和真实伤亡；作者不注入伤俘以伪造验证覆盖。

## 有限人产与军籍责任

原资金总量1020保持不变：北营公账500、独立服役工资账100；南营公账200、独立服役工资账100；玩家私钱120。各NPC和守军家户初始私账为零。两份工资账不列为城内战利公库；旧南营债务继续由南营原工资账承担，不因占城或另立未来责任而销账。

人头总数仍24：北军12、南巡军8、南守军家户预备4。四名旧戍卒在开局只存在于 `cohort.south.reserve`，属于 `household.south_guard`；`unit.south.guard` 是空军籍slot。`source.south_guard` 绑定原家户、原营地、军职、独立组织动员授权及五类状态工资，实际登记才产生士卒与劳力历史。

南堡此次战区储粮明确为120，北营仍480；南方减少的360是新篇开局背景，不在运行中添加或伪造过去消费收据。营署官的一口以及未入籍预备人口由同一南堡公库真实供粮；现役、伤员与被看管俘虏只有原军事供养者。没有生产、自动补库或到时注资。

`integration.south_guard_to_north` 单独声明同一原籍群体对 `org.north` / `garrison.north` 的服役许可。北方五份未来工资义务初始停用，保留原家户收款账户、工资率和日程；只有真实交接才能接续未来责任。旧债、家户身份、已损人口和原劳动历史不重建。军官政策及家户服役许可都不是预制军官接受凭据。

同一政策明确绑定 `stronghold.south`，另列三份停用的接收方未来口粮义务，逐项覆盖仍靠被占公库供养的预备、离队和具名南营署官。新旧义务都引用原地 `inventory.south.public`，不从北营瞬移粮食。未来产权声明只允许此目标据点、此政策列出的未生效义务；真实占领、原责任停用、新责任与唯一供养绑定切换须同候选提交。原南方义务的主体、库存和历史收据保留。

接收方另有 `office.north_received_service`，现任人为玩家，唯一授权 `grant.north_received_service` 只允许原籍 `place.south.camp` 的 `release_reserve`，上限四人，不授资产、招募或转授权限。接收军籍若要退籍，须先依原 `army_return` 真正归回南营原籍，再由仍合法持有实际接收军职 `office.north_leader` 的本人办理；离开原籍、尚在军团或旧守军官代签均不由该授权放行。原source、旧组织职务和原军职身份保留。作者检查仅核授权范围，不能证明已经实际执行过此流程。

默认条款为 `preserve_units_v1`：保留单位指挥、私人财产和旧欠债，交接当时仍存在的合法军公产与真实看管责任，并明示接收方未来服役责任。已有私人物品、任务货物、借调、未解除承诺或受限任命若不能依法续接，运行报价必须阻止整编；本包不以空adapter或军官同意绕过这些限制。

## 核验边界

正式Schema、引用、人口/工资/组织权限检查与composer分别验证作者输入。`python3 -m tools.test_siege_integration_authoring` 定向核正反章节一致性、旧章、有限资金人头、原素材和关键非法政策拒绝。静态成功不代表围城可玩、工资交接、伤俘看管或存档已完成验收；实际Main与冷继续证据由中央组合节点提供。
