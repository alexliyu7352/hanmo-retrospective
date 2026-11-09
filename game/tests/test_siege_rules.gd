## 城防图纯规则断言，以实际观察边界驱动；资源请求未应用前不宣称世界库存已改变。
extends SceneTree
const Rules=preload("res://core/siege_rules.gd")
const F=preload("res://tests/campaign_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 延迟开始独立测试，避免访问任何玩家存档。
func _init() -> void:
	call_deferred("run")

## 失败保留具体标签并影响退出码，禁止吞掉结构或来源错误。
func check(ok: bool,label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 用明确的真实既有观察创建围城，不在被测模块内伪造军团。
func create(data: Dictionary, observation: Dictionary) -> Dictionary:
	var result: Dictionary=Rules.create(data,F.siege_params(),observation); check(result.ok,"真实到城军团建围城")
	return result.get("siege",{})

## 夹具应用返回的局部材料请求并推进观察时钟；此模拟边界不代替MilitaryAssets世界回写验收。
func finish(observation: Dictionary, result: Dictionary) -> void:
	for effect in result.get("asset_effects",[]):
		observation.public_goods[effect.army_id][effect.item_id]=observation.public_goods[effect.army_id].get(effect.item_id,0)+(effect.quantity if effect.kind=="produce" else -effect.quantity)
	observation.tick=result.siege.last_tick

## 原地方案与保存守軍等待一起推进一半日，失败时保留原围城和观察以便定位。
func advance(siege: Dictionary, data: Dictionary, observation: Dictionary, plans: Dictionary={}) -> Dictionary:
	F.wait_ai(siege,observation)
	if plans.is_empty(): plans=F.orders(siege,observation)
	var before: Dictionary=siege.duplicate(true); var watched: Dictionary=observation.duplicate(true)
	var result: Dictionary=Rules.resolve(siege,data,observation,"p",plans,observation.tick+1)
	check(result.ok,"合法围城半日 "+str(result.get("code",""))+" "+str(result.get("path","")))
	check(siege==before and observation==watched,"围城结算只返回副本，观察与原状态不变")
	if result.ok: finish(observation,result); return result.siege
	return siege

## 节点参战名册只由实际成员决定；字典经保存重排后不得改请求数组或来源节点。
func check_contact_membership_order(data: Dictionary) -> void:
	var obs: Dictionary=F.observation(); var siege: Dictionary=create(data,obs)
	siege.node_hp.G=0; siege.unit_nodes={"b1":"I","a2":"G","a1":"G"}; F.wait_ai(siege,obs)
	var plans: Dictionary={"a1":{"action":"move","node_id":"I"},"a2":{"action":"move","node_id":"I"}}
	var restored: Dictionary=Strict.parse_text(JSON.stringify(siege,"",true,true)).value
	check(siege==restored and siege.unit_nodes.keys()!=restored.unit_nodes.keys(),"保存字典只重排键，不改城防与成员")
	var first: Dictionary=Rules.resolve(siege,data,obs,"p",plans,obs.tick+1)
	var second: Dictionary=Rules.resolve(restored,data,obs,"p",plans,obs.tick+1)
	check(first.ok and second.ok and first.battle_requests.size()==1 and second.battle_requests.size()==1,"相同实际成员产生同一内城会战")
	if not first.ok or not second.ok or first.battle_requests.is_empty() or second.battle_requests.is_empty(): return
	check(first.battle_requests[0].unit_ids==["a1","a2","b1"] and second.battle_requests[0].unit_ids==["a1","a2","b1"],"节点请求在生产处稳定参战数组次序")
	check(first.battle_requests[0].origin_nodes=={"a1":"G","a2":"G","b1":"I"},"稳定名册保留各部实际交锋来源")
	check(first==second,"键重排不改变围城候选、资源请求、事件或会战请求")

## 验证门区与内城独立、支路复用、真实补给/援军、器械消耗、保存AI、围困和协商权限。
func run() -> void:
	var data: Dictionary=F.data(); var obs: Dictionary=F.observation(); var siege: Dictionary=create(data,obs)
	check_contact_membership_order(data)
	check(Rules.validate_definition(data.siege_topologies["topology.two"],data).ok,"第二城防拓扑无需改规则")
	var bad: Dictionary=data.siege_topologies["topology.one"].duplicate(true); bad.edges.append({"from":"A","to":"unknown"}); check(not Rules.validate_definition(bad,data).ok,"未知门区连线失败")
	var unseen: Dictionary=obs.duplicate(true); unseen.armies["army.a"].location.settlement_id="city.a"; check(not Rules.create(data,F.siege_params(),unseen).ok,"军队尚未到城不建立围城")
	var invalid_data: Dictionary=data.duplicate(true); invalid_data.strongholds["stronghold.b"].settlement_id="unknown"; check(Rules.create(invalid_data,F.siege_params(),obs).code=="UnknownSiegeStronghold","未定义目标城明确失败")
	var before: Dictionary=siege.duplicate(true); check(not Rules.preview(siege,data,obs,"p","b1",{"action":"wait","node_id":"I"}).ok and siege==before,"敌军不可越权指挥")
	siege.unit_nodes.a1="G"; check(Rules.preview(siege,data,obs,"p","a1",{"action":"move","node_id":"I"}).code=="SiegeGateClosed","主门未破不能直达内城")
	siege.unit_nodes.a1="A"
	var attack: Dictionary=F.orders(siege,obs); attack.a1={"action":"assault","node_id":"G"}; attack.a2={"action":"assault","node_id":"G"}
	siege=advance(siege,data,obs,attack); check(siege.node_hp.G<45 and siege.node_hp.G>0,"真实有效兵员对城门产生耐久损失")
	siege=advance(siege,data,obs,attack); check(siege.node_hp.G==0 and siege.phase=="active" and obs.units.b1.fit_count==80,"破门只开通路，不删除守軍或判占领")
	siege.unit_nodes.a1="G"; check(Rules.preview(siege,data,obs,"p","a1",{"action":"move","node_id":"I"}).ok,"主门破坏开放实际后续边")
	var moving: Dictionary=F.orders(siege,obs); moving.a1={"action":"move","node_id":"I"}; F.wait_ai(siege,obs)
	var contact: Dictionary=Rules.resolve(siege,data,obs,"p",moving,obs.tick+1); check(contact.ok and contact.battle_requests.size()==1,"内城守軍在场触发真实六角会战请求")
	finish(obs,contact); siege=contact.siege; check(siege.inner_hold_ticks==0 and not Rules.preview(siege,data,obs,"p","a1",{"action":"wait","node_id":"I"}).ok,"未回写会战时守点不算成功且不能重复行动")
	check(not Rules.battle_finished(siege,data,obs,"I","invented").ok,"缺少实际战果回写收据不可清除守軍")
	obs.units.b1.fit_count=0; obs.units.b1.available=false; obs.defender_commander_out=true
	obs.receipts.battle={"kind":"military_battle","phase":"completed","source_id":siege.pending_nodes.I.source_id,"winner_side":0,"tick":obs.tick}
	var released: Dictionary=Rules.battle_finished(siege,data,obs,"I","battle"); check(released.ok,"已完成真实战果凭据解除节点占用")
	siege=released.siege; siege=advance(siege,data,obs); check(siege.phase=="active","内城需连续足够半日控制")
	siege=advance(siege,data,obs); check(siege.phase=="result" and siege.result.kind=="occupied" and siege.last_events[0].kind=="occupation_request","内城与守帅退败共同产生公共移交请求")
	obs=F.observation(); siege=create(data,obs); var build: Dictionary=F.orders(siege,obs); build.a1={"action":"build_engine","node_id":"A"}
	siege=advance(siege,data,obs,build); check(obs.public_goods["army.a"].wood==10 and obs.public_goods["army.a"].iron==2 and obs.public_goods["army.a"].ram==0,"器械先消耗真实材料而不立即产出")
	for _i in range(3): siege=advance(siege,data,obs)
	check(obs.public_goods["army.a"].ram==1 and siege.engine_work.is_empty(),"制造耗足四个半日才请求产生实物")
	var with_engine: Dictionary=Rules.preview(siege,data,obs,"p","a1",{"action":"assault","node_id":"G","engine_use":{"inventory_id":"army.bag","item_id":"ram","quantity":1}}); obs.public_goods["army.a"].ram=0
	check(with_engine.damage>Rules.preview(siege,data,obs,"p","a1",{"action":"assault","node_id":"G"}).damage,"实际攻具物品提高攻城效果")
	obs=F.observation(); data.strongholds["stronghold.b"].topology_id="topology.two"; siege=create(data,obs); obs.defender_food=0; siege.node_hp.S=0; siege.node_hp.G=0
	var blockade: Dictionary={"a1":{"action":"blockade","node_id":"D"},"a2":{"action":"blockade","node_id":"D"}}
	siege=advance(siege,data,obs,blockade); check(not siege.blockaded and siege.unsupplied_ticks==0,"两部只封一个补给口不算完全围困")
	blockade.a2.node_id="E"
	for _i in range(3): siege=advance(siege,data,obs,blockade)
	check(siege.blockaded and siege.unsupplied_ticks==3,"每个有效补给口均被真实部队封锁才累计断粮")
	check(not Rules.strategic(siege,data,obs,"outsider",{"kind":"negotiate","terms":"safe_departure"}).ok,"无真实负责人权限不能交城")
	var peace: Dictionary=Rules.strategic(siege,data,obs,"p",{"kind":"negotiate","terms":"safe_departure"})
	check(peace.ok and peace.siege.result.kind=="surrendered" and siege.phase=="active","有实际条件和意愿的协商才产生交城请求")
	obs.defender_food_sources=[{"inventory_id":"store.b","food_item_id":"grain","organization_id":"org.b","recipient_army_id":""}]
	obs.receipts["supply"]={"kind":"military_delivery","item_id":"grain","to_inventory_id":"store.b","recipient_army_id":"","stronghold_id":"stronghold.b","quantity":5,"tick":obs.tick,"recipient_owner":{"kind":"organization","id":"org.b"}}; siege=advance(siege,data,obs,blockade)
	check(siege.unsupplied_ticks==0,"真实补给交付打断连续断粮")
	siege=advance(siege,data,obs,blockade); check(siege.unsupplied_ticks==1,"旧补给收据不会每轮重复补粮")
	obs.armies["army.c"]={"organization_id":"org.a","commander_actor_id":"helper","location":{"kind":"Settlement","settlement_id":"city.a"}}; obs.units.c1={"army_id":"army.c","fit_count":50,"morale":80,"available":true,"engaged":false}; obs.public_goods["army.c"]={}
	obs.receipts.helper={"kind":"campaign_arrival","army_id":"army.c","stronghold_id":"stronghold.b","tick":obs.tick}
	check(not Rules.join(siege,data,obs,"army.c",0,"helper").ok,"有旧消息但援军实际仍在外城不能加入")
	obs.armies["army.c"].location.settlement_id="city.b"; var reinforced: Dictionary=Rules.join(siege,data,obs,"army.c",0,"helper")
	check(reinforced.ok and reinforced.siege.unit_nodes.has("c1") and not siege.unit_nodes.has("c1"),"实际到场援军只登记已有单位并返回副本")
	var retired: Dictionary=Rules.strategic(siege,data,obs,"p",{"kind":"withdraw","army_id":"army.a"}); check(retired.ok and retired.events[0].kind=="withdrawal_request" and obs.armies["army.a"].location.settlement_id=="city.b","撤军只请求恢复真实行程，不瞬移回营")
	var ai_before: Dictionary=obs.duplicate(true); check(Rules.make_ai(siege,data,obs)==Rules.make_ai(siege,data,obs) and obs==ai_before,"合法AI重复结果一致且不改观察")
	obs.receipts["attacker_supply"]={"kind":"military_delivery","item_id":"grain","to_inventory_id":"army.bag","recipient_army_id":"army.a","stronghold_id":"stronghold.b","quantity":5,"tick":obs.tick,"recipient_owner":{"kind":"organization","id":"org.a"}}; siege=advance(siege,data,obs,blockade)
	check(siege.unsupplied_ticks==2,"只给攻軍补给不重置守城断粮")
	var saved: Dictionary=JSON.parse_string(JSON.stringify(siege)); var restored: Dictionary=Rules.validate(saved,data,obs); check(restored.ok,"围城JSON往返保留门区与实际收据 "+str(restored))
	var missing_receipts: Dictionary=obs.duplicate(true); missing_receipts.receipts.erase("supply"); check(not Rules.validate(saved,data,missing_receipts).ok,"保存所依赖的补给事实丢失不能静默接受")
	obs=F.observation(); data.strongholds["stronghold.b"].topology_id="topology.one"; siege=create(data,obs); obs.public_goods["army.a"].wood=10; obs.public_goods["army.a"].iron=2
	var shared: Dictionary={"a1":{"action":"build_engine","node_id":"A"},"a2":{"action":"build_engine","node_id":"A"}}
	F.wait_ai(siege,obs); check(Rules.preview(siege,data,obs,"p","a1",shared.a1).ok and Rules.preview(siege,data,obs,"p","a2",shared.a2).ok,"各部单独可见同一份公库材料")
	check(Rules.validate_orders(siege,data,obs,"p",shared).code=="SiegeSharedStockInsufficient","整套预览合并共享材料，防止一料两用")
	before=siege.duplicate(true); check(not Rules.resolve(siege,data,obs,"p",shared,obs.tick+1).ok and siege==before,"共同材料不足的实际结算不留下半件器械")
	siege.node_hp.G=30; siege.ai_plans={"b1":{"action":"repair","node_id":"G"}}; var plans: Dictionary=F.orders(siege,obs); plans.a1={"action":"assault","node_id":"G"}
	var simultaneous: Dictionary=Rules.resolve(siege,data,obs,"p",plans,obs.tick+1); check(simultaneous.ok and simultaneous.asset_effects.is_empty() and simultaneous.siege.node_hp.G<30,"同轮受袭节点不能修复，未动工木料不消耗")
	obs=F.observation(); siege=create(data,obs); siege.unit_nodes.a1="G"; siege.node_hp.G=0; plans=F.orders(siege,obs); plans.a1={"action":"move","node_id":"I"}; F.wait_ai(siege,obs)
	contact=Rules.resolve(siege,data,obs,"p",plans,obs.tick+1); finish(obs,contact); siege=contact.siege
	var aborted: Dictionary=Rules.terminate(siege,data,obs,"authority_lost")
	check(aborted.ok and aborted.siege.pending_nodes.is_empty() and aborted.siege.ai_plans.is_empty() and aborted.events[0].battle_source_ids.size()==1,"军令失效列出待释放会战，不留下永久节点占用")
	check(obs.armies["army.a"].location.settlement_id=="city.b" and siege.phase=="active","终止候选不瞬移军团或改原战场")
	obs.units.b1.fit_count=50; obs.receipts["survivor_battle"]={"kind":"military_battle","phase":"completed","source_id":siege.pending_nodes.I.source_id,"winner_side":0,"tick":obs.tick}
	var survivors: Dictionary=Rules.battle_finished(siege,data,obs,"I","survivor_battle")
	check(survivors.ok and survivors.siege.unit_nodes.b1=="D" and obs.units.b1.fit_count==50,"原地战败守军沿合法后路撤出节点，幸存人数不变")
	var timed_data: Dictionary=data.duplicate(true); timed_data.siege_topologies["topology.one"].rules.max_ticks=2; F.wait_ai(siege,obs)
	var timeout: Dictionary=Rules.resolve(siege,timed_data,obs,"p",F.orders(siege,obs),obs.tick+1)
	check(timeout.ok and timeout.siege.result.kind=="timeout" and timeout.siege.pending_nodes.is_empty() and timeout.events[-1].kind=="siege_release_request","围城超期连同实际会战释放清单收束，不留下永久节点占用")
	obs.armies["army.a"].location.settlement_id="city.a"; check(Rules.validate(aborted.siege,data,obs).ok,"已终止围城存档允许真实军团开始返程")
	obs=F.observation(); siege=create(data,obs); obs.units.a1.fit_count=0; obs.units.a2.fit_count=0; F.wait_ai(siege,obs)
	var defeated: Dictionary=Rules.resolve(siege,data,obs,"p",{},obs.tick+1)
	check(defeated.ok and defeated.siege.result.kind=="defended" and defeated.events[0].kind=="withdrawal_request","攻方真实兵员归零即结束围城，不空等期限或产生新兵")
	obs=F.observation(); siege=create(data,obs); obs.units.a1.engaged=true; obs.units.a2.engaged=true; F.wait_ai(siege,obs)
	var engaged: Dictionary=Rules.resolve(siege,data,obs,"p",{},obs.tick+1); check(engaged.ok and engaged.siege.phase=="active","真实会战占用不冒充攻方全灭")
	obs.receipts["corrupt"]=7; check(Rules.validate(siege,data,obs).code=="InvalidSiegeObservedReceipt","观察中的损坏事实结构化失败")
	obs=F.observation(); obs.controller_actor_id=""; siege=create(data,obs); var autonomous: Dictionary=Rules.resolve(siege,data,obs,"",{},obs.tick+1)
	check(autonomous.ok and autonomous.siege.node_hp.G<siege.node_hp.G,"无人参与的围城走同一合法AI与耐久结算")
	print("CHECKS=",checks," FAILURES=",failures); quit(1 if failures else 0)
