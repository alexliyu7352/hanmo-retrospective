## 当前玩家成长窗口的有限只读输入；不提供命令资格、不返回权威人物或历史容器。
extends RefCounted
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
const Armor=preload("res://core/domains/personal/personal_armor.gd")

## 沿原有效绘表域投影当前页；行囊提前返回，未知页沿武学分支，不增统一错误协议。
static func query(state: Dictionary, data: Dictionary, page: String="skills") -> Dictionary:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	var view: Dictionary={"actor_id":actor.id,"display_name":_person(state,data,actor.id),"place_id":Location.resolve(state,actor.id).get("place_id",""),"fatigue":actor.get("fatigue",0),"injury":actor.get("injury",0),"action":{}}
	if not action.is_empty(): view.action={"kind":action.kind,"skill_id":action.get("params",{}).get("training",{}).get("skill_id",""),"remaining_ticks":action.remaining_ticks}
	view.portrait_input=CharacterInput.query(state,data,actor.id)
	view.equipment=_equipment(state,data,actor)
	if page=="inventory":
		view.inventory=_inventory(state,data,actor)
		return view.duplicate(true)
	var learned: Dictionary=actor.get("combat",{}).get("learned",{})
	var ids: Array=[]
	for id in learned:
		if data.get("personal_skills",{}).has(id): ids.append(id)
	ids.sort()
	view.skills=[]; view.students=[]
	for id in ids: view.skills.append({"skill_id":id,"mastery":learned[id],"provenance_text":_provenance(state,data,id)})
	if not ids.is_empty(): view.students=_students(state,data)
	return view.duplicate(true)

## 本人正数量装备按类别投影；预留仍列出由正式报价拒绝，护具实际效果只读原规则。
static func _equipment(state: Dictionary, data: Dictionary, actor: Dictionary) -> Dictionary:
	var owned: Dictionary={}; var armors: Dictionary={}
	var inventory: Dictionary=state.get("inventories",{}).get(actor.get("inventory_id",""),{})
	for lot in inventory.get("lots",[]):
		if lot.owner=={"kind":"actor","id":actor.id} and lot.quantity>0 and data.get("items",{}).get(lot.item_id,{}).has("weapon"):
			owned[lot.item_id]=owned.get(lot.item_id,0)+lot.quantity
		if lot.owner=={"kind":"actor","id":actor.id} and lot.quantity>0 and data.get("items",{}).get(lot.item_id,{}).has("armor"):
			armors[lot.item_id]=armors.get(lot.item_id,0)+lot.quantity
	var ids: Array=owned.keys(); ids.sort()
	var options: Array=[]
	for id in ids: options.append({"item_id":id,"quantity":owned[id]})
	var armor_ids: Array=armors.keys(); armor_ids.sort()
	var armor_options: Array=[]
	for id in armor_ids: armor_options.append({"item_id":id,"quantity":armors[id]})
	return {"weapon_id":actor.get("combat",{}).get("weapon_id",""),"heart_id":actor.get("combat",{}).get("heart_id",""),"weapon_options":options,"armor_id":actor.get("combat",{}).get("armor_id",""),"armor_options":armor_options,"armor_effective":Armor.effective(state,data,actor.id)}

## 批次不合并或重排，重量包含公物与预留；只输出既有归属/预留文字及选中标识。
static func _inventory(state: Dictionary, data: Dictionary, actor: Dictionary) -> Dictionary:
	var inventory: Dictionary=state.inventories[actor.inventory_id]
	var result: Dictionary={"weight":Inventory.weight(inventory,data),"capacity":inventory.capacity,"lots":[]}
	for lot in inventory.lots:
		var ownership: String="本人财物"
		if lot.owner.kind=="task":
			var task: Dictionary=state.tasks[lot.owner.id]
			ownership="委托公物 · "+_record_name(data,data.offers[task.offer_id])
		elif lot.owner.kind=="organization": ownership="机构公物 · "+_name(data,"organizations",lot.owner.id,"所属机构")
		elif lot.owner.id!=actor.id: ownership="代管财物 · "+_person(state,data,lot.owner.id)
		var reservation: String="未预留"
		if not lot.reserved_by.is_empty():
			var action: Dictionary=state.actions.get(lot.reserved_by,{})
			var names: Dictionary={"report_offer":"委托交接","trade":"交易交割","army_form":"军团编组","army_supply":"军需补给","estate_build":"庄园营造"}
			reservation="已预留 · "+names.get(action.get("kind",""),"进行中的行动")
		result.lots.append({"lot_id":lot.id,"item_id":lot.item_id,"quantity":lot.quantity,"ownership_text":ownership,"reservation_text":reservation})
	return result

## 同地、存活、在岗人物按岗位ID排序去重；不提前过滤忙碌、可用性或教学资格。
static func _students(state: Dictionary, data: Dictionary) -> Array:
	var result: Array=[]
	var place: String=Location.resolve(state,state.player_actor_id).get("place_id","")
	var seen: Array=[]; var roles: Array=data.get("roles",{}).keys(); roles.sort()
	for id in roles:
		var role: Dictionary=data.roles[id]; var assignment: Dictionary=state.get("roles",{}).get(id,{})
		var person: String=assignment.get("actor_id","")
		if place.is_empty() or role.place_id!=place or not assignment.get("enabled",false) or person==state.player_actor_id or person in seen: continue
		if not state.actors.has(person) or not state.actors[person].alive or Location.resolve(state,person).get("place_id","")!=place: continue
		result.append({"display_name":_person(state,data,person),"context":Interaction.for_role(state,data,id)})
		seen.append(person)
	return result

## 保留原最早师承和最近传授算法；相同tick维持事实遍历先者，自修不虚构师门。
static func _provenance(state: Dictionary, data: Dictionary, skill_id: String) -> String:
	var origin: Dictionary={}; var taught: Dictionary={}
	for fact in state.get("facts",{}).values():
		if not fact is Dictionary or fact.get("kind")!="training" or fact.get("phase")!="completed" or not fact.get("success",false): continue
		var training: Dictionary=fact.get("training",{})
		if training.get("skill_id")!=skill_id or training.get("teacher_id","").is_empty(): continue
		if training.get("student_id")==state.player_actor_id and (origin.is_empty() or fact.completed_tick<origin.completed_tick): origin=fact
		if training.get("teacher_id")==state.player_actor_id and (taught.is_empty() or fact.completed_tick>taught.completed_tick): taught=fact
	var text: String="来源：未见传承记录"
	if not origin.is_empty(): text="师承：%s · %s"%[_person(state,data,origin.training.teacher_id),_name(data,"places",origin.training.place_id,"地点未记")]
	if not taught.is_empty(): text+="\n已传授：%s · %s"%[_person(state,data,taught.training.student_id),_name(data,"places",taught.training.place_id,"地点未记")]
	return text

## 姓名独立解析真实身份与原作者回退；坏外观只影响portrait_input，不使姓名消失。
static func _person(state: Dictionary, data: Dictionary, id: String) -> String:
	var actor: Dictionary=Identity.definition(state,data,id)
	if actor.has("display_name"): return actor.display_name
	return _name(data,"actors",id,"姓名未记")

## 沿原Growth引用回退，不补未知人物或场所定义。
static func _name(data: Dictionary, table: String, id: String, fallback: String) -> String:
	return _record_name(data,data[table][id]) if data.get(table,{}).has(id) else fallback

## 保持原Main显示名、文本键和ID回退表达式，领域查询不依赖UI宿主。
static func _record_name(data: Dictionary, record: Dictionary) -> String:
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))
