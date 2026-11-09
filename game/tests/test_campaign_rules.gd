## 用实际岗位、人物位置和库存操作验证军令生命周期；人产编组与移动由明确夹具边界提供，不冒充完整世界试玩。
extends SceneTree
const Rules=preload("res://core/campaign_rules.gd")
const F=preload("res://tests/campaign_fixture.gd")
const Inventory=preload("res://core/inventory_rules.gd")
var checks: int=0
var failures: int=0

## 启动独立规则断言，不访问玩家存档或实际军队内容。
func _init() -> void:
	call_deferred("run")

## 累积真实断言并让最终退出码反映任何失败。
func check(ok: bool,label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 通过当面岗位受令并绑定夹具中已存在军团，隔离测试战略层职责。
func accepted(data: Dictionary, template: String) -> Dictionary:
	var state: Dictionary=F.state(); var request: Dictionary={"template_id":template,"context":{"place_id":"camp.a","provider_actor_id":"issuer","role_id":"role.order"}}
	var before: Dictionary=state.duplicate(true); check(Rules.preview(state,data,"p","campaign_accept",request).ok and state==before,"受令预览只读")
	var result: Dictionary=Rules.apply(state,data,"p","campaign_accept",request); check(result.ok,"真实在场岗位受令")
	var id: String=result.details.campaign_id; state.armies["army.a"].order_id=id
	check(Rules.bind_army(state,data,id,"army.a").ok,"原子绑定实际军团")
	check(not Rules.bind_army(state,data,id,"army.a").ok,"禁止重复编组绑定")
	return {"state":state,"id":id}

## 模拟根层完成同一半日后更新唯一时钟；军令模块本身不得加时间。
func step(state: Dictionary, data: Dictionary, observations: Dictionary={}) -> Dictionary:
	var tick: int=state.tick; var result: Dictionary=Rules.step(state,data,tick+1,observations)
	check(result.ok and state.tick==tick,"战略步骤不独自推进世界时钟")
	if result.ok: state.tick+=1
	return result

## 覆盖真实到场、资金物品边界、回营凭据、长久结果、负责人失效与明确失败。
func run() -> void:
	var data: Dictionary=F.data(); check(Rules.validate_definitions(data).ok,"军令模板完整引用公职驻军目标")
	var bad: Dictionary=data.duplicate(true); bad.military_orders.occupy.office_id="missing"; check(not Rules.validate_definitions(bad).ok,"未知公职清楚失败")
	var setup: Dictionary=accepted(data,"deliver_supply"); var state: Dictionary=setup.state; var id: String=setup.id
	var initial: Dictionary=state.duplicate(true)
	check(Rules.apply(state,data,"p","campaign_depart",{"campaign_id":id}).ok,"出征产生真实行军请求")
	check(state.actors.p.location==initial.actors.p.location and state.wallets==initial.wallets,"下军令不瞬移或提前扣费")
	step(state,data); check(state.campaigns[id].status=="marching","没有真实到场便继续在途")
	state.actors.p.location={"kind":"Wilderness","map_id":"wrong","point":[90,70]}; step(state,data); check(state.campaigns[id].status=="marching","相同坐标不同地图不算抵达")
	state.actors.p.location={"kind":"Wilderness","map_id":"map.one","point":[90,70]}; step(state,data); check(state.campaigns[id].status=="operating","实际城点触发一次到场事实")
	var arrival: String=state.campaigns[id].arrival_fact_id; step(state,data); check(state.campaigns[id].arrival_fact_id==arrival,"等待不重建到场事实")
	var public_owner: Dictionary={"kind":"organization","id":"org.a"}; var private_owner: Dictionary={"kind":"actor","id":"p"}
	state.inventories["store.b"].capacity=1; check(not Rules.preview(state,data,"p","campaign_deliver",{"campaign_id":id}).ok,"预检接收容量")
	state.inventories["store.b"].capacity=1000
	check(Rules.apply(state,data,"p","campaign_deliver",{"campaign_id":id}).ok,"实物由随军公库交付目标公库")
	check(Inventory.count(state.inventories["army.bag"],"grain",[public_owner])==4 and Inventory.count(state.inventories["army.bag"],"grain",[private_owner])==9,"只扣指定公共批次，随军私产完整")
	check(Inventory.count(state.inventories["store.b"],"grain",[{"kind":"organization","id":"org.b"}])==6,"交接真实改变收货组织所有权")
	check(not Rules.apply(state,data,"p","campaign_deliver",{"campaign_id":id}).ok,"永久成果阻止重复交货")
	check(not Rules.preview(state,data,"p","campaign_report",{"campaign_id":id}).ok,"目标完成不能跳过实际回营交接")
	check(Rules.apply(state,data,"p","campaign_return",{"campaign_id":id}).ok,"返回发出行程请求")
	check(not Rules.army_returned(state,data,id,"invented").ok,"拒绝伪造回营收据")
	state.facts.returned={"kind":"military_return","order_id":id,"army_id":"army.a","garrison_id":"garrison.a","actor_id":"p","tick":state.tick}; check(not Rules.army_returned(state,data,id,"returned").ok,"军团实际资产尚未关闭不能回报")
	state.armies["army.a"].status="closed"; check(Rules.army_returned(state,data,id,"returned").ok,"实际清算回营收据允许回报")
	state.actors.p.location={"kind":"Settlement","settlement_id":"city.a","place_id":"camp.a"}
	check(Rules.apply(state,data,"p","campaign_report",{"campaign_id":id,"context":{"place_id":"camp.a","provider_actor_id":"issuer","role_id":"role.order"}}).ok,"向在场负责人回报关闭")
	check(state.campaigns[id].status=="closed" and Rules.validate_state(state,data).ok,"关闭实例和永久交接可保存")
	check(not Rules.apply(state,data,"p","campaign_report",{"campaign_id":id}).ok,"报告不能重复领取或清算")
	setup=accepted(data,"occupy"); state=setup.state; id=setup.id; Rules.apply(state,data,"p","campaign_depart",{"campaign_id":id}); state.actors.p.location={"kind":"Settlement","settlement_id":"city.b","place_id":"camp.b"}; step(state,data)
	state.facts.win={"kind":"military_battle","phase":"completed","campaign_id":id,"army_id":"army.a","stronghold_id":"stronghold.b","tick":state.tick}; step(state,data,{"completed_receipt_ids":["win"]}); check(state.campaigns[id].objective_result.is_empty(),"野战胜利不是占领城市")
	state.facts.capture={"kind":"military_occupation","phase":"completed","campaign_id":id,"army_id":"army.a","stronghold_id":"stronghold.b","tick":state.tick}; step(state,data,{"completed_receipt_ids":["capture"]}); check(state.campaigns[id].objective_result.kind=="occupied","已完成公共移交收据才确认占领目标")
	setup=accepted(data,"relieve"); state=setup.state; id=setup.id; state.offices["office.p"].enabled=false; var result: Dictionary=step(state,data)
	check(state.campaigns[id].status=="returning" and result.journey_requests.size()==1,"失去公职授权终止后续作战并请求返程")
	var once: Dictionary=state.duplicate(true); check(Rules.step(state,data,state.tick).code=="CampaignInvalidStep" and state==once,"旧半日不会重复处理")
	state.actors.p.alive=false; state.armies["army.a"].status="closed"; state.facts["delegated"]={"kind":"military_return","order_id":id,"army_id":"army.a","garrison_id":"garrison.a","actor_id":"issuer","tick":state.tick}
	check(Rules.army_returned(state,data,id,"delegated").ok and state.campaigns[id].status=="closed","原负责人死亡后由真实公共回报岗位交接，不留死人合同")
	check(Rules.validate_state(state,data).ok,"代理交接后的历史仍可恢复")
	state=F.state(); data.military_orders.deliver_supply.objective.recipient="army"
	state.armies["army.recipient"]={"id":"army.recipient","status":"active","organization_id":"org.a","commander_actor_id":"defender","inventory_id":"recipient.bag"}
	state.inventories["recipient.bag"]={"id":"recipient.bag","owner":{"kind":"organization","id":"org.a"},"capacity":1000,"revision":0,"lots":[]}
	var request: Dictionary={"template_id":"deliver_supply","recipient_army_id":"army.recipient","context":{"place_id":"camp.a","provider_actor_id":"issuer","role_id":"role.order"}}
	var accepted_army: Dictionary=Rules.apply(state,data,"p","campaign_accept",request); check(accepted_army.ok,"当面军令冻结实际接收军团")
	id=accepted_army.details.campaign_id; state.armies["army.a"].order_id=id; Rules.bind_army(state,data,id,"army.a"); Rules.apply(state,data,"p","campaign_depart",{"campaign_id":id})
	state.actors.p.location={"kind":"Settlement","settlement_id":"city.b","place_id":"camp.b"}; step(state,data)
	check(Rules.preview(state,data,"p","campaign_deliver",{"campaign_id":id}).code=="CampaignRecipientNotAtTarget","送粮军到城但收粮军仍在途中不能隔空交付")
	state.actors.defender.location={"kind":"Settlement","settlement_id":"city.b","place_id":"camp.b"}
	check(Rules.apply(state,data,"p","campaign_deliver",{"campaign_id":id}).ok,"双方实际会合后交付攻城军团")
	check(Inventory.count(state.inventories["recipient.bag"],"grain",[public_owner])==6 and state.inventories["store.b"].lots.is_empty(),"援助攻軍不把粮送进敌城仓库")
	var saved: Dictionary=JSON.parse_string(JSON.stringify(state)); check(Rules.validate_state(saved,data).ok,"JSON往返保留实际交货来源")
	var corrupt: Dictionary=saved.duplicate(true); corrupt.campaigns[id].deadline_tick+=1; check(Rules.validate_state(corrupt,data).code=="CampaignDeadlineMismatch","重启不能延长已受令期限")
	corrupt=saved.duplicate(true); corrupt.campaigns[id].objective_result.source_id=17; check(Rules.validate_state(corrupt,data).code=="CampaignResultSourceMissing","错误来源类型结构化拒绝而非脚本崩溃")
	corrupt=saved.duplicate(true); corrupt.facts[corrupt.campaigns[id].objective_result.source_id].army_id="another"; check(not Rules.validate_state(corrupt,data).ok,"另一军团同名交货不能替代实际成果")
	state.offices["office.p"].enabled=false; result=step(state,data)
	check(state.campaigns[id].status=="returning" and state.campaigns[id].objective_result.kind=="supply_delivered" and result.journey_requests.size()==1,"完成交货后免职仍终止军事权限且保留实际功绩")
	print("CHECKS=",checks," FAILURES=",failures); quit(1 if failures else 0)
