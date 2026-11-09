## NPC社会领域回归：使用既有世界工厂、库存和严格JSON，验证有限见闻及真实财产，不代替整局试玩。
extends SceneTree
const Social=preload("res://core/social_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等引擎完成启动后执行领域验证，输出真实断言结果与失败退出码。
func _init() -> void:
	call_deferred("_run")

## 汇总带有行为含义的断言，失败必须显示并影响进程退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 在原内容世界中布置四名同地NPC；玩家仍为既有玩家，社会策略只在夹具注入。
func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	var place: String=data.roles["role.office"].place_id
	for definition in data.actors.values(): definition.initial_place=place; definition.erase("social")
	var runtime: Dictionary=catalog.duplicate(true); runtime.data=data
	var state: Dictionary=Factory.create(runtime,"scenario.lived_city").state
	state.relations={}; state.facts={}; state.battles={}
	Social.initialize(state,data)
	return {"state":state,"data":data,"a":"actor.apprentice","b":"actor.carter","c":"actor.chen","d":"actor.gate","food":"item.grain","place":place}

## 为当前作者角色指定明确行为和食物引用，不向世界规则写入任何固定人物或物品ID。
func policy(f: Dictionary, actor: String, target: String, kind: String="cooperative", share: bool=false, interval: int=1) -> void:
	f.data.actors[actor].social={"kind":kind,"target_actor_id":target,"food_item_id":f.food,"interval_ticks":interval,"share_news":share}

## 将测试初始财产装进真实随身容器；后续社会往来必须通过库存转交才能改变所有者。
func give(f: Dictionary, actor: String, quantity: int, reservation: String="", owner: String="") -> void:
	Inventory.add(f.state,f.state.actors[actor].inventory_id,f.food,quantity,{"kind":"actor","id":actor if owner.is_empty() else owner},reservation)

## 查询实际所有且未预留的口粮，不把代管物品算成财产。
func food(f: Dictionary, actor: String) -> int:
	return Inventory.count(f.state.inventories[f.state.actors[actor].inventory_id],f.food,[{"kind":"actor","id":actor}])

## 设置原关系表中的单向边，避免测试另建社交分数而掩盖集成要求。
func relation(f: Dictionary, from_actor: String, to_actor: String, value: int) -> void:
	for edge in f.state.relations.values():
		if edge.from_actor==from_actor and edge.to_actor==to_actor: edge.trust=value; return
	var id: String=Factory.next_id(f.state,"relation")
	f.state.relations[id]={"id":id,"from_actor":from_actor,"to_actor":to_actor,"trust":value,"affinity":50}

## 模拟WorldStep在增加唯一世界时钟前调用社会领域；整步完成后必须能通过保存校验。
func advance(f: Dictionary) -> Dictionary:
	var result: Dictionary=Social.step(f.state,f.data,f.state.tick+1)
	check(result.ok,"半日社会结算成功："+str(result))
	if result.ok:
		f.state.tick+=1
		check(Social.validate_state(f.state,f.data).ok,"半日社会状态可保存")
	return result

## 从一个结算结果中找出领域事件，避免依赖具体全局实例编号。
func event_of(f: Dictionary, result: Dictionary, kind: String) -> Dictionary:
	for id in result.get("events",[]):
		if f.state.social.events[id].kind==kind: return f.state.social.events[id]
	return {}

## 空初始化不补跑历史；重复结算、跳过半日和读档重放必须保持唯一时钟与财产。
func check_clock_and_replay() -> void:
	var f: Dictionary=fresh()
	f.state.erase("social"); f.state.tick=7
	Social.initialize(f.state,f.data)
	check(f.state.social=={"last_tick":7,"memories":{},"events":{}},"旧世界从当前日期初始化而不补算七个半日")
	var before: Dictionary=f.state.duplicate(true)
	Social.initialize(f.state,f.data)
	check(f.state==before,"重复初始化不改变状态")
	check(not Social.step(f.state,f.data,9).ok and f.state==before,"跳过日期失败且不改变已初始化状态")
	policy(f,f.a,f.b); give(f,f.a,3); relation(f,f.a,f.b,20)
	var result: Dictionary=advance(f)
	check(not event_of(f,result,"aid").is_empty(),"首个实际半日才发生援助")
	before=f.state.duplicate(true)
	var replay: Dictionary=Social.step(f.state,f.data,f.state.tick)
	check(replay.ok and not replay.applied and f.state==before,"重放相同半日不重复转粮或生成ID")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Social.validate_state(parsed.value,f.data).ok,"严格JSON往返后社会记忆通过保存校验")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	check(advance(f)==advance(loaded) and f.state==loaded.state,"读取同一存档继续半日，财产关系事件及ID完全一致")
	before=f.state.duplicate(true)
	check(not Social.step(f.state,f.data,f.state.tick-1).ok and f.state==before,"不能倒退社会时钟")

## 真正援助产生关系成长；对方的信任与后续帮助意愿随实际收粮跨过同一门槛。
func check_cooperation() -> void:
	var f: Dictionary=fresh(); policy(f,f.a,f.b)
	give(f,f.a,3); relation(f,f.a,f.b,20); relation(f,f.b,f.a,10)
	check(not Social.willing_to_help(f.state,f.b,f.a),"受助前反向信任不足")
	var wallets: Dictionary=f.state.wallets.duplicate(true)
	var result: Dictionary=advance(f)
	check(event_of(f,result,"aid").quantity==1 and food(f,f.a)==2 and food(f,f.b)==1,"援助转移一份本人真实粮食，双方总量不变")
	check(Social.trust(f.state,f.b,f.a)==20 and Social.willing_to_help(f.state,f.b,f.a),"收粮提升原单向关系并改变后续帮助意愿")
	check(Social.trust(f.state,f.a,f.b)==22 and f.state.wallets==wallets,"施助者也记住往来，未发生的金钱交易不改钱包")
	result=advance(f)
	check(result.events.is_empty() and food(f,f.a)+food(f,f.b)==3,"不缺粮时不会继续送粮或无意义刷关系")
	var denied: Dictionary=fresh(); policy(denied,denied.a,denied.b)
	give(denied,denied.b,2); relation(denied,denied.a,denied.b,50); relation(denied,denied.b,denied.a,19)
	result=advance(denied)
	check(event_of(denied,result,"refusal").reason=="distrust" and food(denied,denied.a)==0,"求助者相信对方不能替代施助者的实际信任")
	check(Social.trust(denied.state,denied.a,denied.b)==45,"亲历拒绝降低求助者对对方的信任")
	check(advance(denied).events.is_empty(),"相同拒绝被记住，不每半日刷出重复冲突")
	relation(denied,denied.b,denied.a,20)
	check(not event_of(denied,advance(denied),"aid").is_empty(),"关系修复后真实援助恢复")

## 缺粮引发夺取和后续拒助；多轮自动变化无需玩家命令，也不能凭空增加粮食。
func check_conflict_and_autonomy() -> void:
	var f: Dictionary=fresh()
	policy(f,f.a,f.b,"cooperative",false,2)
	policy(f,f.b,f.a,"selfish")
	give(f,f.a,4); relation(f,f.a,f.b,40); relation(f,f.b,f.a,30)
	check(Social.willing_to_help(f.state,f.a,f.b),"冲突前愿意帮助目标")
	var first: Dictionary=advance(f)
	var theft: Dictionary=event_of(f,first,"theft")
	check(not theft.is_empty() and theft.actor_id==f.b and food(f,f.a)==3 and food(f,f.b)==1,"缺粮自利者夺取实际口粮并改变私有权")
	check(Social.trust(f.state,f.a,f.b)==10 and not Social.willing_to_help(f.state,f.a,f.b),"受害者失去信任，后续帮助及同行意愿下降")
	policy(f,f.c,f.b,"selfish")
	var second: Dictionary=advance(f)
	check(event_of(f,second,"theft").actor_id==f.c and food(f,f.b)==0 and food(f,f.c)==1,"另一名缺粮者下一轮自行行动，原人物处境继续变化")
	f.data.actors[f.b].social.kind="cooperative"
	var third: Dictionary=advance(f)
	check(event_of(f,third,"refusal").actor_id==f.a and food(f,f.a)==3 and food(f,f.b)==0,"有粮的前受害者依据已有冲突拒绝求助")
	check(food(f,f.a)+food(f,f.b)+food(f,f.c)==4 and f.state.tick==3,"无玩家干预的三轮事件不生成资源或第二时钟")
	var lone: Dictionary=fresh(); policy(lone,lone.a,lone.b,"selfish")
	check(advance(lone).events.is_empty(),"双方无粮时不凭空抢到粮食")

## 新听闻只能下一半日转告；不相识的其他人物不会自动知道同城发生的事。
func check_news() -> void:
	var f: Dictionary=fresh(); policy(f,f.a,f.b,"selfish",true)
	policy(f,f.b,f.c,"cooperative",true); policy(f,f.c,f.d,"cooperative",true)
	give(f,f.b,1); give(f,f.c,1); give(f,f.d,1)
	relation(f,f.c,f.a,30); relation(f,f.d,f.a,30)
	var first: Dictionary=advance(f)
	var fact_id: String=event_of(f,first,"theft").id
	check(f.state.social.memories[f.a][fact_id].source_actor_id==f.a and f.state.social.memories[f.b][fact_id].source_actor_id==f.b,"真实双方分别保留亲历来源")
	check(not f.state.social.memories.get(f.c,{}).has(fact_id) and not f.state.social.memories.get(f.d,{}).has(fact_id),"同一半日新事件不会变成全城消息")
	f.data.actors[f.a].erase("social")
	advance(f)
	check(f.state.social.memories[f.c][fact_id]=={"learned_tick":2,"source_actor_id":f.b},"下一半日由真实知情人当面传给第三人")
	check(not f.state.social.memories.get(f.d,{}).has(fact_id) and Social.trust(f.state,f.d,f.a)==30,"步初快照与人物占用阻止同半日接力全知")
	check(Social.trust(f.state,f.c,f.a)==20,"只有听闻夺粮者才调整对夺粮人的信任")
	advance(f)
	check(f.state.social.memories[f.d][fact_id]=={"learned_tick":3,"source_actor_id":f.c},"第三人下一半日才能继续转述，来源链留在存档")
	check(Social.trust(f.state,f.d,f.a)==20 and not f.state.social.memories.get(f.state.player_actor_id,{}).has(fact_id),"玩家及无关NPC不自动获取流言")
	var relation_before: Dictionary=f.state.relations.duplicate(true)
	check(advance(f).events.is_empty() and f.state.relations==relation_before,"同一消息不会被重复转述刷关系")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Social.validate_state(parsed.value,f.data).ok,"带有三段来源的社会记忆可读档")

## 修炼与战斗消息仅分配给明确参与者；刚完成的见闻也不能当日再传播。
func check_existing_facts() -> void:
	var f: Dictionary=fresh(); policy(f,f.a,f.c,"cooperative",true)
	f.state.facts["action.lesson"]={"kind":"training","phase":"completed","completed_tick":0,"training":{"student_id":f.a,"teacher_id":f.b}}
	f.state.facts["battle.fought"]={"kind":"personal_battle","tick":0,"participants":[f.d]}
	f.state.facts["action.pending"]={"kind":"training","phase":"started","training":{"student_id":f.c,"teacher_id":f.b}}
	f.state.facts["other.secret"]={"kind":"unknown","participants":[f.c],"tick":0}
	f.state.facts["battle.future"]={"kind":"personal_battle","tick":10,"participants":[f.c]}
	advance(f)
	check(f.state.social.memories[f.a].has("action.lesson") and f.state.social.memories[f.b].has("action.lesson"),"训练完成只有师生亲历")
	check(f.state.social.memories[f.d].has("battle.fought") and not f.state.social.memories.get(f.c,{}).has("action.lesson"),"战斗参与者知道本人经历，其他人同日不获知训练")
	check(not f.state.social.memories.get(f.c,{}).has("action.pending") and not f.state.social.memories.get(f.c,{}).has("other.secret"),"未完成行动和未知事实类型不生成见闻")
	check(not f.state.social.memories.get(f.c,{}).has("battle.future"),"未来日期的记录不会让NPC提前见闻")
	advance(f)
	check(f.state.social.memories[f.c]["action.lesson"].source_actor_id==f.a and not f.state.social.memories[f.c].has("battle.fought"),"NPC仅转述自己知道的已完成事实")

## 实际地点、占用、预留和容积都必须阻止无效交往，不允许社会模拟绕过现有资源边界。
func check_boundaries() -> void:
	for boundary in ["player","target_player","action","target_action","dead","unavailable","injury","remote","road","battle","result","dialogue","reserved","foreign","capacity","one_food","self"]:
		var f: Dictionary=fresh(); policy(f,f.a,f.b); give(f,f.a,2); relation(f,f.a,f.b,80)
		match boundary:
			"player": policy(f,f.state.player_actor_id,f.b); f.data.actors[f.a].erase("social"); give(f,f.state.player_actor_id,2); relation(f,f.state.player_actor_id,f.b,80)
			"target_player": policy(f,f.a,f.state.player_actor_id); relation(f,f.a,f.state.player_actor_id,80)
			"action": f.state.actors[f.a].action_id="action.busy"
			"target_action": f.state.actors[f.b].action_id="action.busy"
			"dead": f.state.actors[f.b].alive=false
			"unavailable": f.state.actors[f.a].available=false
			"injury": f.state.actors[f.b].injury=3
			"remote":
				for place_id in f.data.places:
					if place_id!=f.place: f.state.actors[f.b].location=Factory.settlement_location(f.data,place_id); break
			"road": f.state.actors[f.b].location={"kind":"Road"}
			"battle","result": f.state.battles={"battle:1":{"phase":"planning" if boundary=="battle" else "result","units":{f.b:{}}}}
			"dialogue": f.state.interactions={f.state.player_actor_id:{"context":{"provider_actor_id":f.b}}}
			"reserved":
				f.state.inventories[f.state.actors[f.a].inventory_id].lots=[]; give(f,f.a,2,"action.held")
			"foreign":
				f.state.inventories[f.state.actors[f.a].inventory_id].lots=[]; give(f,f.a,2,"",f.c)
			"capacity": f.state.inventories[f.state.actors[f.b].inventory_id].capacity=0
			"one_food": f.state.inventories[f.state.actors[f.a].inventory_id].lots[0].quantity=1
			"self": policy(f,f.a,f.a)
		var inventories: Dictionary=f.state.inventories.duplicate(true)
		var relations: Dictionary=f.state.relations.duplicate(true)
		check(advance(f).events.is_empty() and f.state.inventories==inventories and f.state.relations==relations,"无效交往不转粮或改关系："+boundary)
	var f: Dictionary=fresh(); policy(f,f.a,f.b,"cooperative",false,2); give(f,f.a,2); relation(f,f.a,f.b,20)
	check(advance(f).events.is_empty() and not event_of(f,advance(f),"aid").is_empty(),"作者间隔以世界半日计，不暗造NPC独立时钟")
	f=fresh(); policy(f,f.a,f.b); give(f,f.a,2); relation(f,f.a,f.b,20)
	f.state.parties["party.fixture"]={"leader_id":f.c,"members":[f.c,f.b]}
	f.state.actors[f.b].location={"kind":"Following","party_id":"party.fixture"}
	f.state.battles={"battle.fixture":{"phase":"closed","units":{f.b:{}}}}
	check(not event_of(f,advance(f),"aid").is_empty(),"跟随者使用领队真实地点，已关闭战斗不持续占用人物")
	f=fresh(); policy(f,f.a,f.b,"cooperative",false); give(f,f.a,2); relation(f,f.a,f.b,20)
	f.state.facts["battle.known"]={"kind":"personal_battle","tick":0,"participants":[f.a]}
	advance(f); advance(f)
	check(not f.state.social.memories.get(f.b,{}).has("battle.known"),"不分享消息的人即使当面相处也不转述私下见闻")
	f=fresh(); policy(f,f.a,f.b); policy(f,f.b,f.a,"selfish",false,8); give(f,f.b,2); relation(f,f.b,f.a,90)
	check(event_of(f,advance(f),"refusal").reason=="selfish" and food(f,f.b)==2,"自利提供者可在高信任下拒绝无偿援助")

## 拒绝伪造的来源、未来事实、同半日传播和错误记录，坏存档不能悄悄获得知识。
func check_invalid_saves() -> void:
	var f: Dictionary=fresh(); policy(f,f.a,f.b,"selfish",true); policy(f,f.b,f.c,"cooperative",true)
	give(f,f.b,1); give(f,f.c,1)
	var fact_id: String=event_of(f,advance(f),"theft").id
	f.data.actors[f.a].erase("social"); advance(f)
	for mutation in ["clock","future_clock","extra","memory_type","unknown_fact","unknown_actor","source","same_tick","witness","quantity","event_actor","event_place","news","missing_witness","source_record","event_type","memory_future","instance"]:
		var broken: Dictionary=f.state.duplicate(true)
		match mutation:
			"clock": broken.social.last_tick-=1
			"future_clock": broken.social.last_tick+=1
			"extra": broken.social.extra=true
			"memory_type": broken.social.memories[f.a][fact_id].learned_tick=1.5
			"unknown_fact": broken.social.memories[f.a]["unknown"]={"learned_tick":1,"source_actor_id":f.a}
			"unknown_actor": broken.social.memories["missing"]={}
			"source": broken.social.memories[f.c][fact_id].source_actor_id=f.d
			"same_tick": broken.social.memories[f.c][fact_id].learned_tick=1
			"witness": broken.social.memories[f.c][fact_id].source_actor_id=f.c
			"quantity": broken.social.events[fact_id].quantity=2
			"event_actor": broken.social.events[fact_id].actor_id="missing"
			"event_place": broken.social.events[fact_id].place_id="missing"
			"news":
				for event in broken.social.events.values():
					if event.kind=="news": event.fact_id="missing"
			"missing_witness": broken.social.memories.erase(f.a)
			"source_record":
				for id in broken.social.events.keys():
					if broken.social.events[id].kind=="news": broken.social.events.erase(id)
			"event_type": broken.social.events[fact_id]=3
			"memory_future": broken.social.memories[f.c][fact_id].learned_tick=3
			"instance": broken.next_instance_id=1
		check(not Social.validate_state(broken,f.data).ok,"拒绝错误社会存档："+mutation)

## 运行所有独立领域场景；成功报告仅覆盖这些实测行为，完整游戏接入由主工程验证。
func _run() -> void:
	check_clock_and_replay()
	check_cooperation()
	check_conflict_and_autonomy()
	check_news()
	check_existing_facts()
	check_boundaries()
	check_invalid_saves()
	print("SOCIAL_RULES_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
