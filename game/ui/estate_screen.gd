## 庄园面板依托真实场所、管事与仓储，只提交已有经营命令，不维护另一套生产状态。
extends RefCounted
const EstatePage=preload("res://core/views/estate_page.gd")
const Art=preload("res://ui/art_catalog.gd")
const DUTIES={"idle":"歇息","labor":"耕作","guard":"守备"}
const FACILITIES={"farm":"田亩","granary":"仓廪","palisade":"木栅"}

## 旧全态入口只作核心在场探针兼容；生产caller经Kernel，不新增owner权限判断。
static func available(state: Dictionary, data: Dictionary, id: String) -> bool:
	return EstatePage.available(state,data,id)

## 展示实际管事、居民、设施与上一半日产消；所有按钮再次查询统一世界边界。
static func render(host, stage: Control, view: Dictionary) -> void:
	if not view.available: return
	var id: String=view.estate_id
	var definition: Dictionary=host._data.estates[id]
	var frame=PanelContainer.new(); frame.name="EstateScreen"; stage.add_child(frame)
	frame.set_meta("estate_id",id); frame.set_meta("estate_scene_epoch",host._scene_counter)
	frame.anchor_left=.025; frame.anchor_right=.975; frame.anchor_top=.12; frame.anchor_bottom=.975
	var surface=StyleBoxFlat.new(); surface.bg_color=Color("19271ff5"); surface.border_color=Color("9e895c"); surface.set_border_width_all(1)
	for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]: surface.set_content_margin(side,18)
	frame.add_theme_stylebox_override("panel",surface)
	var page_scroll=ScrollContainer.new(); page_scroll.name="EstatePageScroll"; page_scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; page_scroll.follow_focus=true; frame.add_child(page_scroll)
	var columns=BoxContainer.new(); columns.name="EstateColumns"; columns.size_flags_horizontal=Control.SIZE_EXPAND_FILL; columns.add_theme_constant_override("separation",24); page_scroll.add_child(columns)
	var summary=VBoxContainer.new(); summary.custom_minimum_size.x=240; summary.size_flags_stretch_ratio=.28; summary.size_flags_horizontal=Control.SIZE_EXPAND_FILL; columns.add_child(summary)
	host._text(summary,host._name(definition),30)
	host._text(summary,"庄主 · "+view.owner.display_name,19)
	if not view.manager.is_empty():
		var portrait=Art.character_control(Art.character_input(view.manager.portrait_input,"half_body")); portrait.name="EstateManagerPortrait"; portrait.custom_minimum_size.y=150
		summary.add_child(portrait)
		host._text(summary,"管事 · "+view.manager.display_name,20)
	else: host._text(summary,"尚未委任管事",20)
	host._text(summary,"在场劳力 %d人\n在场守备 %d人 · 防御 %d"%[view.workforce.workers_count,view.workforce.guards_count,view.workforce.defense],19)
	var last: Dictionary=view.last_production
	if not last.is_empty():
		host._text(summary,"上一半日\n产粮 %d · 食用 %d\n未能收储 %d"%[last.produced_quantity,last.consumed_quantity,last.lost_output],18)
		if last.unfed_count>0: host._text(summary,"有 %d人缺粮，劳作与守备受影响。"%last.unfed_count,17)
	var space=Control.new(); space.size_flags_vertical=Control.SIZE_EXPAND_FILL; summary.add_child(space)
	if not view.activity.is_empty():
		var action: Dictionary=view.activity
		var activity: String="正在办理"
		if action.kind=="estate_build": activity="营造中 · "+FACILITIES[action.facility]
		host._text(summary,activity+"\n还需 %d个半日"%action.remaining_ticks,19).name="EstateActivity"
		command(host,summary,"继续半日","advance",{},"EstateAdvance")
	else:
		var completed: String=_construction_text(view.completion)
		if not completed.is_empty(): host._text(summary,completed,18).name="EstateCompletion"
		command(host,summary,"在庄中歇息半日","wait",{},"EstateWait")
	var close: Button=host._button(summary,"回到庄院","estate_close","",_close.bind(host,frame)); close.name="EstateClose"; close.custom_minimum_size.y=44
	var detail=VBoxContainer.new(); detail.size_flags_stretch_ratio=.72; detail.size_flags_horizontal=Control.SIZE_EXPAND_FILL; columns.add_child(detail)
	var tabs=HBoxContainer.new(); tabs.add_theme_constant_override("separation",8); detail.add_child(tabs)
	var selected: String=host.get_meta("estate_tab","people")
	for tab in [["people","庄中人手"],["build","庄内设施"],["storage","钱粮出入"]]:
		var control: Button=host._button(tabs,tab[1],"estate_tab",tab[0],_select_tab.bind(host,tab[0],frame))
		control.custom_minimum_size.y=44; control.toggle_mode=true; control.button_pressed=selected==tab[0]; control.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host.status_label=host._text(detail,host._message,17); host.status_label.name="EstateStatus"; host.status_label.custom_minimum_size.y=28
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",16); detail.add_child(body)
	match selected:
		"build": facilities(host,body,view)
		"storage": storage(host,body,view)
		_: residents(host,body,view)
	frame.resized.connect(_reflow.bind(frame,columns,summary))
	_reflow(frame,columns,summary)

## 分页只保存在当前界面元数据中，不写入人物、庄产或存档，也不推进半日。
static func _select_tab(host, tab: String, source: Control) -> void:
	if not _current_source(host,source,"",true): return
	host.set_meta("estate_tab",tab)
	host._render()

## 仅格式化已投影的最近营造摘要，不扫描世界历史或发放结果。
static func _construction_text(completed: Dictionary) -> String:
	if completed.is_empty(): return ""
	return "%s已扩建至%d级"%[FACILITIES[completed.facility],completed.to_level] if completed.success else FACILITIES[completed.facility]+"营造已中断"

## 每位居民的分工来自其实际状态，缺席或被借调时保留名字但禁用无法履行的任命。
static func residents(host, parent: Control, view: Dictionary) -> void:
	var id: String=view.estate_id
	host._text(parent,"分工与管事",23)
	host._text(parent,"分工在下方选择；外出或借调的人暂不能任用。",17)
	for resident in view.residents:
		var person: String=resident.actor_id
		var row=BoxContainer.new(); row.set_meta("estate_responsive_row",true); row.name="EstateResident_"+person; row.add_theme_constant_override("separation",16); row.custom_minimum_size.y=56; parent.add_child(row)
		var absent: bool=resident.absent
		var status: String="外出" if absent else ("借调中" if resident.detached else "管事" if person==view.manager.get("actor_id","") else DUTIES[resident.duty])
		var label: Label=host._text(row,resident.display_name+"\n"+status,18); label.custom_minimum_size.x=160; label.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		if resident.detached:
			command(host,row,"办理归庄","estate_return",{"estate_id":id,"actor_ids":[person],"source_id":resident.source_id},"EstateReturn_"+person)
		var duties=OptionButton.new(); duties.name="EstateDuty_"+person; duties.custom_minimum_size=Vector2(120,44); duties.size_flags_vertical=Control.SIZE_SHRINK_CENTER; row.add_child(duties)
		duties.set_meta("resident_actor_id",person)
		var index: int=0; var current_index: int=-1; var can_change: bool=false
		for duty in DUTIES:
			duties.add_item(DUTIES[duty]); duties.set_item_metadata(index,duty)
			var preview: Dictionary=host.kernel.preview("estate_assign",{"estate_id":id,"resident_actor_id":person,"duty":duty})
			duties.set_item_disabled(index,not preview.ok)
			if duty==resident.duty: current_index=index
			elif preview.ok: can_change=true
			index+=1
		duties.disabled=not can_change or host._advancing
		# 全部选项就绪后再显示保存职责，避免禁用选中项时add_item自动改选后项。
		duties.select(current_index)
		duties.item_selected.connect(_assign_duty.bind(host,id,person,duties))
		if person==view.manager.get("actor_id",""):
			var current: Label=host._text(row,"现任管事",17); current.custom_minimum_size.x=120; current.vertical_alignment=VERTICAL_ALIGNMENT_CENTER
		else:
			var appoint: Button=command(host,row,"委任管事","estate_appoint",{"estate_id":id,"manager_actor_id":person},"EstateAppoint_"+person)
			appoint.custom_minimum_size.x=120; appoint.size_flags_vertical=Control.SIZE_SHRINK_CENTER

## 选择分工后仍提交正式命令，由世界再次校验在场、占用和职务限制。
static func _assign_duty(index: int, host, id: String, person: String, options: OptionButton) -> void:
	if options.disabled or index<0 or index>=options.item_count or options.is_item_disabled(index) or not _current_source(host,options,id): return
	host._perform("estate_assign",{"estate_id":id,"resident_actor_id":person,"duty":options.get_item_metadata(index)})

## 设施总览只列有限投影中的真实条目；零级明确未营造，选择本身不提交命令。
static func facilities(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"庄内设施",23)
	host._text(parent,"选择设施查看现状与营造事务。",17)
	var id: String=view.estate_id
	var entries: Array=view.get("facilities",[])
	if entries.is_empty():
		host._text(parent,"此处暂无可查看的设施记录。",19); return
	var selected: String=host.get_meta("estate_facility","") if host.get_meta("estate_facility_source","")==id else ""
	var present: bool=false
	for entry in entries:
		if entry.facility_id==selected: present=true
	if not present: selected=entries[0].facility_id
	var choices=HBoxContainer.new(); choices.name="EstateFacilityChoices"; choices.add_theme_constant_override("separation",10); parent.add_child(choices)
	var current: Dictionary={}
	for entry in entries:
		var facility: String=entry.facility_id
		var state_text: String="尚未营造" if entry.level==0 else "%d级"%entry.level
		var choice: Button=host._button(choices,FACILITIES[facility]+"\n"+state_text,"estate_facility",facility,_select_facility.bind(host,choices,id,facility))
		choice.name="EstateFacility_"+facility; choice.custom_minimum_size.y=76; choice.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		choice.toggle_mode=true; choice.button_pressed=selected==facility
		if selected==facility: current=entry
	if current.is_empty(): return
	var facility: String=current.facility_id
	host._text(parent,FACILITIES[facility]+(" · 尚未营造" if current.level==0 else " · %d级"%current.level),23).name="EstateFacilityDetail"
	var request: Dictionary={"estate_id":id,"facility":facility}
	var quote: Dictionary=host.kernel.preview("estate_build",request)
	if quote.ok:
		host._text(parent,"庄中支取 %d钱、%d份%s\n工期 %d个半日"%[quote.terms.money_cost,quote.terms.material_quantity,host._name(host._data.items[quote.terms.material_item_id]),quote.duration],19).name="EstateFacilityQuote"
	else: host._text(parent,host._reason(quote.get("code","Unknown")),19).name="EstateFacilityQuote"
	command(host,parent,("营造" if current.level==0 else "扩建")+FACILITIES[facility],"estate_build",request,"EstateBuild_"+facility)

## 只接受仍属当前庄院和当前画面的设施选择；按稳定ID留存，不按图片位置推造设施。
static func _select_facility(host, source: Control, id: String, facility: String) -> void:
	if not _current_source(host,source,id,true): return
	host.set_meta("estate_facility_source",id); host.set_meta("estate_facility",facility); host._render()

## 钱柜与粮仓均读取真实私产，领存默认十份实物；其他数量也由同一规则预览检查。
static func storage(host, parent: Control, view: Dictionary) -> void:
	var id: String=view.estate_id
	var store: Dictionary=view.storage
	host._text(parent,"钱柜 · %d钱"%store.wallet_balance,23)
	command(host,parent,"从随身私钱存入100钱","estate_fund",{"estate_id":id,"quantity":100},"EstateFund")
	host._text(parent,"仓储 · %d / %d"%[store.weight,store.capacity],21)
	for item in store.items:
		var item_id: String=item.item_id
		var row=BoxContainer.new(); row.set_meta("estate_responsive_row",true); row.add_theme_constant_override("separation",12); parent.add_child(row)
		var label: Label=host._text(row,"%s · %d份"%[host._name(host._data.items[item_id]),item.available_quantity],18); label.custom_minimum_size.x=180; label.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		for kind in ["estate_withdraw","estate_deposit"]:
			command(host,row,"领出10份" if kind=="estate_withdraw" else "存入10份",kind,{"estate_id":id,"item_id":item_id,"quantity":10},kind+"_"+item_id)

## 控件只使用正式预览和提交结果，拒绝原因保留在提示中，禁用状态不能绕过权限。
static func command(host, parent: Control, label: String, kind: String, params: Dictionary, name: String) -> Button:
	var preview: Dictionary=host.kernel.preview(kind,params)
	var button: Button=host._button(parent,label,kind,params.get("estate_id",""),_submit.bind(host,parent,kind,params.duplicate(true),bool(preview.ok)),not preview.ok or host._advancing)
	button.custom_minimum_size.y=44
	button.name=name
	if not preview.ok: button.tooltip_text=host._reason(preview.get("code","Unknown"))
	return button

## 当前画面、庄院和忙碌边界共用一处，旧回调不能替新庄院办理事务。
static func _current_source(host, source: Control, id: String="", allow_busy: bool=false) -> bool:
	if not is_instance_valid(source) or not source.is_inside_tree() or source.is_queued_for_deletion() or (host._advancing and not allow_busy): return false
	var frame: Node=source
	while frame!=null and not frame.has_meta("estate_scene_epoch"): frame=frame.get_parent()
	if frame==null or frame.is_queued_for_deletion(): return false
	var source_id: String=frame.get_meta("estate_id","")
	return source_id==host._estate_id and (id.is_empty() or id==source_id) and frame.get_meta("estate_scene_epoch",-1)==host._scene_counter

## 命令仍交原Main统一预览与submit；来源失效时不重定向、不改参数或伪造成功。
static func _submit(host, source: Control, kind: String, params: Dictionary, permitted: bool) -> void:
	if not permitted or not _current_source(host,source,params.get("estate_id","")): return
	host._perform(kind,params)

## 关闭只作用于原庄院原画面，办理中仍可返回，不取消既有行动。
static func _close(host, source: Control) -> void:
	if _current_source(host,source,"",true): host._close_estate()

## 窄窗改为上下结构，整页滚动保持人手和钱粮操作可到达，不缩小点击目标。
static func _reflow(frame: Control, columns: BoxContainer, summary: Control) -> void:
	if not is_instance_valid(frame) or not is_instance_valid(columns) or not is_instance_valid(summary): return
	var narrow: bool=frame.size.x<920
	columns.vertical=narrow; summary.custom_minimum_size.x=0 if narrow else 240
	for row in frame.find_children("*","BoxContainer",true,false):
		if row.get_meta("estate_responsive_row",false): row.vertical=narrow
