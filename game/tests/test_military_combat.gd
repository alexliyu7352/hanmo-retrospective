## 军事桥接领域集成：真正任官编组、六角战斗和人产落账；测试抵达边界不冒充完整Main或地图验收。
extends SceneTree
const F=preload("res://tests/military_combat_fixture.gd")
const Combat=preload("res://core/military_combat.gd")
const Assets=preload("res://core/military_assets.gd")
const Round=preload("res://core/military_round.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎开始后才构建独立领域世界，不读取任何实际玩家存档。
func _init() -> void:
	call_deferred("run")

## 每项失败打印领域原因并影响总退出码，统一使用发行门禁识别的汇总格式。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 双方都通过正式当面任命与编组，未成功时保留具体失败避免误报战斗验证。
func prepared(second: bool=true) -> Dictionary:
	var f: Dictionary=F.fresh(); check(f.initialized.ok,"真实工厂军事初始化")
	var a: Dictionary=F.mobilize(f,false,second); check(a.ok and a.get("success",false),"己方实际任官受令编组："+str(a))
	var b: Dictionary=F.mobilize(f,true); check(b.ok and b.get("success",false),"敌方同源任官受令编组："+str(b))
	f.army_a=a.get("details",{}).get("army_id",""); f.army_b=b.get("details",{}).get("army_id","")
	check(Assets.validate_state(f.state,f.data).ok,"编组后真实人产可保存")
	return f

## 为真正已编组单位创建接敌请求；预览和创建引用相同永久接触事实。
func begin(f: Dictionary, controller: String="actor.player", second: bool=true) -> String:
	var ids: Array=[f.unit,f.second_unit,f.enemy_unit] if second else [f.unit,f.enemy_unit]
	var request: Dictionary=F.contact(f,[f.army_a,f.army_b],ids,controller)
	var created: Dictionary=Combat.create(f.state,f.data,request)
	check(created.ok,"以真实位置和接敌事实建会战："+str(created))
	return created.get("details",{}).get("battle_id","")

## 玩家计划经正式入口冻结；后续世界推进不得从未提交的测试草案读取决定。
func submit(f: Dictionary, id: String) -> void:
	var battle: Dictionary=f.state.battles[id]
	var params: Dictionary={"battle_id":id,"orders":F.plans(battle,f.data)}
	var before: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Combat.preview(f.state,f.data,f.state.player_actor_id,"military_plan",params)
	check(quote.ok and f.state==before,"合法计划预览不改变世界："+str(quote))
	var result: Dictionary=Combat.apply(f.state,f.data,f.state.player_actor_id,"military_plan",params)
	check(result.ok and f.state.tick==before.tick,"提交只入候选，不单独推进半日")
	check(f.state.battles[id].ai_plans==before.battles[id].ai_plans,"玩家提交不重算敌方已保存计划")

## 执行来源拒绝、权限边界、轮间重启、实物口粮和实际战损闭环。
func run() -> void:
	var f: Dictionary=prepared(); var units: Array=[f.unit,f.second_unit,f.enemy_unit]
	var request: Dictionary=F.contact(f,[f.army_a,f.army_b],units)
	var bad: Dictionary=request.duplicate(true); bad.source_id="invented"; var before: Dictionary=f.state.duplicate(true)
	check(not Combat.create(f.state,f.data,bad).ok and f.state==before,"任意UI来源不能建局或消耗实例号")
	bad=request.duplicate(true); bad.unit_ids=[f.unit,"invented"]
	check(not Combat.create(f.state,f.data,bad).ok,"永久接敌名单之外不能凭空增兵")
	f.state.actors[f.enemy].location={"kind":"Settlement","settlement_id":"other","place_id":"other"}
	check(Combat.create(f.state,f.data,request).code=="MilitaryContactPositionChanged","旧接敌消息不能把已离场军队拉回来")
	f.state.actors[f.enemy].location=f.state.facts[request.source_id].army_locations[f.army_b].duplicate(true)
	var created: Dictionary=Combat.create(f.state,f.data,request); check(created.ok,"恢复真实到场后建立会战："+str(created))
	var id: String=created.get("details",{}).get("battle_id","")
	if id.is_empty(): print("CHECKS=",checks," FAILURES=",failures); quit(1); return
	check(f.state.battles[id].command_scope=="army" and f.state.battles[id].command_unit_ids==[f.unit,f.second_unit],"现任统帅只能指挥实际所辖两部")
	check(Combat.validate_state(f.state,f.data).ok,"会战与冻结AI可保存："+str(Combat.validate_state(f.state,f.data)))
	check(not Combat.create(f.state,f.data,request).ok,"同一永久接敌事实只能消费一次")
	var duplicate: Dictionary=F.contact(f,[f.army_a,f.army_b],units)
	check(Combat.create(f.state,f.data,duplicate).code=="MilitaryUnitAlreadyEngaged","另一接敌消息也不能重复使用正在作战单位")
	before=f.state.duplicate(true); var blocked: Dictionary=F.advance(f)
	check(blocked.code=="MilitaryPlanRequired" and blocked.battle_id==id and f.state==before,"任何世界写入之前提示具体未计划战场")
	submit(f,id); check(Combat.apply(f.state,f.data,f.actor,"military_plan",{"battle_id":id,"orders":{}}).code=="MilitaryPlanAlreadySubmitted","一半日计划不能覆盖")
	var saved: Dictionary=Strict.parse_text(JSON.stringify(f.state)); check(saved.ok,"计划提交后严格JSON保存")
	var loaded: Dictionary=f.duplicate(true); loaded.state=saved.value
	check(Combat.validate_state(loaded.state,loaded.data).ok,"已保存军团与会战恢复："+str(Combat.validate_state(loaded.state,loaded.data)))
	var a: Dictionary=F.advance(f); var b: Dictionary=F.advance(loaded)
	check(a.ok and b.ok and f.state==loaded.state,"原局和读档逐军计粮、战斗与随机流完全一致："+str(a))
	check(Assets.validate_state(f.state,f.data).ok and Combat.validate_state(f.state,f.data).ok,"真实首轮人产和会话可同时保存")
	var limit: int=0
	while f.state.battles[id].phase=="planning" and limit<20:
		submit(f,id); var tick: int=f.state.tick; var result: Dictionary=F.advance(f)
		check(result.ok and f.state.tick==tick+1,"真实战场轮只消费一次全局半日："+str(result))
		if not result.ok: break
		check(Assets.validate_state(f.state,f.data).ok and Combat.validate_state(f.state,f.data).ok,"每轮实际兵员分账可保存")
		limit+=1
	var battle: Dictionary=f.state.battles[id]
	check(battle.phase=="result" and battle.applied and f.state.facts[id].phase=="completed","实际Round终局先形成永久战果再落账")
	var lost: int=0
	for unit_id in battle.units:
		var unit: Dictionary=battle.units[unit_id]; var actual: Dictionary=f.state.military_units[unit_id]
		for group in Assets.GROUPS: check(unit[group]==actual[group],"战后真实分类与战术结果一致："+group)
		lost+=actual.dead.size()+actual.wounded.size()+actual.captured.size()+actual.deserted.size()
	check(lost>0,"真实交锋产生永久伤亡或溃散，不是只改胜负字段")
	check(Combat.apply(f.state,f.data,f.actor,"military_close",{"battle_id":id}).ok,"实际落账后才可关闭结果")
	before=f.state.duplicate(true); check(not Combat.apply(f.state,f.data,f.actor,"military_close",{"battle_id":id}).ok and f.state==before,"关闭不能重复结算战果")
	check(Combat.validate_state(f.state,f.data).ok,"关闭历史仍可恢复")
	var returned: Dictionary=F.action(f,f.actor,"army_return",{"army_id":f.army_a})
	check(returned.ok and returned.get("success",false),"关闭战果后按实际剩余兵员钱货回营交接："+str(returned))
	var campaign=load("res://core/campaign_rules.gd")
	check(campaign.army_returned(f.state,f.data,returned.details.order_id,returned.details.receipt_id).ok,"实际人产交接完毕后军令进入待回报")
	check(Combat.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"部队已回驻军仍能恢复旧会战历史")
	var interaction=load("res://core/interaction_rules.gd")
	var report: Dictionary=campaign.apply(f.state,f.data,f.actor,"campaign_report",{"campaign_id":returned.details.order_id,"context":interaction.for_role(f.state,f.data,f.data.military_orders[f.office].report_role_id)})
	check(report.ok and f.state.campaigns[returned.details.order_id].status=="closed","向实际在场岗位报告完成整条军事交接")
	check(f.state.campaigns[returned.details.order_id].objective_result.kind=="withdrawn","野战胜利不会擅自替代实际占城战略目标")
	f=prepared(); f.state.player_actor_id="actor.carter"; id=begin(f,"actor.carter")
	check(f.state.battles[id].command_scope=="unit" and f.state.battles[id].command_unit_ids==[f.second_unit] and f.state.battles[id].ai_plans.has(f.unit),"真实部将只能指挥本人部，统帅本部依保存AI行动")
	var forbidden: Dictionary=F.plans(f.state.battles[id],f.data); forbidden[f.unit]=f.state.battles[id].ai_plans[f.unit]
	check(not Combat.preview(f.state,f.data,"actor.carter","military_plan",{"battle_id":id,"orders":forbidden}).ok,"部将越权改同侧部队被拒")
	f=prepared(false); id=begin(f,"",false); var auto_before: int=f.state.tick
	var autonomous: Dictionary=F.advance(f); check(autonomous.ok and f.state.tick==auto_before+1 and not f.state.battles[id].ai_plans.is_empty(),"远方无人指挥会战同样自动计粮推进")
	var ended: Dictionary=Combat.terminate(f.state,f.data,id,"truce")
	check(ended.ok and f.state.battles[id].phase=="closed" and f.state.facts[id].result.reason=="truce","实际停战清空占用并保留既有战损")
	check(Combat.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"停战后世界账目和关闭记录一致")
	check_parallel_and_supply()
	check_retainer_and_restore()
	check_captivity()
	check_spawns_and_source()
	check_delivery_and_legacy()
	print("CHECKS=",checks," FAILURES=",failures); quit(1 if failures else 0)

## 两处实际会战共享一个世界半日和同军一次口粮；缺粮士气由真实口粮账同步，不用UI补记。
func check_parallel_and_supply() -> void:
	var f: Dictionary=F.fresh(true); var a: Dictionary=F.mobilize(f); var b: Dictionary=F.mobilize(f,true)
	check(a.ok and b.ok,"两军各自实际编组两部")
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var request: Dictionary=F.contact(f,[f.army_a,f.army_b],[f.unit,f.enemy_unit])
	var first: Dictionary=Combat.create(f.state,f.data,request); check(first.ok,"第一处会战使用各军第一部")
	request=F.contact(f,[f.army_a,f.army_b],[f.second_unit,f.enemy_second_unit],"","field.ford")
	var second: Dictionary=Combat.create(f.state,f.data,request); check(second.ok,"第二处无人战场使用两军其他真实部队")
	var before: Dictionary=f.state.duplicate(true); check(F.advance(f).code=="MilitaryPlanRequired" and f.state==before,"任一玩家场缺计划便阻止远方战场和口粮先推进")
	submit(f,first.details.battle_id)
	var tick: int=f.state.tick; var result: Dictionary=F.advance(f)
	check(result.ok and f.state.tick==tick+1 and result.round_fact_ids.size()==2,"两处战场各一轮只走一个世界半日")
	var supplies: Dictionary={}
	for receipt_id in result.supply_events:
		var fact: Dictionary=f.state.facts[receipt_id]
		if not fact.army_id.is_empty(): supplies[fact.army_id]=supplies.get(fact.army_id,0)+1
	check(supplies=={f.army_a:1,f.army_b:1},"同一支军团分两处交战也只实际吃一次口粮")
	before=f.state.duplicate(true); var repeated: Dictionary=Combat.step(f.state,f.data,f.state.tick)
	check(not repeated.ok and f.state==before,"旧时刻重复请求不再扣人或重写战果")
	check(Assets.validate_state(f.state,f.data).ok and Combat.validate_state(f.state,f.data).ok,"多场半日后真实人产和会话均可保存")
	f=prepared(false); var id: String=begin(f,"",false)
	var bag: Dictionary=f.state.inventories[f.state.armies[f.army_a].inventory_id]
	var store: Dictionary=f.state.inventories[f.data.garrisons[f.camp].inventory_id]
	var grain: String=f.data.garrisons[f.camp].food_item_id
	var inventory=load("res://core/inventory_rules.gd")
	var quantity: int=inventory.count(bag,grain,[bag.owner])
	check(inventory.transfer(f.state,bag.id,store.id,grain,quantity,[bag.owner]).ok,"夹具把实际随军口粮交回公库形成真实短粮")
	var old_morale: int=f.state.military_units[f.unit].morale; tick=f.state.tick
	check(Combat.preview_step(f.state,f.data,tick+1).ok,"无人会战无需虚构玩家提交")
	check(Assets.step(f.state,f.data,tick+1).ok and f.state.military_units[f.unit].morale==old_morale-8,"口粮先真实短缺并减少人产士气")
	var expected: Dictionary=f.state.battles[id].duplicate(true)
	for unit_id in expected.units: expected.units[unit_id].morale=f.state.military_units[unit_id].morale
	var pure: Dictionary=Round.resolve(expected,f.data,"",{})
	result=Combat.step(f.state,f.data,tick+1)
	check(result.ok and f.state.battles[id].units==pure.battle.units,"真实断粮士气参与本轮命中和溃退，与同源Round结果一致")
	check(f.state.battles[id].ai_plans==pure.battle.ai_plans,"下一轮AI只来自已消费断粮及战斗结果")
	before=f.state.duplicate(true); repeated=Combat.step(f.state,f.data,tick+1)
	check(repeated.ok and repeated.round_fact_ids.is_empty() and f.state==before,"同一候选重调已结算半日不会重复落账")

## 真实私人部曲借调入伍后参与伤亡，存档与下一口粮名单均反映正式人产返回的真人后果。
func check_retainer_and_restore() -> void:
	var f: Dictionary=F.fresh(); var a: Dictionary=F.mobilize(f,false,false,[f.retainer]); var b: Dictionary=F.mobilize(f,true)
	check(a.ok and b.ok,"真实部曲经本人借调和公库装备后随军")
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var id: String=begin(f,"",false); var seen: bool=false; var loops: int=0
	while f.state.battles[id].phase=="planning" and loops<20:
		var result: Dictionary=F.advance(f); check(result.ok,"真人部曲参加实际自动交锋")
		if not result.ok: break
		for change in result.actor_changes:
			if change.actor_id==f.retainer: seen=true
		check(Assets.validate_state(f.state,f.data).ok,"逐轮真人伤亡已按正式字段回写")
		loops+=1
	check(seen and not f.state.actors[f.retainer].alive and f.retainer in f.state.military_units[f.unit].dead,"真实部曲死亡来自命中分类和actor_changes，并非人工改结果")
	var result: Dictionary=F.advance(f); check(result.ok,"战后仍按下一真实半日计粮")
	var found: bool=false
	for receipt_id in result.supply_events:
		var fact: Dictionary=f.state.facts[receipt_id]
		if fact.army_id==f.army_a: found=true; check(not f.retainer in fact.mouth_ids,"已死真人不再列入下一半日军粮名单")
	check(found,"确有该军下一半日实际供给账")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state)); check(parsed.ok and Combat.validate_state(parsed.value,f.data).ok and Assets.validate_state(parsed.value,f.data).ok,"真人损失和自动关闭终局严格保存恢复")
	var broken: Dictionary=f.state.duplicate(true); broken.facts.erase(broken.battles[id].settlement_receipt_id)
	check(not Combat.validate_state(broken,f.data).ok,"缺失永久战损收据拒绝恢复，不能再次结算修补")

## 出生容量与节点来源属于真实作者/世界边界；错误配置或篡改保存阵营须清楚失败。
func check_spawns_and_source() -> void:
	var f: Dictionary=prepared(); var request: Dictionary=F.contact(f,[f.army_a,f.army_b],[f.unit,f.second_unit,f.enemy_unit])
	var before: Dictionary=f.state.duplicate(true); var limited: Dictionary=f.data.duplicate(true); limited.battlefields[request.battlefield_id].spawns.pop_back()
	check(Combat.create(f.state,limited,request).code=="MilitaryBattlefieldCapacity" and f.state==before,"作者出生位不足不造人、不把部队放进障碍")
	f.state.facts[request.source_id].siege_id="missing"; f.state.facts[request.source_id].node_id="gate"
	check(Combat.create(f.state,f.data,request).code=="MilitarySiegeSourceMissing","伪造节点来源不能绕过真实围城占用")
	f.state.facts[request.source_id].erase("siege_id"); f.state.facts[request.source_id].erase("node_id")
	var created: Dictionary=Combat.create(f.state,f.data,request); check(created.ok,"完整合法出生位置可复用")
	var id: String=created.details.battle_id
	var corrupt: Dictionary=f.state.duplicate(true); corrupt.battles[id].units[f.enemy_unit].side=0
	check(not Combat.validate_state(corrupt,f.data).ok,"存档不能把接敌时敌军改成可控友军")
	corrupt=f.state.duplicate(true); corrupt.battles[id].participant_actor_ids.append("actor.chen")
	check(not Combat.validate_state(corrupt,f.data).ok,"不能在存档附带遥远无关人物占用")
	var denied: Dictionary=Combat.terminate(f.state,f.data,id,"invented")
	check(not denied.ok,"未知收尾原因不能静默结束军事任务")
	check(Combat.terminate(f.state,f.data,id,"authority_lost").ok and Combat.validate_state(f.state,f.data).ok,"失效命令可在当前边界结束，不凭空再多走一个半日")
	before=f.state.duplicate(true); check(Combat.terminate(f.state,f.data,id,"authority_lost").get("already_completed",false) and f.state==before,"根层释放请求幂等，不重复清算")
	f=prepared(); request=F.contact(f,[f.army_a,f.army_b],[f.unit,f.second_unit,f.enemy_unit])
	for cell in f.data.battlefields[request.battlefield_id].cells:
		if cell.q==0 and cell.r==2: cell.terrain="water"
	f.data.troop_types["troop.foot"].movement_costs.water=1
	created=Combat.create(f.state,f.data,request); check(created.ok,"出生格可按兵种实际通行重新匹配")
	id=created.details.battle_id
	check(f.state.battles[id].units[f.unit].position==[0,2] and f.state.battles[id].units[f.second_unit].position==[0,1],"能涉水的部队让出唯一岸上格给不能涉水的部队")

## 实际连续缺粮造成低士气，真实近身交锋才发生溃退；俘虏由溃退事件中在场敌军看管。
func check_captivity() -> void:
	var f: Dictionary=prepared(false)
	var inventory=load("res://core/inventory_rules.gd")
	var bag: Dictionary=f.state.inventories[f.state.armies[f.army_a].inventory_id]
	var store: Dictionary=f.state.inventories[f.data.garrisons[f.camp].inventory_id]
	var grain: String=f.data.garrisons[f.camp].food_item_id
	check(inventory.transfer(f.state,bag.id,store.id,grain,inventory.count(bag,grain,[bag.owner]),[bag.owner]).ok,"耗粮前将真实粮食交回公库")
	for _i in range(8): check(F.advance(f).ok,"实际缺粮半日形成士气损失")
	check(f.state.military_units[f.unit].morale==6,"连续缺粮来自八份真实口粮事实")
	f.data.battlefields["field.open"].spawns[1].position=[1,1]
	var id: String=begin(f,"actor.player",false); var battle: Dictionary=f.state.battles[id]
	var unit: Dictionary=battle.units[f.unit]
	var order: Dictionary={"action":"wait","destination":unit.position.duplicate(),"heading":unit.heading,"target_id":"","tactic_id":""}
	check(Combat.apply(f.state,f.data,f.actor,"military_plan",{"battle_id":id,"orders":{f.unit:order}}).ok,"低士气部队可提交真实原地等待")
	var result: Dictionary=F.advance(f); check(result.ok,"近身交锋实际伤亡和溃退结算："+str(result))
	var actual: Dictionary=f.state.military_units[f.unit]
	check(not actual.captured.is_empty() and not actual.deserted.is_empty(),"交锋真实区分被俘与逃散，未把全军统一算死亡")
	for person in actual.captured:
		check(actual.captors[person]==f.army_b and person in f.state.armies[f.army_b].prisoner_ids,"每名新俘虏均由在场实际敌军登记看管")
	check(Assets.validate_state(f.state,f.data).ok and Combat.validate_state(f.state,f.data).ok,"近身俘虏和同轮人员守恒通过真实领域校验")

## 军令交货与口粮日账采用不同永久事实，旧个人战内容也不因新军事桥接被要求增加空定义表。
func check_delivery_and_legacy() -> void:
	var f: Dictionary=F.fresh(); var campaign=load("res://core/campaign_rules.gd")
	f.data.military_orders[f.office].objective={"kind":"deliver_supply","item_id":"item.grain","quantity":6}
	var formation: Dictionary=F.mobilize(f,false,false); check(formation.ok,"实际补给军令正常组军")
	var army: Dictionary=f.state.armies[formation.details.army_id]
	check(campaign.apply(f.state,f.data,f.actor,"campaign_depart",{"campaign_id":army.order_id}).ok,"实际供给军按已受军令出征")
	var factory=load("res://core/world_factory.gd")
	f.state.actors[f.actor].location=factory.settlement_location(f.data,f.data.garrisons[f.enemy_camp].place_id)
	check(F.advance(f).ok,"真实抵达边界和口粮同半日办理")
	var delivered: Dictionary=campaign.apply(f.state,f.data,f.actor,"campaign_deliver",{"campaign_id":army.order_id})
	check(delivered.ok and f.state.facts[delivered.details.receipt_id].kind=="military_delivery","公共实物交付写独立永久交货事实")
	check(Assets.validate_state(f.state,f.data).ok and campaign.validate_state(f.state,f.data).ok,"交货事实与真实口粮账共存，不误套同一个收据Schema")
	var legacy: Dictionary={"tick":0,"battles":{"old":{"kind":"personal"}}}
	check(Combat.validate_state(legacy,{}).ok and Combat.preview_step(legacy,{},1).ok,"缺少可选军事定义的旧个人战包兼容")
