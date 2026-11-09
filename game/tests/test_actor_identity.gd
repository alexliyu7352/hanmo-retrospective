## 动态人物身份回归：原世界工厂创建基线，独立私产、来源事实和NPC记忆在同一状态中验证。
extends SceneTree
const Identity=preload("res://core/actor_identity.gd")
const Social=preload("res://core/social_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎就绪后执行真实领域验证，不把未集成的继任命令当作已完成。
func _init() -> void:
	call_deferred("_run")

## 每个失败都输出行为说明并影响退出码，禁止吞掉坏状态。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从既有目录建立世界，再为领域夹具布置已有成长与私产，便于检查新人物不会复制前代。
func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	var prototype: String="actor.carter"
	var place: String=data.roles["role.office"].place_id
	data.actors[prototype].attributes.martial=73
	data.actors[prototype].skills.transport=240
	data.actors[prototype].combat={"weapon_id":"item.test_sword","learned":[{"skill_id":"skill.test_cut","mastery":90}]}
	data.actors[prototype].initial_goods=[{"item_id":"item.grain","quantity":5}]
	var runtime: Dictionary=catalog.duplicate(true); runtime.data=data
	var state: Dictionary=Factory.create(runtime,"scenario.lived_city").state
	state.facts={}; state.battles={}
	state.actors[prototype].combat={"weapon_id":"item.test_sword","learned":{"skill.test_cut":90},"heart_id":"skill.test_heart"}
	state.actors[prototype].merit=80; state.actors[prototype].fatigue=30; state.actors[prototype].injury=1
	Inventory.add(state,state.actors[prototype].inventory_id,"item.grain",5,{"kind":"actor","id":prototype})
	state.wallets[state.actors[prototype].wallet_id].balance=900
	Social.initialize(state,data)
	return {"state":state,"data":data,"prototype":prototype,"place":place}

## 永久来源由上层家族命令先建立；身份模块只引用此事实，不替上层授权收养或出生。
func source(f: Dictionary, kind: String="family_adoption") -> String:
	var id: String=Factory.next_id(f.state,"action")
	f.state.facts[id]={"kind":kind,"tick":f.state.tick}
	return id

## 按已授权永久来源调用实例化，检查真实返回与完整身份校验。
func create(f: Dictionary, name: String="新生人物", birth_tick: int=-18*720, prototype: String="") -> Dictionary:
	var creation_source: String=source(f)
	var result: Dictionary=Identity.instantiate(f.state,f.data,f.prototype if prototype.is_empty() else prototype,name,birth_tick,f.place,creation_source)
	check(result.ok and result.created,"来源充分时建立独立人物："+str(result))
	check(Identity.validate(f.state,f.data).ok,"新人物身份与资产可保存")
	result.source=creation_source
	return result

## 同一个原型产生不同真人，各自有名字、私产和空成长，不借复制前代冒充传承。
func check_creation() -> void:
	var f: Dictionary=fresh()
	var initial_count: int=f.state.actors.size()
	var data_before: Dictionary=f.data.duplicate(true)
	var original: Dictionary=f.state.actors[f.prototype].duplicate(true)
	var wallets: Dictionary=f.state.wallets.duplicate(true)
	var inventories: Dictionary=f.state.inventories.duplicate(true)
	var roles: Dictionary=f.state.roles.duplicate(true)
	var relations: Dictionary=f.state.relations.duplicate(true)
	var itineraries: Dictionary=f.state.itineraries.duplicate(true)
	var player: String=f.state.player_actor_id
	var first: Dictionary=create(f,"陆衡")
	var second: Dictionary=create(f,"陆澄",-12*720)
	var a: Dictionary=f.state.actors[first.actor_id]
	var b: Dictionary=f.state.actors[second.actor_id]
	check(first.actor_id!=second.actor_id and f.state.actors.size()==initial_count+2,"相同原型创建两个不同稳定ID，保留全部原人物")
	check(f.state.actors[f.prototype]==original and f.state.player_actor_id==player,"原人物的成长与当前主角身份保持原状态")
	check(a.prototype_id==f.prototype and Identity.definition(f.state,f.data,a.id).display_name=="陆衡" and Identity.definition(f.state,f.data,b.id).display_name=="陆澄","共用原型但显示名分别来自运行身份")
	check(a.attributes==f.data.actors[f.prototype].attributes and a.skills==f.data.actors[f.prototype].skills,"普通能力按原型建立")
	check(a.combat=={"weapon_id":"","learned":{},"heart_id":""} and a.merit==0 and a.fatigue==0 and a.injury==0,"不复制初始武器、掌握、心法、功绩与伤疲")
	check(a.wallet_id!=b.wallet_id and a.inventory_id!=b.inventory_id and f.state.wallets[a.wallet_id].balance==0 and f.state.inventories[a.inventory_id].lots.is_empty(),"新人物各有真实零余额钱包和空背包")
	check(f.state.knowledge[a.id].is_empty() and f.state.request_sequences[a.id]==0 and f.state.social.memories[a.id].is_empty(),"新人的任务见闻、请求序列和社交记忆独立空白")
	check(f.state.roles==roles and f.state.relations==relations and f.state.itineraries==itineraries,"身份创建不复制官职、关系或自动行旅")
	check(f.state.facts[a.id]=={"kind":"actor_creation","prototype_id":f.prototype,"actor_id":a.id,"created_tick":0,"birth_tick":-18*720,"display_name":"陆衡","source":first.source},"动态人物ID对应永久创建依据")
	for id in wallets: check(f.state.wallets[id]==wallets[id],"创建不挪用已有钱包："+id)
	for id in inventories: check(f.state.inventories[id]==inventories[id],"创建不挪用已有库存："+id)
	check(f.data==data_before,"不可变作者目录中没有塞入任何动态人物或显示名")
	a.attributes.martial=74
	check(b.attributes.martial==73 and f.data.actors[f.prototype].attributes.martial==73,"真人能力增长不串改另一个人物或作者原型")
	var definition: Dictionary=Identity.definition(f.state,f.data,a.id)
	definition.attributes.martial=100; definition.skills.transport=1
	check(f.data.actors[f.prototype].attributes.martial==73 and a.attributes.martial==74,"定义查询是可丢弃深副本，不能反向修改真实能力")
	check(Identity.validate(f.state,f.data).ok,"合法后续成长不因创建时初值而被拒绝")

## 周岁按720半日计算，负出生日期允许开局成人；未知旧人年龄不会被猜成年满某岁。
func check_age_and_base() -> void:
	var f: Dictionary=fresh()
	check(Identity.validate(f.state,f.data).ok,"缺少新字段的既有基础人物仍可在迁移前解析")
	check(Identity.definition(f.state,f.data,f.prototype).id==f.prototype and Identity.definition(f.state,f.data,f.prototype).display_name==f.data.texts[f.data.actors[f.prototype].name_key],"基础人物缺省原型是自身，名字回退作者文本")
	check(Identity.age_years(f.state,f.prototype)==-1 and Identity.age_years(f.state,"missing")==-1,"未迁移出生日期或未知人物年龄明确未知")
	f.state.actors[f.prototype].prototype_id=f.prototype
	f.state.actors[f.prototype].birth_tick=-30*720
	f.state.actors[f.prototype].created_tick=0
	check(Identity.age_years(f.state,f.prototype)==30 and Identity.validate(f.state,f.data).ok,"迁入出生日期后基础人物能正确计算年龄")
	var child: Dictionary=create(f,"幼子",0)
	check(Identity.age_years(f.state,child.actor_id)==0,"出生时年龄为零")
	f.state.tick=719; f.state.social.last_tick=719
	check(Identity.age_years(f.state,child.actor_id)==0,"未过720半日不提前增长一岁")
	f.state.tick=720; f.state.social.last_tick=720
	check(Identity.age_years(f.state,child.actor_id)==1 and Identity.age_years(f.state,f.prototype)==31,"所有人物沿用同一世界日期增长年龄")
	check(Identity.definition(f.state,f.data,"missing").is_empty(),"未知人物查询返回空定义")

## 永久来源只产生一次身份，读档重放不复制资产，也不会因人物后来移动而将其搬回出生地。
func check_replay_and_player_display() -> void:
	var f: Dictionary=fresh(); var made: Dictionary=create(f,"行旅继业者")
	var actor: Dictionary=f.state.actors[made.actor_id]
	Inventory.add(f.state,actor.inventory_id,"item.grain",2,{"kind":"actor","id":actor.id})
	f.state.wallets[actor.wallet_id].balance=30
	actor.combat.learned["skill.later"]=20
	f.state.tick=5; f.state.social.last_tick=5
	for place in f.data.places:
		if place!=f.place: actor.location=Factory.settlement_location(f.data,place); break
	var before: Dictionary=f.state.duplicate(true)
	var replay: Dictionary=Identity.instantiate(f.state,f.data,f.prototype,"行旅继业者",-18*720,f.place,made.source)
	check(replay.ok and not replay.created and replay.actor_id==made.actor_id and f.state==before,"同一来源重放保留后来成长、财产、位置与计数器")
	check(not Identity.instantiate(f.state,f.data,f.prototype,"另一人",-18*720,f.place,made.source).ok and f.state==before,"同一来源不能改名再创建另一人物")
	check(not Identity.instantiate(f.state,f.data,"actor.chen","行旅继业者",-18*720,f.place,made.source).ok and f.state==before,"同一来源不能换原型创建另一个人")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Identity.validate(parsed.value,f.data).ok and parsed.value==f.state,"保存往返保留动态身份、私产和永久来源")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	check(Identity.instantiate(loaded.state,loaded.data,f.prototype,"行旅继业者",-18*720,f.place,made.source)==replay and loaded.state==f.state,"读档后同一来源仍不重复结算")
	loaded.state.player_actor_id=made.actor_id
	check(Identity.validate(loaded.state,loaded.data).ok and Identity.definition(loaded.state,loaded.data,loaded.state.player_actor_id).display_name=="行旅继业者","身份查询能显示真实动态玩家，控制权合法性留给继任领域")
	check(f.state.player_actor_id!=made.actor_id,"仅查询动态玩家显示不执行任何继任命令")

## 非法来源或身份参数必须在分配ID前失败，失败不得留下半个人物或资产。
func check_rejected_creation() -> void:
	for mutation in ["prototype","name_empty","name_space","name_control","name_long","name_type","future_birth","float_birth","bool_birth","old_birth","place","source_missing","source_wrong","source_type","source_creation","counter","counter_limit","collision"]:
		var f: Dictionary=fresh()
		var params: Array=[f.prototype,"合规人物",-720,f.place,source(f)]
		match mutation:
			"prototype": params[0]="missing"
			"name_empty": params[1]=""
			"name_space": params[1]=" 人物 "
			"name_control": params[1]="人物\n姓名"
			"name_long": params[1]="人".repeat(81)
			"name_type": params[1]=17
			"future_birth": params[2]=1
			"float_birth": params[2]=-1.5
			"bool_birth": params[2]=false
			"old_birth": params[2]=-2147483648
			"place": params[3]="missing"
			"source_missing": params[4]="missing"
			"source_wrong": f.state.facts[params[4]].kind="training"
			"source_type": params[4]={"kind":"family_birth"}
			"source_creation": params[4]=create(f).actor_id
			"counter": f.state.next_instance_id=0
			"counter_limit": f.state.next_instance_id=2147483647
			"collision": f.state.facts["actor:"+str(f.state.next_instance_id)]={"kind":"unrelated_fact"}
		var before: Dictionary=f.state.duplicate(true)
		var result: Dictionary=Identity.instantiate(f.state,f.data,params[0],params[1],params[2],params[3],params[4])
		check(not result.ok and f.state==before,"拒绝创建且完全无副作用："+mutation)
	var f: Dictionary=fresh(); f.state.erase("social")
	var newborn: Dictionary=Identity.instantiate(f.state,f.data,f.prototype,"初生者",0,f.place,source(f,"family_birth"))
	check(newborn.ok and not f.state.has("social") and Identity.validate(f.state,f.data).ok,"已有出生事实可创建新人，旧世界不被隐式装入社会集合")

## 校验人物与永久事实逐项相符，不能通过改存档复制前代身份或共享其私产。
func check_invalid_saves() -> void:
	var f: Dictionary=fresh(); var first: Dictionary=create(f,"甲"); var second: Dictionary=create(f,"乙")
	for mutation in ["missing_base","prototype","base_prototype","name","name_mismatch","birth","birth_mismatch","created","base_created","fact_missing","fact_type","fact_extra","source_missing","source_kind","source_reused","actor_id","counter","wallet","owner","inventory","knowledge","sequence","memory","player","orphan"]:
		var broken: Dictionary=f.state.duplicate(true)
		var actor: Dictionary=broken.actors[first.actor_id]
		var fact: Dictionary=broken.facts[first.actor_id]
		match mutation:
			"missing_base": broken.actors.erase(f.prototype)
			"prototype": actor.prototype_id="missing"
			"base_prototype": broken.actors[f.prototype].prototype_id="actor.chen"
			"name": actor.display_name=""
			"name_mismatch": actor.display_name="改名"
			"birth": actor.birth_tick=1
			"birth_mismatch": fact.birth_tick-=720
			"created": actor.created_tick=1
			"base_created": broken.actors[f.prototype].created_tick=0.0
			"fact_missing": broken.facts.erase(first.actor_id)
			"fact_type": broken.facts[first.actor_id]=[]
			"fact_extra": fact.extra=true
			"source_missing": broken.facts.erase(first.source)
			"source_kind": broken.facts[first.source].kind="result"
			"source_reused": broken.facts[second.actor_id].source=first.source
			"actor_id": actor.id=f.prototype
			"counter": broken.next_instance_id=1
			"wallet": actor.wallet_id=broken.actors[f.prototype].wallet_id
			"owner": broken.wallets[actor.wallet_id].owner.id=f.prototype
			"inventory": actor.inventory_id=broken.actors[f.prototype].inventory_id
			"knowledge": broken.knowledge.erase(actor.id)
			"sequence": broken.request_sequences[actor.id]=false
			"memory": broken.social.memories[actor.id]=[]
			"player": broken.player_actor_id="missing"
			"orphan": broken.facts["actor:999"]={"kind":"actor_creation"}
		var before: Dictionary=broken.duplicate(true)
		check(not Identity.validate(broken,f.data).ok and broken==before,"坏身份存档只读拒绝："+mutation)

## 新人物通过原型参与同一NPC社会调度，但没有原型人物的关系、记忆或任何全知消息。
func check_dynamic_social() -> void:
	var f: Dictionary=fresh()
	var target: String="actor.chen"
	f.data.actors[f.prototype].social={"kind":"selfish","target_actor_id":target,"food_item_id":"item.grain","interval_ticks":1,"share_news":true}
	f.state.actors[target].location=Factory.settlement_location(f.data,f.place)
	Inventory.add(f.state,f.state.actors[target].inventory_id,"item.grain",2,{"kind":"actor","id":target})
	f.state.facts["battle.past"]={"kind":"personal_battle","tick":0,"participants":[f.prototype]}
	f.state.social.memories[f.prototype]={"battle.past":{"learned_tick":0,"source_actor_id":f.prototype}}
	var created: Dictionary=create(f,"新来者")
	check(f.state.social.memories[created.actor_id].is_empty() and not f.data.actors.has(created.actor_id),"新人没有原型知闻，也不需要伪造新的作者定义")
	var player: String=f.state.player_actor_id
	var result: Dictionary=Social.step(f.state,f.data,1)
	check(result.ok,"动态人物进入现有社会半日调度")
	f.state.tick=1
	var acted: bool=false
	for event in f.state.social.events.values():
		if event.kind=="theft" and event.actor_id==created.actor_id and event.target_actor_id==target: acted=true
	check(acted and Inventory.count(f.state.inventories[f.state.actors[created.actor_id].inventory_id],"item.grain",[{"kind":"actor","id":created.actor_id}])==1,"动态NPC按原型的处境规则夺取真实粮食")
	check(not f.state.social.memories[created.actor_id].has("battle.past") and f.state.player_actor_id==player,"执行新事件仍不复制前代消息或接管玩家")
	check(Identity.validate(f.state,f.data).ok and Social.validate_state(f.state,f.data).ok,"动态人物与新社会记录同时通过保存校验")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.state)).value
	check(Identity.validate(loaded,f.data).ok and Social.validate_state(loaded,f.data).ok,"动态NPC的身份及来源传播存档往返一致")

## 问讯只交付说话人此前知道的一条事实；亲历于当前半日和他人私下消息都不能变成玩家全知。
func check_player_news() -> void:
	var f: Dictionary=fresh()
	var player: String=f.state.player_actor_id
	f.state.actors[f.prototype].location=Factory.settlement_location(f.data,f.place)
	f.state.actors[player].location=Factory.settlement_location(f.data,f.place)
	f.state.tick=1; f.state.social.last_tick=1
	f.state.facts["battle.old"]={"kind":"personal_battle","tick":0,"participants":[f.prototype]}
	f.state.facts["battle.now"]={"kind":"personal_battle","tick":1,"participants":[f.prototype]}
	f.state.facts["battle.private"]={"kind":"personal_battle","tick":0,"participants":["actor.chen"]}
	f.state.social.memories[f.prototype]={"battle.old":{"learned_tick":0,"source_actor_id":f.prototype},"battle.now":{"learned_tick":1,"source_actor_id":f.prototype}}
	f.state.social.memories["actor.chen"]={"battle.private":{"learned_tick":0,"source_actor_id":"actor.chen"}}
	var before: Dictionary=f.state.duplicate(true)
	check(not Social.hear_news(f.state,f.data,player,player,1).ok and f.state==before,"不能向自己问讯而生成传述来源")
	check(not Social.hear_news(f.state,f.data,player,"missing",1).ok and f.state==before,"不存在人物不能成为说话来源")
	check(not Social.hear_news(f.state,f.data,player,f.prototype,2).ok and f.state==before,"问讯只在已结算的真实半日完成")
	var result: Dictionary=Social.hear_news(f.state,f.data,player,f.prototype,1)
	check(result.ok and result.events.size()==1,"问讯只交付一条较早已知事实")
	check(f.state.social.memories[player]=={"battle.old":{"learned_tick":1,"source_actor_id":f.prototype}},"玩家保存真实说话人和时间，未获得对方本半日见闻或他人私事")
	check(Social.validate_state(f.state,f.data).ok,"问讯来源与原news事件保持同一存档契约")
	before=f.state.duplicate(true)
	check(Social.hear_news(f.state,f.data,player,f.prototype,1).events.is_empty() and f.state==before,"重复听闻不改写来源，同半日的新见闻仍不能传播")
	f.state.tick=2; f.state.social.last_tick=2
	check(Social.hear_news(f.state,f.data,player,f.prototype,2).events.size()==1 and f.state.social.memories[player].has("battle.now") and not f.state.social.memories[player].has("battle.private"),"下一半日才能继续讲述新经历，消息范围仍受知情人约束")

## 汇总身份与调度适配验证；完整Kernel、界面及合法继任由主工程另行集成。
func _run() -> void:
	check_creation()
	check_age_and_base()
	check_replay_and_player_display()
	check_rejected_creation()
	check_invalid_saves()
	check_dynamic_social()
	check_player_news()
	print("ACTOR_IDENTITY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
