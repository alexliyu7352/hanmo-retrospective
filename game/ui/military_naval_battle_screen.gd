## 舟战只读页与本人草案；正式计划、半日结算及接管各自沿原Kernel事务执行。
extends Control
const Canvas=preload("res://ui/military_naval_battle_canvas.gd")
var host
var view: Dictionary={}
var orders: Dictionary={}
var selected: String=""
var target: String=""
var canvas: Control
var feedback: Label
var submit: Button
var _kernel
var _scene: int=0
var _load_generation: int=0
var _source: Dictionary={}
var _busy: bool=false
var _valid_draft: bool=false

## 宿主传有限Naval页，不把World、AI或目录对象交给界面。
static func render(main, stage: Control, input: Dictionary) -> Control:
	var screen=load("res://ui/military_naval_battle_screen.gd").new()
	screen.host=main;screen.view=input.duplicate(true);screen.name="NavalBattleScreen"
	stage.add_child(screen);screen.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);screen._build();return screen

## 页面同时绑定真实Kernel、读档代数、场景代数与完整来源；过期控件只失效。
func _current() -> bool:
	if _busy or not is_instance_valid(host) or not is_inside_tree() or is_queued_for_deletion() or not is_visible_in_tree(): return false
	if host.kernel!=_kernel or host._scene_counter!=_scene or int(host.get_meta("personal_battle_generation",0))!=_load_generation: return false
	if host._advancing or not host._storage_intent.is_empty() or _kernel.source_identity()!=_source: return false
	var context: Dictionary=_kernel.command_context()
	return context.get("ok",false) and context.get("actor_id")==view.actor_id and context.get("revision")==_source.revision

## 本人规划与异步报价独立；失能船仍可查看或按原规则明确投降。
func _editable() -> bool:
	return _current() and view.get("can_plan",false) and orders.has(selected)

## 一次命令绑定本次世界修订，重复点击只会复用同一业务键。
func _key(intent: String) -> String:
	return "naval-ui:%s:%s:%s:%s"%[_source.get("execution_epoch",0),_source.get("revision",0),view.battle_id,intent]

## 仅同一来源可恢复草案，正式提交后不自动继承NPC随后发起的旅行或其他行动。
func _build() -> void:
	_kernel=host.kernel;_scene=host._scene_counter;_load_generation=int(host.get_meta("personal_battle_generation",0));_source=view.source.duplicate(true)
	var prior: Dictionary=host.get_meta("naval_battle_draft",{})
	if prior.get("source")==_source and prior.get("battle_id")==view.battle_id and prior.get("actor_id")==view.actor_id:
		orders=prior.get("orders",{}).duplicate(true);selected=prior.get("selected","");target=prior.get("target","")
	else:
		for id in view.controlled_unit_ids:
			if not view.units.has(id): continue
			var unit: Dictionary=view.units[id]
			orders[id]={"action":"hold","target_unit_id":"","destination":unit.position.duplicate(),"heading":unit.heading}
			if selected.is_empty(): selected=id
	canvas=Canvas.new();canvas.geometry=view.geometry;canvas.units=view.units;canvas.phase=view.phase;canvas.controller_side=view.controller_side;canvas.events=[]
	add_child(canvas);canvas.anchor_left=.01;canvas.anchor_top=.11;canvas.anchor_right=.72;canvas.anchor_bottom=.96
	canvas.unit_chosen.connect(_choose_unit);canvas.destination_chosen.connect(_choose_destination);canvas.heading_chosen.connect(_choose_heading)
	var scroll=ScrollContainer.new();scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED;add_child(scroll)
	scroll.anchor_left=.74;scroll.anchor_top=.11;scroll.anchor_right=.99;scroll.anchor_bottom=.98
	var panel=VBoxContainer.new();panel.size_flags_horizontal=Control.SIZE_EXPAND_FILL;panel.add_theme_constant_override("separation",9);scroll.add_child(panel)
	host._text(panel,"内河舟战 · 第%d/%d轮"%[view.round,view.max_rounds],24)
	host._text(panel,"船损失能仍浮水。只有真实接舷胜利或明确投降，才可办理接管。",16)
	if view.has("tactical_navigation"):
		var sailing: Dictionary=view.tactical_navigation
		host._text(panel,"转角沿整条航迹累计，每满或不足90度耗%d移动点；驶入逆流格另耗%d。水格箭头表示下游。"%[sailing.quarter_turn_cost,sailing.upstream_cost],16)
	if view.phase=="planning" and view.can_plan:
		var action=OptionButton.new();action.name="NavalTacticalAction";panel.add_child(action)
		for entry in [["hold","原地待机"],["move","航行"],["board","接舷"],["ram","冲撞"],["withdraw","驶向本方退路"],["surrender","明确投降"]]:
			action.add_item(entry[1]);action.set_item_metadata(action.item_count-1,entry[0])
		if view.has("gunnery"):
			action.add_item("舰载攻击");action.set_item_metadata(action.item_count-1,"hull_attack")
		action.item_selected.connect(func(index: int):
			if not _editable() or not is_instance_valid(action) or not action.is_visible_in_tree() or index<0 or index>=action.item_count: return
			orders[selected].action=action.get_item_metadata(index)
			if orders[selected].action not in ["board","ram","surrender","hull_attack"]: orders[selected].target_unit_id=""
			if orders[selected].action in ["hold","surrender"]: orders[selected].destination=view.units[selected].position.duplicate()
			_store();_refresh())
		host._text(panel,"点本船选部队，点敌船选目标，点水格定落点；右键定朝向。",16)
		feedback=host._text(panel,"正在核对军议…",16)
		submit=_button(panel,"提交本轮军议","naval_battle_plan",{},false)
		submit.pressed.connect(_submit_plan)
	elif view.get("can_advance",false):
		host._text(panel,"本轮军议已冻结，继续后全世界推进一个半日。",17)
		_button(panel,"推进一个半日","advance",{},true)
	elif view.phase=="planning": host._text(panel,"本人无本轮战术指挥权，可查看实际船只与兵员。",17)
	if view.phase=="settlement":
		for claim in view.claims:
			host._text(panel,"%s · %s"%[claim.vessel_id,{"boarding_win":"接舷胜利","surrender":"对方投降"}.get(claim.kind,claim.kind)],18)
			if not claim.decision.is_empty(): host._text(panel,"已接管" if claim.decision=="captured" else "已放弃本轮接管",16);continue
			if not claim.own_commander: host._text(panel,"由该军实际统帅裁定。",16);continue
			var params: Dictionary={"battle_id":view.battle_id,"round_source_id":view.round_source_id,"vessel_id":claim.vessel_id,"captor_unit_id":claim.captor_unit_id}
			var take: Dictionary=params.duplicate(true);take.request_key=_key("capture:"+claim.vessel_id);take.assignments=claim.get("assignments",[]).duplicate(true)
			_button(panel,"接管实船与实际船员","naval_capture",take,claim.get("capture_ok",false))
			if not claim.get("capture_ok",false): host._text(panel,reason(claim.get("capture_code","")),16)
			params.request_key=_key("decline:"+claim.vessel_id)
			_button(panel,"放弃本轮接管","naval_claim_decline",params,claim.get("decline_ok",false))
		_button(panel,"确认战果并继续","naval_battle_continue",{"battle_id":view.battle_id,"round_source_id":view.round_source_id,"request_key":_key("continue")},view.get("can_continue",false))
		if not view.get("can_continue",false): host._text(panel,reason(view.get("continue_code","")),16)
	if view.phase=="closed":
		host._text(panel,"本场已结束。实船、幸存人员和接管归属仍留在同一世界。",18)
		var back=Button.new();back.text="返回行旅";panel.add_child(back);back.pressed.connect(func():
			if _current(): host._render())
	for event in view.events:
		if event.kind == "commander_loss":
			var affected: Dictionary = view.units.get(event.unit_id, {})
			var outcome := "阵亡" if event.code == "NavalCommanderKilled" else "负伤"
			host._text(panel,"%s · 军官%s（不重复计入兵员减员）" % [affected.get("commander_actor_id", event.unit_id), outcome], 15)
		else:
			host._text(panel,"%s · 兵损%d · 船损%d"%[event.kind,event.casualties,event.hull_damage],15)
	_store();_refresh()

## 原控件的允许位只用于呈现，点击仍须重验本页来源并走正式preview/commit。
func _button(parent: Control, label: String, kind: String, params: Dictionary, enabled: bool) -> Button:
	var button=Button.new();button.text=label;button.name="NavalBattle_"+kind;parent.add_child(button);button.disabled=not enabled
	if kind!="naval_battle_plan": button.pressed.connect(_send.bind(button,kind,params.duplicate(true)))
	return button

## 屏幕不会代替本人切到NPC统帅，也不在零时长成功后自动推进世界。
func _send(button: Button, kind: String, params: Dictionary) -> void:
	if not _current() or not is_instance_valid(button) or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	_busy=true;button.disabled=true;host._perform(kind,params)

## 只有当前被规则完整报价接受的本人草案才可以提交。
func _submit_plan() -> void:
	if not _editable() or not _valid_draft or submit.disabled: return
	_send(submit,"naval_battle_plan",{"battle_id":view.battle_id,"request_key":_key("plan"),"orders":orders.duplicate(true)})

## 已发生的revision变化淘汰旧草案；选船和目标不会写入游戏存档。
func _store() -> void:
	host.set_meta("naval_battle_draft",{"source":_source.duplicate(true),"battle_id":view.battle_id,"actor_id":view.actor_id,"orders":orders.duplicate(true),"selected":selected,"target":target})

## 点敌船仅设置本部意图；不取得敌军或友军的控制权。
func _choose_unit(id: String) -> void:
	if not _current() or not view.units.has(id): return
	canvas.inspected_id=id
	if orders.has(id): selected=id
	elif _editable(): target=id;orders[selected].target_unit_id=id
	_store();_refresh()

## 落点仍由原水域路径规则报价，屏幕不自行计算可达或收费。
func _choose_destination(point: Array) -> void:
	if not _editable(): return
	orders[selected].destination=point.duplicate();_store();_refresh()

## 朝向只改本轮草案，非当前规划页不会响应迟到鼠标信号。
func _choose_heading(heading: int) -> void:
	if not _editable() or heading<0 or heading>5: return
	orders[selected].heading=heading;_store();_refresh()

## 有限异步查询不携AI计划；返回的来源必须与本页完全相等。
func _refresh() -> void:
	if not _current():
		if submit!=null: submit.disabled=true
		return
	canvas.selected_id=selected
	var drawn: Dictionary=orders.duplicate(true)
	for id in drawn: drawn[id].target_id=drawn[id].target_unit_id
	canvas.orders=drawn;canvas.queue_redraw()
	if feedback==null or not view.can_plan: return
	_valid_draft=false;submit.disabled=true
	if not _kernel.has_method("naval_battle_draft"):
		feedback.text="舟战读取入口尚未就绪。";return
	var result: Dictionary=_kernel.naval_battle_draft(view.battle_id,selected,orders)
	if result.get("code")=="NativeAsyncRequired": feedback.text="正在核对本轮军议…";return
	if result.get("source",{})!=_source or result.get("battle_id","")!=view.battle_id: feedback.text="此页已过期，请重新查看战场。";return
	_valid_draft=result.get("ok",false);submit.disabled=not _valid_draft
	feedback.text="军议可提交；提交本身不耗半日。" if _valid_draft else reason(result.get("code",""))
	if view.has("tactical_navigation") and orders.has(selected):
		for option in result.get("movement",[]):
			if option.position==orders[selected].destination:
				feedback.text+=" 本船路径与最终航向合计%d移动点。"%int(option.cost)
				break
	if view.has("gunnery") and result.has("gunnery_quote"):
		var shot: Dictionary=result.gunnery_quote
		feedback.text+=" 入战水战%d级，船体压制上限%d，命中%d%%；兵损依本轮真实人员结算。"%[int(shot.naval_level),int(shot.hull_damage),int(shot.hit_chance)]
	canvas.movement=result.get("movement",[]);canvas.queue_redraw()

## 暂时的指挥船限制与资源拒绝明确显示，不能把拒绝翻译成已沉没或已经接管。
static func reason(code: String) -> String:
	if code=="NavalDestinationUnreachable": return "本船当前不能按所选落点与最终航向完成行动。"
	if code in ["NavalGunneryOutOfReach","MilitaryTargetOutOfRange","MilitarySightBlocked"]: return "目标不在本船实际武器射程或视线内。"
	if code in ["NavalGunneryPreparationRequired","NavalGunnerySkillReadRequired","NavalGunneryBattleSourceMismatch"]: return "本场入战技艺来源不完整，不能提交舰载攻击。"
	if code=="InvalidNavalTacticalNavigationInput": return "当前航向或航行预算无效，请重新选择。"
	return {"NavalCaptureEconomicResponsibilityPending":"这只船舱或随船公库仍有经济责任，尚不能转移。可放弃本轮接管。","NavalCaptureNeedResponsibilityPending":"随船公库仍被实际需求引用，尚不能转移。","NavalCaptureTaskResponsibilityPending":"船舱或随船公库仍绑定真实任务，尚不能转移。","NavalCaptureAssetBoundaryRequired":"当前世界没有完整接管边界，不能办理产权转移。","NavalCommandVesselMustBeLast":"须先处理这支敌军的其他实船，当前指挥船暂不能接管。","NavalCapturedHoldOverCapacity":"目标船舱装不下实际军资，仍可放弃本轮接管。","NavalPeopleOverCapacity":"接舷船容纳不下全部幸存者，仍可放弃接管。","NavalCargoOverCapacity":"接管后实载超重，不能凭空丢弃货物。","NavalCapturedCargoReserved":"船舱存在未解除责任或预留，暂不能转移产权。","NavalClaimsPending":"请先处理本人尚未决定的接管权。","NavalCaptureAuthorityRequired":"本人没有接管该船的真实统帅权限。","NavalShipCannotSail":"船体或可工作船员不足，不能航行或冲撞。","NavalCrewCannotAct":"本部已无可战能力，可查看或依规则投降。","NavalTargetRequired":"请点选真实目标船。","NavalExitRequired":"退航终点须为本方真实退路。","NavalContinueAuthorityRequired":"此项由当前真实战场控制者处理。"}.get(code,"当前规则拒绝："+code)
