## 个人战界面保留独立草案并提交统一世界命令；面板不计算伤势、奖励、时间或AI计划。
extends Control
const Canvas = preload("res://ui/personal_battle_canvas.gd")
const BattleView = preload("res://core/views/personal_battle_view.gd")
const Art = preload("res://ui/art_catalog.gd")
const BladePoseBinding = preload("res://ui/adult_blade_pose_binding.gd")
const PlaybackInput = preload("res://ui/personal_battle_playback_input.gd")
const Playback = preload("res://ui/personal_battle_playback.gd")
var host
var page_input: Dictionary = {}
var battle: Dictionary = {}
var data: Dictionary = {}
var draft: Dictionary = {}
var selected_id: String = ""
var canvas: Control
var _preview: Label
var _target: Label
var _heading_label: Label
var _heading_slider: HSlider
var _submit: Button
var _actions: Dictionary = {}
var _skills: OptionButton
var _skill_ids: Array = []
var _committing: bool = false
var _stale: bool = false
var _bound_kernel_id: int = 0
var _bound_scene_epoch: int = 0
var _bound_load_generation: int = 0
var _draft_signature: String = ""
var _tactical_preview: Dictionary = {}
var _playback: RefCounted
var _playback_clip: Dictionary = {}
var _playback_label: Label
var _playback_pause_button: Button
var _playback_speed_button: Button
var _playback_skip_button: Button
var _plan_allowed: bool = false

## 查找包含玩家的未关闭会话，优先玩家控制者；只读会话身份，不凭地点或剧情名称猜测战斗。
static func active(state: Dictionary) -> String:
	return BattleView.active(state)

## 把真实会话装入场景下的界面节点；该节点销毁不会改变会话、未交付结果或行程。
static func render(main, stage: Control, current_state: Dictionary, battle_id: String) -> Control:
	return render_view(main,stage,BattleView.for_battle(current_state,main._data,battle_id))

## 新生产入口只接当前会话有限投影；旧render复用同装配，不在节点保存整世界。
static func render_view(main,stage: Control,projection: Dictionary) -> Control:
	var view: Control = load("res://ui/personal_battle_screen.gd").new()
	view.host = main
	if not view.configure_view(projection,main._data).ok: view.free(); return null
	view.name = "PersonalBattleScreen"
	stage.add_child(view)
	view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	view._build()
	return view

## 显式有限输入供装配和原节点夹具共用；深拷不让UI草案或测试回写核心结果。
func configure_view(projection: Dictionary,definitions: Dictionary) -> Dictionary:
	if not projection.get("ok",false) or not projection.get("battle") is Dictionary: return {"ok":false,"code":"InvalidPersonalBattleViewSource"}
	page_input=projection.duplicate(true); battle=page_input.battle; data=definitions
	return {"ok":true}

## 仅同一读档代数和公开轮次恢复UI草案；成功换世界即从真实位置开始，失败或普通重绘保留候选。
func _restore_draft() -> void:
	_bound_kernel_id = host.kernel.get_instance_id(); _bound_scene_epoch = host._scene_counter
	_bound_load_generation = int(host.get_meta("personal_battle_generation",0))
	var stored: Dictionary = host.get_meta("personal_battle_draft", {})
	var unit: Dictionary = battle.units[battle.controller_id]
	_draft_signature = "%s:%s:%s:%s:%s:%s:%s" % [_bound_kernel_id, _bound_load_generation, page_input.actor_id, battle.id, battle.round, battle.phase, page_input.revision]
	if stored.get("signature") == _draft_signature and stored.get("plan") is Dictionary:
		draft = stored.plan.duplicate(true); selected_id = str(stored.get("selected_id", ""))
	else:
		var targets: Array = page_input.selectable_target_ids
		selected_id = targets[0] if not targets.is_empty() else ""
		draft = {"destination":unit.position.duplicate(),"heading":unit.heading,"action":"attack" if not unit.weapon_id.is_empty() and not selected_id.is_empty() else "wait","target_id":selected_id,"skill_id":""}
		if draft.action == "wait": draft.target_id = ""
	_sanitize_selection()
	_store_draft()

## 目标池只读个人投影；退出人物不能从旧选中或已缓存target_id重新成为攻击对象，友方仍可查看但不是攻击目标。
func _sanitize_selection() -> void:
	var present: Array = page_input.present_actor_ids
	var targets: Array = page_input.selectable_target_ids
	if not selected_id in present: selected_id = targets[0] if not targets.is_empty() else ""
	var skill: Dictionary = data.personal_skills.get(draft.get("skill_id",""),{})
	var attacking: bool = draft.get("action") == "attack" or (draft.get("action") == "skill" and skill.get("kind") == "strike")
	draft.target_id = selected_id if attacking and selected_id in targets else ""
	if attacking and targets.is_empty(): draft.action = "wait"; draft.skill_id = ""; draft.target_id = ""

## 草案绑定真实Kernel、玩家和revision；重绘可恢复同版本草案，但旧页面不能删除新页面的候选。
func _store_draft() -> void:
	host.set_meta("personal_battle_draft",{"signature":_draft_signature,"view_epoch":_bound_scene_epoch,"plan":draft.duplicate(true),"selected_id":selected_id})

## 输入/提交前比对原会话与成功读档代数；即使同Kernel和revision恢复，也不允许旧计划重放。
func _matches_live_context() -> bool:
	if _stale or is_queued_for_deletion() or host.kernel.get_instance_id() != _bound_kernel_id or host._scene_counter != _bound_scene_epoch: return false
	if int(host.get_meta("personal_battle_generation",0)) != _bound_load_generation: return false
	return _same_source(host.kernel.personal_battle_context(battle.id))

## 比对原绑定，不用较新投影刷新旧页面的actor/revision或战场权限。
func _same_source(value: Dictionary) -> bool:
	return value.get("ok",false) and value.get("revision")==page_input.revision and value.get("actor_id")==page_input.actor_id and value.get("battle_id")==battle.id and value.get("round")==battle.round and value.get("phase")==battle.phase and value.get("controller_id")==battle.controller_id

## 失效页只清自己的元数据并请求现有宿主重绘；不覆盖新页草案、不发送命令。
func _invalidate_draft() -> void:
	if _stale: return
	_stale = true
	if _playback != null: _playback.invalidate()
	var stored: Dictionary = host.get_meta("personal_battle_draft",{})
	if stored.get("signature") == _draft_signature and stored.get("view_epoch") == _bound_scene_epoch: host.remove_meta("personal_battle_draft")
	if _submit != null: _submit.disabled = true
	if _preview != null: _preview.text = reason("StalePreview")
	host.call_deferred("_render")

## 复用原Kernel纯预览出口与原actor权限；新展示投影不能代替控制者门或版本门。
func _command_preview(kind: String, params: Dictionary) -> Dictionary:
	if not _matches_live_context(): return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary = host.kernel.preview(kind,params,page_input.actor_id)
	if quote.get("revision") != page_input.revision: return {"ok":false,"code":"StalePreview"}
	return quote

## 只有当前玩家的活规划页可修改草案，旁观和旧页都不会改写别人的候选。
func _can_edit() -> bool:
	if _committing or _playback_active() or battle.phase != "planning" or battle.controller_id != page_input.actor_id: return false
	if not _matches_live_context(): _invalidate_draft(); return false
	return true

## 主体保留完整战场，右侧仅安排当前目标与本轮决定，统一顶栏存档入口仍由原场景提供。
func _build() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_restore_draft()
	var matte: ColorRect = ColorRect.new(); matte.color = Color("1b2521"); matte.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(matte); matte.anchor_top = 0.085; matte.anchor_right = 1.0; matte.anchor_bottom = 1.0
	canvas = Canvas.new(); canvas.battle = battle; canvas.data = data; canvas.draft = draft; canvas.selected_id = selected_id
	# 内容摘要是当前Kernel已有公开身份；缺失时不启用固定Alpha表现。
	var content_digest: String = ""
	if host.kernel.has_method("content"):
		var content_identity: Variant = host.kernel.content()
		if content_identity is Dictionary and content_identity.get("content_digest") is String: content_digest = content_identity.content_digest
	for actor_id in battle.units:
		canvas.battle_descriptors[actor_id] = Art.character_input(page_input.character_inputs[actor_id], "battle", BladePoseBinding.public_context(actor_id, battle.units[actor_id], Art.catalog(), content_digest))
		canvas.identity_descriptors[actor_id] = Art.character_input(page_input.character_inputs[actor_id], "avatar", {"resolution": [64, 80]})
	_prepare_playback()
	canvas.playback = _playback; canvas.playback_clip = _playback_clip.duplicate(true)
	canvas.playback_frame_changed.connect(_playback_frame_changed)
	if not _canvas_inputs(): canvas.free(); canvas=null; return
	add_child(canvas); canvas.anchor_left = 0.015; canvas.anchor_right = 0.748; canvas.anchor_top = 0.10; canvas.anchor_bottom = 0.92
	canvas.destination_chosen.connect(_choose_destination); canvas.actor_chosen.connect(_choose_actor); canvas.heading_chosen.connect(_choose_heading)
	var help: Label = host._text(self, "左键选择落点或人物 · 右键调整朝向 · 白圈为移动上限" if battle.phase == "planning" else "交锋已结束，确认结果后返回原处。", 16)
	help.anchor_left = 0.028; help.anchor_right = 0.73; help.anchor_top = 0.925; help.anchor_bottom = 0.98
	var shade: ColorRect = ColorRect.new(); shade.color = Color(0.055, 0.08, 0.065, 0.98); shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(shade); shade.anchor_left = 0.76; shade.anchor_right = 1.0; shade.anchor_top = 0.095; shade.anchor_bottom = 1.0
	var scroll: ScrollContainer = ScrollContainer.new(); scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; scroll.follow_focus = true
	add_child(scroll); scroll.anchor_left = 0.775; scroll.anchor_right = 0.985; scroll.anchor_top = 0.12; scroll.anchor_bottom = 0.98
	var box: VBoxContainer = VBoxContainer.new(); box.size_flags_horizontal = Control.SIZE_EXPAND_FILL; box.add_theme_constant_override("separation", 10); scroll.add_child(box)
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	host._text(box, host._name(arena), 25)
	var objective: Dictionary = data.personal_objectives[battle.objective_id]
	var kinds: Dictionary = {"sparring": "切磋", "skirmish": "交锋", "escort": "护卫"}
	host._text(box, "%s · 第 %d / %d 回合" % [kinds.get(objective.kind, "交锋"), battle.round, objective.max_rounds], 17)
	host._text(box, _goal_text(page_input.objective), 17)
	_build_playback_controls(box)
	host.status_label = host._text(box, host._message, 16)
	host.status_label.visible = not host._message.is_empty()
	if battle.phase == "result":
		_result(box); return
	var controller: Dictionary = battle.units[battle.controller_id]
	var condition: Label=host._text(box, "%s　体力 %d/%d\n气势 %d / 100 · %s" % [_name(battle.controller_id), controller.hp, controller.initial_hp, controller.stamina,_armor_summary(battle.controller_id,true)], 18)
	condition.name="PersonalBattleCondition"
	if not _status_words(controller).is_empty(): host._text(box, _status_words(controller), 16)
	var weapon: Dictionary = data.items.get(controller.weapon_id, {})
	var weapon_text: String = "未装备武器" if weapon.is_empty() else host._name(weapon)
	if weapon.has("weapon"): weapon_text += " · 距离 %d–%d" % [weapon.weapon.minimum_range, weapon.weapon.maximum_range]
	host._text(box, weapon_text, 16)
	if controller.recovery_until > battle.round: host._text(box, "正在装填，本轮需原地防御或等待。", 16)
	_target = host._text(box, "", 19); _target.name = "PersonalBattleTarget"
	var grid: GridContainer = GridContainer.new(); grid.columns = 2; grid.add_theme_constant_override("h_separation", 8); grid.add_theme_constant_override("v_separation", 7); box.add_child(grid)
	for entry in [["attack", "攻击"], ["defend", "防御"], ["wait", "等待"], ["withdraw", "退避"]]:
		var button: Button = host._button(grid, entry[1], "battle_action", entry[0], _choose_action.bind(entry[0]))
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL; button.toggle_mode = true; _actions[entry[0]] = button
	_skills = OptionButton.new(); _skills.name = "PersonalBattleSkill"; _skills.custom_minimum_size.y = 38; _skills.add_theme_font_size_override("font_size", 17); box.add_child(_skills)
	_skills.add_item("选择已掌握的秘技"); _skill_ids.append("")
	for id in controller.learned:
		var skill: Dictionary = data.personal_skills.get(id, {})
		if controller.learned[id] <= 0 or not skill.get("kind") in ["strike", "guard"]: continue
		if not weapon.has("weapon") or not weapon.weapon.category in skill.weapons: continue
		_skills.add_item("%s · 气势%d" % [host._name(skill), skill.stamina_cost]); _skill_ids.append(id)
		if draft.skill_id == id: _skills.select(_skill_ids.size() - 1)
	_skills.disabled = _skill_ids.size() == 1; _skills.item_selected.connect(_choose_skill)
	var heading_box: HBoxContainer = HBoxContainer.new(); box.add_child(heading_box)
	_heading_label = host._text(heading_box, "", 16); _heading_label.custom_minimum_size.x = 74
	_heading_slider = HSlider.new(); _heading_slider.name = "PersonalBattleHeading"; _heading_slider.min_value = 0; _heading_slider.max_value = 359; _heading_slider.step = 1; _heading_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL; _heading_slider.custom_minimum_size.y = 30
	heading_box.add_child(_heading_slider); _heading_slider.value_changed.connect(func(value): _choose_heading(int(value)))
	_preview = host._text(box, "", 17); _preview.name = "PersonalBattlePreview"; _preview.custom_minimum_size.y = 62
	_submit = host._button(box, "确认本轮", "battle_submit", battle.id, _submit_plan)
	_submit.custom_minimum_size.y = 48
	host._button(box, "重新选择", "battle_reset", battle.id, _reset_plan)
	if battle.controller_id != page_input.actor_id: host._text(box, "由" + _name(battle.controller_id) + "指挥，本人随队观战。", 16)
	_events(box)
	_equipment_section(box)
	_refresh()

## 结果只展示核心已结算事实；返回仍交给 battle_close，面板不补发报酬或擅自恢复行程。
func _result(box: VBoxContainer) -> void:
	var quote: Dictionary = _command_preview("battle_close",{"battle_id":battle.id})
	var projected: Dictionary = quote.get("result_preview",page_input.result)
	var labels: Dictionary = {"victory":"此战获胜","defeat":"此战失利","protected_lost":"护卫未成","withdrawn":"已经退离","draw":"未分胜负"}
	host._text(box,labels.get(projected.get("outcome",battle.outcome),"交锋结束"),30)
	if battle.outcome == "protected_lost": host._text(box,"保护对象已经倒下，需承担这次护卫的后果。",18)
	for person in projected.get("participants",[]):
		var status: String = {"withdrawn":" · 已离场","down":" · 倒地","standing":""}.get(person.status,"")
		host._text(box,"%s　体力 %d / %d%s" % [_name(person.actor_id),person.hp,person.initial_hp,status],18)
	var report: PackedStringArray = _events(box,projected.get("events",[]),false)
	_submit = host._button(box,"返回原处，继续生活","battle_close",battle.id,_close_result,not quote.ok or host._advancing or battle.controller_id != page_input.actor_id)
	_event_details(box,report)
	_equipment_section(box)

## 护具详情在原常用操作之后按需展开，点按可读；不移动确认区、不读取新报价或修改草案。
func _equipment_section(box: VBoxContainer) -> void:
	var toggle: Button=Button.new(); toggle.name="PersonalBattleEquipmentToggle"; toggle.text="护具与伤害说明"; toggle.toggle_mode=true
	toggle.custom_minimum_size.y=44
	toggle.add_theme_font_size_override("font_size",16); box.add_child(toggle)
	var details: Label=host._text(box,_equipment_details(),16); details.name="PersonalBattleEquipmentDetails"
	details.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; details.visible=false
	toggle.toggled.connect(func(opened: bool): details.visible=opened)

## 只有页面明确授予的本人冻结数据可显示精确防御；未知他人不默认为零或借装备画面推断。
func _armor_summary(actor_id: String,compact: bool=false) -> String:
	var source: Dictionary=page_input.get("equipment_feedback",{})
	if actor_id!=page_input.actor_id or source.get("actor_id")!=actor_id: return "护具未知"
	return ("甲防%d" if compact else "护具防御 %d") % source.armor_defense

## 展示正式快照的来源与作用，不把护具值说成最终挡伤量，也不在UI重算任何伤害公式。
func _equipment_details() -> String:
	var source: Dictionary=page_input.get("equipment_feedback",{})
	if source.get("actor_id")!=page_input.actor_id: return "护具信息尚未公开。"
	var title: String="本场无有效护具" if source.armor_id.is_empty() else host._name(data.items[source.armor_id])
	return "%s：%s\n护具防御 %d（入战时固定）。\n护具防御作用于基础伤害；最终伤害见当前预览与已结算简报。\n其他参战者的护具尚未公开。" % [_name(source.actor_id),title,source.armor_defense]

## 原目标语义决定必要提示，不把出口护送错误显示成歼敌，也不宣称战术胜利会直接发任务钱。
func _goal_text(goal: Dictionary) -> String:
	var text: String = "击倒所有对手。"
	if goal.goal_rule == "protected_exit": text = "护住保护对象，使其实际抵达己方出口；战术胜利不代替旅程送达。"
	elif goal.goal_rule == "protect_and_defeat_opponents": text = "护住保护对象，击倒对手。"
	if goal.timeout_rule == "protected_exit_or_truce": text += "超时未到则停战。"
	if goal.get("mutual_defeat_rule", "") == "draw_v1": text += "本人和全部对手同时倒地时判平局；对手撤离不算同时倒地。"
	if goal.allow_withdrawal: text += "安全退避仍须满足己方出口条件。"
	return text

## 默认保留原最近三条简报；完整内容只来自同一公开事件，结果页把展开区放在确认之后。
func _events(box: VBoxContainer, supplied: Array = [], expandable: bool = true) -> PackedStringArray:
	var actual: Array = supplied if not supplied.is_empty() else page_input.events
	var lines: PackedStringArray = _event_lines(actual)
	if not lines.is_empty(): host._text(box,"\n".join(lines.slice(maxi(0,lines.size()-3))),16)
	if expandable: _event_details(box,lines)
	return lines

## 点按或键盘展开完整最近一轮；纯显示开合，不重查世界、恢复草案或移动已有确认控件。
func _event_details(box: VBoxContainer, lines: PackedStringArray) -> void:
	if lines.is_empty(): return
	var toggle: Button = Button.new(); toggle.name = "PersonalBattleEventsToggle"; toggle.toggle_mode = true
	var caption: String = "完整简报 · %d条" % lines.size()
	toggle.text = caption; toggle.custom_minimum_size.y = 44; toggle.add_theme_font_size_override("font_size",16)
	box.add_child(toggle)
	var details: Label = host._text(box,"\n".join(lines),16); details.name = "PersonalBattleEventsDetails"
	details.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; details.visible = false
	toggle.toggled.connect(func(opened: bool):
		details.visible = opened
		toggle.text = "收起完整简报" if opened else caption)

## 将统一结果语义映射到原文本控件；本函数只读并可在无图形资源的节点门验证。
func _event_lines(actual: Array) -> PackedStringArray:
	var lines: PackedStringArray=[]
	for event in actual:
		if event.get("action") in ["attack","skill","counter"] and event.has("target_id"):
			var value: String = ("反击伤害 %d" if event.action=="counter" else "伤害 %d") % event.damage if event.get("damage",0)>0 else reason(event.get("code",""))
			lines.append("%s → %s：%s" % [_name(event.actor_id),_name(event.target_id),value])
		elif event.get("action")=="withdraw": lines.append(_name(event.actor_id)+"："+reason(event.get("code","")))
		elif event.get("action")=="objective": lines.append(reason(event.get("code","")))
		elif event.get("action")=="stamina": lines.append("%s 气势+%d" % [_name(event.actor_id),event.amount])
	return lines

## 战果确认仍委托原命令，仅在绑定会话和revision没有变化时发送一次。
func _close_result() -> void:
	if _committing: return
	var quote: Dictionary = _command_preview("battle_close",{"battle_id":battle.id})
	if not quote.ok:
		if quote.get("code")=="StalePreview": _invalidate_draft()
		return
	_committing=true
	if _submit != null: _submit.disabled=true
	host._perform("battle_close",{"battle_id":battle.id})

## 地面点击仅更新候选位置；无效点保留以解释原因，提交按钮通过同一规则禁用。
func _choose_destination(point: Array) -> void:
	if not _can_edit(): return
	draft.destination = point.duplicate(); _refresh()

## 人物选择改变查看目标；敌方目标自动朝向其公开位置，选择本人则回到原地草案。
func _choose_actor(id: String) -> void:
	if not _can_edit() or not id in page_input.present_actor_ids: return
	selected_id = id
	if id == battle.controller_id: draft.destination = battle.units[id].position.duplicate()
	else:
		var point: Array = battle.units[id].position
		var angle: float = atan2(float(point[1]) - draft.destination[1], float(point[0]) - draft.destination[0])
		draft.heading = posmod(roundi(rad_to_deg(angle)), 360)
	_refresh()

## 调整朝向只影响本轮草案和范围预览，不旋转权威人物或重新生成AI计划。
func _choose_heading(heading: int) -> void:
	if not _can_edit(): return
	draft.heading = heading; _refresh()

## 基础行动清除主动技能，仍保留已选人物作为查看对象。
func _choose_action(action: String) -> void:
	if not _can_edit(): return
	if action == "defend": draft.destination = battle.units[battle.controller_id].position.duplicate()
	draft.action = action; draft.skill_id = ""; _skills.select(0); _refresh()

## 本领选择使用人物实际学习记录，类型决定是否需要敌方目标。
func _choose_skill(index: int) -> void:
	if not _can_edit() or index >= _skill_ids.size(): return
	if index <= 0: _choose_action("wait"); return
	draft.action = "skill"; draft.skill_id = _skill_ids[index]
	if data.personal_skills[draft.skill_id].get("requires_stationary",false): draft.destination = battle.units[battle.controller_id].position.duplicate()
	_refresh()

## 重选从真实位置与朝向重新开始，不回滚已经提交的战斗轮次。
func _reset_plan() -> void:
	if not _can_edit(): return
	var unit: Dictionary = battle.units[battle.controller_id]
	draft.destination = unit.position.duplicate(); draft.heading = unit.heading
	_choose_action("attack" if not unit.weapon_id.is_empty() else "wait")

## 汇合同源合法性与伤害查询，所有预览状态仅存宿主节点元数据，保存世界不包含UI草案。
func _refresh() -> void:
	if not _matches_live_context(): _invalidate_draft(); return
	_sanitize_selection()
	_store_draft() # 纯预览或几何异步等待会触发重绘，必须先保留本次同源用户选择。
	var skill: Dictionary = data.personal_skills.get(draft.skill_id,{})
	var params: Dictionary = draft.duplicate(true); params.battle_id = battle.id
	var valid: Dictionary = _command_preview("battle_plan",params)
	_tactical_preview = valid.get("tactical_preview",{})
	if not _canvas_inputs(): return
	canvas.draft = draft; canvas.selected_id = selected_id; canvas.tactical_preview = _tactical_preview; canvas.refresh_preview()
	if _preview == null: _store_draft(); return
	_heading_slider.set_value_no_signal(draft.heading); _heading_label.text = "朝向 %d°" % draft.heading
	for action in _actions: _actions[action].set_pressed_no_signal(action == draft.action)
	var target: Dictionary = battle.units.get(selected_id,{})
	_target.text = "点选一名对手" if target.is_empty() else "%s　体力 %d\n气势 %d%s" % [_name(selected_id),target.hp,target.stamina," · 保护对象" if target.protected else ""]
	if not target.is_empty(): _target.text += " · "+_armor_summary(selected_id)
	if not target.is_empty() and not _status_words(target).is_empty(): _target.text += "\n"+_status_words(target)
	_plan_allowed = valid.ok and not host._advancing and not _committing and battle.controller_id == page_input.actor_id
	_submit.disabled = not _plan_allowed or _playback_active()
	if not valid.ok: _preview.text = reason(valid.get("code","InvalidPlan"))
	elif _tactical_preview.has("attack"):
		var attack: Dictionary = _tactical_preview.attack
		if attack.ok:
			var rows: PackedStringArray=[]
			for affected in attack.get("targets",[{"target_id":draft.target_id,"damage":attack.damage,"hit_bp":attack.get("hit_bp",10000)}]):
				var chance: String = " · 命中%.2f%%" % (affected.hit_bp/100.0) if affected.has("hit_bp") else ""
				rows.append("%s：预计%d%s" % [_name(affected.target_id),affected.damage,chance])
			_preview.text = ("当前区域目标：\n" if attack.has("targets") else "")+"\n".join(rows)+"\n一次气势%d；共同移动和防御后再判定。" % _tactical_preview.nominal_stamina_cost
		else: _preview.text = reason(attack.get("code",""))+"\n移动后重新判断；此次预览不是已发生的命中。"
	elif draft.action == "withdraw": _preview.text = "需实际抵达己方退路且本轮未受近战命中；护卫退避须保护对象同行。"
	elif draft.action == "defend": _preview.text = "原地转向防御，只减轻正面伤害。"
	elif draft.action == "skill": _preview.text = "使用"+host._name(skill)+("，仅正面守势。" if _tactical_preview.get("guard_facing")=="front" else "守护自身。")
	else: _preview.text = "等待恢复气势；移动等待在新模型中不算原地休息。"

## 正式quote已给战术数据时直接复用；仅原画布回退需要核心候选，动画帧不查询世界。
func _canvas_inputs() -> bool:
	canvas.configure_view(battle,data,page_input.present_actor_ids)
	canvas.geometry_input={}
	if battle.phase=="planning" and _tactical_preview.is_empty():
		var projected: Dictionary=host.kernel.personal_battle_geometry(battle.id,draft)
		if projected.get("code")=="NativeAsyncRequired":
			if _submit!=null: _submit.disabled=true
			return false # 保留当前同源草案，等纯读到达后重绘。
		if not _same_source(projected): _invalidate_draft(); return false
		canvas.geometry_input=projected
	return true

## 点击一次只发送一次正式计划，宿主负责事务、保存和重建画面；不直接调用 resolve 修改世界。
func _submit_plan() -> void:
	if not _can_edit() or _submit.disabled: return
	_sanitize_selection()
	var params: Dictionary = draft.duplicate(true); params.battle_id = battle.id
	var quote: Dictionary = _command_preview("battle_plan",params)
	if not quote.ok:
		if quote.get("code")=="StalePreview": _invalidate_draft()
		else: _refresh()
		return
	_committing = true; _submit.disabled = true
	host._perform("battle_plan",params)

## 人物名称来自内容文本，缺失文本时保留稳定ID便于作者定位。
func _name(actor_id: String) -> String:
	return page_input.display_names.get(actor_id,host._name(data.actors.get(actor_id,{"id":actor_id})))

## 仅显示仍在期限内的状态及剩余轮数，画面重绘不会延长或移除实际状态。
func _status_words(unit: Dictionary) -> String:
	var words: PackedStringArray = []
	for status in unit.statuses:
		if unit.statuses[status] >= battle.round: words.append("%s %d轮" % [{"slow": "迟缓", "exposed": "破绽"}.get(status, status), unit.statuses[status] - battle.round + 1])
	return " · ".join(words)

## 将规则失败码转为操作原因；未覆盖代码保留诊断信息，不把失败显示成成功。
static func reason(code: String) -> String:
	var labels: Dictionary = {"StalePreview":"战斗状态已变化，请重新选择本轮计划。","ActorWithdrawn":"此人已经离场。","DefendRequiresStationary":"普通防御必须留在原地，可调整朝向。","StrikeRequiresStationary":"这项秘技必须原地发动。","OutsideStrikeArea":"目标不在这项秘技的当前区域内。","Miss":"本次攻击未命中。","Withdrawn":"已经从真实出口离场。","WithdrawalMeleeHit":"本轮受到近战命中，暂不能安全离场。","ProtectedExitNotReached":"保护对象尚未抵达己方出口。","EscortExitReached":"保护对象已抵达战场出口；仍须继续真实护送旅程。","EscortExitTimeout":"护卫战超时，保护对象尚未到出口，本次停战。","MoveTooFar": "落点超出本轮移动范围。", "MovementBlocked": "途中有障碍，无法这样移动。", "InvalidDestination": "落点被障碍阻挡或超出场地。", "InsideMinimumRange": "目标太近，处于武器内射程。", "OutOfRange": "目标目前在攻击范围外。", "WrongFacing": "当前朝向无法攻击目标。", "LineBlocked": "障碍挡住了攻击视线。", "NotEnoughStamina": "气势不足，请防御或等待恢复。", "WeaponRecovering": "武器尚在装填，本轮需原地防御或等待。", "FriendlyTarget": "请选择敌方人物作为攻击目标。", "InvalidTarget": "请选择仍能交手的目标。", "NoWeapon": "没有装备武器，可移动、防御或等待。", "SkillNotLearned": "尚未掌握这项本领。", "SkillWeaponMismatch": "这项本领不适合当前武器。", "WithdrawalForbidden": "此次交锋不能从退路离场。", "ExitNotReached": "尚未抵达己方退路。", "InvalidPlan": "请重新选择落点和朝向。"}
	return labels.get(code, "无法执行：" + code)

## 表现绑定只取当前kernel与公开来源，旧世界/读档代数不能借同battle_id续播。
func _playback_binding() -> Dictionary:
	var source: Dictionary = host.kernel.source_identity() if host.kernel.has_method("source_identity") else {}
	return {"kernel_id":host.kernel.get_instance_id(), "load_generation":int(host.get_meta("personal_battle_generation",0)), "actor_id":page_input.actor_id, "world_id":str(source.get("world_id","")), "execution_epoch":int(source.get("execution_epoch",0))}

## 原生读取已缓存来源，GD读取命令上下文；回放帧不排新查询、不读取AI计划。
func _playback_current() -> bool:
	if _stale or is_queued_for_deletion() or not is_instance_valid(host) or not is_instance_valid(host.kernel): return false
	if host._scene_counter != _bound_scene_epoch or host.kernel.get_instance_id() != _bound_kernel_id: return false
	if _playback_binding() != _playback_clip.get("binding",{}): return false
	var context: Dictionary = host.kernel.command_context()
	return context.get("revision",-1) == page_input.revision and context.get("actor_id","") == page_input.actor_id

## 同一页重绘恢复纯UI进度；新轮只由合法上一公开帧建片，冷恢复直接终态。
func _prepare_playback() -> void:
	var binding: Dictionary = _playback_binding()
	var previous: Dictionary = host.get_meta("personal_battle_playback_frame",{})
	var saved: Dictionary = host.get_meta("personal_battle_playback_state",{})
	var resume: Dictionary = {}
	if saved.get("binding") == binding and saved.get("revision") == page_input.revision and saved.get("clip",{}).get("battle_id") == battle.id:
		_playback_clip = saved.clip.duplicate(true); resume = saved.get("state",{}).duplicate(true)
	else:
		_playback_clip = PlaybackInput.prepare(previous,page_input,binding)
	_playback = Playback.new()
	_playback.configure(_playback_clip,resume)
	host.set_meta("personal_battle_playback_frame",PlaybackInput.capture(page_input,binding))
	_store_playback()

## UI快照只存在宿主元数据，未通过当前来源核验的旧页不能覆盖新页进度。
func _store_playback() -> void:
	if _playback == null or not _playback_current(): return
	host.set_meta("personal_battle_playback_state",{"clip":_playback_clip.duplicate(true), "state":_playback.snapshot(), "revision":page_input.revision, "binding":_playback_binding()})

## 公开有限只读状态供控件与集成检查使用，不给调用者可写播放器内部字典。
func playback_frame() -> Dictionary:
	return _playback.frame() if _playback != null else {"active":false,"complete":true}

## 暂停也属于未完成回放；只有跳过或完成后才可确认下一轮。
func _playback_active() -> bool:
	return _playback != null and not bool(_playback.frame().get("complete",true))

## 操作固定放在当前目标之后，不藏在事件长日志底部，也不代替战果确认。
func _build_playback_controls(box: VBoxContainer) -> void:
	_playback_label = host._text(box,"",16); _playback_label.name = "PersonalBattlePlaybackStatus"
	var row: HFlowContainer = HFlowContainer.new(); row.name = "PersonalBattlePlaybackControls"; box.add_child(row)
	_playback_pause_button = host._button(row,"暂停回放","battle_playback_pause",battle.id,_toggle_playback_pause)
	_playback_speed_button = host._button(row,"2倍播放","battle_playback_speed",battle.id,_toggle_playback_speed)
	_playback_skip_button = host._button(row,"跳过回放","battle_playback_skip",battle.id,_skip_playback)
	_update_playback_controls()

## 本地时钟变更后只重排本页文字和确认状态，不再次请求报价或提交命令。
func _update_playback_controls() -> void:
	if _playback_label == null: return
	var frame: Dictionary = playback_frame()
	var active: bool = not frame.get("complete",true)
	var labels: Dictionary = {"prepare":"准备", "move":"位置过渡", "strike":"出手", "impact":"交锋反馈", "state":"状态变化", "settle":"轮末"}
	_playback_label.text = ("回放 · " + labels.get(frame.get("phase_kind","settle"),"轮末") + (" · 已暂停" if frame.get("paused",false) else "")) if active else "已显示本轮结果"
	_playback_pause_button.text = "继续回放" if frame.get("paused",false) else "暂停回放"
	_playback_speed_button.text = "正常速度" if frame.get("speed",1.0) == 2.0 else "2倍播放"
	for button in [_playback_pause_button,_playback_speed_button,_playback_skip_button]: button.disabled = not active
	if _submit != null and battle.phase == "planning": _submit.disabled = not _plan_allowed or active

## 画布仅报告本地帧；源变化立即终止旧播放器且不改新宿主元数据。
func _playback_frame_changed(_frame: Dictionary) -> void:
	if not _playback_current():
		if _playback != null: _playback.invalidate()
		return
	_store_playback(); _update_playback_controls()

## 暂停与恢复都只改变本地表现，不能停止或推进已经结算的世界。
func _toggle_playback_pause() -> void:
	if not _playback_current() or not _matches_live_context() or not _playback_active(): return
	_playback.set_paused(not _playback.frame().get("paused",false)); _playback_controls_changed()

## 倍速仅有明确1x/2x，不使用速度改变动作收益或世界时间。
func _toggle_playback_speed() -> void:
	if not _playback_current() or not _matches_live_context() or not _playback_active(): return
	_playback.set_speed(1.0 if _playback.frame().get("speed",1.0) == 2.0 else 2.0); _playback_controls_changed()

## 跳过只显示既定终态，玩家仍须自行确认下一轮或关闭战果。
func _skip_playback() -> void:
	if not _playback_current() or not _matches_live_context() or _playback == null: return
	_playback.skip(); _playback_controls_changed()

## 将控件结果写入纯UI缓存并唤醒画布；该路径没有规则、随机流或存档调用。
func _playback_controls_changed() -> void:
	_store_playback(); _update_playback_controls()
	if is_instance_valid(canvas): canvas.apply_playback_frame()
