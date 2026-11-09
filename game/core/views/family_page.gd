## 当前玩家家事/休整页面的有限输入；不决定收养、继任、资产交接或自动推进资格。
extends RefCounted
const Identity=preload("res://core/actor_identity.gd")
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const CharacterInput=preload("res://core/views/character_input.gd")

## 仅在开窗时按作者原插入序选首岗位；保真实任职，不跳过失效或空岗位寻找可办理者。
static func provider_context(state: Dictionary, data: Dictionary) -> Dictionary:
	var place: String=Location.resolve(state,state.player_actor_id).get("place_id","")
	for role in data.roles.values():
		if role.place_id==place and "family_adopt" in role.services: return Interaction.for_role(state,data,role.id)
	return {}

## 沿旧有效绘表域只读取选中页；姓名与画像独立，原preview及UI开窗context不由此重选。
static func query(state: Dictionary, data: Dictionary, tab: String="family", provider_context: Dictionary={}) -> Dictionary:
	var id: String=state.player_actor_id; var actor: Dictionary=state.actors[id]
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	var view: Dictionary={"actor_id":id,"display_name":_person(state,data,id),"age_years":Identity.age_years(state,id),"place_id":Location.resolve(state,id).get("place_id",""),"hp":actor.get("hp",0),"fatigue":actor.get("fatigue",0),"portrait_input":CharacterInput.query(state,data,id),"guardian":{},"heir":{},"action":{}}
	var adoption: Dictionary=state.get("facts",{}).get(state.get("family",{}).get("children",{}).get(id,""),{})
	if not adoption.is_empty(): view.guardian={"display_name":_person(state,data,adoption.guardian_actor_id)}
	var heir: String=_heir(state,id)
	if not heir.is_empty(): view.heir={"actor_id":heir,"display_name":_person(state,data,heir)}
	if not action.is_empty(): view.action={"kind":action.kind,"remaining_ticks":action.remaining_ticks}
	if tab=="rest":
		view.estates=_rest_rows(state,data,action)
		return view.duplicate(true)
	view.children=[]; view.adoption={}
	var children: Array=[]
	for child in state.get("family",{}).get("children",{}):
		var fact: Dictionary=state.facts.get(state.family.children[child],{})
		if fact.get("kind")=="family_adoption" and fact.get("guardian_actor_id")==id and fact.get("success",false) and state.actors.has(child): children.append(child)
	children.sort()
	var here: Dictionary=Location.resolve(state,id)
	for child in children:
		var fact: Dictionary=state.facts[state.family.children[child]]
		var person: Dictionary=state.actors[child]; var location: Dictionary=Location.resolve(state,child)
		view.children.append({"actor_id":child,"display_name":_person(state,data,child),"age_years":Identity.age_years(state,child),"heir_min_age":fact.terms.policy.heir_min_age,"place_id":location.get("place_id",""),"together":here.get("kind")=="Settlement" and here==location,"alive":person.get("alive",false),"busy":not person.get("action_id","").is_empty(),"portrait_input":CharacterInput.query(state,data,child)})
	var role: Dictionary=data.get("roles",{}).get(provider_context.get("role_id",""),{})
	if not provider_context.is_empty() and "family_adopt" in role.get("services",[]):
		view.adoption={"pending":action.get("kind")=="family_adopt"}
		if not view.adoption.pending: view.adoption.merge({"context":provider_context.duplicate(true),"provider_display_name":_person(state,data,provider_context.get("provider_actor_id",""))})
	return view.duplicate(true)

## 私庄按原ID排序；活动休整匹配即停止该行历史读取，账本缺失也保原continue与非本地庄园可见性。
static func _rest_rows(state: Dictionary, data: Dictionary, action: Dictionary) -> Array:
	var ids: Array=[]; var rows: Array=[]
	for id in state.get("estates",{}):
		if state.estates[id].owner_actor_id==state.player_actor_id and data.get("estates",{}).has(id): ids.append(id)
	ids.sort()
	for id in ids:
		var row: Dictionary={"estate_id":id,"active_period":{},"active_ledger":{},"latest_completed":{}}
		if action.get("kind")=="long_rest" and action.params.estate_id==id:
			row.active_period={"duration":action.params.duration,"remaining_ticks":action.remaining_ticks}
			var fact: Dictionary=state.facts.get(state.actors[state.player_actor_id].action_id,{})
			if fact.get("kind")=="long_rest":
				row.active_ledger={"elapsed":fact.elapsed,"consumed":fact.consumed,"food_item_id":fact.food_item_id}
			rows.append(row); continue
		var latest: Dictionary={}
		for fact in state.get("facts",{}).values():
			if fact.get("kind")!="long_rest" or fact.get("phase")!="completed" or fact.get("actor_id")!=state.player_actor_id or fact.get("estate_id")!=id: continue
			if latest.is_empty() or fact.completed_tick>latest.completed_tick: latest=fact
		if not latest.is_empty(): row.latest_completed={"stop_reason":latest.stop_reason,"elapsed":latest.elapsed,"consumed":latest.consumed,"food_item_id":latest.food_item_id}
		rows.append(row)
	return rows

## 指定来源只沿原家庭索引，不提前重判继任资格或以同名人物替代。
static func _heir(state: Dictionary, guardian: String) -> String:
	var source: String=state.get("family",{}).get("designations",{}).get(guardian,"")
	return state.get("facts",{}).get(source,{}).get("heir_actor_id","")

## 家事姓名保原Identity后Main._name回退；未知可为空，不套用成长页文案或验证外观。
static func _person(state: Dictionary, data: Dictionary, id: String) -> String:
	var record: Dictionary=Identity.definition(state,data,id)
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))
