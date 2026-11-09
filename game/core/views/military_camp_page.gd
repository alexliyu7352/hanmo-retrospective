## 军营四页与即时报价所需的有限来源；不裁决任官、军令、钱粮、训练或出征权限。
extends RefCounted
const Identity=preload("res://core/actor_identity.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Location=preload("res://core/world_location.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const CharacterInput=preload("res://core/views/character_input.gd")

## 开窗按作者公职原序选首岗位，不跳过失效、空缺或不在场者寻找可办理人。
static func provider_context(state: Dictionary, data: Dictionary) -> Dictionary:
	var place: String=Location.resolve(state,state.player_actor_id).get("place_id","")
	for office in data.get("offices",{}).values():
		var role: Dictionary=data.roles[office.appointing_role_id]
		if role.place_id==place: return Interaction.for_role(state,data,role.id)
	return {}

## 只读取当前页与原显示分支；选项回落不写UI草案，返回值不携带世界容器引用。
static func query(state: Dictionary, data: Dictionary, tab: String="orders", selection: Dictionary={}, provider_context: Dictionary={}) -> Dictionary:
	var actor: String=state.player_actor_id; var here: Dictionary=Location.resolve(state,actor)
	var view: Dictionary={"actor_id":actor,"display_name":_person(state,data,actor),"portrait_input":CharacterInput.query(state,data,actor),"location_kind":here.get("kind",""),"place_id":here.get("place_id",""),"office_ids":[],"own_army_count":_own_armies(state,actor).size(),"entry_provider":_provider(state,data,provider_context),"last_result":{},"action":{}}
	for office in state.get("offices",{}).values():
		if office.enabled and office.holder_actor_id==actor: view.office_ids.append(office.id)
	var latest: Dictionary={}
	for fact in state.facts.values():
		if fact.get("kind")!="military_action" or fact.get("actor_id")!=actor or fact.get("phase")!="completed": continue
		if latest.is_empty() or fact.completed_tick>latest.completed_tick: latest=fact
	if not latest.is_empty(): view.last_result={"command":latest.command,"success":latest.success}
	var action: Dictionary=state.actions.get(state.actors[actor].action_id,{})
	if not action.is_empty(): view.action={"remaining_ticks":action.remaining_ticks}
	match tab:
		"training": view.training=training_rows(state,data)
		"formation": view.merge(_formation(state,data,selection))
		"supply": view.merge(_supply(state,data,selection))
		_: view.merge(_orders(state,data,selection))
	return view.duplicate(true)

## 原训练候选按现任公职与单位嵌套原序，重复行和无档案行均保留给正式preview。
static func training_rows(state: Dictionary, data: Dictionary) -> Array:
	var actor: String=state.player_actor_id; var here: String=Location.resolve(state,actor).get("place_id",""); var rows: Array=[]
	for office in state.get("offices",{}).values():
		if not office.enabled or office.holder_actor_id!=actor: continue
		var definition: Dictionary=data.offices[office.id]; var camp: Dictionary=data.garrisons[definition.garrison_id]
		if camp.place_id!=here or not camp.has("training_wallet_id"): continue
		for unit in state.get("military_units",{}).values():
			if unit.garrison_id!=camp.id or unit.organization_id!=definition.organization_id: continue
			var row: Dictionary={"actor_id":actor,"office_id":office.id,"garrison_id":camp.id,"unit_id":unit.id,"fit_count":unit.fit.size()}
			if unit.has("training"): row.profile={"training":unit.training,"fatigue":unit.fatigue}
			rows.append(row)
	return rows.duplicate(true)

## 首出发报价成功后才读取；已到目标保原早退，不提前索取统帅字段。
static func departure_source(state: Dictionary, data: Dictionary, campaign_id: String) -> Dictionary:
	var campaign: Dictionary=state.campaigns[campaign_id]
	var definition: Dictionary=data.military_orders[campaign.template_id]
	var destination: String=data.strongholds[definition.target_stronghold_id].settlement_id
	if Campaign.at_settlement(state,data,campaign.army_id,destination): return {"at_target":true,"destination":destination,"commander_actor_id":""}
	return {"at_target":false,"destination":destination,"commander_actor_id":state.armies[campaign.army_id].commander_actor_id}

## 成功报价参与者沿原顺序及重复取名，缺人保无人，姓名不要求外观验证。
static func participant_names(state: Dictionary, data: Dictionary, actor_ids: Array) -> Array:
	var result: Array=[]
	for id in actor_ids: result.append(_person(state,data,id))
	return result

## 仅供原NotAtCityExit分支读取真实城口，不新增坏状态守卫或其它失败协议。
static func departure_guide_place(state: Dictionary, data: Dictionary) -> String:
	var here: Dictionary=Location.resolve(state,state.player_actor_id)
	return data.settlements[here.settlement_id].entry_place

## 任官与军令保原显示筛选，真正授权及目标可交付性仍由每个按钮的即时预览核验。
static func _orders(state: Dictionary, data: Dictionary, selection: Dictionary) -> Dictionary:
	var active: Array=_campaigns(state); var result: Dictionary={"campaigns":[],"office_rows":[],"order_rows":[]}
	for campaign in active:
		var definition: Dictionary=data.military_orders[campaign.template_id]
		var role: String=definition.report_role_id if campaign.status=="awaiting_report" else definition.issuer_role_id
		result.campaigns.append({"campaign_id":campaign.id,"template_id":campaign.template_id,"status":campaign.status,"deadline_remaining":maxi(0,campaign.deadline_tick-state.tick),"provider":_provider(state,data,Interaction.for_role(state,data,role))})
	for id in data.get("offices",{}):
		var office: Dictionary=data.offices[id]; var current: Dictionary=state.offices.get(id,{})
		if not active.is_empty() and current.get("holder_actor_id")!=state.player_actor_id: continue
		if data.roles[office.appointing_role_id].place_id!=Location.resolve(state,state.player_actor_id).get("place_id") and current.get("holder_actor_id")!=state.player_actor_id: continue
		result.office_rows.append({"office_id":id,"own":current.get("enabled",false) and current.get("holder_actor_id")==state.player_actor_id,"provider":_provider(state,data,Interaction.for_role(state,data,office.appointing_role_id))})
	if not active.is_empty(): return result
	for definition in data.get("military_orders",{}).values():
		if data.roles[definition.issuer_role_id].place_id!=Location.resolve(state,state.player_actor_id).get("place_id"): continue
		var row: Dictionary={"template_id":definition.id,"provider":_provider(state,data,Interaction.for_role(state,data,definition.issuer_role_id)),"recipient_army_choices":[],"recipient_army_id":""}
		if definition.objective.get("recipient","garrison")=="army":
			var organization: String=definition.objective.get("recipient_organization_id",data.offices[definition.office_id].organization_id)
			for army in state.armies.values():
				if army.status=="active" and army.organization_id==organization: row.recipient_army_choices.append({"id":army.id,"label":_army_name(state,data,army)})
			row.recipient_army_id=_selected(row.recipient_army_choices,selection.get("recipients",{}),definition.id)
		result.order_rows.append(row)
	return result

## 编组只展开已选受令行；没有待编组军令时仍展示真实既有进展，不读无关库房或私庄。
static func _formation(state: Dictionary, data: Dictionary, selection: Dictionary) -> Dictionary:
	var campaigns: Array=_campaigns(state); var result: Dictionary={"campaign_choices":[],"selected_order_id":"","progress":[],"formation":{}}
	for campaign in campaigns:
		if campaign.status=="accepted": result.campaign_choices.append({"id":campaign.id,"label":_name(data,"military_orders",campaign.template_id)})
	result.selected_order_id=_selected(result.campaign_choices,selection,"formation_order_id")
	if result.selected_order_id.is_empty():
		for campaign in campaigns:
			var row: Dictionary={"campaign_id":campaign.id,"template_id":campaign.template_id,"status":campaign.status,"active_army":{}}
			var army: Dictionary=state.get("armies",{}).get(campaign.army_id,{})
			if army.get("status")=="active":
				var units: Array=[]
				for id in army.unit_ids: units.append(_unit(state,data,state.military_units[id],false))
				row.active_army={"display_name":_army_name(state,data,army),"wallet_balance":state.wallets[army.wallet_id].balance,"units":units,"supply_quote":_supply_quote(state,data,army.id)}
			result.progress.append(row)
		return result
	var campaign: Dictionary=state.campaigns[result.selected_order_id]; var camp: Dictionary=data.garrisons[campaign.garrison_id]
	var details: Dictionary={"order_id":campaign.id,"garrison_id":camp.id,"single":data.offices[campaign.office_id].command_scope=="unit","ledger":_ledger(state,data,camp.id),"units":[],"pledged":[],"pledge_estate_choices":[],"selected_pledge_estate_id":"","pledge_residents":[]}
	for id in Assets.stationed_units(state,data,camp.id):
		var unit: Dictionary=state.military_units[id]; var row: Dictionary=_unit(state,data,unit,false)
		row.blocked=not unit.army_id.is_empty() or not unit.mobilizing_action_id.is_empty() or unit.fit.is_empty(); details.units.append(row)
	for estate in state.get("estates",{}).values():
		for person in estate.detached:
			var fact: Dictionary=state.facts.get(estate.detached[person],{})
			if fact.get("kind")=="military_muster" and fact.get("order_id")==campaign.id: details.pledged.append({"actor_id":person,"display_name":_person(state,data,person)})
	for estate in state.get("estates",{}).values():
		if state.player_actor_id in [estate.owner_actor_id,estate.manager_actor_id]: details.pledge_estate_choices.append({"id":estate.id,"label":_name(data,"estates",estate.id)})
	details.selected_pledge_estate_id=_selected(details.pledge_estate_choices,selection,"pledge_estate_id")
	if not details.selected_pledge_estate_id.is_empty():
		var estate: Dictionary=state.estates[details.selected_pledge_estate_id]
		for person in estate.resident_actor_ids:
			if person!=state.player_actor_id: details.pledge_residents.append({"actor_id":person,"display_name":_person(state,data,person),"detached":estate.detached.has(person)})
	result.formation=details
	return result

## 补给选择保原所有作者营地与默认驻营；归庄回执不去重，成功后去重仍由原预览循环决定。
static func _supply(state: Dictionary, data: Dictionary, selection: Dictionary) -> Dictionary:
	var result: Dictionary={"return_candidates":[],"army_choices":[],"selected_army_id":"","garrison_choices":[],"selected_garrison_id":"","supply_army":{},"ledger":{}}
	for id in state.facts:
		var fact: Dictionary=state.facts[id]
		if fact.get("kind")!="military_return": continue
		for person in fact.returned_retainers: result.return_candidates.append({"return_receipt_id":id,"actor_id":person,"display_name":_person(state,data,person)})
	for army in _own_armies(state,state.player_actor_id): result.army_choices.append({"id":army.id,"label":_army_name(state,data,army)})
	result.selected_army_id=_selected(result.army_choices,selection,"supply_army_id")
	if result.selected_army_id.is_empty(): return result
	var army: Dictionary=state.armies[result.selected_army_id]
	for camp in data.garrisons.values(): result.garrison_choices.append({"id":camp.id,"label":_name(data,"garrisons",camp.id)})
	var selected: Dictionary=selection if selection.get("supply_garrison_army_id","")==army.id else {}
	result.selected_garrison_id=_selected(result.garrison_choices,selected,"supply_garrison_id",army.garrison_id)
	if result.selected_garrison_id.is_empty(): return result
	result.ledger=_ledger(state,data,result.selected_garrison_id)
	var bag: Dictionary=state.inventories[army.inventory_id]; var units: Array=[]
	for id in army.unit_ids: units.append(_unit(state,data,state.military_units[id],true))
	result.supply_army={"army_id":army.id,"commander_display_name":_person(state,data,army.commander_actor_id),"wallet_balance":state.wallets[army.wallet_id].balance,"weight":Inventory.weight(bag,data),"capacity":bag.capacity,"units":units,"supply_quote":_supply_quote(state,data,army.id)}
	return result

## 下拉候选只有此处回落；未给值可用原默认，显式空或失效值回首项，不写选择。
static func _selected(choices: Array, selection: Dictionary, key: String, preferred: String="") -> String:
	if choices.is_empty(): return ""
	var value: String=selection.get(key,preferred)
	for row in choices:
		if row.id==value: return value
	return choices[0].id

## 当前公库分别读取真实owner、可用钱与该owner未预留粮，不重算产权或追加同主守卫。
static func _ledger(state: Dictionary, data: Dictionary, id: String) -> Dictionary:
	var camp: Dictionary=data.garrisons[id]; var bag: Dictionary=state.inventories[camp.inventory_id]
	return {"garrison_id":id,"inventory_owner_id":bag.owner.id,"spendable_money":Inventory.spendable(state.wallets[camp.wallet_id]),"food_quantity":Inventory.count(bag,camp.food_item_id,[bag.owner])}

## 只给本人统领者显示原供给查询，失败由原render_quote隐去，不在投影重算口粮。
static func _supply_quote(state: Dictionary, data: Dictionary, army_id: String) -> Dictionary:
	if state.get("armies",{}).get(army_id,{}).get("commander_actor_id")!=state.player_actor_id: return {}
	return Assets.supply_preview(state,data,army_id)

## 单位只给此页所需人数和标题，源名册与装备不离开后端。
static func _unit(state: Dictionary, data: Dictionary, unit: Dictionary, losses: bool) -> Dictionary:
	var row: Dictionary={"unit_id":unit.id,"display_name":_unit_name(state,data,unit),"fit_count":unit.fit.size(),"wounded_count":unit.wounded.size()}
	if losses: row.merge({"dead_count":unit.dead.size(),"captured_count":unit.captured.size(),"deserted_count":unit.deserted.size()})
	return row

## 岗位上下文保原空分支，人物是否在场只比较原place_id，不增加存活或可办条件。
static func _provider(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	if context.is_empty(): return {}
	return {"context":context.duplicate(true),"display_name":_person(state,data,context.provider_actor_id),"place_id":context.place_id,"at_place":Location.resolve(state,context.provider_actor_id).get("place_id")==context.place_id}

## 本人军令只排除原closed阶段，保持运行插入序。
static func _campaigns(state: Dictionary) -> Array:
	var result: Array=[]
	for campaign in state.get("campaigns",{}).values():
		if campaign.actor_id==state.player_actor_id and campaign.status!="closed": result.append(campaign)
	return result

## 补给和侧栏仅统计本人当前活动军团，保原顺序。
static func _own_armies(state: Dictionary, actor: String) -> Array:
	var result: Array=[]
	for army in state.get("armies",{}).values():
		if army.status=="active" and army.commander_actor_id==actor: result.append(army)
	return result

## 部队先用作者来源标题，否则兵种和实际军官姓名保持原拼接。
static func _unit_name(state: Dictionary, data: Dictionary, unit: Dictionary) -> String:
	for entry in data.garrisons[unit.source_garrison_id].units:
		if entry.id==unit.id and entry.has("name_key"): return entry.get("display_name",data.texts.get(entry.get("name_key",entry.get("title_key","")),entry.get("id","")))
	return _name(data,"troop_types",unit.troop_type_id)+" · "+_person(state,data,unit.commander_actor_id)

## 军团标题沿实际统帅与编制数量，不新增虚构军名。
static func _army_name(state: Dictionary, data: Dictionary, army: Dictionary) -> String:
	return _person(state,data,army.commander_actor_id)+"所部 · %d部"%army.unit_ids.size()

## 只取Identity定义与原Main命名回退，不让姓名依赖外观验证。
static func _person(state: Dictionary, data: Dictionary, id: String) -> String:
	if not state.actors.has(id): return "无人"
	var record: Dictionary=Identity.definition(state,data,id)
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))

## 作者定义缺失保原尚未指定，存在时使用原显示名和文本键回退。
static func _name(data: Dictionary, table: String, id: String) -> String:
	var record: Dictionary=data.get(table,{}).get(id,{})
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id",""))) if not record.is_empty() else "尚未指定"
