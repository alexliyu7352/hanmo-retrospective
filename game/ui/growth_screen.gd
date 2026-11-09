## 成长界面只展示已有装备、功法和经历；所有世界变更交回主场景，选择卡片不改变存档。
extends RefCounted
const Art=preload("res://ui/art_catalog.gd")
const TYPES={"strike":"武功","guard":"架势","heart":"心法"}
const WEAPONS={"sword":"刀剑","spear":"长兵","bow":"弓"}
const DRAFT_META="growth_ui_draft"

## 草案来源只取轻量命令身份；原生另绑世界和执行代，不用每次重绘都会变化的scene计数作草案键。
static func _draft_source(host, actor_id: String) -> Dictionary:
	var context: Dictionary=host.kernel.command_context()
	var identity: Dictionary={"kernel_id":host.kernel.get_instance_id(),"actor_id":context.get("actor_id",actor_id)}
	if host.kernel.has_method("source_identity"):
		var source: Dictionary=host.kernel.source_identity()
		identity["world_id"]=source.get("world_id","")
		identity["execution_epoch"]=source.get("execution_epoch",-1)
	return {"identity":identity,"revision":context.get("revision",-1)}

## 同源纯预览重绘保留尚未提交的装备；真实版本变化重新读取装备，换人物或世界同时清掉旧卡片。
static func _draft(host, view: Dictionary) -> Dictionary:
	var source: Dictionary=_draft_source(host,view.actor_id)
	var draft: Dictionary=host.get_meta(DRAFT_META,{})
	if draft.get("source",{}).get("identity")!=source.identity:
		if not draft.is_empty() and host.has_meta("growth_selected_skill"): host.remove_meta("growth_selected_skill")
		draft={}
	if draft.get("source")!=source:
		draft["source"]=source
		draft["weapon_id"]=view.equipment.weapon_id
		draft["armor_id"]=view.equipment.armor_id
	host.set_meta(DRAFT_META,draft)
	return draft

## 草案只能选本次有限名单；已失效的实际护具引用仍由原占位项和报价解释，不擅改人物配置。
static func _equipment_choice(draft: Dictionary, field: String, choice: OptionButton, fallback: String) -> String:
	var selected: String=draft.get(field,fallback)
	for index in range(choice.item_count):
		if choice.get_item_metadata(index)==selected:
			choice.select(index); return selected
	for index in range(choice.item_count):
		if choice.get_item_metadata(index)==fallback: choice.select(index); break
	selected=choice.get_selected_metadata()
	draft[field]=selected
	return selected

## 旧下拉回调不能改新世界或新版本的草案；合法选择仅保存UI值，随后仍须纯预览与明确确认。
static func _remember_choice(host, draft: Dictionary, field: String, value: String) -> bool:
	if draft.source!=_draft_source(host,draft.source.identity.actor_id): return false
	if host.get_meta(DRAFT_META,{})!=draft: return false
	draft[field]=value
	host.set_meta(DRAFT_META,draft)
	return true

## 明确点击只消费当前活控件、来源和最新成功报价；陈旧、忙碌或异步待回执一律不提交。
static func _submit_choice(host, kind: String, request: Dictionary, binding: Dictionary) -> void:
	var button: Button=binding.button
	var status: Label=binding.status
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._growth_open or host._advancing or binding.scene_epoch!=host._scene_counter: return
	var source: Dictionary=_draft_source(host,binding.source.identity.actor_id)
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or source!=binding.source:
		button.disabled=true; status.text="行囊状态已变化，请重新打开成长页。"; button.tooltip_text=status.text
		return
	var quote: Dictionary=_preview(host,button,status,kind,request)
	if not quote.get("ok",false) or quote.get("revision",-1)!=context.revision: return
	if source!=_draft_source(host,context.actor_id): return
	host._perform(kind,request.duplicate(true))

## 将有限成长输入组成面板；闭包保留开窗时投影，所有资格与变化仍走实时Kernel报价和命令。
static func build(host, parent: Control, view: Dictionary) -> void:
	var data: Dictionary=host._data
	_draft(host,view)
	var margin=MarginContainer.new(); margin.name="GrowthScreen"; parent.add_child(margin)
	margin.anchor_left=.025; margin.anchor_right=.975; margin.anchor_top=.115; margin.anchor_bottom=.975
	var frame=PanelContainer.new(); frame.add_theme_stylebox_override("panel",_surface(Color("18271ff5"),Color("9e895c"),18)); margin.add_child(frame)
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); frame.add_child(column)
	var heading=HBoxContainer.new(); heading.add_theme_constant_override("separation",18); column.add_child(heading)
	var page_title: Label=host._text(heading,"武学与行囊",30); page_title.autowrap_mode=TextServer.AUTOWRAP_OFF
	var current_place: String=view.place_id
	var context: String=_name(host,data,"places",current_place,"行旅途中")
	var context_label: Label=host._text(heading,context,17); context_label.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var back: Button=host._button(heading,"返回场所","growth_close","",host._close_growth)
	back.name="GrowthClose"; back.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	column.add_child(HSeparator.new())
	var tabs=HBoxContainer.new(); tabs.add_theme_constant_override("separation",12); column.add_child(tabs)
	for tab in ["skills","inventory"]:
		var label: String="武学修行" if tab=="skills" else "随身行囊"
		var button: Button=host._button(tabs,label,"growth_tab",tab,func(): host.set_meta("growth_page",tab); host._render())
		button.name="GrowthTabSkills" if tab=="skills" else "GrowthTabInventory"
		button.disabled=host.get_meta("growth_page","skills")==tab
	if not host._message.is_empty():
		var notice: Label=host._text(column,host._message,16); notice.name="GrowthNotice"
	_current_action(host,column,view,data)
	var body=HBoxContainer.new(); body.size_flags_vertical=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",20); column.add_child(body)
	var identity_scroll=ScrollContainer.new(); identity_scroll.custom_minimum_size.x=210; identity_scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; body.add_child(identity_scroll)
	var sidebar=VBoxContainer.new(); sidebar.size_flags_horizontal=Control.SIZE_EXPAND_FILL; sidebar.add_theme_constant_override("separation",8); identity_scroll.add_child(sidebar)
	_identity(host,sidebar,view,data)
	if host.get_meta("growth_page","skills")=="inventory":
		_inventory(host,body,view.inventory,data)
		return
	var library=VBoxContainer.new(); library.custom_minimum_size.x=200; library.add_theme_constant_override("separation",10); body.add_child(library)
	var rows: Dictionary={}; var ids: Array=[]
	for card in view.skills:
		ids.append(card.skill_id); rows[card.skill_id]=card
	host._text(library,"已学卡片 · %d"%ids.size(),20)
	var cards=ItemList.new(); cards.name="GrowthCards"; cards.size_flags_vertical=Control.SIZE_EXPAND_FILL
	cards.custom_minimum_size.x=200; cards.fixed_column_width=196; cards.fixed_icon_size=Vector2.ZERO
	cards.add_theme_constant_override("v_separation",14); cards.add_theme_font_size_override("font_size",18)
	cards.add_theme_color_override("font_color",Color("e9dec5")); cards.add_theme_color_override("font_selected_color",Color("f4e6bf"))
	cards.add_theme_stylebox_override("panel",_surface(Color("152019"),Color("334838"),8))
	cards.add_theme_stylebox_override("selected",_surface(Color("43503c"),Color("be9e60"),10))
	cards.add_theme_stylebox_override("selected_focus",_surface(Color("43503c"),Color("dac086"),10))
	library.add_child(cards)
	for id in ids:
		var skill: Dictionary=data.personal_skills[id]
		cards.add_item(host._name(skill)+" · %d"%rows[id].mastery)
		cards.set_item_tooltip(cards.item_count-1,TYPES.get(skill.kind,"功法")+" · 熟练 %d / 100\n"%rows[id].mastery+data.texts.get(skill.get("description_key",""),""))
	var detail=PanelContainer.new(); detail.name="GrowthCardDetail"; detail.set_meta("paper",true)
	detail.size_flags_horizontal=Control.SIZE_EXPAND_FILL; detail.add_theme_stylebox_override("panel",_surface(Color("e3d6b9"),Color("bba576"),20)); body.add_child(detail)
	if ids.is_empty():
		var empty=VBoxContainer.new(); empty.add_theme_constant_override("separation",18); detail.add_child(empty)
		host._text(empty,"尚未习得武学",30)
		host._text(empty,"前往武馆，与师傅当面拜师求教。学得的武功与心法会记在这里。",21)
		host._text(empty,"已持有的武器可在左侧换装。",17)
		return
	var selected: String=host.get_meta("growth_selected_skill","")
	if not selected in ids: selected=ids[0]
	var card_source: Dictionary=_draft_source(host,view.actor_id)
	var card_epoch: int=host._scene_counter
	cards.select(ids.find(selected))
	_show_skill(host,detail,view,data,rows[selected])
	cards.item_selected.connect(func(index: int):
		if not is_instance_valid(cards) or cards.is_queued_for_deletion() or not cards.is_inside_tree() or card_epoch!=host._scene_counter or card_source!=_draft_source(host,view.actor_id): return
		host.set_meta("growth_selected_skill",ids[index])
		_show_skill(host,detail,view,data,rows[ids[index]]))
	_fade(detail)

## 原生读取失败先显示不可用且不造继续键；正常行动仍显示原半日并走统一advance。
static func _current_action(host, column: VBoxContainer, view: Dictionary, data: Dictionary) -> void:
	var action: Dictionary=view.action
	if action.get("ok") is bool and not action.ok:
		var unavailable: Label=host._text(column,"原生行动摘要暂不可用",18); unavailable.name="GrowthActionUnavailable"
		unavailable.tooltip_text=str(action.get("code",""))+" "+str(action.get("path",""))
		return
	if action.is_empty(): return
	var strip=HBoxContainer.new(); strip.add_theme_constant_override("separation",16); column.add_child(strip)
	var names: Dictionary={"learn_skill":"学习中","practice_skill":"修炼中","teach_skill":"传授中"}
	var skill: String=action.skill_id
	var title: String=names.get(action.kind,"行动进行中")+" · "+_name(host,data,"personal_skills",skill,"")
	var note: Label=host._text(strip,title+"　剩余 %d 个半日"%action.remaining_ticks,18); note.name="GrowthActionStatus"; note.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var advance: Button=host._button(strip,"继续半日","advance","",host._perform.bind("advance",{}))
	advance.name="GrowthAdvance"
	var status: Label=host._text(column,"",15)
	_preview(host,advance,status,"advance",{})

## 消费公开人物输入和原武器菜单；预留武器仍交正式报价拒绝，不在表现层决定资格。
static func _identity(host, box: VBoxContainer, view: Dictionary, data: Dictionary) -> void:
	var draft: Dictionary=_draft(host,view)
	host._text(box,view.display_name,24)
	var portrait: Control=Art.character_control(Art.character_input(view.portrait_input,"half_body")); portrait.name="GrowthPortrait"; portrait.custom_minimum_size=Vector2(190,140)
	box.add_child(portrait)
	host._text(box,"疲劳 %d / 100　伤势 %d"%[view.fatigue,view.injury],16)
	box.add_child(HSeparator.new())
	host._text(box,"随身装备",20)
	var choice=OptionButton.new(); choice.name="GrowthWeaponChoice"; choice.custom_minimum_size=Vector2(200,42); choice.fit_to_longest_item=false; box.add_child(choice)
	choice.add_item("空手"); choice.set_item_metadata(0,"")
	for option in view.equipment.weapon_options:
		var id: String=option.item_id
		choice.add_item(host._name(data.items[id])+" ×%d"%option.quantity); choice.set_item_metadata(choice.item_count-1,id)
		if id==view.equipment.weapon_id: choice.select(choice.item_count-1)
	var info: Label=host._text(box,"",15)
	var request: Dictionary={"weapon_id":_equipment_choice(draft,"weapon_id",choice,view.equipment.weapon_id)}
	var binding: Dictionary={"source":draft.source.duplicate(true),"scene_epoch":host._scene_counter}
	var equip: Button=host._button(box,"换装","equip_weapon",request.weapon_id,func(): _submit_choice(host,"equip_weapon",request,binding))
	equip.name="GrowthEquip"
	var status: Label=host._text(box,"",15)
	binding["button"]=equip; binding["status"]=status
	var update: Callable=func():
		if not is_instance_valid(choice) or choice.is_queued_for_deletion() or not choice.is_inside_tree() or binding.scene_epoch!=host._scene_counter: return
		if not _remember_choice(host,draft,"weapon_id",choice.get_selected_metadata()): return
		request["weapon_id"]=choice.get_selected_metadata()
		equip.set_meta("target_id",request.weapon_id)
		var weapon: Dictionary=data.get("items",{}).get(request.weapon_id,{}).get("weapon",{})
		info.text="未持武器" if weapon.is_empty() else "%s · 攻击距离 %d—%d"%[WEAPONS.get(weapon.category,"武器"),weapon.minimum_range,weapon.maximum_range]
		_preview(host,equip,status,"equip_weapon",request)
	update.call(); choice.item_selected.connect(func(_index: int): update.call())
	var heart: String=view.equipment.heart_id
	host._text(box,"所用心法\n"+_name(host,data,"personal_skills",heart,"尚未运转心法"),16)
	if view.equipment.weapon_options.is_empty(): host._text(box,"行囊中暂无属于自己的武器。",15)
	_armor(host,box,view,data)

## 护具草案与当前实际效果分开显示；失去实物不清理选择，资格只由原报价解释。
static func _armor(host, parent: VBoxContainer, view: Dictionary, data: Dictionary) -> void:
	var draft: Dictionary=_draft(host,view)
	var box=VBoxContainer.new(); box.name="GrowthArmorControls"; box.add_theme_constant_override("separation",8); parent.add_child(box)
	box.add_child(HSeparator.new())
	var selected: String=view.equipment.armor_id
	host._text(box,"护具",20)
	host._text(box,"已选护具："+_name(host,data,"items",selected,"未穿护具"),16).name="GrowthArmorSelected"
	var effective: Dictionary=view.equipment.armor_effective
	var effect_text: String="护具效果暂不可用"
	if effective.ok:
		effect_text="实际防御 %d"%effective.armor_defense
		if not selected.is_empty() and effective.armor_id.is_empty(): effect_text="当前选择已不可用 · "+effect_text
		elif not effective.armor_id.is_empty(): effect_text="实际护具："+_name(host,data,"items",effective.armor_id,"护具")+" · "+effect_text
	host._text(box,effect_text,15).name="GrowthArmorEffective"
	var choice=OptionButton.new(); choice.name="GrowthArmorChoice"; choice.custom_minimum_size=Vector2(200,42); choice.fit_to_longest_item=false; box.add_child(choice)
	choice.add_item("卸下护具"); choice.set_item_metadata(0,"")
	var found: bool=selected.is_empty()
	for option in view.equipment.armor_options:
		var id: String=option.item_id
		choice.add_item(_name(host,data,"items",id,"护具")+" ×%d"%option.quantity); choice.set_item_metadata(choice.item_count-1,id)
		if id==selected: choice.select(choice.item_count-1); found=true
	if not found:
		choice.add_item(_name(host,data,"items",selected,"护具")+" · 当前已不可用")
		choice.set_item_metadata(choice.item_count-1,selected); choice.select(choice.item_count-1)
	var request: Dictionary={"armor_id":_equipment_choice(draft,"armor_id",choice,selected)}
	var binding: Dictionary={"source":draft.source.duplicate(true),"scene_epoch":host._scene_counter}
	var equip: Button=host._button(box,"穿戴护具","equip_armor",request.armor_id,func(): _submit_choice(host,"equip_armor",request,binding))
	equip.name="GrowthArmorEquip"
	var status: Label=host._text(box,"",15); status.name="GrowthArmorStatus"
	binding["button"]=equip; binding["status"]=status
	var update: Callable=func():
		if not is_instance_valid(choice) or choice.is_queued_for_deletion() or not choice.is_inside_tree() or binding.scene_epoch!=host._scene_counter: return
		if not _remember_choice(host,draft,"armor_id",choice.get_selected_metadata()): return
		request["armor_id"]=choice.get_selected_metadata(); equip.set_meta("target_id",request.armor_id)
		equip.text="卸下护具" if request.armor_id.is_empty() else "穿戴护具"
		_preview(host,equip,status,"equip_armor",request)
	update.call(); choice.item_selected.connect(func(_index: int): update.call())

## 按真实批次展示随身实物、产权与预留；只读视图不把公物计为私人可用库存。
static func _inventory(host, parent: Control, inventory: Dictionary, data: Dictionary) -> void:
	var panel=PanelContainer.new(); panel.name="GrowthInventory"; panel.set_meta("paper",true)
	panel.size_flags_horizontal=Control.SIZE_EXPAND_FILL; panel.add_theme_stylebox_override("panel",_surface(Color("e3d6b9"),Color("bba576"),20)); parent.add_child(panel)
	var box=VBoxContainer.new(); box.add_theme_constant_override("separation",12); panel.add_child(box)
	host._text(box,"随身行囊",26)
	host._text(box,"携带重量 %d / %d · 含代管与预留物品"%[inventory.weight,inventory.capacity],17).name="GrowthInventoryWeight"
	var scroll=ScrollContainer.new(); scroll.name="GrowthInventoryScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; box.add_child(scroll)
	var rows=VBoxContainer.new(); rows.name="GrowthInventoryLots"; rows.size_flags_horizontal=Control.SIZE_EXPAND_FILL; rows.add_theme_constant_override("separation",12); scroll.add_child(rows)
	if inventory.lots.is_empty(): host._text(rows,"行囊里暂无物品。",20)
	for lot in inventory.lots:
		var row=VBoxContainer.new(); row.set_meta("lot_id",lot.lot_id); row.set_meta("item_id",lot.item_id); rows.add_child(row)
		host._text(row,"%s ×%d"%[host._name(data.items[lot.item_id]),lot.quantity],22)
		host._text(row,lot.ownership_text+" · "+lot.reservation_text,17).name="LotOwnership"
		row.add_child(HSeparator.new())

## 重建选中功法卡的阅读与操作区；只读视图选择保留在主场景元数据，不写入人物知识。
static func _show_skill(host, panel: Control, view: Dictionary, data: Dictionary, card: Dictionary) -> void:
	for child in panel.get_children(): panel.remove_child(child); child.queue_free()
	var scroll=ScrollContainer.new(); scroll.name="GrowthDetailScroll"; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; panel.add_child(scroll)
	var box=VBoxContainer.new(); box.size_flags_horizontal=Control.SIZE_EXPAND_FILL; box.add_theme_constant_override("separation",12); scroll.add_child(box)
	var skill_id: String=card.skill_id
	var skill: Dictionary=data.personal_skills[skill_id]
	var mastery: int=card.mastery
	host._text(box,TYPES.get(skill.kind,"功法")+"卡",16)
	var title: Label=host._text(box,host._name(skill),32); title.name="GrowthSkillTitle"
	var compatible: Array=[]
	for weapon in skill.get("weapons",[]): compatible.append(WEAPONS.get(weapon,weapon))
	host._text(box,"适用："+"、".join(compatible),16)
	var description: Label=host._text(box,data.texts.get(skill.get("description_key",""),"暂无功法说明。"),20); description.name="GrowthSkillDescription"
	host._text(box,"熟练 %d / 100"%mastery,19)
	var bar=ProgressBar.new(); bar.name="GrowthMastery"; bar.value=mastery; bar.show_percentage=false; bar.custom_minimum_size.y=8
	bar.add_theme_stylebox_override("background",_surface(Color("c4b493"),Color.TRANSPARENT,0))
	bar.add_theme_stylebox_override("fill",_surface(Color("53694c"),Color.TRANSPARENT,0)); box.add_child(bar)
	var source: Label=host._text(box,card.provenance_text,15); source.name="GrowthSkillSource"
	box.add_child(HSeparator.new())
	var practice_row=HBoxContainer.new(); practice_row.add_theme_constant_override("separation",12); box.add_child(practice_row)
	var ticks=SpinBox.new(); ticks.name="GrowthPracticeTicks"; ticks.min_value=1; ticks.max_value=8; ticks.step=1; ticks.suffix="个半日"
	ticks.add_theme_color_override("up_icon_modulate",Color("3b4935")); ticks.add_theme_color_override("down_icon_modulate",Color("3b4935"))
	ticks.value=clampi(host.get_meta("growth_practice_ticks",1),1,8); ticks.custom_minimum_size=Vector2(135,42); practice_row.add_child(ticks)
	var request: Dictionary={"skill_id":skill_id,"ticks":int(ticks.value)}
	var practice: Button=host._button(practice_row,"开始修炼","practice_skill",skill_id,func(): host._perform("practice_skill",request.duplicate(true)))
	practice.name="GrowthPractice"; practice.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var practice_status: Label=host._text(box,"",16); practice_status.name="GrowthPracticeStatus"
	_preview(host,practice,practice_status,"practice_skill",request)
	ticks.value_changed.connect(func(value: float):
		request.ticks=int(value); host.set_meta("growth_practice_ticks",int(value))
		_preview(host,practice,practice_status,"practice_skill",request))
	if skill.kind=="heart":
		var active: bool=view.equipment.heart_id==skill_id
		var heart_request: Dictionary={"skill_id":"" if active else skill_id}
		var binding: Dictionary={"source":_draft_source(host,view.actor_id),"scene_epoch":host._scene_counter}
		var heart: Button=host._button(box,"撤下心法" if active else "运转此心法","configure_heart",skill_id,func(): _submit_choice(host,"configure_heart",heart_request,binding))
		heart.name="GrowthHeart"
		var heart_status: Label=host._text(box,"",15)
		binding["button"]=heart; binding["status"]=heart_status
		_preview(host,heart,heart_status,"configure_heart",heart_request)
	box.add_child(HSeparator.new())
	_teaching(host,box,view,skill_id)
	_fade(scroll)

## 只列出此刻在同一场所履职的真实人物；选择岗位后继续用领域预览核验熟练度、占用与教学资格。
static func _teaching(host, box: VBoxContainer, view: Dictionary, skill_id: String) -> void:
	host._text(box,"当面传授",20)
	var row=HBoxContainer.new(); row.add_theme_constant_override("separation",12); box.add_child(row)
	var people=OptionButton.new(); people.name="GrowthStudentChoice"; people.fit_to_longest_item=false
	people.custom_minimum_size=Vector2(150,42); people.size_flags_horizontal=Control.SIZE_EXPAND_FILL; row.add_child(people)
	for student in view.students:
		people.add_item(student.display_name); people.set_item_metadata(people.item_count-1,student.context)
	var request: Dictionary={"skill_id":skill_id,"context":{} if people.item_count==0 else people.get_item_metadata(0)}
	var teach: Button=host._button(row,"传授","teach_skill",skill_id,func(): host._perform("teach_skill",request.duplicate(true)))
	teach.name="GrowthTeach"
	var status: Label=host._text(box,"",16); status.name="GrowthTeachStatus"
	if people.item_count==0:
		people.add_item("此处暂无受教人物"); people.disabled=true
	_preview(host,teach,status,"teach_skill",request)
	if people.disabled:
		teach.disabled=true; status.text="需与受教人物在场所中当面相处。"; teach.tooltip_text=status.text
	people.item_selected.connect(func(index: int):
		request.context=people.get_item_metadata(index)
		_preview(host,teach,status,"teach_skill",request))

## 展示正式规则预览的成本与结果范围；按钮禁用原因始终可见，界面不自行计算成长或推进半日。
static func _preview(host, button: Button, label: Label, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=host.kernel.preview(kind,params)
	button.disabled=not quote.ok or host._advancing
	if host._advancing: label.text="正在办理当前行动。"
	elif not quote.ok: label.text=_reason(host,quote.get("code",""))
	elif kind in ["practice_skill","teach_skill"]:
		var training: Dictionary=quote.get("training",{})
		label.text="%d 个半日 · 熟练 %d → %d"%[quote.get("duration",0),maxi(0,training.get("mastery_before",0)),training.get("mastery_after",0)]
		if kind=="practice_skill": label.text+=" · 疲劳 +%d"%training.get("student_fatigue",0)
	elif kind=="advance": label.text="推进一个半日；可返回场所保存，稍后再继续。"
	else: label.text="确认后生效，不消耗半日。"
	button.tooltip_text=label.text
	return quote

## 从已有内容文本取名称；空引用使用明确的界面说明，不新增表现资源 ID。
static func _name(host, data: Dictionary, table: String, id: String, fallback: String) -> String:
	return host._name(data[table][id]) if data.get(table,{}).has(id) else fallback

## 将成长与当面交互失败转成行动提示；未识别诊断不直接暴露协议代码。
static func _reason(host, code: String) -> String:
	var reasons: Dictionary={"ActorBusy":"已有行动尚未完成。","BattlePending":"请先结束当前战斗。","ActorUnavailable":"人物当前无法行动。","MissingPersonalGrowth":"本局尚无可用的个人修行记录。","TrainingNeedsRest":"伤势或疲劳过重，先休息再修炼。","TeacherNeedsRest":"对方伤势或疲劳过重，暂不能教学。","TrainingPlaceRequired":"请先抵达一处场所，再静心修行。","SkillAlreadyMastered":"已经融会贯通，无需继续重复修炼。","SkillNotLearned":"尚未习得此功法，需先当面求教。","TeacherNotQualified":"熟练达到 80 后方可传授。","StudentAlreadyQualified":"对方对此功法的掌握已不逊于你。","WeaponNotOwned":"此武器不在可用的私人行囊中，或已被预留。","NotAWeapon":"此物不能作为个人战武器。","ArmorNotOwned":"此护具不在可用的私人行囊中，或已被预留。","NotAnArmor":"此物不能作为护具。","InvalidArmorChoice":"请重新选择护具。","MissingPersonalInventory":"当前没有可用的随身行囊。","HeartNotLearned":"需要先习得这门心法。","NotAtPlace":"双方须在同一场所。","PersonAbsent":"对方目前不在这里或无法参与。","RoleNotAuthorized":"此岗位当前无权办理此事。","MissingInteractionContext":"请先选择在场的受教人物。","InsufficientFunds":"随身钱款不足。","TeacherTrustRequired":"师傅对你的信任尚不足。","LessonNotOffered":"这位师傅尚不教授此功法。"}
	if reasons.has(code): return reasons[code]
	if host.has_method("_reason"):
		var text: String=host._reason(code)
		if text!=code: return text
	return "当前条件尚不满足，请先查看人物与行动状态。"

## 为本面板创建克制的纸面与墨色边界，不改变全局主题或资源注册表。
static func _surface(color: Color, border: Color, margin: int) -> StyleBoxFlat:
	var style=StyleBoxFlat.new(); style.bg_color=color; style.border_color=border; style.set_border_width_all(1); style.set_corner_radius_all(3)
	for edge in ["left","right","top","bottom"]: style.set("content_margin_"+edge,margin)
	return style

## 只淡入当前阅读区域，卡片选择有清楚的视觉过渡；动画不触碰状态、随机流或世界时钟。
static func _fade(control: Control) -> void:
	control.modulate.a=.65
	control.create_tween().tween_property(control,"modulate:a",1.0,.14)
