## 军团停驻接敌领域集成：真实任官编组、冻结地形计划、同源移动与战果落账；宏观报价边界不冒充完整Kernel。
extends SceneTree
const F=preload("res://tests/garrison_ownership_fixture.gd")
const Military=preload("res://tests/military_combat_fixture.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Combat=preload("res://core/military_combat.gd")
const Contact=preload("res://core/terrain_contact.gd")
const Rules=preload("res://core/military_contact.gd")
const Journey=preload("res://core/terrain_journey.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Maps=preload("res://tests/test_world_terrain.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎初始化后运行测试，不打开玩家存档或主窗口。
func _init() -> void:
	call_deferred("run")

## 保留每个领域失败标签，任何失败都会影响退出码。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 世界工厂、当面任官、实际编组均正式执行；本测试只把宏观出城位置作为清楚的输入边界。
func prepared() -> Dictionary:
	var f: Dictionary=F.fresh(); check(f.initialized.ok,"真实三营工厂初始化")
	var a: Dictionary=Military.mobilize(f); var b: Dictionary=Military.mobilize(f,true)
	check(a.ok and b.ok,"双方都真实领装编组")
	if not a.ok or not b.ok: return {}
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var departed: Dictionary=Campaign.apply(f.state,f.data,f.actor,"campaign_depart",{"campaign_id":f.state.armies[f.army_a].order_id})
	check(departed.ok,"真实征讨军令请求出发，不伪造敌对旗标")
	var map: Dictionary=Maps.plain_map(200,100,20)
	map.id="map.contact"; map.budgets={"foot":1000,"army":1000,"boat":1000}; map.money_per_tick=3; map.fatigue_per_tick=1
	f.data.world_maps={map.id:map}; f.data.scenarios[f.state.scenario_id].world_map_id=map.id
	var index: int=0
	for settlement in f.data.settlements.values(): settlement.map_xy=[5+index*180,5]; index+=1
	for row in [[f.actor,[10,50],f.camp],[f.enemy,[40,50],f.enemy_camp]]:
		f.state.actors[row[0]].location={"kind":"Wilderness","map_id":map.id,"point":row[1].duplicate(),"origin":f.data.places[f.data.garrisons[row[2]].place_id].settlement_id}
	f.data.military_objectives["goal.repel"].max_rounds=1
	return f

## 使用实际地形规划并由正式 Journey.begin 保存永久依据；军团旅行报价由根层集成，本夹具明确提供预算与付款公库。
func travel(f: Dictionary, army_id: String, destination: Array) -> String:
	var army: Dictionary=f.state.armies[army_id]; var actor: Dictionary=f.state.actors[army.commander_actor_id]
	var map: Dictionary=f.data.world_maps["map.contact"]
	var route: Dictionary=Terrain.plan(map,actor.location.point,destination,"army",f.state.flags)
	check(route.ok,"真实地图产生军团路线")
	var id: String=Factory.next_id(f.state,"action")
	var quote: Dictionary={"map_id":map.id,"points":route.points,"destination":destination.duplicate(),"origin":actor.location.origin,"mode":"army","budget":1000,"starting_load":0,"duration":maxi(1,(route.cost+999)/1000),"payer_wallet_id":army.wallet_id,"money_per_tick":3,"fatigue_per_tick":1}
	var action: Dictionary={"id":id,"actor_id":actor.id,"kind":"travel","participants":army.member_actor_ids.duplicate(),"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":{}}
	f.state.actions[id]=action
	for person in army.member_actor_ids: f.state.actors[person].action_id=id
	Journey.begin(f.state,f.data,action,quote)
	return id

## 固定半日里先人产供粮再沿已有旅行走一次，安装接敌本身不再调用任何时钟或支付接口。
func pay_and_move(f: Dictionary, action_ids: Array) -> void:
	check(Assets.step(f.state,f.data,f.state.tick+1).ok,"本半日统一供粮一次")
	for id in action_ids:
		var result: Dictionary=Journey.step(f.state,f.data,f.state.actions[id]); check(result.ok,"本半日真实旅行支付和移动一次")

## 静止形状保留真实位置和空行动身份，同一连续几何照常排除全静止、远距与城门范围。
func geometry() -> void:
	var location: Dictionary={"kind":"Wilderness","map_id":"map.contact","point":[40,50],"origin":"city.a"}
	var a: Dictionary=Contact.stationary("a",location); var b: Dictionary=Contact.stationary("b",location)
	check(Contact.validate_trace(a).ok and a.action_id.is_empty() and a.params.points==[[40,50]],"静止几何不生成旅行行动或假目的地")
	check(Contact.intersection(a,b).is_empty() and Contact.detect([a,b]).is_empty(),"双方都不动不会反复接触")
	var bad: Dictionary=a.duplicate(true); bad.segments[0].to=[42,50]
	check(not Contact.validate_trace(bad).ok,"静止标记不能伪造移动")
	bad=a.duplicate(true); bad.action_id="action:invented"
	check(not Contact.validate_trace(bad).ok,"静止轨迹拒绝虚构行动 ID")
	check(Contact.stationary("a",{"kind":"Settlement"}).is_empty(),"城内没有行程不能被伪装原野拦路者")
	bad=a.duplicate(true); bad.location="invented"
	check(not Contact.validate_trace(bad).ok,"损坏静止位置以失败返回，不能抛类型错误")
	var moving: Dictionary={"actor_id":"m","action_id":"action:m","map_id":"map.contact","origin":"city.a","params":{"points":[[100,50]]},"budget":100,"segments":[{"from":[10,50],"to":[80,50],"start_cost":0,"end_cost":100,"point_index":0}]}
	var found: Array=Contact.detect([moving,a,b])
	check(found.size()==1 and found[0].a=="a" and found[0].b=="m","多名无动作停驻者不被相同空ID整体过滤，仍按最早稳定顺序选择")
	var snapshot: Dictionary=location.duplicate(true); a.location.point[0]=99
	check(location==snapshot,"静止几何返回副本不改变真实世界位置")

## 移动对停驻接触必须引用实际军团、军令与路线；接战后保存、计划门禁和军粮都走正式桥接。
func stationary_contact() -> void:
	var f: Dictionary=prepared()
	if f.is_empty(): return
	var action_id: String=travel(f,f.army_a,[150,50])
	var before: Dictionary=f.state.duplicate(true); var hits: Array=Rules.detect(f.state,f.data,[])
	check(hits.size()==1 and f.state==before,"真实行军接触无行动停驻军团，检测只读")
	if hits.is_empty(): return
	var hit: Dictionary=hits[0]
	check(hit.traces[f.enemy].stationary and hit.traces[f.enemy].action_id=="" and hit.anchors[f.enemy]==f.state.actors[f.enemy].location,"停驻军团保持原位置与空行动")
	check(hit.anchors[f.actor].point==[36,50] and hit.anchors[f.enemy].point==[40,50],"同源半径四连续求交保留双方各自锚点")
	check(Rules.detect(f.state,f.data,[f.enemy]).is_empty(),"统一hold中的停驻军团不会被另一场战斗拉走")
	var far: Dictionary=f.state.duplicate(true); far.actors[f.enemy].location.point=[40,60]
	check(Rules.detect(far,f.data,[]).is_empty(),"停驻军团不在真实路径接触范围时不战")
	var peaceful: Dictionary=f.state.duplicate(true); peaceful.campaigns[peaceful.armies[f.army_a].order_id].status="returning"
	check(Rules.detect(peaceful,f.data,[]).is_empty(),"已退出征讨军令不因异组织近距而开战")
	check(Rules.install(f.state,f.data,hit).code=="MilitaryContactHalfDayRequired" and f.state==before,"未支付统一半日不能提前安装接敌")
	var unpaid: Dictionary=f.duplicate(true); check(Assets.step(unpaid.state,unpaid.data,unpaid.state.tick+1).ok,"独立失效案例已有口粮结算")
	var unpaid_before: Dictionary=unpaid.state.duplicate(true)
	check(Rules.install(unpaid.state,unpaid.data,hit).code=="MilitaryTravelStepRequired" and unpaid.state==unpaid_before,"尚未实际走完本半日不得伪造已付旅行接触")
	var static_location: Dictionary=f.state.actors[f.enemy].location.duplicate(true)
	var money_before: int=f.state.wallets[f.state.armies[f.army_a].wallet_id].balance
	pay_and_move(f,[action_id])
	var paid: Dictionary=f.state.duplicate(true)
	for changed in ["position","action","plan","army","anchor","time"]:
		var altered: Dictionary=f.state.duplicate(true); var forged: Dictionary=hit.duplicate(true)
		match changed:
			"position": altered.actors[f.enemy].location.point=[41,50]
			"action": forged.motions[f.actor].action_id="action:invented"
			"plan": altered.facts.erase(altered.actions[action_id].params.plan_id)
			"army": forged.army_ids[f.enemy]="army:invented"
			"anchor": forged.anchors[f.enemy].point=[41,50]
			"time": forged.time_num+=1
		var snapshot: Dictionary=altered.duplicate(true)
		check(not Rules.install(altered,f.data,forged).ok and altered==snapshot,"来源或几何篡改原子拒绝："+changed)
	var result: Dictionary=Rules.install(f.state,f.data,hit)
	check(result.ok,"真实停驻接战建立正式军事会话："+str(result))
	if not result.ok: return
	check(f.state.tick==paid.tick and f.state.wallets==paid.wallets and f.state.inventories==paid.inventories,"安装会战不再次扣钱粮或推进半日")
	check(f.state.wallets[f.state.armies[f.army_a].wallet_id].balance==money_before-3,"本半日旅行费恰好一次")
	check(f.state.actors[f.enemy].location==static_location and f.state.actors[f.enemy].action_id.is_empty() and f.state.actions.keys()==paid.actions.keys(),"停驻军团无新动作、无移动、无假旅程")
	check(f.state.actions[action_id].params.met_actions.is_empty(),"没有将空动作或伪造action写入旅行去重")
	var battle_id: String=result.details.battle_id; var battle: Dictionary=f.state.battles[battle_id]
	var source: Dictionary=f.state.facts[battle.source_id]
	check(source.motion_sources[f.army_b].action_id=="" and source.motion_sources[f.army_a].plan_id==f.state.actions[action_id].params.plan_id,"永久接敌记录真实旅行来源和静止来源")
	check(battle.round==1 and battle.last_tick==f.state.tick+1 and battle.round_receipt_ids.is_empty(),"新战场本半日只建立并冻结 AI，下半日才结算第一轮")
	check(Campaign.step(f.state,f.data,f.state.tick+1).ok,"共同半日军令时钟正常推进")
	f.state.tick+=1
	check(Assets.validate_state(f.state,f.data).ok and Combat.validate_state(f.state,f.data).ok,"接敌后真实人产与战场可保存")
	check(Combat.validate_state(Strict.parse_text(JSON.stringify(f.state)).value,f.data).ok,"保存重启不丢静止者位置或真实战场来源")
	before=f.state.duplicate(true)
	check(not Combat.preview_step(f.state,f.data,f.state.tick+1).ok and f.state==before,"玩家未提交计划时预检阻止下一半日，未先扣第二次粮")
	check(Rules.detect(f.state,f.data,[]).is_empty(),"已有战场的部队不重复建局")
	var orders: Dictionary=Military.plans(battle,f.data)
	check(Combat.apply(f.state,f.data,f.actor,"military_plan",{"battle_id":battle_id,"orders":orders}).ok,"双方照同一合法规则提交与消费冻结计划")
	var advanced: Dictionary=Military.advance(f)
	check(advanced.ok and f.state.tick==before.tick+1,"下一半日军事规则只结算一次实际战损")
	battle=f.state.battles[battle_id]
	check(battle.phase=="result" and battle.applied and f.state.facts[battle_id].kind=="military_battle","有限目标正式结束并落实际五类人产账")
	check(Combat.apply(f.state,f.data,f.actor,"military_close",{"battle_id":battle_id}).ok,"正式关闭结果释放原有军团")
	check(Rules.detect(f.state,f.data,[]).is_empty(),"同一次旅行与原停驻军团已有接敌来源，关闭后不立即重开")

## 两名真实旅人和暂停旅人保持既有能力，无行动停驻扩展不改变其支付或相遇条件。
func moving_contacts() -> void:
	var f: Dictionary=prepared()
	if f.is_empty(): return
	f.state.actors[f.enemy].location.point=[90,50]
	var a: String=travel(f,f.army_a,[150,50]); var b: String=travel(f,f.army_b,[5,50])
	var hits: Array=Rules.detect(f.state,f.data,[]); check(hits.size()==1,"双方实际行军仍连续接敌")
	if hits.is_empty(): return
	pay_and_move(f,[a,b]); var installed: Dictionary=Rules.install(f.state,f.data,hits[0])
	check(installed.ok,"双方移动接敌仍可正式建局："+str(installed))
	if installed.ok: check(f.state.actions[a].params.met_actions==[b] and f.state.actions[b].params.met_actions==[a],"真实双方行动继续用原 met_actions 去重")
	f=prepared()
	if f.is_empty(): return
	a=travel(f,f.army_a,[150,50]); b=travel(f,f.army_b,[100,50]); f.state.actions[b].params.paused=true
	hits=Rules.detect(f.state,f.data,[]); check(hits.size()==1,"真实暂停行程仍可被行军接触")
	if hits.is_empty(): return
	pay_and_move(f,[a]); installed=Rules.install(f.state,f.data,hits[0])
	check(installed.ok and f.state.actions[b].params.paused,"暂停接战保持原有暂停和真实余程："+str(installed))

## 逐项执行来源与半日边界，汇总只声明这里真正跑过的范围。
func run() -> void:
	geometry(); stationary_contact(); moving_contacts()
	print("CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
