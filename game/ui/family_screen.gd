## 家庭页面只读取同一世界的真实身份、收养来源和休整行动；办理结果统一交给Kernel。
extends RefCounted
const TimeRules=preload("res://core/base/time_rules.gd")
const Art=preload("res://ui/art_catalog.gd")
const PlayerPlanControls=preload("res://ui/player_plan_controls.gd")

## 在实际场所展示本人、家人和庄中休整；传入的办理上下文仍须通过正式预览核验。
static func render(host, stage: Control, view: Dictionary) -> void:
	var frame=PanelContainer.new(); frame.name="FamilyScreen"; stage.add_child(frame)
	frame.anchor_left=.025; frame.anchor_right=.975; frame.anchor_top=.12; frame.anchor_bottom=.975
	var surface=StyleBoxFlat.new(); surface.bg_color=Color("19271ff5"); surface.border_color=Color("9e895c"); surface.set_border_width_all(1)
	for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]: surface.set_content_margin(side,18)
	frame.add_theme_stylebox_override("panel",surface)
	var columns=HBoxContainer.new(); columns.add_theme_constant_override("separation",24); frame.add_child(columns)
	var summary=VBoxContainer.new(); summary.custom_minimum_size.x=270; summary.size_flags_stretch_ratio=.28; summary.size_flags_horizontal=Control.SIZE_EXPAND_FILL; summary.add_theme_constant_override("separation",8); columns.add_child(summary)
	_identity(host,summary,view)
	var space=Control.new(); space.size_flags_vertical=Control.SIZE_EXPAND_FILL; summary.add_child(space)
	_activity(host,summary,view)
	var footer=HBoxContainer.new(); footer.add_theme_constant_override("separation",8); summary.add_child(footer)
	host._button(footer,"保存行旅","save","family",host._save,host._advancing).name="FamilySave"
	host._button(footer,"返回场所","family_close","",host._close_family).name="FamilyClose"
	var detail=VBoxContainer.new(); detail.size_flags_stretch_ratio=.72; detail.size_flags_horizontal=Control.SIZE_EXPAND_FILL; detail.add_theme_constant_override("separation",12); columns.add_child(detail)
	var tabs=HBoxContainer.new(); tabs.add_theme_constant_override("separation",10); detail.add_child(tabs)
	var selected: String=host.get_meta("family_tab","family")
	var available_tabs: Array=[["family","家庭传承"],["rest","庄中休整"]]
	if host._resident_mode and host.kernel.has_method("adult_adoption_enabled") and host.kernel.adult_adoption_enabled(): available_tabs.append(["adult_adoption","成年收养"])
	if host._resident_mode and host.kernel.has_method("voluntary_succession_enabled") and host.kernel.voluntary_succession_enabled(): available_tabs.append(["family_voluntary_paths","生前自愿接续"] if host.kernel.has_method("family_voluntary_paths_enabled") and host.kernel.family_voluntary_paths_enabled() else ["voluntary_succession","接续方案"])
	if host._resident_mode and host.kernel.has_method("family_legacy_enabled") and host.kernel.family_legacy_enabled(): available_tabs.append(["family_legacy","人生接续许可"])
	if host._resident_mode and host.kernel.has_method("family_bequest_enabled") and host.kernel.family_bequest_enabled(): available_tabs.append(["family_bequest","分项遗赠"])
	if host._resident_mode and host.kernel.has_method("family_inheritance_enabled") and host.kernel.family_inheritance_enabled(): available_tabs.append(["family_inheritance","遗赠公开与受领"])
	if host._resident_mode and host.kernel.has_method("family_marriage_enabled") and host.kernel.family_marriage_enabled(): available_tabs.append(["family_marriage","本人婚议"])
	if host._resident_mode and host.kernel.has_method("family_birth_enabled") and host.kernel.family_birth_enabled(): available_tabs.append(["family_birth","生育与照护"])
	if host._resident_mode and host.kernel.has_method("family_care_relief_enabled") and host.kernel.family_care_relief_enabled(): available_tabs.append(["family_care_relief","照护接手"])
	if host._resident_mode and host.kernel.has_method("family_apprenticeship_enabled") and host.kernel.family_apprenticeship_enabled(): available_tabs.append(["family_apprenticeship","本人师承"])
	for tab in available_tabs:
		var control: Button=host._button(tabs,tab[1],"family_tab",tab[0],_select_tab.bind(host,tab[0]))
		control.toggle_mode=true; control.button_pressed=selected==tab[0]; control.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host.status_label=host._text(detail,host._message,17); host.status_label.name="FamilyStatus"; host.status_label.custom_minimum_size.y=28
	var scroll=ScrollContainer.new(); scroll.name="FamilyDetailScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; detail.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",18); scroll.add_child(body)
	if host._resident_mode and view.action.get("kind")=="long_rest": host.ActivityContinuationControls.render(host,body,"long_rest")
	if selected=="rest": _rest(host,body,view)
	else: _family(host,body,view)

## 页签仅属于显示状态，重新排版不提交命令、增长年龄或改动家庭索引。
static func _select_tab(host, tab: String) -> void:
	host._stop_journey_auto()
	host.set_meta("family_tab",tab)
	host._render()

## 使用运行身份的实际姓名、原型立绘和周岁；未知生日保持未知，不套用原型年龄。
static func _identity(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"家事与休整",30)
	var portrait: Control=Art.character_control(Art.character_input(view.portrait_input,"half_body")); portrait.name="FamilyPortrait"; portrait.custom_minimum_size.y=140
	parent.add_child(portrait)
	host._text(parent,view.display_name,25).name="FamilyPlayerName"
	host._text(parent,_age(view.age_years)+" · "+_place(host,view.place_id),18).name="FamilyPlayerAge"
	host._text(parent,"体力 %d · 疲劳 %d"%[view.hp,view.fatigue],17)
	if not view.guardian.is_empty(): host._text(parent,"养亲 · "+view.guardian.display_name,17)
	host._text(parent,"继任人 · "+(view.heir.display_name if not view.heir.is_empty() else "尚未指定"),18).name="FamilyCurrentHeir"

## 持续行动读取真实剩余半日；自动开关只控制主场景推进，取消仍须提交正式命令。
static func _activity(host, parent: Control, view: Dictionary) -> void:
	var action: Dictionary=view.action
	if action.is_empty(): return
	var labels: Dictionary={"family_adopt":"收养办理中","long_rest":"庄中休整中"}
	host._text(parent,labels.get(action.kind,"事务办理中")+"\n还需 %d 个半日"%action.remaining_ticks,19).name="FamilyActionStatus"
	_command(host,parent,"继续半日","advance",{},"FamilyAdvance")
	if action.kind=="long_rest":
		if not host._resident_mode:
			var toggle: Button=host._button(parent,"停止自动" if host._rest_auto else "自动休整","rest_auto","",host._toggle_rest_auto)
			toggle.name="FamilyRestAuto"
		_command(host,parent,"结束休整","rest_cancel",{},"FamilyRestCancel")

## 只从成功收养索引列出本人的子女；没有完成的收养只出现在真实行动状态中。
static func _family(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"家中子女 · %d人"%view.children.size(),24)
	if view.children.is_empty(): host._text(parent,"尚无记入家中的子女。",18)
	for child in view.children: _child(host,parent,view,child)
	if not view.adoption.is_empty():
		parent.add_child(HSeparator.new())
		_adoption(host,parent,view.adoption)
	parent.add_child(HSeparator.new())
	_retirement(host,parent,view)

## 原子女继任门槛沿原收养事实；成年收养只展示关系，不把收养同意替换成继任同意。
static func _child(host, parent: Control, view: Dictionary, child: Dictionary) -> void:
	var id: String=child.actor_id
	var age: int=child.age_years
	var adult_adoption: bool=child.get("relationship_kind","")=="adult_adoption"
	var threshold: int=int(child.get("heir_min_age",0))
	var growth: String="成年收养，保留原有身份与财产" if adult_adoption else "已到继任年岁" if age>=threshold else "尚未满 %d岁"%threshold if age>=0 else "出生年月不详"
	var presence: String="同在此处" if child.together else _place(host,child.place_id)
	if not child.alive: presence="已经离世"
	elif child.busy: presence+=" · 正在办事"
	var row=HBoxContainer.new(); row.set_meta("family_actor_id",id); row.add_theme_constant_override("separation",18); parent.add_child(row)
	var portrait: Control=Art.character_control(Art.character_input(child.portrait_input,"avatar",{"resolution":[96,96]})); portrait.name="FamilyChildPortrait"; portrait.custom_minimum_size=Vector2(72,72); row.add_child(portrait)
	var text: Label=host._text(row,child.display_name+" · "+_age(child.age_years)+"\n"+presence+" · "+growth,18); text.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	if view.heir.get("actor_id","")==id:
		var assigned: Label=host._text(row,"现定继任人",17); assigned.custom_minimum_size.x=120; assigned.vertical_alignment=VERTICAL_ALIGNMENT_CENTER
	elif adult_adoption:
		var consent: Label=host._text(row,"继任尚需本人另行同意",17); consent.name="FamilySuccessionConsentRequired"; consent.custom_minimum_size.x=180; consent.vertical_alignment=VERTICAL_ALIGNMENT_CENTER
	else:
		var choose: Button=_command(host,row,"指定继任","family_designate",{"heir_actor_id":id},"FamilyDesignate")
		choose.set_meta("heir_actor_id",id); choose.custom_minimum_size.x=120; choose.size_flags_vertical=Control.SIZE_SHRINK_CENTER

## 收养只展示实际提供人的条款；姓名未改时省略可选字段，让规则采用作者默认姓名。
static func _adoption(host, parent: Control, adoption: Dictionary) -> void:
	host._text(parent,"办理收养",23)
	if adoption.pending:
		host._text(parent,"收养手续尚在办理，办妥后才会记入家中。",18)
		return
	var context: Dictionary=adoption.context
	var role: Dictionary=host._data.roles[context.role_id]
	var params: Dictionary={"context":context.duplicate(true)}
	var quote: Dictionary=host.kernel.preview("family_adopt",params)
	var provider: String=adoption.provider_display_name
	if quote.ok:
		var terms: Dictionary=quote.training
		host._text(parent,"由%s办理 · 拟收养%s（%d岁）\n%d钱 · 需要%d个半日"%[provider,terms.display_name,role.family.child_age_years,quote.cost,quote.duration],18)
	else: host._text(parent,provider+" · "+host._reason(quote.get("code","Unknown")),17)
	_command(host,parent,"开始办理收养","family_adopt",params,"FamilyAdopt")

## 生前交接展示真实继任对象与资产报价；不会把知识、人脉、功勋和公职当作可复制财产。
static func _retirement(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"退隐与接掌",23)
	var quote: Dictionary=host.kernel.preview("family_retire",{})
	if quote.ok:
		var assets: Dictionary=quote.family.assets
		var money: int=0
		for wallet in assets.wallets: money+=wallet.amount
		host._text(parent,"交接后，由%s继续行走世间。\n可移交私钱 %d钱 · 庄园 %d处 · 物品 %d批"%[view.heir.display_name,money,assets.estates.size(),assets.goods.size()],18)
	else: host._text(parent,host._reason(quote.get("code","Unknown")),17)
	_command(host,parent,"退隐，由继任人接掌","family_retire",{},"FamilyRetire")

## 休整只列本人实际持有的庄园，每半日口粮来自当次预览，不编造整年预付价格。
static func _rest(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"在自家庄中休整",24)
	host._text(parent,"岁月按半日流逝，庄中照常生产、供养与来往；可随时停止自动或结束休整。",18)
	if view.estates.is_empty(): host._text(parent,"眼下没有属于自己的庄园。",18)
	for estate in view.estates:
		var id: String=estate.estate_id
		parent.add_child(HSeparator.new())
		host._text(parent,host._name(host._data.estates[id]),22)
		if not estate.active_period.is_empty():
			host._text(parent,"已安排 %d 个半日 · 还剩 %d 个半日\n庄中口粮仍逐半日支取，收支以当时在场的人手为准。"%[estate.active_period.duration,estate.active_period.remaining_ticks],18)
			if not estate.active_ledger.is_empty():
				var food: String=host._name(host._data.items[estate.active_ledger.food_item_id])
				host._text(parent,"已度过 %d 个半日 · 本次已用 %d份%s"%[estate.active_ledger.elapsed,estate.active_ledger.consumed,food],18).name="FamilyRestLedger"
			continue
		var latest: Dictionary=estate.latest_completed
		if not latest.is_empty():
			var ending: String="已完成预定休整" if latest.stop_reason=="duration" else "已结束休整" if latest.stop_reason=="cancelled" else host._reason(latest.stop_reason)
			var food: String=host._name(host._data.items[latest.food_item_id])
			host._text(parent,"上次%s · 度过 %d 个半日\n共用 %d份%s"%[ending,latest.elapsed,latest.consumed,food],18).name="FamilyRestSummary"
		var quote: Dictionary=host.kernel.preview("long_rest",{"estate_id":id,"duration":180})
		if quote.ok:
			var food: String=host._name(host._data.items[quote.food_item_id])
			host._text(parent,"此半日供养 %d人 · 每人需%d份%s\n口粮由庄中私仓实际支取。"%[quote.mouths.size(),quote.ration_per_person,food],18)
		else: host._text(parent,host._reason(quote.get("code","Unknown")),17)
		var choices=HBoxContainer.new(); choices.add_theme_constant_override("separation",12); parent.add_child(choices)
		for duration in [180,TimeRules.TICKS_PER_YEAR]:
			var start: Button=_command(host,choices,"休整三个月" if duration==180 else "休整一年","long_rest",{"estate_id":id,"duration":duration},"FamilyRestStart")
			start.set_meta("rest_duration",duration)
		PlayerPlanControls.append(host,parent,"rest",{"estate_id":id,"duration":2},"将自宅休整一日加入生活安排")

## 所有世界按钮先查询正式预览，失败原因保留为中文提示；点击只交还主场景提交。
static func _command(host, parent: Control, label: String, kind: String, params: Dictionary, name: String) -> Button:
	var quote: Dictionary=host.kernel.preview(kind,params)
	var target: String=params.get("heir_actor_id",params.get("estate_id",""))
	var control: Button=host._button(parent,label,kind,target,host._perform.bind(kind,params),not quote.ok or host._advancing)
	control.name=name
	if not quote.ok: control.tooltip_text=host._reason(quote.get("code","Unknown"))
	return control

## 年岁文案只包装有限年龄，未知生日仍保原说明。
static func _age(age: int) -> String:
	return "%d岁"%age if age>=0 else "年岁不详"

## 名称只查静态作者表；有限位置没有场所时保原行旅回退。
static func _place(host, place: String) -> String:
	return host._name(host._data.places[place]) if host._data.get("places",{}).has(place) else "行旅途中"
