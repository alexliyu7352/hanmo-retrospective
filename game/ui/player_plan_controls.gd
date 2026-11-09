## 原场所中的生活安排入口；只保存本人看过的有限目标，不接受任意命令或提前占用资源。
extends RefCounted
const Session=preload("res://ui/activity_session.gd")
const KINDS=["move_local","travel","work","course","rest","continue_action","task_reminder"]
const SKILL_NAMES={"infantry":"步兵","cavalry":"骑兵","archery":"弓术","martial":"武艺","tactics":"兵法","commerce":"商术","negotiation":"议价","rites":"礼法","debate":"辩论","transport":"运输"}

## 仅新情景启用；旧存档与旧能力清单保持原样，不在UI默加计划模块。
static func enabled(host) -> bool:
	return host._resident_mode and host.has_method("_player_plan_enabled") and host._player_plan_enabled()

## 原合法来源生成一步草案，预授权独立勾选且默认关闭；报价未知时不能代填零价。
static func append(host, parent: Control, kind: String, params: Dictionary, label: String="加入生活安排") -> Control:
	if not enabled(host) or kind not in KINDS: return null
	var box=VBoxContainer.new(); box.name="PlayerPlanSource"; parent.add_child(box)
	box.set_meta("player_plan_source",true)
	box.set_meta("player_plan_kind",kind); box.set_meta("player_plan_params",params.duplicate(true))
	var session=Session.create(host,box)
	var request: Dictionary={"kind":kind,"params":params.duplicate(true),"authorized":false}
	var quoted: Dictionary=host.kernel.preview("player_plan_add",request)
	if not quoted.get("ok",false):
		host._text(box,"暂不能记入安排："+Session.reason(host,quoted),15)
		return box
	var terms: Dictionary=quoted.get("terms",{})
	if kind in ["work","course"]:
		terms_text(host,box,terms,params)
		var key: String=_draft_key(host,kind,params)
		var draft: Dictionary=host.get_meta("player_plan_source_consent",{})
		var selected: bool=draft.get("key")==key and draft.get("authorized",false)
		var consent=CheckBox.new(); consent.name="PlayerPlanPreauthorize"; consent.text="到场后，允许按上列同一提供人和原条款开始"; consent.button_pressed=selected
		consent.disabled=terms.is_empty(); consent.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; box.add_child(consent)
		host._text(box,"不勾选会在开始前停住。价格、提供人、资格或课程内容改变都须重新确认。",15)
		var buttons=VBoxContainer.new(); box.add_child(buttons)
		var shown: Dictionary=request.duplicate(true); shown.authorized=selected
		if selected: shown.terms=terms.duplicate(true)
		_append_button(host,buttons,shown,label)
		consent.toggled.connect(func(checked: bool):
			if not session.guard().get("ok",false): return
			host.set_meta("player_plan_source_consent",{"key":key,"authorized":checked})
			for child in buttons.get_children(): buttons.remove_child(child); child.queue_free()
			var edited: Dictionary=request.duplicate(true); edited.authorized=checked
			if checked: edited.terms=terms.duplicate(true)
			_append_button(host,buttons,edited,label))
	else: _append_button(host,box,request,label)
	return box

## 每次草案选择重建独立输入会话；旧按钮随子树失效，不能借新参数重发。
static func _append_button(host, parent: Control, request: Dictionary, label: String) -> void:
	var root=VBoxContainer.new(); parent.add_child(root)
	var session=Session.create(host,root)
	var button: Button=session.command(root,label,"player_plan_add",request)
	button.name="PlayerPlanAdd"; button.set_meta("player_plan_kind",request.kind)
	button.set_meta("player_plan_params",request.params.duplicate(true))

## 已有待办的许可也须显式选择后确认；保留原冻结条款，不能以新上限替换旧报价。
static func authorization(host, parent: Control, step: Dictionary) -> void:
	var root=VBoxContainer.new(); parent.add_child(root); var guard=Session.create(host,root)
	var key: String=_draft_key(host,"authorize",{"step_id":step.step_id})
	var draft: Dictionary=host.get_meta("player_plan_step_consent",{})
	var selected: bool=draft.get("authorized",false) if draft.get("key")==key else step.get("authorized",false)
	var consent=CheckBox.new(); consent.name="PlayerPlanStepPreauthorize"; consent.text="允许这一步到场后按上列原条款开始"; consent.button_pressed=selected; root.add_child(consent)
	var buttons=VBoxContainer.new(); root.add_child(buttons)
	if draft.get("key")==key: _authorize_button(host,buttons,step,selected)
	consent.toggled.connect(func(checked: bool):
		if not guard.guard().get("ok",false): return
		host.set_meta("player_plan_step_consent",{"key":key,"authorized":checked})
		for child in buttons.get_children(): buttons.remove_child(child); child.queue_free()
		_authorize_button(host,buttons,step,checked))

## 异步只读报价后恢复本次显式选择；新世界、修订或不同目标不会继承勾选。
static func _draft_key(host, kind: String, params: Dictionary) -> String:
	return JSON.stringify([host.kernel.source_identity(),kind,params],"",true)

## 更改已有一步的许可保留同一原条款；提交的Session仍核对实际页面和世界身份。
static func _authorize_button(host, parent: Control, step: Dictionary, checked: bool) -> void:
	var leaf=VBoxContainer.new(); parent.add_child(leaf); var session=Session.create(host,leaf)
	var params: Dictionary={"step_id":step.step_id,"authorized":checked}
	if checked: params.terms=step.get("terms",{}).duplicate(true)
	session.command(leaf,"确认这一步的开始许可","player_plan_authorize",params).name="PlayerPlanAuthorize"

## 只翻译已知来源ID与原生明确条款，未提供字段保未知；不从目录推算远方价格。
static func terms_text(host, parent: Control, terms: Dictionary, params: Dictionary={}) -> void:
	if terms.is_empty(): host._text(parent,"条款尚未知；到场后须重新查看。",15); return
	var context: Dictionary=params.get("context",{})
	var provider: String=str(terms.get("provider_actor_id",context.get("provider_actor_id","")))
	var place: String=str(terms.get("place_id",context.get("place_id","")))
	host._text(parent,"提供人："+known_name(host,"actors",provider)+" · 场所："+known_name(host,"places",place),16)
	host._text(parent,"原价费用："+_number(terms,"cost","钱")+" · 报酬："+_number(terms,"wage","钱")+" · 所需："+_number(terms,"duration","个半日"),16)
	var skill: String=str(terms.get("skill_id",params.get("skill_id","")))
	if not skill.is_empty():
		host._text(parent,"技艺："+SKILL_NAMES.get(skill,skill)+" · 课时："+_number(params,"ticks","个半日")+" · 目标等级："+_number(terms,"target_level","")+" · 经验上限："+_number(terms,"xp_limit",""),15)
		host._text(parent,"练习方式："+("原自动基线" if terms.get("challenge_mode",params.get("challenge_mode",""))=="automatic" else "须本人作答"),15)
		host._text(parent,"授课方式："+("原设施课程" if terms.get("mode")=="facility" else "原教师亲授"),15)
	host._text(parent,"资格要求："+("原普通修业资格" if not skill.is_empty() else "原普通短工资格")+"，开始时重新核验。",15)

## 缺失数值不等于免费或零耗时，明确显示未知并由原领域拒绝不完整授权。
static func _number(value: Dictionary, key: String, unit: String) -> String:
	return str(value[key])+unit if value.has(key) and value[key]!=null else "未知"

## 只为原生已给出的ID取作者名称，不枚举隐藏人物、库存或未传到的消息。
static func known_name(host, table: String, id: String) -> String:
	if id.is_empty(): return "未知"
	var record: Dictionary=host._data.get(table,{}).get(id,{})
	return host._name(record) if not record.is_empty() else id
