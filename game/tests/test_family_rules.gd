## 家庭领域回归：真实收养、半日成长、本人资产和永久控制链复用同一世界，不以换原型冒充传承。
extends SceneTree
const Family=preload("res://core/family_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Estate=preload("res://core/estate_rules.gd")
const Social=preload("res://core/social_rules.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎就绪后执行领域用例，失败影响真实进程退出码。
func _init() -> void:
	call_deferred("_run")

## 输出具体失败行为，不能把脚本启动或断言数量冒充游戏体验。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 两个服务使用不同原型、姓名、费用与成年门槛，所有人和钱包仍从已有内容工厂创建。
func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	var place: String=data.roles["role.office"].place_id
	var options: Array=[{"id":"role.family_a","provider":"actor.chen","prototype":"actor.carter","name":"家中长子","fee":12,"age":18},{"id":"role.family_b","provider":"actor.zheng","prototype":"actor.he","name":"另一家幼女","fee":23,"age":19}]
	for config in options:
		var role: Dictionary=data.roles["role.office"].duplicate(true)
		role.id=config.id; role.initial_actor=config.provider; role.organization_id=null; role.services=["family_adopt"]
		data.texts["family.name."+config.id]=config.name
		var wallet_id: String="wallet.family:"+config.provider
		data.wallets[wallet_id]={"id":wallet_id,"owner":{"kind":"actor","id":config.provider},"initial_balance":10}
		role.family={"prototype_id":config.prototype,"child_name_key":"family.name."+config.id,"child_age_years":16,"fee":config.fee,"wallet_id":wallet_id,"adoption_ticks":2,"guardian_min_age":30,"heir_min_age":config.age,"retirement_min_age":40}
		data.roles[config.id]=role
		data.actors[config.provider].initial_place=place
	data.actors["actor.player"].initial_place=place; data.actors["actor.shen"].initial_place=place
	data.actors["actor.carter"].attributes.martial=71; data.actors["actor.he"].attributes.martial=63
	data.actors["actor.carter"].combat={"weapon_id":"","learned":[{"skill_id":"skill.legacy","mastery":100}]}
	var runtime: Dictionary=catalog.duplicate(true); runtime.data=data
	var state: Dictionary=Factory.create(runtime,"scenario.lived_city").state
	state.facts={}; state.battles={}
	for actor in state.actors.values():
		actor.birth_tick=-38*720; actor.created_tick=0; actor.injury=0
		actor.combat={"weapon_id":"","heart_id":"","learned":{}}
	state.actors["actor.shen"].birth_tick=-46*720
	state.wallets[state.actors["actor.player"].wallet_id].balance=300
	state.wallets[state.actors["actor.shen"].wallet_id].balance=400
	Social.initialize(state,data)
	check(Family.initialize(state,data).ok,"在已有世界上建立家庭索引")
	return {"state":state,"data":data,"guardian":"actor.player","other":"actor.shen","place":place,"role":"role.family_a","second_role":"role.family_b"}

## 在测试候选中按Kernel协议占用双方并开始收养，实际写入仍由Family负责。
func start(f: Dictionary, guardian: String="", role_id: String="", name: String="") -> Dictionary:
	if guardian.is_empty(): guardian=f.guardian
	if role_id.is_empty(): role_id=f.role
	var params: Dictionary={"context":Interaction.for_role(f.state,f.data,role_id)}
	if not name.is_empty(): params.display_name=name
	var quote: Dictionary=Family.preview(f.state,f.data,guardian,"family_adopt",params)
	check(quote.ok and quote.duration==2,"正常收养报价包含两个实际半日："+str(quote.get("code","")))
	if not quote.ok: return {}
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"family_adopt","actor_id":guardian,"participants":[guardian,quote.context.provider_actor_id],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":params}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	var result: Dictionary=Family.begin(f.state,f.data,action,quote)
	check(result.ok,"收养占用双方并冻结真实条款："+str(result))
	check(Family.validate_action(f.state,f.data,action).ok and Family.validate_state(f.state,f.data).ok,"进行中的家庭事务可保存")
	return action

## 单一半日先让社会运行，再推进时钟并完成收养；不让新人物在出生前参加该半日社会行动。
func advance(f: Dictionary, action: Dictionary={}, estate_prepared: bool=false) -> Dictionary:
	if f.state.has("estates") and not estate_prepared:
		var production: Dictionary=Estate.step(f.state,f.data,f.state.tick+1)
		if not production.ok: return production
	var society: Dictionary=Social.step(f.state,f.data,f.state.tick+1)
	if not society.ok: return society
	if not action.is_empty(): action.remaining_ticks-=1
	f.state.tick+=1
	var result: Dictionary={"ok":true}
	if not action.is_empty() and action.remaining_ticks==0:
		result=Family.finish(f.state,f.data,action)
		for person in action.participants:
			if f.state.actors[person].action_id==action.id: f.state.actors[person].action_id=""
		f.state.actions.erase(action.id)
	return result

## 完成一次真实收养，便于其他用例关注成年和交接；失败不会用测试伪造孩子。
func adopt(f: Dictionary, guardian: String="", role_id: String="", name: String="") -> String:
	var action: Dictionary=start(f,guardian,role_id,name)
	if action.is_empty(): return ""
	check(advance(f,action).ok,"首半日尚未完成收养")
	var result: Dictionary=advance(f,action)
	check(result.ok and result.get("success",false),"第二半日完成真实收养："+str(result))
	check(Family.validate_state(f.state,f.data).ok and Social.validate_state(f.state,f.data).ok,"孩子与独立社会记忆进入同一份可保存世界")
	return result.get("actor_id","")

## 逐半日推进至真实成年，不调用Family改日期或向人物复制前代知识。
func grow(f: Dictionary, child: String, years: int=18) -> void:
	var target: int=f.state.actors[child].birth_tick+years*720
	while f.state.tick<target:
		var result: Dictionary=advance(f)
		if not result.ok: check(false,"长期半日推进："+str(result)); return
	check(Identity.age_years(f.state,child)==years and Family.validate_state(f.state,f.data).ok,"唯一半日时钟带来真实成年资格")

## 执行已有预览和应用入口，指定本身不控制人物也不移交物品。
func designate(f: Dictionary, child: String, guardian: String="") -> Dictionary:
	if guardian.is_empty(): guardian=f.guardian
	var quote: Dictionary=Family.preview(f.state,f.data,guardian,"family_designate",{"heir_actor_id":child})
	check(quote.ok,"成年本人名下孩子可以当面指定："+str(quote.get("code","")))
	if not quote.ok: return quote
	var result: Dictionary=Family.apply(f.state,f.data,guardian,"family_designate",quote)
	check(result.ok and Family.validate_state(f.state,f.data).ok,"指定留下永久事实且可保存")
	return result

## 总量按所有实际钱包和库存统计，不把仅变更所有权当作增发资产。
func totals(f: Dictionary) -> Dictionary:
	var total: Dictionary={"money":0,"goods":{}}
	for wallet in f.state.wallets.values(): total.money+=wallet.balance
	for inventory in f.state.inventories.values():
		for lot in inventory.lots: total.goods[lot.item_id]=total.goods.get(lot.item_id,0)+lot.quantity
	return total

## 建立实际私庄钱柜和仓库，保留公共寄存货物，以检验移交仅修改私人来源。
func add_estate(f: Dictionary) -> String:
	var id: String="estate.family"
	var owner: Dictionary={"kind":"actor","id":f.guardian}
	var inventory_id: String="inventory.family_estate"
	var wallet_id: String="wallet.family_estate"
	f.data.estates={id:{"id":id,"name_key":"name.actor.player","place_id":f.place,"owner_actor_id":f.guardian,"manager_actor_id":"actor.carter","resident_actor_ids":["actor.carter"],"inventory_id":inventory_id,"wallet_id":wallet_id,"food_item_id":"item.grain","ration_per_person":1,"output_item_id":"item.grain","output_per_worker":3,"granary_capacity_per_level":20,"palisade_defense_per_guard":5,"initial_facilities":{"farm":1,"granary":0,"palisade":1},"initial_duties":[{"actor_id":"actor.carter","duty":"idle"}],"construction":{"max_level":3,"build_ticks":2,"money_cost":10,"material_item_id":"item.grain","material_cost":2,"payee_wallet_id":f.data.roles[f.role].family.wallet_id}}}
	f.data.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":80,"goods":[]}
	f.data.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"initial_balance":170}
	f.state.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":80,"revision":0,"lots":[]}
	f.state.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"balance":170,"reserved":{}}
	Inventory.add(f.state,inventory_id,"item.grain",12,owner)
	Inventory.add(f.state,inventory_id,"item.grain",2,{"kind":"organization","id":f.data.organizations.keys()[0]})
	check(Estate.initialize(f.state,f.data).ok and Family.validate_state(f.state,f.data).ok,"继任夹具具有受现有庄园验证约束的真实私产")
	return id

## 先通过现存修炼模块实际传授一部分掌握，再验证后续继任不会偷学余下功法。
func teach(f: Dictionary, child: String) -> void:
	f.data.personal_skills={"skill.shared":{"id":"skill.shared","kind":"strike","practice_ticks":10}}
	f.state.actors[f.guardian].combat.learned={"skill.legacy":100,"skill.shared":90}
	f.state.roles["role.office"].actor_id=child
	var params: Dictionary={"skill_id":"skill.shared","context":Interaction.for_role(f.state,f.data,"role.office")}
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.guardian,"teach_skill",params)
	check(quote.ok,"前代通过已有修炼领域当面传授")
	if not quote.ok: return
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"teach_skill","actor_id":f.guardian,"participants":[f.guardian,child],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":params}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	check(Cultivation.begin(f.state,f.data,action,quote).ok,"传授也占用同一世界两名真人")
	for index in range(quote.duration):
		action.remaining_ticks-=1
		if action.remaining_ticks==0:
			check(Cultivation.finish(f.state,f.data,action).get("success",false),"实际半日传授完成")
			for person in action.participants: f.state.actors[person].action_id=""
			f.state.actions.erase(id)
		check(advance(f).ok,"传授推进共享半日")
	check(f.state.actors[child].combat.learned=={"skill.shared":10},"后代仅掌握实际传授所得的一部分")

## 保留费用、完工、新人物成长隔离和永久重放共同验证正常收养路径。
func check_adoption() -> void:
	var f: Dictionary=fresh()
	var before: Dictionary=f.state.duplicate(true)
	check(Family.initialize(f.state,f.data).ok and f.state==before,"再次初始化不重置家庭、时间或玩家")
	var action: Dictionary=start(f,"","","陆新")
	var terms: Dictionary=action.params.training
	check(f.state.actors.size()==before.actors.size() and f.state.wallets[terms.payer_wallet_id].balance==before.wallets[terms.payer_wallet_id].balance,"签约时不凭空造人或提前转出费用")
	check(Inventory.spendable(f.state.wallets[terms.payer_wallet_id])==before.wallets[terms.payer_wallet_id].balance-12,"实际保留费用影响当前可用资金")
	var started: Dictionary=f.state.duplicate(true)
	check(not Family.begin(f.state,f.data,action,{"training":terms,"context":action.params.context}).ok and f.state==started,"同一收养开始不能再次保留或收费")
	check(advance(f,action).ok and f.state.family.children.is_empty(),"一个半日不足以收养完工")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Family.validate_state(parsed.value,f.data).ok,"收养中途可严格JSON保存")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	var a: Dictionary=advance(f,action)
	var b: Dictionary=advance(loaded,loaded.state.actions[action.id])
	check(a==b and f.state==loaded.state,"中途读档在相同半日产生同一人物和结算")
	var child: String=a.actor_id
	check(f.state.actors[child].created_tick==2 and Identity.age_years(f.state,child)==16 and f.state.facts[child].source==action.id,"人物创建、收养来源和真实年龄一致")
	check(f.state.actors[child].display_name=="陆新" and f.state.actors[child].attributes.martial==71,"合法姓名覆盖作者默认值，普通能力来自指定原型")
	check(f.state.actors[child].combat.learned.is_empty() and f.state.actors[child].merit==0 and f.state.knowledge[child].is_empty() and f.state.social.memories[child].is_empty(),"新人物不复制原型的掌握、前代见闻、人脉或功勋")
	check(f.state.wallets[terms.payer_wallet_id].balance==before.wallets[terms.payer_wallet_id].balance-12 and f.state.wallets[terms.recipient_wallet_id].balance==before.wallets[terms.recipient_wallet_id].balance+12,"完工费用真实从监护人转入提供人钱包")
	var complete: Dictionary=f.state.duplicate(true)
	check(Family.finish(f.state,f.data,action).get("already_completed",false) and f.state==complete,"永久收养事实防止读档重放多造人或多付钱")
	check(not Family.preview(f.state,f.data,f.guardian,"family_designate",{"heir_actor_id":child}).ok,"十六岁孩子不能立即继任")
	var second: String=adopt(f,f.other,f.second_role)
	check(second!=child and f.state.actors[second].prototype_id=="actor.he" and f.state.actors[second].attributes.martial==63 and f.state.actors[second].display_name=="另一家幼女","第二个家庭使用不同原型、中文名字与真实身份")
	check(f.state.facts[f.state.family.children[second]].terms.fee==23 and f.state.facts[f.state.family.children[second]].guardian_actor_id==f.other,"另一配置独立收费和建立监护关系")
	check(not Family.preview(f.state,f.data,f.guardian,"family_designate",{"heir_actor_id":second}).ok,"别人的孩子不能冒认为自己的合法继任人")
	var tampered: Dictionary=f.state.duplicate(true)
	tampered.facts[action.id].terms.policy.heir_min_age=18.0
	check(not Family.validate_state(tampered,f.data).ok,"永久门槛必须保存为整数，不能用小数绕过结构约束")

## 缺年龄、授权、资金或当面条件明确失败；行动中的失效占用不得继续支付并创建孩子。
func check_invalid_adoption() -> void:
	for condition in ["unknown_age","young_guardian","small_gap","absent","role","wallet","funds","name","party","conversation"]:
		var f: Dictionary=fresh()
		var params: Dictionary={"context":Interaction.for_role(f.state,f.data,f.role)}
		match condition:
			"unknown_age": f.state.actors[f.guardian].erase("birth_tick")
			"young_guardian": f.state.actors[f.guardian].birth_tick=-29*720
			"small_gap": f.state.actors[f.guardian].birth_tick=-30*720
			"absent": f.state.actors[params.context.provider_actor_id].location={"kind":"OnRoad"}
			"role": f.state.roles[f.role].enabled=false
			"wallet": f.data.roles[f.role].family.wallet_id=f.state.actors[f.guardian].wallet_id
			"funds": f.state.wallets[f.state.actors[f.guardian].wallet_id].balance=1
			"name": params.display_name="坏\n姓名"
			"party": f.state.parties["party.test"]={"leader_id":f.guardian,"members":[f.guardian,f.other]}
			"conversation": f.state.interactions[f.other]={"context":{"provider_actor_id":f.guardian}}
		var before: Dictionary=f.state.duplicate(true)
		check(not Family.preview(f.state,f.data,f.guardian,"family_adopt",params).ok and f.state==before,"无副作用拒绝不满足收养条件："+condition)
	var appointed: Dictionary=fresh()
	appointed.state.tasks["task.unsettled"]={"assignee":appointed.guardian,"status":"active"}
	appointed.state.offices={"office.test":{"holder_actor_id":appointed.guardian,"enabled":true}}
	var original: Dictionary=appointed.state.duplicate(true)
	check(Family.preview(appointed.state,appointed.data,appointed.guardian,"family_adopt",{"context":Interaction.for_role(appointed.state,appointed.data,appointed.role)}).ok and appointed.state==original,"任官或持有托付期间仍可当面办理收养，资格查询不处分公职")
	for condition in ["moved","busy","disabled","injured"]:
		var f: Dictionary=fresh()
		var money: Dictionary=totals(f)
		var action: Dictionary=start(f)
		advance(f,action)
		var provider: String=action.participants[1]
		match condition:
			"moved": f.state.actors[provider].location={"kind":"OnRoad"}
			"busy": f.state.actors[provider].action_id="action.other"
			"disabled": f.state.roles[f.role].enabled=false
			"injured": f.state.actors[provider].injury=3
		var result: Dictionary=advance(f,action)
		check(result.ok and not result.success and result.code=="FamilyAdoptionInterrupted","完工时实际占用失效留下失败结果："+condition)
		check(f.state.family.children.is_empty() and totals(f)==money and f.state.wallets[action.params.training.payer_wallet_id].reserved.is_empty(),"中断不造人、不收费用并释放自己的保留额")
		check(Family.validate_state(f.state,f.data).ok,"中断事实仍可保存和审计")

## 接续只交接真实私產，保留独立人脉、既有传授、前代本人和庄园管事；公物不随控制权转移。
func check_succession() -> void:
	var f: Dictionary=fresh()
	var child: String=adopt(f)
	grow(f,child)
	check(Identity.age_years(f.state,f.guardian)==40,"孩子成年时监护人也由同一时钟达到退隐年龄")
	teach(f,child)
	designate(f,child)
	var estate_id: String=add_estate(f)
	var definition: Dictionary=f.data.estates[estate_id]
	var owner: Dictionary={"kind":"actor","id":f.guardian}
	var actor: Dictionary=f.state.actors[f.guardian]
	var heir: Dictionary=f.state.actors[child]
	Inventory.add(f.state,actor.inventory_id,"item.grain",3,owner)
	f.state.knowledge[f.guardian]={"private_old_news":{"source":"old"}}; f.state.knowledge[child]={"earned_news":{"source":"own"}}
	actor.merit=80; heir.merit=3
	f.state.roles["role.office"].actor_id=f.guardian
	var relationships: Dictionary=f.state.relations.duplicate(true)
	var memories: Dictionary=f.state.social.memories.duplicate(true)
	var knowledge: Dictionary=f.state.knowledge.duplicate(true)
	var growth: Dictionary=heir.combat.duplicate(true)
	var roles: Dictionary=f.state.roles.duplicate(true)
	var estate: Dictionary=f.state.estates[estate_id].duplicate(true)
	var wallet_before: int=f.state.wallets[actor.wallet_id].balance
	var public_assets: Dictionary={}
	for id in f.state.wallets:
		if f.state.wallets[id].owner.kind=="organization": public_assets[id]=f.state.wallets[id].duplicate(true)
	var before: Dictionary=totals(f)
	var quote: Dictionary=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	check(quote.ok and quote.duration==0 and quote.family.assets.estates.size()==1,"退隐报价列出真实私袋、钱包及庄园来源")
	var result: Dictionary=Family.apply(f.state,f.data,f.guardian,"family_retire",quote)
	check(result.ok and f.state.player_actor_id==child and f.state.actors[f.guardian].alive,"合法控制转给本人养子，前代仍活着独立存在："+str(result))
	check(totals(f)==before and f.state.wallets[actor.wallet_id].balance==0 and f.state.wallets[heir.wallet_id].balance==wallet_before,"私财真正转入后代本人钱包，全世界钱货总量守恒")
	check(Inventory.count(f.state.inventories[heir.inventory_id],"item.grain",[{"kind":"actor","id":child}])==3 and f.state.inventories[actor.inventory_id].lots.is_empty(),"随身实物移入后代独立行囊，没有生成第二份")
	check(f.state.knowledge==knowledge and f.state.relations==relationships and f.state.social.memories==memories and heir.combat==growth and heir.merit==3,"知识只保留此前所得，关系、社会记忆、功勋和整套功法均不复制")
	check(f.state.estates[estate_id].owner_actor_id==child and f.state.estates[estate_id].manager_actor_id==estate.manager_actor_id and f.state.estates[estate_id].duties==estate.duties,"私庄所有权交接，原有管事与劳作配置延续")
	check(f.state.wallets[definition.wallet_id].balance==170 and f.state.wallets[definition.wallet_id].owner.id==child and f.state.inventories[definition.inventory_id].owner.id==child,"独立钱柜和仓库保留实体与余额，仅依法改所有权")
	check(Inventory.count(f.state.inventories[definition.inventory_id],"item.grain",[{"kind":"actor","id":child}])==12 and f.state.inventories[definition.inventory_id].lots[1].owner.kind=="organization","仓库私粮移交，公共寄存粮保持原所有者")
	check(not f.state.roles["role.office"].enabled and f.state.roles["role.office"].actor_id==f.guardian,"前代本人任职停用，职位不自动授予后代")
	for id in public_assets: check(f.state.wallets[id]==public_assets[id],"组织公库不动："+id)
	for id in roles:
		if roles[id].actor_id!=f.guardian: check(f.state.roles[id]==roles[id],"他人岗位保持原职："+id)
	check(Family.validate_state(f.state,f.data).ok and Identity.validate(f.state,f.data).ok and Estate.validate_state(f.state,f.data).ok,"人物、庄园和永久继任链共同通过存档校验")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Family.validate_state(parsed.value,f.data).ok and parsed.value==f.state,"完整继任世界严格保存读回一致")
	var after: Dictionary=f.state.duplicate(true)
	check(not Family.apply(f.state,f.data,f.guardian,"family_retire",quote).ok and f.state==after,"同一前代重复退隐不移交第二次")
	var forged: Dictionary=f.state.duplicate(true); forged.player_actor_id=f.guardian
	check(not Family.validate_state(forged,f.data).ok,"不能只改玩家字段倒转永久控制链")
	forged=f.state.duplicate(true); forged.estates[estate_id].owner_actor_id=f.guardian
	forged.wallets[definition.wallet_id].owner=owner; forged.inventories[definition.inventory_id].owner=owner
	check(not Family.validate_state(forged,f.data).ok,"绕过永久资产来源改回私庄所有者被拒绝")
	forged=f.state.duplicate(true); forged.facts[result.fact_id].assets.wallets[0].source_id=public_assets.keys()[0]
	check(not Family.validate_state(forged,f.data).ok,"伪造组织钱库为继任资产被拒绝")

## 成年以各自配置判断，指定和退隐都重新核验当前在场、容量以及未解除的公共义务。
func check_succession_rejections() -> void:
	var f: Dictionary=fresh()
	var child: String=adopt(f)
	var other_child: String=adopt(f,f.other,f.second_role)
	grow(f,other_child)
	check(not Family.preview(f.state,f.data,f.other,"family_designate",{"heir_actor_id":other_child}).ok,"第二家庭规定十九岁，十八岁不能借首配置通过")
	grow(f,other_child,19)
	designate(f,other_child,f.other)
	check(not Family.preview(f.state,f.data,f.other,"family_retire",{}).ok,"非当前控制人的家庭不能夺取玩家控制")
	designate(f,child)
	var baseline: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	for condition in ["busy","far","party","battle","contract","office","army","capacity","reserved","foreign","elsewhere","dead","young","choice"]:
		f.state=baseline.duplicate(true)
		var heir: Dictionary=f.state.actors[child]
		match condition:
			"busy": heir.action_id="action.work"
			"far": heir.location={"kind":"OnRoad"}
			"party": f.state.parties["party.test"]={"leader_id":child,"members":[child,f.other]}
			"battle": f.state.battles={"battle.test":{"phase":"planning","units":{child:{}}}}
			"contract": f.state.tasks["task.test"]={"assignee":child,"status":"ready"}
			"office": f.state.offices={"office.test":{"holder_actor_id":f.guardian}}
			"army": f.state.armies={"army.test":{"commander_actor_id":f.other,"member_actor_ids":[child]}}
			"capacity": Inventory.add(f.state,f.state.actors[f.guardian].inventory_id,"item.grain",100,{"kind":"actor","id":f.guardian})
			"reserved": f.state.wallets[f.state.actors[f.guardian].wallet_id].reserved["action.pending"]=1
			"foreign": Inventory.add(f.state,heir.inventory_id,"item.grain",1,{"kind":"actor","id":f.guardian})
			"elsewhere": Inventory.add(f.state,f.state.actors[f.other].inventory_id,"item.grain",1,{"kind":"actor","id":f.guardian})
			"dead": heir.alive=false
			"young": f.state.actors[f.guardian].birth_tick=-30*720
			"choice": f.state.family.designations.clear()
		var before: Dictionary=f.state.duplicate(true)
		check(not Family.apply(f.state,f.data,f.guardian,"family_retire",quote).ok and f.state==before,"旧报价不能绕过新出现的不满足条件："+condition)
	f.state=baseline.duplicate(true)
	Inventory.add(f.state,f.state.actors[f.guardian].inventory_id,"item.grain",1,{"kind":"actor","id":f.guardian})
	var before: Dictionary=f.state.duplicate(true)
	check(not Family.apply(f.state,f.data,f.guardian,"family_retire",quote).ok and f.state==before,"即使仍能装下，变化的资产也须重新预览确认具体来源")
	f.state=baseline.duplicate(true)
	var malformed: Dictionary=f.state.duplicate(true); malformed.family.children[child]="missing"
	check(not Family.validate_state(malformed,f.data).ok,"悬空收养来源不能产生继任资格")
	malformed=f.state.duplicate(true); malformed.family.successions=["missing"]
	check(not Family.validate_state(malformed,f.data).ok,"未发生的永久继任记录不能进入控制链")
	malformed=f.state.duplicate(true); malformed.facts[malformed.family.children[child]].terms.policy.heir_min_age=16
	check(not Family.validate_state(malformed,f.data).ok,"存档不能降低作者成年门槛")

## 尚未完工的庄园建设和未归部曲保持真实义务，不能借生前退隐绕过。
func check_estate_obligations() -> void:
	var f: Dictionary=fresh()
	var child: String=adopt(f)
	grow(f,child)
	designate(f,child)
	var id: String=add_estate(f)
	var manager: String=f.state.estates[id].manager_actor_id
	f.state.actors[manager].location=Factory.settlement_location(f.data,f.place)
	var params: Dictionary={"estate_id":id,"facility":"granary"}
	var quote: Dictionary=Estate.preview(f.state,f.data,manager,"estate_build",params)
	check(quote.ok,"实际管事可以在私庄开始建设")
	var action_id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":action_id,"kind":"estate_build","actor_id":manager,"participants":[manager],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":params}
	f.state.actions[action_id]=action; f.state.actors[manager].action_id=action_id
	check(Estate.begin(f.state,f.data,action,quote).ok,"管事建设按现有领域真实支付材料和费用")
	var blocked: Dictionary=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	check(not blocked.ok and blocked.code=="FamilyEstateConstructionPending","本人虽空闲，管事尚未完工的产权合同仍阻断交接")
	while action.remaining_ticks>0:
		check(Estate.step(f.state,f.data,f.state.tick+1).ok,"庄园建设期间仍逐半日结算生产与口粮")
		action.remaining_ticks-=1
		if action.remaining_ticks==0:
			check(Estate.finish(f.state,f.data,action).get("success",false),"建设真实完工后才能解除相关义务")
			f.state.actors[manager].action_id=""; f.state.actions.erase(action_id)
		check(advance(f,{},true).ok,"建设与家庭使用同一个世界日期")
	check(Family.preview(f.state,f.data,f.guardian,"family_retire",{}).ok,"建设完工后恢复合法交接预览")
	var source_id: String=Factory.next_id(f.state,"action")
	f.state.facts[source_id]={"kind":"military_muster","estate_id":id}
	quote=Estate.preview(f.state,f.data,f.guardian,"estate_muster",{"estate_id":id,"actor_ids":[manager],"source_id":source_id})
	check(quote.ok and Estate.apply(f.state,f.data,f.guardian,"estate_muster",quote).ok,"同一真人管事实际借调为部曲")
	blocked=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	check(not blocked.ok and blocked.code=="FamilyEstateRetainersAway","借调未归还期间不能把私人军事义务转嫁继任者")
	quote=Estate.preview(f.state,f.data,f.guardian,"estate_return",{"estate_id":id,"actor_ids":[manager],"source_id":source_id})
	check(quote.ok and Estate.apply(f.state,f.data,f.guardian,"estate_return",quote).ok and Family.preview(f.state,f.data,f.guardian,"family_retire",{}).ok,"真实归还部曲后恢复交接资格")

## 两次接续沿同一人物与世界时钟展开，证明控制链可超过一次替换且旧人物始终保留。
func check_multiple_generations() -> void:
	var f: Dictionary=fresh()
	var child: String=adopt(f)
	grow(f,child)
	designate(f,child)
	var quote: Dictionary=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	check(Family.apply(f.state,f.data,f.guardian,"family_retire",quote).ok,"首代生前退隐")
	grow(f,child,38)
	var grandchild: String=adopt(f,child)
	grow(f,grandchild)
	designate(f,grandchild,child)
	quote=Family.preview(f.state,f.data,child,"family_retire",{})
	var result: Dictionary=Family.apply(f.state,f.data,child,"family_retire",quote)
	check(result.ok and f.state.player_actor_id==grandchild and f.state.family.successions.size()==2,"第二次合法接续转给真实第三代，链从剧本原玩家连续展开")
	check(f.state.actors[f.guardian].alive and f.state.actors[child].alive and f.state.actors[grandchild].alive,"三代均作为独立真人同时存在")
	check(f.state.actors[grandchild].combat.learned.is_empty() and f.state.knowledge[grandchild].is_empty(),"第二次接续也不会累积复制历代全部掌握和见闻")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Family.validate_state(parsed.value,f.data).ok,"多代永久来源链可完整保存读回")
	var forged: Dictionary=f.state.duplicate(true); forged.family.successions.reverse()
	check(not Family.validate_state(forged,f.data).ok,"同样两条事实不能颠倒为非法控制链")

## 提供人允许用真实私庄钱柜收款；其后合法继任不能倒改已经完成的付款授权。
func check_historical_payment() -> void:
	var f: Dictionary=fresh()
	var child: String=adopt(f)
	grow(f,child)
	designate(f,child)
	var estate_id: String=add_estate(f)
	f.data.roles[f.second_role].family.wallet_id=f.data.estates[estate_id].wallet_id
	f.state.roles[f.second_role].actor_id=f.guardian
	var other_child: String=adopt(f,f.other,f.second_role)
	check(not other_child.is_empty(),"提供人用自己当前所有的钱柜收取另一家庭的实际收养费用")
	var quote: Dictionary=Family.preview(f.state,f.data,f.guardian,"family_retire",{})
	var result: Dictionary=Family.apply(f.state,f.data,f.guardian,"family_retire",quote)
	check(result.ok and Family.validate_state(f.state,f.data).ok,"收款钱柜继任后，原收养按发生时来源继续有效："+str(result))
	var fact: Dictionary=f.state.facts[f.state.family.children[other_child]]
	check(fact.terms.provider_actor_id==f.guardian and f.state.wallets[fact.terms.recipient_wallet_id].owner.id==child,"保留历史提供人，不把旧付款改写成后代办理")

## 汇总正常与失败路径，输出统一发布门禁可识别的领域结果。
func _run() -> void:
	check_adoption()
	check_invalid_adoption()
	check_succession()
	check_succession_rejections()
	check_estate_obligations()
	check_multiple_generations()
	check_historical_payment()
	print("FAMILY_RULES_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
