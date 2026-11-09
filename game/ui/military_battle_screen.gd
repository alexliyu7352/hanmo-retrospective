## 野战界面保存未提交命令到宿主元数据；正式世界时间、军粮、伤亡回写与存档均交给统一命令入口。
extends Control
const Canvas=preload("res://ui/military_battle_canvas.gd")
const Logistics=preload("res://ui/military_logistics.gd")
const Rout=preload("res://ui/military_rout_panel.gd")
const Shock=preload("res://ui/military_command_shock_panel.gd")
const CombatConditions=preload("res://ui/military_combat_conditions_panel.gd")
const CommandedDamage=preload("res://ui/military_commanded_damage_panel.gd")
const FieldTerrain=preload("res://ui/military_field_terrain_panel.gd")
const Sequential=preload("res://ui/military_sequential_panel.gd")
const CommandResources=preload("res://ui/military_command_resources_panel.gd")
var host
var page_view: Dictionary={}
var identity: Dictionary={}
var header: Dictionary={}
var units: Dictionary={}
var orders: Dictionary={}
var selected_id: String=""
var inspected_id: String=""
var canvas: Control
var _inspector: VBoxContainer
var _footer: VBoxContainer
var _preview: Label
var _submit: Button
var _committing: bool=false
var _inspection: VBoxContainer
var _controls_box: VBoxContainer
var _roster: OptionButton
var _target_label: Label
var _draft_result: Dictionary={}
var _draft_key: Dictionary={}
var _refresh_requested: bool=false
var _stale: bool=false
var _bound_kernel_id: int=0
var _bound_scene_epoch: int=0
var _bound_load_generation: int=0
var _bound_source: Dictionary={}
var _draft_signature: Dictionary={}

## 宿主只传已选中的有限页面；规则来源由Kernel私有持有，不绑定全state或目录。
static func render(main, stage: Control, input: Dictionary) -> Control:
	var view: Control=load("res://ui/military_battle_screen.gd").new()
	view.host=main; view.page_view=input.duplicate(true); view.identity=view.page_view.identity; view.header=view.page_view.header; view.units=view.page_view.units
	view.name="MilitaryBattleScreen"; stage.add_child(view); view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); view._build(); return view

## 同一公开轮次保留各部草案；轮次、会话或世界修订改变则从实际位置默认防御，不读下一轮AI意图。
func _restore() -> void:
	_bound_kernel_id=host.kernel.get_instance_id(); _bound_scene_epoch=host._scene_counter
	_bound_load_generation=int(host.get_meta("personal_battle_generation",0))
	_bound_source=host.kernel.source_identity().duplicate(true) if host.kernel.has_method("source_identity") else {}
	_draft_signature={"kernel_id":_bound_kernel_id,"load_generation":_bound_load_generation,"source":_bound_source.duplicate(true),"identity":identity.duplicate(true)}
	var stored: Dictionary=host.get_meta("military_battle_draft",{})
	if stored.get("signature")==_draft_signature and stored.get("orders") is Dictionary:
		orders=stored.orders.duplicate(true); selected_id=stored.selected_id; inspected_id=stored.inspected_id
	elif _restore_request_draft(): pass
	else:
		for id in header.command_unit_ids:
			if not _controllable(id): continue
			var unit: Dictionary=units[id]
			orders[id]={"action":"defend","destination":unit.position.duplicate(),"heading":unit.heading,"target_id":"","tactic_id":""}
			if selected_id.is_empty(): selected_id=id
		inspected_id=selected_id
	_store()

## 即时请求成功只保留同会话/同轮/恰增一修订的本人草案，且需公开原回执证明该次请求。
func _restore_request_draft() -> bool:
	var pending: Dictionary=host.get_meta("military_request_draft",{})
	if pending.is_empty(): return false
	var signature: Dictionary=pending.get("signature",{})
	var previous: Dictionary=signature.get("identity",{})
	if signature.get("kernel_id")!=_bound_kernel_id or signature.get("load_generation")!=_bound_load_generation: return false
	var previous_source: Dictionary=signature.get("source",{})
	var current_source: Dictionary=_bound_source.duplicate(true)
	if previous_source.has("revision"):
		if previous_source.revision!=previous.get("revision") or current_source.get("revision")!=identity.revision: return false
		current_source.revision=previous_source.revision
	if previous_source!=current_source: return false
	if previous.get("actor_id")!=identity.actor_id or previous.get("battle_id")!=identity.battle_id or previous.get("round")!=identity.round or identity.phase!="planning" or identity.revision!=int(previous.get("revision",-2))+1: return false
	for answer in page_view.get("requests",{}).get("history",[]):
		if answer.request_key!=pending.get("request_key") or answer.round!=identity.round: continue
		orders=pending.orders.duplicate(true); selected_id=pending.selected_id; inspected_id=pending.inspected_id
		host.remove_meta("military_request_draft"); return true
	return false

## 活页绑定Kernel、成功读档代数、完整公开来源和本人；查询不占用序号，也不读取世界私有状态。
func _matches_live_context() -> bool:
	if _stale or not is_instance_valid(host) or not is_inside_tree() or is_queued_for_deletion() or not is_visible_in_tree(): return false
	if host.kernel.get_instance_id()!=_bound_kernel_id or host._scene_counter!=_bound_scene_epoch: return false
	if int(host.get_meta("personal_battle_generation",0))!=_bound_load_generation: return false
	if host.kernel.has_method("source_identity") and host.kernel.source_identity()!=_bound_source: return false
	var context: Dictionary=host.kernel.command_context()
	return context.get("ok",false) and context.get("actor_id")==identity.actor_id and context.get("revision")==identity.revision

## 旧页只禁用自己的确认；草案所有权已转移时不删除新候选，也不替新页安排重绘。
func _invalidate() -> void:
	if _stale: return
	_stale=true
	if _submit!=null: _submit.disabled=true
	var stored: Dictionary=host.get_meta("military_battle_draft",{})
	if not _refresh_requested and is_inside_tree() and not is_queued_for_deletion() and host._scene_counter==_bound_scene_epoch and stored.get("signature")==_draft_signature and stored.get("view_epoch")==_bound_scene_epoch:
		_refresh_requested=true; host.call_deferred("_render")

## 正在推进或存读时保持原候选；只有当前规划页可以编辑，过期页不得先写草案再等待报价失败。
func _can_edit() -> bool:
	if _committing or identity.phase!="planning" or host._advancing or not host._storage_intent.is_empty(): return false
	if not _matches_live_context(): _invalidate(); return false
	return true

## 重建侧栏后旧控件的迟到信号不能再改令，仍在活页中的实际控件才有输入资格。
func _control_current(control: Variant) -> bool:
	return is_instance_valid(control) and control is Control and control.is_inside_tree() and not control.is_queued_for_deletion() and control.is_visible_in_tree() and is_ancestor_of(control) and _can_edit()

## 下拉索引先验证原控件寿命与范围，不从旧控件读取metadata或借当前选将发送选择。
func _option_selected(index: int, control: Variant, kind: String) -> void:
	if not _control_current(control) or index<0 or index>=control.item_count or control.is_item_disabled(index): return
	if kind=="heading": _choose_heading(index)
	elif kind=="tactic": _choose_tactic(str(control.get_item_metadata(index)))
	else: _choose_unit(str(control.get_item_metadata(index)),true)

## 指挥权同时受实际编制、阵营、军级范围和部队状态约束；最终提交仍由纯规则完整复核。
func _controllable(id: String) -> bool:
	return units.has(id) and units[id].controllable

## 地图占主要面积，右栏集中目标、查看对象与命令；滚动容器保证较矮窗口仍可操作。
func _build() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE; _restore()
	var matte: ColorRect=ColorRect.new(); matte.color=Color("17251f"); matte.mouse_filter=Control.MOUSE_FILTER_IGNORE; add_child(matte)
	matte.anchor_top=0.085; matte.anchor_right=1; matte.anchor_bottom=1
	canvas=Canvas.new(); canvas.geometry=page_view.geometry; canvas.units=units; canvas.phase=identity.phase; canvas.controller_side=header.controller_side; canvas.events=page_view.events; canvas.orders=orders.duplicate(true); canvas.selected_id=selected_id; canvas.inspected_id=inspected_id
	var previous: Dictionary=host.get_meta("military_battle_frame",{})
	if previous.get("id")==identity.battle_id and previous.get("round")!=identity.round: canvas.previous_positions=previous.positions.duplicate(true)
	var positions: Dictionary={}
	for id in units: positions[id]=units[id].position.duplicate()
	host.set_meta("military_battle_frame",{"id":identity.battle_id,"round":identity.round,"positions":positions})
	add_child(canvas); canvas.anchor_left=0.012; canvas.anchor_right=0.73; canvas.anchor_top=0.105; canvas.anchor_bottom=0.90
	canvas.unit_chosen.connect(_choose_unit); canvas.destination_chosen.connect(_choose_destination); canvas.heading_chosen.connect(_choose_heading)
	var help: Label=host._text(self,"点选部队或落点 · 右键设朝向 · 点击对手设攻击目标\n每次确认推进半日，其他会战与军粮一并结算。",16)
	help.anchor_left=0.025; help.anchor_right=0.72; help.anchor_top=0.91; help.anchor_bottom=0.995
	var scroll: ScrollContainer=ScrollContainer.new(); scroll.name="MilitaryInspectorScroll"; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; add_child(scroll)
	scroll.anchor_left=0.75; scroll.anchor_right=0.985; scroll.anchor_top=0.105; scroll.anchor_bottom=0.745 if identity.phase=="planning" else 0.88
	_inspector=VBoxContainer.new(); _inspector.size_flags_horizontal=Control.SIZE_EXPAND_FILL; _inspector.add_theme_constant_override("separation",8); scroll.add_child(_inspector)
	_footer=VBoxContainer.new(); _footer.add_theme_constant_override("separation",7); add_child(_footer)
	_footer.anchor_left=0.75; _footer.anchor_right=0.985; _footer.anchor_top=0.76 if identity.phase=="planning" else 0.91; _footer.anchor_bottom=0.99
	_panel()

## 静态页只构建一次；长反馈在独立区滚动，确认按钮不被推出视口；草案改变不重取名册或重建侧栏。
func _panel() -> void:
	host._text(_inspector,header.display_name,27)
	host._text(_inspector,"第 %d / %d 轮　%s" % [identity.round,header.max_rounds,"统领全军" if header.command_scope=="army" else "率领本部"],17)
	var goal: Label=host._text(_inspector,page_view.objective.text,17); goal.name="MilitaryObjective"
	Logistics.render_quote(host,_inspector,page_view.logistics)
	if header.has("rout_retreat"): host._text(_inspector,header.rout_retreat,15)
	host.status_label=host._text(_inspector,host._message,16); host.status_label.visible=not host._message.is_empty()
	if identity.phase=="result": _result(); return
	_roster=OptionButton.new(); _roster.name="MilitaryRoster"; _roster.allow_reselect=true; _roster.custom_minimum_size.y=36; _roster.add_theme_font_size_override("font_size",16); _inspector.add_child(_roster)
	for id in units:
		var tag: String="可指挥" if _controllable(id) else ("友军·只读" if units[id].side==header.controller_side else "敌军·只读")
		_roster.add_item(_unit_name(id)+" · "+tag); _roster.set_item_metadata(_roster.item_count-1,id)
	_roster.item_selected.connect(_option_selected.bind(_roster,"roster"))
	_inspection=VBoxContainer.new(); _inspection.add_theme_constant_override("separation",8); _inspector.add_child(_inspection)
	_controls_box=VBoxContainer.new(); _controls_box.add_theme_constant_override("separation",8); _inspector.add_child(_controls_box)
	_target_label=host._text(_footer,"",16)
	var preview_scroll: ScrollContainer=ScrollContainer.new(); preview_scroll.name="MilitaryPreviewScroll"; preview_scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; preview_scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; preview_scroll.custom_minimum_size.y=50; _footer.add_child(preview_scroll)
	_preview=host._text(preview_scroll,"",16); _preview.name="MilitaryPreview"; _preview.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	_submit=host._button(_footer,"确认军令 · 推进半日","military_submit",identity.battle_id,_submit_orders); _submit.custom_minimum_size.y=46
	host._text(_footer,"已安排 %d 部 · 初始命令为原地防御" % orders.size(),14)
	_request_history(); _events(); _inspect(); _update_controls(); _refresh()

## 查看对象局部重绘，不因只读友军点击重复查询全军草案。
func _inspect() -> void:
	for child in _inspection.get_children(): _inspection.remove_child(child); child.queue_free()
	for index in _roster.item_count:
		if _roster.get_item_metadata(index)==inspected_id: _roster.select(index)
	if not units.has(inspected_id): return
	var unit: Dictionary=units[inspected_id]
	host._text(_inspection,_unit_name(inspected_id)+"　"+unit.troop_display_name,23)
	if unit.protected: host._text(_inspection,"本战保护对象",16)
	host._text(_inspection,classification(unit),16)
	host._text(_inspection,"士气 %d / 100　战法点 %d / %d\n移动 %d　射程 %d–%d 格" % [unit.morale,unit.tactic_points,unit.tactic_capacity,unit.movement,unit.minimum_range,unit.maximum_range],16)
	for line in FieldTerrain.lines(unit): host._text(_inspection,line,15)

	if unit.has("condition"): _inspect_condition(unit.condition)
	var statuses: PackedStringArray=[]
	for status in unit.statuses:
		if status=="military_rout_started_v1" or status=="military_rout_blocked_v1": continue
		if status=="military_commander_loss_v1":
			statuses.append("本部军官阵亡" if unit.statuses[status]==2 else "本部军官负伤")
			continue
		if unit.statuses[status]>=identity.round: statuses.append("%s %d轮" % [{"inspired":"振奋","suppressed":"受压","exposed":"破绽","hindered":"行动受阻"}.get(status,status),unit.statuses[status]-identity.round+1])
	if not statuses.is_empty(): host._text(_inspection," · ".join(statuses),16)
	Rout.render(host,_inspection,unit)
	Shock.render(host,_inspection,unit)
	CombatConditions.render(host,_inspection,unit.get("combat_conditions",{}))
	CommandedDamage.render(host,_inspection,unit.get("commanded_damage",{}))
	Sequential.render(host,_inspection,unit.get("sequential",{}))
	CommandResources.render(host,_inspection,unit.get("command_resources",{}))
	if not _controllable(inspected_id):
		host._text(_inspection,"本部军官已阵亡，停止新指令；兵员与物资仍保原归属。" if unit.statuses.get("military_commander_loss_v1",0)==2 else ("此部正在自动撤离，不能再下攻击或等待恢复士气的命令。" if unit.get("rout",{}).get("active",false) else "此部由其将领指挥，只能查看。"),16)
		if not selected_id.is_empty(): host._button(_inspection,"继续安排"+_unit_name(selected_id),"military_select",selected_id,_choose_unit.bind(selected_id,true))

## 条件检查栏只消费有限页已授权的数值；展开来源是纯表现，不重新查询全世界或调整军令/伤害。
func _inspect_condition(condition: Dictionary) -> void:
	var summary: Label=host._text(_inspection,"训练 %d / 100　疲劳 %d / 100" % [condition.training,condition.fatigue],16); summary.name="MilitaryConditionSummary"
	var toggle: Button=Button.new(); toggle.name="MilitaryConditionHistoryToggle"; toggle.toggle_mode=true; toggle.custom_minimum_size.y=34
	toggle.text="部队条件来源 · %d 条记录" % condition.history_count
	_inspection.add_child(toggle)
	var details: Label=host._text(_inspection,condition_history_text(condition),14); details.name="MilitaryConditionHistory"; details.visible=false
	toggle.toggled.connect(func(expanded): details.visible=expanded)

## 展示原初值、课时与执行来源身份及前后值；不足六条不补事件，截断时明确说明省略数量。
static func condition_history_text(condition: Dictionary) -> String:
	var lines: PackedStringArray=["当前部队记录 · "+condition.source.unit_id]
	var initial: Dictionary=condition.initial_source
	if not initial.is_empty(): lines.append("初始驻军 · %s / %s\n训练 %d　疲劳 %d" % [initial.garrison_id,initial.unit_id,initial.training,initial.fatigue])
	if condition.history_count==0: lines.append("尚无部队条件记录。")
	elif condition.history_count>condition.history.size(): lines.append("显示最近 %d / %d 条，较早 %d 条未展开。" % [condition.history.size(),condition.history_count,condition.history_count-condition.history.size()])
	for row in condition.history:
		lines.append("半日 %d · %s\n训练 %d → %d　疲劳 %d → %d\n来源 %s · 供给 %s" % [row.tick,"训练课时" if row.kind=="military_training" else ("驻地休整" if row.kind=="military_garrison_rest" else "真实行军"),row.training_before,row.training_after,row.fatigue_before,row.fatigue_after,row.source_id,row.supply_fact_id])
		if row.kind=="military_march": lines.append("%s · 行动 %s · 路线 %s" % [row.rule,row.action_id,row.plan_id])
	return "\n".join(lines)

## 命令控件只反映当前草案；既有目标和只读人员详情保持独立节点。
func _update_controls() -> void:
	for child in _controls_box.get_children(): _controls_box.remove_child(child); child.queue_free()
	if not selected_id.is_empty() and orders.has(selected_id):
		host._text(_controls_box,"本轮命令 · "+_unit_name(selected_id),18); _controls()
		_request_controls()
	_target_label.text=("%s → %s" % [_unit_name(selected_id),_unit_name(orders[selected_id].target_id)] if not orders[selected_id].target_id.is_empty() else "本轮 · "+_unit_name(selected_id)) if orders.has(selected_id) else ""

## 部将三种意图独立提交，统帅可以拒绝；按钮不提交本部草案或提前推进回合。
func _request_controls() -> void:
	var requests: Dictionary=page_view.get("requests",{})
	if requests.is_empty() or header.command_scope!="unit": return
	host._text(_controls_box,"向统帅请示 · 本轮 %d / %d" % [requests.used_this_round,requests.per_actor_round_limit],16)
	host._text(_controls_box,"统帅可采纳或拒绝；拒绝也占本轮请求次数。",14)
	for row in [["support","请求支援"],["withdrawal","申请撤退"],["focus_target","提议集中攻击所查看敌部"]]:
		var target_ok: bool=row[0]!="focus_target" or (units.has(inspected_id) and units[inspected_id].side!=header.controller_side and not units[inspected_id].withdrawn and units[inspected_id].fit_count>0)
		var button: Button=host._button(_controls_box,row[1],"military_request",row[0],func(): pass,not requests.boundary_open or requests.used_this_round>=requests.per_actor_round_limit or not target_ok or host._advancing)
		button.name="MilitaryRequest_"+row[0]
		button.pressed.connect(_send_request.bind(row[0],button))

## 只取当前活页本人部队与公开所选敌部；幂等键随本次来源冻结，迟到按钮不能借新页发送。
func _send_request(kind: String, button: Button) -> void:
	if not _control_current(button) or button.disabled or not _controllable(selected_id): return
	var requests: Dictionary=page_view.get("requests",{})
	if not requests.get("boundary_open",false) or requests.get("used_this_round",0)>=requests.get("per_actor_round_limit",0): return
	if not kind in ["support","withdrawal","focus_target"]: return
	var target: String=inspected_id if kind=="focus_target" else ""
	if kind=="focus_target" and (not units.has(target) or units[target].side==header.controller_side or units[target].withdrawn or units[target].fit_count<=0): return
	var key: String="ui:%s:%d:%s:%d:%s" % [identity.battle_id,identity.round,identity.actor_id,identity.revision,kind]
	host.set_meta("military_request_draft",{"signature":_draft_signature.duplicate(true),"request_key":key,"orders":orders.duplicate(true),"selected_id":selected_id,"inspected_id":inspected_id})
	_committing=true; button.disabled=true
	if _submit!=null: _submit.disabled=true
	host._perform("military_request",{"battle_id":identity.battle_id,"request_key":key,"request_kind":kind,"unit_id":selected_id,"target_unit_id":target})

## 回执只消费领域已裁定公开答复，明确撤退同意仍须本部按原出口规则执行。
func _request_history() -> void:
	var requests: Dictionary=page_view.get("requests",{})
	if requests.get("history",[]).is_empty(): return
	var lines: PackedStringArray=[]
	for answer in requests.history:
		var intent: String={"support":"请求支援","withdrawal":"申请撤退","focus_target":"提议集中攻击"}.get(answer.request_kind,answer.request_kind)
		lines.append("第 %d 轮 · %s：%s\n%s" % [answer.round,intent,"采纳" if answer.accepted else "拒绝",request_reason(answer.reason)])
		if answer.accepted: lines.append("响应部队 · "+_unit_name(answer.response_unit_id)+"；随本轮军令共同执行。")
		if answer.accepted and answer.request_kind=="withdrawal": lines.append("本部仍须安排退兵并实际到达本方退路，尚未离场。")
	if requests.history_count>requests.history.size(): lines.append("显示最近 %d / %d 条请求回执。" % [requests.history.size(),requests.history_count])
	var label: Label=host._text(_inspector,"统帅答复\n"+"\n".join(lines),15); label.name="MilitaryRequestHistory"

## 开发政策只解释原生裁定码，不由界面重新决定是否同意。
static func request_reason(code: String) -> String:
	return {"MilitarySupportAccepted":"已安排合法友部协防。","MilitaryWithdrawalApprovedWithCover":"同意撤退，并安排友部掩护。","MilitaryFocusAccepted":"已安排友部攻击提议的目标。","MilitaryCommanderMaintainsObjective":"统帅决定继续当前作战目标。","MilitaryRetreatExitUnreachable":"本轮无法到达己方退路。","MilitaryNoLegalSupport":"当前没有能提供协防的友部。","MilitaryNoLegalAttack":"当前没有可攻击该目标的友部。"}.get(code,reason(code))

## 五类人数来自真实独立分类计数，保持原显示格式与退离标识。
static func classification(unit: Dictionary) -> String:
	return "可战 %d　伤员 %d　阵亡 %d\n被俘 %d　逃散 %d%s" % [unit.fit_count,unit.wounded_count,unit.dead_count,unit.captured_count,unit.deserted_count," · 已退离" if unit.withdrawn else ""]

## 基础命令与战法选项只修改当前可控部队；查看未授权人物时仍清楚标明命令实际接收者。
func _controls() -> void:
	var draft: Dictionary=orders[selected_id]; var grid: GridContainer=GridContainer.new(); grid.columns=3; grid.add_theme_constant_override("h_separation",5); grid.add_theme_constant_override("v_separation",5); _controls_box.add_child(grid)
	for row in [["move","移动"],["attack","攻击"],["defend","防御"],["wait","等待"],["withdraw","退兵"]]:
		var button: Button=host._button(grid,row[1],"military_action",row[0],_choose_action.bind(row[0]),host._advancing)
		button.custom_minimum_size=Vector2(68,36); button.add_theme_font_size_override("font_size",16); button.size_flags_horizontal=Control.SIZE_EXPAND_FILL; button.toggle_mode=true; button.set_pressed_no_signal(draft.action==row[0])
	var tactic: OptionButton=OptionButton.new(); tactic.name="MilitaryTactic"; tactic.custom_minimum_size.y=36; tactic.add_theme_font_size_override("font_size",16); _controls_box.add_child(tactic); tactic.add_item("选择战法"); tactic.set_item_metadata(0,"")
	for definition in units[selected_id].tactics:
		tactic.add_item("%s · %d 战法点" % [definition.display_name,definition.point_cost]); tactic.set_item_metadata(tactic.item_count-1,definition.id)
		if definition.id==draft.tactic_id: tactic.select(tactic.item_count-1)
	tactic.disabled=host._advancing or tactic.item_count==1
	tactic.item_selected.connect(_option_selected.bind(tactic,"tactic"))
	var heading: OptionButton=OptionButton.new(); heading.name="MilitaryHeading"; heading.custom_minimum_size.y=34; heading.add_theme_font_size_override("font_size",16); _controls_box.add_child(heading)
	for label in ["向东","向东南","向西南","向西","向西北","向东北"]: heading.add_item("朝向 · "+label)
	heading.select(draft.heading); heading.disabled=host._advancing; heading.item_selected.connect(_option_selected.bind(heading,"heading"))
	host._text(_controls_box,"点击邻接友部设协防目标；用上方名册切换指挥部队。" if _support_selected() else "背侧受击更危险；攻击范围由距离、地形与视线决定。",14)
	if not draft.target_id.is_empty(): host._text(_controls_box,"目标 · "+_unit_name(draft.target_id),16)

## 协防草案的地图点击只选目标；名册显式切换指挥对象，任何目标都不因此获得命令权。
func _choose_unit(id: String, command_selection: bool=false) -> void:
	if not _can_edit() or not units.has(id): return
	inspected_id=id
	if not command_selection and _support_selected(): orders[selected_id].target_id=id
	elif _controllable(id): selected_id=id
	elif orders.has(selected_id) and units[id].side!=header.controller_side:
		var receives_target: bool=true
		if orders[selected_id].action=="tactic":
			for tactic in units[selected_id].tactics:
				if tactic.id==orders[selected_id].tactic_id and tactic.kind=="rally": receives_target=false
		if receives_target: orders[selected_id].target_id=id
		if orders[selected_id].action!="tactic": orders[selected_id].action="attack"; orders[selected_id].tactic_id=""
	_store(); _inspect(); _update_controls(); _refresh()

## 当前战法种类只从公开兵种读取；非法或不存在的草案不能成为协防选择模式。
func _support_selected() -> bool:
	if not orders.has(selected_id) or orders[selected_id].action!="tactic": return false
	for definition in units[selected_id].tactics:
		if definition.id==orders[selected_id].tactic_id: return definition.kind=="support"
	return false

## 协防为原地声明，不接收地面落点；其他格点击保留原非法草案反馈和默认移动行为。
func _choose_destination(point: Array) -> void:
	if not _can_edit() or not _controllable(selected_id) or _support_selected(): return
	orders[selected_id].destination=point.duplicate()
	if orders[selected_id].action in ["wait","defend"]: orders[selected_id].action="move"
	_store(); _update_controls(); _refresh()

## 方向来自六个实际军阵朝向；旋转草案不改权威部队或随机数。
func _choose_heading(heading: int) -> void:
	if not _can_edit() or not _controllable(selected_id): return
	orders[selected_id].heading=posmod(heading,6); _store(); _update_controls(); _refresh()

## 原地行动重置落点，攻击保留已点目标；任何命令都仍需统一预览批准后才能提交。
func _choose_action(action: String) -> void:
	if not _can_edit() or not _controllable(selected_id): return
	var draft: Dictionary=orders[selected_id]; draft.action=action; draft.tactic_id=""
	if action in ["wait","defend"]: draft.destination=units[selected_id].position.duplicate()
	if action!="attack": draft.target_id=""
	_store(); _update_controls(); _refresh()

## 战法从实际兵种配置选择；协防同鼓舞、齐射原地发动，改选时不沿用敌军目标。
func _choose_tactic(id: String) -> void:
	if not _can_edit() or not _controllable(selected_id): return
	if id.is_empty(): _choose_action("defend"); return
	for definition in units[selected_id].tactics:
		if definition.id!=id: continue
		orders[selected_id].action="tactic"; orders[selected_id].tactic_id=id
		if definition.kind in ["rally","volley","support"]: orders[selected_id].destination=units[selected_id].position.duplicate()
		if definition.kind=="rally": orders[selected_id].target_id=""
		if definition.kind=="support":
			var target: String=orders[selected_id].target_id
			if not units.has(target) or units[target].side!=header.controller_side: orders[selected_id].target_id=""
		_store(); _update_controls(); _refresh(); return

## 全军草案一起复核权限和完整性；单部预估只是当前阵势报价，不承诺下一轮必然命中。
func _refresh() -> void:
	if not _matches_live_context(): _invalidate(); return
	var key: Dictionary={"selected_id":selected_id,"orders":orders}
	if _draft_key!=key or _draft_result.is_empty():
		var result: Dictionary=host.kernel.military_battle_draft(identity.battle_id,selected_id,orders)
		if result.get("code")=="NativeAsyncRequired":
			_submit.disabled=true; _preview.text="正在核对当前军令…"
			return # 纯读已排队，不能用新frame抢在它之前反复刷新。
		if not result.get("ok",false) or result.get("identity",{})!=identity:
			# 新读取只触发宿主原重绘；不改旧页身份，不混合新报价和旧地形。
			_invalidate(); return
		_draft_result=result; _draft_key=key.duplicate(true)
	canvas.orders=orders.duplicate(true); canvas.selected_id=selected_id; canvas.inspected_id=inspected_id; canvas.apply_preview(_draft_result)
	var valid: Dictionary=_draft_result.orders_validation
	_submit.disabled=not valid.ok or host._advancing or _committing
	if not valid.ok:
		var failed_id: String=str(valid.get("path","")).get_slice("/",0)
		_preview.text=(_unit_name(failed_id)+"：" if header.command_scope=="army" and units.has(failed_id) else "")+reason(valid.get("code","")); return
	if not orders.has(selected_id): _preview.text="本部已退离或失去战力，确认后由其他部队继续。"; return
	var quote: Dictionary=_draft_result.selected_quote
	if not quote.ok: _preview.text=reason(quote.get("code","")); return
	_preview.text=_quote_text(quote,orders[selected_id])

## 只解释当前有限战法与真实报价；不在界面推算伤害、移动预算或额外规则字段。
func _quote_text(quote: Dictionary, order: Dictionary) -> String:
	var tactic: Dictionary={}
	if order.action=="tactic":
		for definition in units[selected_id].tactics:
			if definition.id==order.tactic_id: tactic=definition; break
	var lines: PackedStringArray=[]
	var cost: String="移动消耗 %d" % quote.cost if quote.has("cost") else ""
	if not tactic.is_empty(): cost+=(" · " if not cost.is_empty() else "")+"消耗 %d 战法点" % tactic.point_cost
	if not cost.is_empty(): lines.append(cost)
	if quote.has("support_share_bp"):
		lines.append("原地协防 %s：首次实际减员最多分担 %d%%。\n共同移动后须仍相邻；未受攻击不分摊。" % [_unit_name(order.target_id),quote.support_share_bp/100])
	else:
		if quote.has("hit_chance"):
			lines.append("命中 %d%% · 预计伤亡 %d–%d" % [quote.hit_chance,quote.casualties_min,quote.casualties_max])
		if order.action=="tactic":
			lines.append({"rally":"原地鼓舞本部及邻接友部士气，并给予振奋。","march":"急行扩大本轮移动范围；仍受地形、占位和敌控制区限制。","feint":"佯退向轮初朝向的后方移动，穿过敌控制区。\n仍受地形和占位限制，抵达退路也不会离场。","flank":"侧击须实际移动至少两格，并从目标侧后攻击。","charge":"突击须移动后攻击，并使本部露出破绽。","volley":"原地齐射；造成减员后压制目标。","suppress":"压制射击造成减员后，使目标下轮行动受阻。"}.get(tactic.get("kind",""),"战法合法。"))
		elif not quote.has("hit_chance"):
			lines.append({"move":"按标记路径移动，进入敌军控制区便止步。","defend":"原地防御，减轻本轮伤亡。","wait":"原地等待，恢复士气与战法点。","withdraw":"抵达本方退路才会离场，途中受阻则留在战场。"}.get(order.action,"命令合法。"))
		if quote.has("hit_chance"): lines.append("按当前阵势估算，共同移动后复核。")
	if quote.has("incoming_damage_bp"):
		lines.append("本轮承受直接伤害为正常的 %d%%。" % (quote.incoming_damage_bp/100))
	return "\n".join(lines)

## 元数据仅保留UI草案；复制避免画布或重绘通过引用改写会话。
func _store() -> void:
	if host._advancing or not host._storage_intent.is_empty() or not _matches_live_context(): return
	host.set_meta("military_battle_draft",{"signature":_draft_signature.duplicate(true),"view_epoch":_bound_scene_epoch,"orders":orders.duplicate(true),"selected_id":selected_id,"inspected_id":inspected_id})

## 一次点击发送完整授权计划一次；宿主负责推进全局半日和重建结果，不在此调用纯 resolve。
func _submit_orders() -> void:
	if not _can_edit() or _submit.disabled: return
	_committing=true; _submit.disabled=true; host._perform("military_plan",{"battle_id":identity.battle_id,"orders":orders.duplicate(true)})

## 已结算结果展示真实双方分账，关闭也使用正式命令以便世界处理后续行程与职责。
func _result() -> void:
	var winner: int=page_view.result.get("winner_side",-1)
	host._text(_inspector,"未分胜负" if winner<0 else ("此战获胜" if winner==header.controller_side else "此战失利"),29)
	host._text(_inspector,reason(page_view.result.get("reason","")),17)
	for id in units:
		host._text(_inspector,_unit_name(id)+(" · 我军" if units[id].side==header.controller_side else " · 敌军"),18)
		host._text(_inspector,classification(units[id]),16)
		var loss: int=int(units[id].statuses.get("military_commander_loss_v1",0))
		if loss>0: host._text(_inspector,"本部军官阵亡" if loss==2 else "本部军官负伤",16)
	_events()
	_submit=host._button(_footer,"确认战果，返回行程","military_close",identity.battle_id,_close_result,host._advancing)

## 战果关闭冻结同一活页与来源，成功进入原宿主后拒绝同实例重复确认。
func _close_result() -> void:
	if _committing or identity.phase!="result" or host._advancing or not host._storage_intent.is_empty(): return
	if not _matches_live_context(): _invalidate(); return
	_committing=true; _submit.disabled=true
	host._perform("military_close",{"battle_id":identity.battle_id})

## 战报显示实际命中、伤亡、溃退和退兵事实；没有发生的损失不从预估补写。
func _events() -> void:
	var lines: PackedStringArray=[]
	for event in page_view.events:
		match event.get("kind"):
			"attack": lines.append("%s → %s：%s" % [_unit_name(event.unit_id),_unit_name(event.target_id),"伤亡 %d" % event.casualties if event.get("casualties",0)>0 else reason(event.get("code",""))])
			"support": lines.append("%s为%s协防：分担减员 %d。" % [_unit_name(event.unit_id),_unit_name(event.target_id),event.casualties])
			"commander_loss": lines.append(_unit_name(event.unit_id)+"："+reason(event.get("code","")))
			"rally": lines.append(_unit_name(event.unit_id)+"鼓舞士气。")
			"rout": lines.append("%s溃退：被俘 %d，逃散 %d。" % [_unit_name(event.unit_id),event.captured,event.deserted])
			"command_shock": lines.append(_unit_name(event.unit_id)+"：本军统帅退出，士气受挫。")
			"rout_started", "rout_retreat": lines.append(_unit_name(event.unit_id)+"："+reason(event.get("code","")))
			"withdraw": lines.append(_unit_name(event.unit_id)+"已经由退路离场。")
			"move":
				if event.code!="Moved": lines.append(_unit_name(event.unit_id)+"行军受阻。")
	if not lines.is_empty():
		var label: Label=host._text(_inspector,"上一轮\n"+"\n".join(lines),15); label.name="MilitaryEvents"

## 名字已经由只读来源按运行人物解析，未知事件ID仍保留原诊断回退。
func _unit_name(id: String) -> String:
	return units[id].display_name if units.has(id) else id

## 规则失败码转为可读指令原因，未知定义仍清楚失败并保留诊断码，不假称行动成功。
static func reason(code: String) -> String:
	var aliases: Dictionary={"HexDestinationUnreachable":"MilitaryPathUnavailable","MilitaryFriendlyTarget":"MilitaryInvalidTarget","MilitaryTargetNotEnemy":"MilitaryInvalidTarget","MilitaryTacticNotKnown":"MilitaryTacticUnavailable","NotEnoughMilitaryTacticPoints":"MilitaryNotEnoughTacticPoints","MilitaryActionRequiresHold":"MilitaryMustHoldPosition","MilitaryChargeNeedsMovement":"MilitaryChargeMustMove","MissingMilitaryUnitOrder":"MilitaryOrdersIncomplete","MilitaryUnitUnauthorized":"MilitaryOrderOutsideScope"}
	code=aliases.get(code,code)
	return {"NotEnoughMilitaryTacticPoints":"本部此次行动可用指挥资源不足。","MilitaryCommandResourcesCapacityRequired":"此规则要求作者明确配置100点指挥资源上限。","MilitaryCommandResourcesSkillHistoryMismatch":"原入战兵法来源不一致，不能补写当前技艺。","MilitaryRoutAutomaticOrder":"此部只能按真实退路自动撤离。","MilitaryRoutRetreatPending":"士气归零，轮到该部的真实行动时自动撤离。","MilitarySequentialUnitUnavailable":"前序行动已令该部退离或失能，本轮不补攻击。","MilitaryZoneRetreatCannotAttack":"起点受敌控制，后撤不能同时攻击。","MilitaryZoneRetreatRequired":"受控位置须向敌控制区外后撤。","MilitaryRoutReachedExit":"已沿本方退路离场，保留现有兵员。","MilitaryRoutRetreatMoved":"正在沿真实退路撤离。","MilitaryRoutRetreatBlocked":"本轮退路受阻。","MilitaryRoutBlockedCaptured":"连续两轮退路被阻，已由相邻敌军接管俘虏。","MilitaryCommanderKilled":"军官在本轮实际阵亡，原指挥或旅行已终止。","MilitaryCommanderWounded":"军官在本轮负伤。","MilitaryCommanderCaptured":"旅行领队在本轮被俘。","MilitaryCommanderDeserted":"旅行领队在本轮离散，原旅行终止。","MilitaryFeintNeedsRetreat":"佯退须向轮初朝向的后方移动，不能原地或前进。","MilitaryFlankNeedsMovement":"侧击须实际移动至少两格，地形消耗不代替步数。","MilitaryFlankNeedsSideOrRear":"侧击须从目标侧后攻击；共同移动后会再次复核。","UnexpectedMilitaryTarget":"此行动不接受攻击目标，请重新选择命令或战法清除目标。","MilitarySupportRequiresOtherUnit":"协防须选择另一支邻接友部。","MilitarySupportUnavailable":"请点选仍有战力、尚未离场的邻接友部。","MilitarySupportRequiresAlly":"协防只能保护友军。","MilitarySupportRequiresAdjacent":"协防目标必须与本部相邻。","MilitaryTargetOutOfRange":"目标不在当前射程内。","MilitarySightBlocked":"地形挡住了攻击视线。","MilitaryPathUnavailable":"落点不可达：路途、地形或其他部队阻挡。","MilitaryMoveTooFar":"落点超过本轮移动范围。","MilitaryDestinationOccupied":"落点已有部队。","MilitaryInvalidTarget":"请选择一支可攻击的敌军。","UnknownMilitaryTarget":"请点选攻击目标。","MilitaryTargetUnavailable":"该目标已离场或失去战力。","MilitaryNotEnoughTacticPoints":"战法点不足。","MilitaryTacticUnavailable":"此兵种不能施展该战法。","MilitaryMustHoldPosition":"此行动必须在原地进行。","MilitaryChargeMustMove":"突击必须移动后攻击。","MilitaryExitRequired":"退兵落点必须是本方退路。","MilitaryWithdrawalForbidden":"本次目标不允许自行退兵。","MilitaryUnitUnavailable":"此部已退离或失去战力。","MilitaryOrderOutsideScope":"此部不在你的指挥权限内。","MilitaryOrdersIncomplete":"请为所有可控部队安排命令。","MilitaryMoveBlocked":"行军受阻。","Miss":"未命中。","Hit":"已经命中。","AlreadyAccounted":"目标已失去战力。","ProtectedMilitaryUnitLost":"保护部队已失去全部战力。","EscortDelivered":"保护部队已经安全离场。","RetreatCovered":"已经完成掩护撤退。","MilitaryBreakthrough":"已经达到突破所需部数。","MilitaryPointHeld":"已经连续守住目标地点。","MutualMilitaryRout":"双方均已失去战力。","MilitarySideRepelled":"目标阵营已被击退。","EnemyMilitaryRepelled":"对方作战部队已被击退。","MilitaryObjectiveTimeout":"规定轮数内未完成目标。","MilitaryBattleNotPlanning":"此战已经结算。"}.get(code,"命令暂不可用（%s）。" % code)
