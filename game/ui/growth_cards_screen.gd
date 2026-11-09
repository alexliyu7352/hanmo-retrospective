## 收藏页只消费本人卡片投影；跨局发现不提供本局能力，配置始终走原预览与命令。
extends RefCounted
const Practice=preload("res://ui/growth_cards_practice.gd")
const ActorCards=preload("res://ui/actor_unlock_cards.gd")
const InsightCards=preload("res://ui/insight_cards.gd")
const CATEGORY_NAMES={"technique":"武功","heart":"心法"}

## 打开时绑定人物、世界、执行代和revision；旧页面不能对另一个人物继续提交。
static func _source(host, actor_id: String) -> Dictionary:
	var context: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity() if host.kernel.has_method("source_identity") else {}
	return {"ok":context.get("ok",false) and context.get("actor_id","")==actor_id,"actor_id":actor_id,"revision":context.get("revision",-1),"kernel_id":host.kernel.get_instance_id(),"world_id":identity.get("world_id",""),"execution_epoch":identity.get("execution_epoch",-1)}

## 关闭、重绘和重复点击都必须失效；最新正式报价成功之后只提交一次。
static func _configure(host_ref: WeakRef, panel_ref: WeakRef, button_ref: WeakRef, status_ref: WeakRef, source: Dictionary, request: Dictionary) -> void:
	var host=host_ref.get_ref()
	var panel: Control=panel_ref.get_ref()
	var button: Button=button_ref.get_ref()
	var status: Label=status_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(status): return
	if not is_instance_valid(panel) or panel.is_queued_for_deletion() or not panel.is_inside_tree() or not panel.is_visible_in_tree(): return
	if not is_instance_valid(button) or button.disabled or host._advancing or panel.get_meta("card_command_pending",false): return
	if source!=_source(host,source.actor_id) or not source.ok:
		button.disabled=true; status.text="人物状态已变化，请重新打开收藏页。"; return
	var quote: Dictionary=host.kernel.preview("configure_heart",request)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text="当前无法配置，请查看人物与行动状态。"; return
	if source!=_source(host,source.actor_id): return
	panel.set_meta("card_command_pending",true); button.disabled=true
	host._perform("configure_heart",request.duplicate(true))

## 只显示目前接通的武功、心法及其实际状态，使用宿主字体和控件，不读取整世界。
static func build(host, parent: Control, view: Dictionary, on_close: Callable, on_page: Callable, on_actor_page: Callable=Callable(), on_insight_page: Callable=Callable()) -> Control:
	var panel=PanelContainer.new(); panel.name="GrowthCardsScreen"; parent.add_child(panel)
	# 弱引用让迟到Callable在节点释放后仍能进入空值守卫，不传递已释放的Control。
	var panel_ref: WeakRef=weakref(panel)
	panel.anchor_left=.025; panel.anchor_right=.975; panel.anchor_top=.115; panel.anchor_bottom=.975
	var surface=StyleBoxFlat.new(); surface.bg_color=Color("182022f5"); surface.border_color=Color("8a7550"); surface.set_border_width_all(1)
	for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]: surface.set_content_margin(side,18)
	panel.add_theme_stylebox_override("panel",surface)
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var header=HBoxContainer.new(); column.add_child(header)
	var title: Label=host._text(header,"卡片收藏" if view.get("actor_cards") is Dictionary or view.get("insight_cards") is Dictionary else "卡片收藏 · 武功与心法",28); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var close: Button=host._button(header,"返回","growth_cards_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call())
	close.name="GrowthCardsClose"
	host._text(column,"曾经发现的卡片保留在图鉴中；本局掌握与所用心法以当前人物为准。",16)
	if not str(view.get("discovery_notice","")).is_empty(): host._text(column,view.discovery_notice,16)
	var scroll=ScrollContainer.new(); scroll.name="GrowthCardsScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var rows=VBoxContainer.new(); rows.name="GrowthCardRows"; rows.size_flags_horizontal=Control.SIZE_EXPAND_FILL; rows.add_theme_constant_override("separation",14); scroll.add_child(rows)
	var source: Dictionary=_source(host,view.actor_id)
	for row in view.cards: _card(host,rows,panel,row,source)
	if view.cards.is_empty(): host._text(rows,"本情景尚无可展示的武功或心法卡片。",20)
	if view.get("actor_cards") is Dictionary:
		ActorCards.build(host,rows,panel,view.actor_cards,source,func(current_host,actor_id: String): return _source(current_host,actor_id),on_actor_page)
	if view.get("insight_cards") is Dictionary:
		InsightCards.build(host,rows,panel,view.insight_cards,source,func(current_host,actor_id: String): return _source(current_host,actor_id),on_insight_page)
	var pages=HBoxContainer.new(); column.add_child(pages)
	var offset: int=view.get("offset",0); var limit: int=view.get("limit",32)
	if offset>0:
		var previous: Button=host._button(pages,"上一页","growth_cards_page","",func():
			var current: Control=panel_ref.get_ref()
			if is_instance_valid(current) and current.is_inside_tree() and not current.is_queued_for_deletion() and current.is_visible_in_tree() and on_page.is_valid(): on_page.call(maxi(0,offset-limit)))
		previous.name="GrowthCardsPrevious"
	if view.get("next_offset")!=null:
		var next: Button=host._button(pages,"下一页","growth_cards_page","",func():
			var current: Control=panel_ref.get_ref()
			if is_instance_valid(current) and current.is_inside_tree() and not current.is_queued_for_deletion() and current.is_visible_in_tree() and on_page.is_valid(): on_page.call(int(view.next_offset)))
		next.name="GrowthCardsNext"
	return panel

## 同一行区分发现、本局取得和当前掌握；只给已学心法显示真正配置入口。
static func _card(host, parent: Control, panel: Control, row: Dictionary, source: Dictionary) -> void:
	var box=VBoxContainer.new(); box.name="Card_"+str(row.card_id); box.set_meta("card_id",row.card_id); parent.add_child(box)
	host._text(box,CATEGORY_NAMES.get(row.category,"卡片")+" · "+str(row.name),23)
	var state: String="尚未发现"
	if row.learned: state="本局已学会 · 掌握 %d / 100"%int(row.mastery)
	elif row.discovered: state="曾经发现 · 本局未学会"
	if row.get("discovered_free",false): state+=" · 自由创建记录"
	if row.configured: state+=" · 当前运转"
	host._text(box,state,18).name="CardState"
	if not str(row.description).is_empty(): host._text(box,row.description,17)
	if row.acquired_this_world and row.get("acquired_tick")!=null:
		host._text(box,"本局取得：第 %d 日"%(int(row.acquired_tick)/2+1),15)
	elif not row.learned: host._text(box,row.hint,16)
	if row.learned and not str(row.personal_skill_id).is_empty(): Practice.build(host,box,panel,row,source,func(current_host,actor_id: String): return _source(current_host,actor_id))
	if row.category=="heart" and row.learned and not str(row.personal_skill_id).is_empty():
		var request: Dictionary={"skill_id":"" if row.configured else row.personal_skill_id}
		var status: Label=host._text(box,"",15); status.name="CardConfigureStatus"
		var binding: Dictionary={}
		var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel); var status_ref: WeakRef=weakref(status)
		var button: Button=host._button(box,"撤下心法" if row.configured else "运转此心法","configure_heart",row.personal_skill_id,func(): _configure(host_ref,panel_ref,binding.button_ref,status_ref,source,request))
		button.name="CardConfigureHeart"; binding["button_ref"]=weakref(button)
		var quote: Dictionary=host.kernel.preview("configure_heart",request)
		button.disabled=not source.ok or not quote.get("ok",false) or host._advancing
		status.text="确认后生效，不消耗半日。" if not button.disabled else "当前无法配置，请先结束行动或查看人物状态。"
	box.add_child(HSeparator.new())
