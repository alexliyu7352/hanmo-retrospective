## 舟师营务只保存本页选船草案；人货仍引用真实军团，报价与提交走同一Kernel。
extends RefCounted

## 来源代次或军团变化即丢弃旧草案，不把跨局选择套到同名人物。
static func draft_for(view: Dictionary, previous: Dictionary) -> Dictionary:
	if previous.get("source",{})==view.get("source",{}) and previous.get("army_id","")==view.get("army_id",""):
		return previous.duplicate(true)
	return {"source":view.get("source",{}).duplicate(true),"army_id":view.get("army_id",""),"choices":{}}

## 只由当前实际名册构造完整意图；未知船或漏装人员返回空草案，不能补造单位。
static func formation(view: Dictionary, choices: Dictionary) -> Dictionary:
	if not view.get("can_command",false) or not view.get("fleet_id","").is_empty(): return {}
	var ships: Array=[]
	for ship in view.get("ships",[]):
		if ship.get("available",false): ships.append(ship.id)
	var units: Array=view.get("units",[])
	if units.is_empty() or ships.size()<units.size(): return {}
	var assignments: Array=[]
	for i in units.size():
		var vessel: String=choices.get("unit:"+units[i].id,ships[i])
		if not vessel in ships: return {}
		assignments.append({"vessel_id":vessel,"unit_id":units[i].id,"passenger_ids":[],"prisoner_ids":[],"extra_inventory_ids":[]})
	for pair in [["passengers","passenger_ids"],["prisoners","prisoner_ids"]]:
		for person in view.get(pair[0],[]):
			var vessel: String=choices.get(pair[0]+":"+person.id,assignments[0].vessel_id)
			var found: bool=false
			for binding in assignments:
				if binding.vessel_id==vessel and not found:
					binding[pair[1]].append(person.id);found=true
			if not found: return {}
	var cargo: String=choices.get("cargo",assignments[0].vessel_id)
	var assigned: bool=false
	for binding in assignments:
		if binding.vessel_id==cargo and not assigned:
			binding.extra_inventory_ids.append(view.get("army_inventory_id",""));assigned=true
	if not assigned: return {}
	return {"request_key":request_key(view,"form"),"army_id":view.army_id,"assignments":assignments}

## 幂等键绑定本次源身份；失败可重试，成功后新revision才会产生另一次新意图。
static func request_key(view: Dictionary, intent: String) -> String:
	var source: Dictionary=view.get("source",{})
	return "naval:%s:%s:%s:%s"%[source.get("world_id",""),source.get("execution_epoch",0),source.get("revision",0),intent]

## 每次只显示有限投影，不读取远方舰队或私人行囊内容。
static func render(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"舟师与实船",23)
	host._text(parent,"编队不另花半日；出发仍走原军令与行旅。人员行囊和军资各保原产权。",16)
	if view.get("limited",false): host._text(parent,"当前名册超过本页容量，请缩小编组后再办理。",17)
	for ship in view.get("ships",[]):
		var name: String=host._name({"name_key":ship.name_key})
		host._text(parent,"%s · 船体%d/%d · 载人%d · 载货%d"%[name,ship.integrity,ship.max_integrity,ship.person_capacity,ship.cargo_capacity],18)
		if ship.has("load"):
			host._text(parent,"现载%d人（可工作%d） · 货重%d"%[ship.load.people,ship.load.working_crew,ship.load.cargo_weight],16)
	if view.get("ships",[]).is_empty(): host._text(parent,"此处没有本人可用的实船。",18)
	if view.has("navigation"): _navigation(host,parent,view.navigation)
	if not view.get("blocked_code","").is_empty(): host._text(parent,reason(view.blocked_code),16)
	for relief in view.get("relief",[]):
		_command(host,parent,"按原返营权源代办泊地解编 "+relief.fleet_id,"naval_disband",{"request_key":request_key(view,"relief:"+relief.fleet_id),"fleet_id":relief.fleet_id},view)
	if not view.get("can_command",false):
		host._text(parent,"随军只读舟师现况；编队由本军统帅办理。",17)
		return
	if view.has("repairs"): _repairs(host,parent,view)
	if not view.get("fleet_id","").is_empty():
		_command(host,parent,"在此泊地解除编队","naval_disband",{"request_key":request_key(view,"disband"),"fleet_id":view.fleet_id},view)
	else:
		var draft: Dictionary=draft_for(view,host.get_meta("naval_camp_draft",{}))
		host.set_meta("naval_camp_draft",draft)
		var ships: Array=[]
		for ship in view.get("ships",[]):
			if ship.get("available",false): ships.append({"id":ship.id,"label":host._name({"name_key":ship.name_key})})
		for i in view.get("units",[]).size():
			var unit: Dictionary=view.units[i]
			_choice(host,parent,"本部 %s（可战%d / 伤%d）"%[unit.id,unit.fit_count,unit.wounded_count],"unit:"+unit.id,ships,ships[i].id if i<ships.size() else "",draft,view)
		var params: Dictionary=formation(view,draft.choices)
		var aboard: Array=[]
		for binding in params.get("assignments",[]):
			for ship in ships:
				if ship.id==binding.vessel_id: aboard.append(ship)
		for category in ["passengers","prisoners"]:
			for person in view.get(category,[]):
				_choice(host,parent,("随军成员 " if category=="passengers" else "在押人员 ")+person.id,category+":"+person.id,aboard,aboard[0].id if not aboard.is_empty() else "",draft,view)
		_choice(host,parent,"原军资容器", "cargo",aboard,aboard[0].id if not aboard.is_empty() else "",draft,view)
		params=formation(view,draft.choices)
		if params.is_empty(): host._text(parent,"需要每部一艘可用船，并为全体人员和原军资选定载船。",17)
		else: _command(host,parent,"核准当前编队","naval_form",params,view)
	for receipt in view.get("history",[]):
		host._text(parent,"%s · %s · %s"%[receipt.fleet_id,"已解编" if receipt.status=="released" else "编队中",receipt.source_id],15)

## 只展示原生逐船航行事实，不计现实时长、不改修缮费用或行旅报价。
static func _navigation(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"航行与船体保养",21)
	host._text(parent,"每实际航行%d半日磨损%d船体；暂停、战斗停航不累计。"%[view.wear_interval,view.wear_damage],16)
	host._text(parent,"新计划达到%d半日视为远航，启程船体须不少于%d；已有行程保原出航条款。"%[view.long_voyage_min_half_days,view.long_voyage_min_integrity],16)
	for vessel in view.get("vessels",[]):
		host._text(parent,"%s · 已实航%d半日 · 再航%d半日发生下一次磨损"%[vessel.vessel_id,vessel.completed_half_days,vessel.until_next_wear],16)

## 下拉只改当前来源草案；旧控件失效后不能污染新军团或新世界。
static func _choice(host, parent: Control, label: String, key: String, options: Array, fallback: String, draft: Dictionary, view: Dictionary) -> void:
	if options.is_empty(): return
	host._text(parent,label,16)
	var control=OptionButton.new();control.name="NavalChoice_"+key;parent.add_child(control)
	var selected: String=draft.choices.get(key,fallback)
	for option in options:
		control.add_item(option.label);control.set_item_metadata(control.item_count-1,option.id)
		if option.id==selected: control.select(control.item_count-1)
	var kernel=host.kernel
	control.item_selected.connect(func(index: int):
		if not is_instance_valid(control) or not control.is_inside_tree() or host.kernel!=kernel: return
		var current: Dictionary=host.get_meta("naval_camp_draft",{})
		if current.get("source",{})!=view.source or current.get("army_id","")!=view.army_id: return
		current.choices[key]=control.get_item_metadata(index);host.set_meta("naval_camp_draft",current);host._render())

## 原正式报价决定是否可点；点击前再核同页来源，不让旧按钮代替新一局下令。
static func _command(host, parent: Control, label: String, kind: String, params: Dictionary, view: Dictionary) -> void:
	var button=Button.new();button.text=label;button.name="NavalCommand_"+kind;parent.add_child(button)
	var offer: Dictionary=host.kernel.military_camp_offer(kind,params)
	var quote: Dictionary=offer.get("quote",{})
	button.disabled=host._advancing or not offer.get("ok",true) or not quote.get("ok",false)
	var code: String=offer.get("code",quote.get("code",""))
	if button.disabled: host._text(parent,"正在核对当前军务…" if code=="NativeAsyncRequired" else reason(code),16)
	for load in quote.get("naval",{}).get("loads",[]):
		host._text(parent,"%s：%d人 / 货重%d"%[load.vessel_id,load.people,load.cargo_weight],16)
	var kernel=host.kernel
	if kind in ["naval_repair_start","naval_repair_cancel"]:
		button.set_meta("naval_repair_context",{"actor_id":view.get("actor_id",""),"scene":host._scene_counter,"load":int(host.get_meta("personal_battle_generation",0))})
	button.pressed.connect(_send.bind(host,button,kernel,view.source.duplicate(true),kind,params.duplicate(true)))

## 有限工单列表展示真实物料和半日；完成或接敌退出由原生Action驱动。
static func _repairs(host, parent: Control, view: Dictionary) -> void:
	var repairs: Dictionary=view.repairs
	host._text(parent,"舟船修缮",21)
	host._text(parent,"停驻修理占用本舰队；开工预约材料，完工才消费。受敌接触会退料停工。",16)
	if repairs.get("charges_enabled",false): host._text(parent,"公营船坞的明示费用由原军团钱袋预约；完工付原服务方，停工退回未付本金。",16)
	var active: Variant=repairs.get("active")
	if active is Dictionary:
		host._text(parent,"%s · 尚需%d/%d半日 · 完工船体%d"%[active.vessel_id,active.remaining_ticks,active.duration,active.integrity_after],17)
		if active.get("charge") is Dictionary: host._text(parent,"原工单预约%d钱 · 收款服务%s"%[active.charge.amount,active.charge.service_id],16)
		_command(host,parent,"停止修缮并退还工料预约" if active.get("charge") is Dictionary else "停止修缮并退还材料预约","naval_repair_cancel",{"request_key":request_key(view,"repair-cancel"),"action_id":active.action_id},view)
	else:
		for option in repairs.get("options",[]):
			var costs: PackedStringArray=[]
			for material in option.get("materials",[]): costs.append("%s×%d"%[material.item_id,material.quantity])
			if int(option.get("fee",0))>0: costs.append("船坞费用%d钱"%int(option.fee))
			host._text(parent,"%s · 船体%d→%d · %d半日 · 至少%d名工作船员 · %s"%[option.vessel_id,option.integrity_before,option.integrity_after,option.duration,option.minimum_crew,"、".join(costs)],16)
			if option.get("ok",false): _command(host,parent,"按此工法修缮","naval_repair_start",option.params,view)
			else: host._text(parent,reason(option.get("code","")),15)
	for record in repairs.get("history",[]):
		host._text(parent,"第%d半日 · %s · %s"%[record.tick,record.vessel_id,reason(record.reason)],15)
		if record.has("paid"): host._text(parent,"实付%d钱 · 解锁未付本金%d钱"%[record.paid,record.released],15)

## 核原后端已确认的当前源；异步未就绪/换局/旧revision均不能提交。
static func _send(host, button: Button, kernel, expected: Dictionary, kind: String, params: Dictionary) -> void:
	if not is_instance_valid(button) or not button.is_inside_tree() or button.disabled or host._advancing or host.kernel!=kernel: return
	if kernel.source_identity()!=expected: return
	if kind in ["naval_repair_start","naval_repair_cancel"]:
		var bound: Dictionary=button.get_meta("naval_repair_context",{})
		if bound.is_empty() or not button.is_visible_in_tree() or button.is_queued_for_deletion() or not host._storage_intent.is_empty(): return
		if host._scene_counter!=bound.scene or int(host.get_meta("personal_battle_generation",0))!=bound.load: return
		var current: Dictionary=kernel.command_context()
		if not current.get("ok",false) or current.get("actor_id","")!=bound.actor_id or current.get("revision",-1)!=expected.get("revision",-2): return
	button.disabled=true
	host._perform(kind,params)

## 有限拒绝文案给出真实操作边界，未知码保留可诊断原值。
static func reason(code: String) -> String:
	return {"NavalLongVoyageHullRequired":"本次远航所需船体不足，请先修缮或选择较短航程。","NavalNavigationDisabled":"实际航损使船体失能，原行程已停止，人员和实船留在真实水上位置。","NavalRepairServiceChanged":"原船坞经营权源已改变，停工并退回原工料预约。","NavalRepairServiceSuspended":"原公营服务已经停业，不能向新Owner默认付费。","NavalRepairServiceRequired":"此泊地没有作者明确的公营维修服务。","NavalRepairPaymentCapacity":"原收款库当前无法接收全部费用，可停止工单退还预约。","NavalRepairMaterialsRequired":"本舰队所选容器的可用材料不足。","NavalRepairCrewUnavailable":"此船实际可工作的船员或船长不足。","NavalRepairAlreadyActive":"本舰队已有修缮工单。","NavalRepairPersonOccupied":"随军成员正在执行另一项行动。","NavalRepairFleetEngaged":"战斗尚未结束，不能开工。","NavalRepairLimitReached":"已达到此处工法的修复上限。","NavalRepairBerthRequired":"完全修复须在真实泊地办理。","NavalRepairMaterialAuthorityRequired":"材料须为本舰队实际承载的同属公物。","NavalRepairCompleted":"修缮完工，原材料已消费。","NavalRepairCancelled":"本人停工，材料预约已退还。","NavalRepairContact":"实际接敌，修缮停止并退料。","NavalRepairCrewChanged":"工作船员发生变化，修缮停止并退料。","NavalBerthRequired":"请在真实泊地办理编队或解编。","NavalLocalBerthRequired":"登船后只能沿原通路移泊；进入岸上场所前先解编。","NavalDestinationBerthRequired":"目标据点没有已声明的抵岸泊地。","NavalReliefMustBeAtFriendlyCamp":"代办解编须在原友军营地与遗留军团同址。","NavalVesselDisabled":"船体失能，当前不能开航。","NavalPeopleOverCapacity":"此船载人超额，请重新分配。","NavalCargoOverCapacity":"此船货重超额，请重新分配军资或人员。","NavalWorkingCrewInsufficient":"本部可工作的船员不足。","NavalCaptainUnavailable":"本部船长状态或水军技艺不足。","NavalAssignmentOverlap":"各部必须使用不同的实船。","NavalCommanderAuthorityRequired":"当前没有统领这支军团的实际权限。","NavalUnitOccupied":"部队正在执行另一项事务。","NavalDisbandRequired":"请先在泊地解编，再办理岸上事务。"}.get(code,"暂不能办理："+code)
