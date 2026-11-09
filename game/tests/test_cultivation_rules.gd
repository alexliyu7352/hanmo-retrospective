## 修炼领域回归：使用原世界工厂、实际钱包和库存，核验候选行动与保存数据，不冒充整局试玩。
extends SceneTree
const Cultivation=preload("res://core/cultivation_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎启动后执行领域行为测试，结果用明确退出码报告。
func _init() -> void:
	call_deferred("_run")

## 记录每个不满足的规则，不能吞掉失败来增加通过数量。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 为不同功法建立相同已支持类型的作者定义，不在核心特判测试技能ID。
func skill(id: String, kind: String, ticks: int) -> Dictionary:
	return {"id":id,"name_key":"name.actor.player","kind":kind,"weapons":["sword","spear","bow"],"stamina_cost":10,"power_bp":14000,"defense_bp":6000,"move_bonus":20,"status":"none","status_rounds":0,"practice_ticks":ticks,"description_key":"name.actor.player"}

## 从原内容和WorldFactory构建独立世界，只在夹具中补入尚未集成的成长初始值。
func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	var player: String="actor.player"
	var teacher: String=data.roles["role.office"].initial_actor
	var pupil: String=data.roles["role.carter"].initial_actor
	var place: String=data.roles["role.office"].place_id
	data.actors[player].initial_place=place
	data.actors[pupil].initial_place=place; data.roles["role.carter"].place_id=place
	data.roles["role.office"].services.append("learn_skill")
	data.personal_skills={"skill.cut":skill("skill.cut","strike",10),"skill.heart":skill("skill.heart","heart",25),"skill.guard":skill("skill.guard","guard",20)}
	data.items["item.training_sword"]={"id":"item.training_sword","name_key":"name.actor.player","kind":"commodity","weight":1,"base_price":100,"weapon":{"category":"sword","minimum_range":0,"maximum_range":150,"arc_degrees":100,"damage":20,"stamina_cost":10,"recovery_rounds":0,"move_distance":120}}
	var runtime_catalog: Dictionary=catalog.duplicate(true); runtime_catalog.data=data
	var state: Dictionary=Factory.create(runtime_catalog,"scenario.lived_city").state
	state.facts={}; state.battles={}
	for actor in state.actors.values():
		actor.injury=0; actor.combat={"weapon_id":"","learned":{},"heart_id":""}
	state.actors[teacher].combat.learned={"skill.cut":100,"skill.heart":100,"skill.guard":100}
	state.wallets[state.actors[player].wallet_id].balance=1000
	var payment_wallet: String=""
	for wallet in state.wallets.values():
		if wallet.owner=={"kind":"organization","id":data.roles["role.office"].organization_id}: payment_wallet=wallet.id; break
	data.roles["role.office"].lessons=[]
	for id in data.personal_skills:
		data.roles["role.office"].lessons.append({"skill_id":id,"fee":30,"wallet_id":payment_wallet,"study_ticks":2,"required_trust":40})
	for key in state.relations.keys():
		var edge: Dictionary=state.relations[key]
		if edge.from_actor==teacher and edge.to_actor==player: state.relations.erase(key)
	var relation_id: String=Factory.next_id(state,"relation")
	state.relations[relation_id]={"id":relation_id,"from_actor":teacher,"to_actor":player,"affinity":40,"trust":60}
	return {"state":state,"data":data,"player":player,"teacher":teacher,"pupil":pupil,"wallet":payment_wallet,"relation":relation_id,"context":Interaction.for_role(state,data,"role.office"),"pupil_context":Interaction.for_role(state,data,"role.carter")}

## 模拟Kernel已核验并占用人物的通用行动边界，开始效果仍由被测领域执行。
func start(f: Dictionary, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.player,kind,params)
	check(quote.ok,"行动可开始："+kind+" "+str(quote.get("code","")))
	if not quote.ok: return {}
	var participants: Array=[f.player]
	if quote.has("context"): participants.append(quote.context.provider_actor_id)
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":kind,"actor_id":f.player,"participants":participants,"remaining_ticks":quote.duration,"start_tick":f.state.tick,"params":params.duplicate(true)}
	f.state.actions[id]=action
	for person in participants: f.state.actors[person].action_id=id
	var begun: Dictionary=Cultivation.begin(f.state,f.data,action,quote)
	check(begun.ok,"开始实际提交条款："+str(begun))
	return action

## 以现有WorldStep的到期顺序推进候选半日；不引入另一套日期或自动补足成长。
func step(f: Dictionary, action: Dictionary) -> Dictionary:
	action.remaining_ticks-=1
	var result: Dictionary={"ok":true}
	if action.remaining_ticks==0:
		result=Cultivation.finish(f.state,f.data,action)
		check(result.ok,"到期结算有明确结果："+str(result))
		if result.ok:
			Inventory.release(f.state,action.id)
			for person in action.participants: f.state.actors[person].action_id=""
			f.state.actions.erase(action.id)
	f.state.tick+=1
	return result

## 装备只选择实际所有且未预留的随身物品，学习资格不依赖武器持有。
func check_equipment() -> void:
	var f: Dictionary=fresh()
	var params: Dictionary={"weapon_id":"item.training_sword"}
	check(not Cultivation.preview(f.state,f.data,f.player,"equip_weapon",params).ok,"不能凭空装备")
	var bag: String=f.state.actors[f.player].inventory_id
	Inventory.add(f.state,bag,"item.training_sword",1,{"kind":"actor","id":f.teacher})
	check(not Cultivation.preview(f.state,f.data,f.player,"equip_weapon",params).ok,"代管他人物品不能装备")
	Inventory.add(f.state,bag,"item.training_sword",1,{"kind":"actor","id":f.player},"action.other")
	check(not Cultivation.preview(f.state,f.data,f.player,"equip_weapon",params).ok,"被预留的物品不能装备")
	Inventory.release(f.state,"action.other")
	var before: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.player,"equip_weapon",params)
	check(quote.ok and f.state==before,"装备预览不修改任何世界字段")
	check(Cultivation.apply_instant(f.state,f.data,f.player,"equip_weapon",quote).ok,"装备真实随身武器")
	check(f.state.inventories==before.inventories and f.state.tick==0,"装备不复制库存或推进时间")
	check(Cultivation.apply_instant(f.state,f.data,f.player,"equip_weapon",{"weapon_id":""}).ok,"可卸下武器")
	check(not Cultivation.preview(f.state,f.data,f.player,"configure_heart",{"skill_id":"skill.heart"}).ok,"未学心法不能配置")
	f.state.actors[f.player].combat.learned["skill.heart"]=0
	check(Cultivation.apply_instant(f.state,f.data,f.player,"configure_heart",{"skill_id":"skill.heart"}).ok,"已学但未精通的心法可以配置")
	check(not Cultivation.preview(f.state,f.data,f.player,"configure_heart",{"skill_id":"skill.cut"}).ok,"攻击秘技不能伪装为心法")
	check(Cultivation.apply_instant(f.state,f.data,f.player,"configure_heart",{"skill_id":""}).ok,"可卸下心法而不遗忘")
	f.state.inventories[bag].lots=[]
	check(not Cultivation.apply_instant(f.state,f.data,f.player,"equip_weapon",quote).ok,"提交前失去武器使旧报价失效")

## 学习验证真实师承资格、单向信任、实际收费与完成时间。
func check_learning() -> void:
	var f: Dictionary=fresh()
	var params: Dictionary={"skill_id":"skill.cut","context":f.context}
	var before: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.player,"learn_skill",params)
	check(quote.ok and quote.duration==2 and quote.cost==30 and f.state==before,"无武器也可预览学习且不扣费")
	f.state.relations[f.relation].trust=0
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"教师不信任学习者时拒绝")
	f.state.relations["relation.reverse"]={"from_actor":f.player,"to_actor":f.teacher,"trust":100,"affinity":100}
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"学习者对教师的信任不能替代教师意愿")
	f.state.relations[f.relation].trust=60
	f.state.actors[f.teacher].combat.learned["skill.cut"]=79
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"未达80掌握的教师不能授业")
	f.state.actors[f.teacher].combat.learned["skill.cut"]=100
	f.state.wallets[f.state.actors[f.player].wallet_id].reserved={"action.other":980}
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"预留资金不算可付学费")
	f.state.wallets[f.state.actors[f.player].wallet_id].reserved={}
	var money: int=f.state.wallets[f.wallet].balance
	var action: Dictionary=start(f,"learn_skill",params)
	check(f.state.wallets[f.state.actors[f.player].wallet_id].balance==970 and f.state.wallets[f.wallet].balance==money+30,"学费从真实个人钱包转入授权钱包")
	check(not f.state.actors[f.player].combat.learned.has("skill.cut"),"开始学习不提前解锁")
	check(action.params.size()==2 and action.params.has("training") and not action.params.has("skill_id"),"存档使用冻结条款而非原始请求")
	check(Cultivation.validate_action(f.state,f.data,action).ok,"开始后稳定行动通过严格检查")
	before=f.state.duplicate(true)
	check(not Cultivation.begin(f.state,f.data,action,quote).ok and f.state==before,"重复开始由永久事实阻止且不重复收费")
	check(not Cultivation.finish(f.state,f.data,action).ok,"未到期不能提前结算")
	step(f,action)
	check(not f.state.actors[f.player].combat.learned.has("skill.cut") and Cultivation.validate_action(f.state,f.data,action).ok,"半程仍占用且没有本领奖励")
	var resumed: Dictionary=f.duplicate(true)
	resumed.state=Strict.parse_text(JSON.stringify(f.state)).value
	var restored_action: Dictionary=resumed.state.actions[action.id]
	check(Cultivation.validate_action(resumed.state,resumed.data,restored_action).ok,"行动与事实经过严格JSON往返仍可恢复")
	step(f,action); step(resumed,restored_action)
	check(f.state==resumed.state,"恢复后到期结果与未中断执行完全一致")
	check(f.state.actors[f.player].combat.learned["skill.cut"]==0 and f.state.tick==2,"学习到期仅解锁，掌握从0开始")
	check(f.state.actors[f.teacher].action_id=="" and f.state.actors[f.player].action_id=="","结束后双方占用可释放")
	f.state.results.clear(); f.state.history.clear()
	before=f.state.duplicate(true)
	check(Cultivation.finish(f.state,f.data,action).ok and f.state==before,"清空短缓存后重试仍不重复结算")
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"已学技能不能重复付费刷取")

## 不同配置的修炼都经过真实半日，疲劳减缓进步，传授只影响选定学生和本领。
func check_practice_and_teaching() -> void:
	var f: Dictionary=fresh()
	check(not Cultivation.preview(f.state,f.data,f.player,"practice_skill",{"skill_id":"skill.cut","ticks":2}).ok,"未学技能不可练习")
	f.state.actors[f.player].combat.learned={"skill.cut":0,"skill.guard":0,"skill.heart":0}
	for ticks in [0,9,-1,1.0,true]:
		check(not Cultivation.preview(f.state,f.data,f.player,"practice_skill",{"skill_id":"skill.cut","ticks":ticks}).ok,"拒绝非法修炼半日参数："+str(ticks))
	var action: Dictionary=start(f,"practice_skill",{"skill_id":"skill.cut","ticks":2})
	check(action.participants==[f.player],"独自修炼不占用教师")
	step(f,action)
	check(f.state.actors[f.player].combat.learned["skill.cut"]==0,"第一半日不能提前获得两半日成果")
	step(f,action)
	check(f.state.actors[f.player].combat.learned["skill.cut"]==20 and f.state.actors[f.player].fatigue==12,"两半日实际增加掌握及疲劳")
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.player,"practice_skill",{"skill_id":"skill.guard","ticks":2})
	check(quote.ok and quote.training.mastery_after==10,"第二功法使用不同修炼周期，无核心特判")
	action=start(f,"practice_skill",{"skill_id":"skill.guard","ticks":2})
	step(f,action); step(f,action)
	check(f.state.actors[f.player].combat.learned["skill.guard"]==10 and f.state.actors[f.player].combat.learned["skill.cut"]==20,"第二配置实际成长且不改变第一项技能")
	f.state.actors[f.player].fatigue=60
	quote=Cultivation.preview(f.state,f.data,f.player,"practice_skill",{"skill_id":"skill.cut","ticks":2})
	check(quote.ok and quote.training.mastery_after==30,"疲劳降低本次修炼收益")
	f.state.actors[f.player].fatigue=0; f.state.actors[f.player].combat.learned["skill.cut"]=80
	var params: Dictionary={"skill_id":"skill.cut","context":f.pupil_context}
	var before_money: Dictionary=f.state.wallets.duplicate(true)
	action=start(f,"teach_skill",params)
	check(action.participants==[f.player,f.pupil] and action.remaining_ticks==2,"传授同时占用真实师生两半日")
	check(not f.state.actors[f.pupil].combat.learned.has("skill.cut"),"传授开始不复制本领")
	step(f,action); step(f,action)
	check(f.state.actors[f.pupil].combat.learned=={"skill.cut":10},"学生仅获得所传技能的部分掌握")
	check(f.state.wallets==before_money and f.state.actors[f.player].combat.learned["skill.heart"]==0,"传授不创造学费或复制其他技能")
	f.state.actors[f.pupil].combat.learned["skill.cut"]=75
	quote=Cultivation.preview(f.state,f.data,f.player,"teach_skill",params)
	check(quote.ok and quote.training.mastery_after==80,"学生不能超过当前教师掌握")
	f.state.actors[f.player].combat.learned["skill.cut"]=79
	check(not Cultivation.preview(f.state,f.data,f.player,"teach_skill",params).ok,"玩家也遵守相同授业门槛")

## 错误位置、忙碌、重伤和恶意请求不能越过人物与资产边界。
func check_rejections() -> void:
	var f: Dictionary=fresh()
	var params: Dictionary={"skill_id":"skill.cut","context":f.context}
	for context in [null,12,"bad",{"kind":"encounter"}]:
		check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",{"skill_id":"skill.cut","context":context}).ok,"拒绝非法或道路教学上下文")
	f.state.actors[f.teacher].action_id="action.other"
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"不能占用忙碌教师")
	f.state.actors[f.teacher].action_id=""
	f.state.actors[f.player].injury=3
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"重伤需先休养")
	f.state.actors[f.player].injury=0
	f.state.battles["battle.active"]={"phase":"planning","units":{f.player:{}}}
	check(not Cultivation.preview(f.state,f.data,f.player,"configure_heart",{"skill_id":""}).ok,"参战时不能从生活入口改变配置")
	f.state.battles={}
	f.data.roles["role.office"].lessons[0].wallet_id=f.state.actors[f.player].wallet_id
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"教师不能用别人的钱包收款")
	f.data.roles["role.office"].lessons[0].wallet_id=f.wallet
	f.state.wallets[f.wallet].balance=2147483647
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",params).ok,"入账溢出在预览时被拒绝")
	f.state.wallets[f.wallet].balance=100
	var action: Dictionary=start(f,"learn_skill",params)
	f.state.actors[f.teacher].alive=false
	step(f,action)
	var result: Dictionary=step(f,action)
	check(result.ok and not result.success and not f.state.actors[f.player].combat.learned.has("skill.cut"),"教师无法授课时正常中断而非卡住全局半日")
	check(f.state.facts[action.id].phase=="completed" and not f.state.facts[action.id].success,"中断也有永久结算事实")

## 保存检查拒绝被改写的条款、成长、人员和时间，不借校验重放学习。
func check_save_integrity() -> void:
	var f: Dictionary=fresh()
	var action: Dictionary=start(f,"learn_skill",{"skill_id":"skill.cut","context":f.context})
	var original: Dictionary=f.state.duplicate(true)
	for mutation in ["fee","mastery","duration","participant","fact","clock","extra"]:
		var state: Dictionary=original.duplicate(true)
		var copy: Dictionary=state.actions[action.id]
		match mutation:
			"fee": copy.params.training.fee=999; state.facts[action.id].training.fee=999
			"mastery": copy.params.training.mastery_after=100; state.facts[action.id].training.mastery_after=100
			"duration": copy.remaining_ticks=3
			"participant": copy.participants=[f.player]
			"fact": state.facts.erase(action.id)
			"clock": state.tick+=1
			"extra": copy.params.training.extra=1; state.facts[action.id].training.extra=1
		var before: Dictionary=state.duplicate(true)
		check(not Cultivation.validate_action(state,f.data,copy).ok and state==before,"拒绝坏教学存档且不修改内容："+mutation)
	check(Cultivation.validate_action(f.state,f.data,action).ok and f.state==original,"正常保存检查保持所有世界状态不变")

## 岗位换任使用当前人物，私人授课收款与过期报价都遵守同一权属边界。
func check_provider_identity() -> void:
	var f: Dictionary=fresh()
	f.state.roles["role.office"].actor_id=f.pupil
	f.state.actors[f.pupil].combat.learned["skill.cut"]=100
	f.data.roles["role.office"].lessons[0].required_trust=0
	check(not Cultivation.preview(f.state,f.data,f.player,"learn_skill",{"skill_id":"skill.cut","context":f.context}).ok,"换任后原教师上下文失效")
	f.context=Interaction.for_role(f.state,f.data,"role.office")
	check(Cultivation.preview(f.state,f.data,f.player,"learn_skill",{"skill_id":"skill.cut","context":f.context}).ok,"实际换任人物可按组织授权授课")
	f.data.roles["role.office"].organization_id=null
	var recipient: String=f.state.actors[f.pupil].wallet_id
	f.data.roles["role.office"].lessons[0].wallet_id=recipient
	var money: int=f.state.wallets[recipient].balance
	var action: Dictionary=start(f,"learn_skill",{"skill_id":"skill.cut","context":f.context})
	check(f.state.wallets[recipient].balance==money+30,"私人教师收取自己的实际学费")
	step(f,action); step(f,action)
	check(f.state.actors[f.player].combat.learned["skill.cut"]==0,"私人授课也有正常学习终点")
	f=fresh()
	var params: Dictionary={"skill_id":"skill.cut","context":f.context}
	var quote: Dictionary=Cultivation.preview(f.state,f.data,f.player,"learn_skill",params)
	f.data.roles["role.office"].lessons[0].fee=31
	var id: String=Factory.next_id(f.state,"action")
	action={"id":id,"kind":"learn_skill","actor_id":f.player,"participants":[f.player,f.teacher],"remaining_ticks":2,"start_tick":f.state.tick,"params":params}
	f.state.actions[id]=action
	f.state.actors[f.player].action_id=id; f.state.actors[f.teacher].action_id=id
	var before: Dictionary=f.state.duplicate(true)
	check(not Cultivation.begin(f.state,f.data,action,quote).ok and f.state==before,"收费条款变化使旧报价失效，领域提交不扣款或留下事实")

## 执行独立领域检查并保留失败退出码；Kernel和场景链由主工程继续集成验收。
func _run() -> void:
	check_equipment()
	check_learning()
	check_practice_and_teaching()
	check_rejections()
	check_save_integrity()
	check_provider_identity()
	print("CULTIVATION_RULES_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
