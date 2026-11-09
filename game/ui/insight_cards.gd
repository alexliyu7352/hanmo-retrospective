## 见闻在原收藏页阅读；本人实际经历与跨局发现分别展示，不提供能力或军职操作。
extends RefCounted

## 分页只绑定活页面和原本人来源；关闭、换世界、重绘后的回调安全失效。
static func _page(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary,source_reader: Callable,on_page: Callable,offset: int) -> void:
	var host=host_ref.get_ref();var panel: Control=panel_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(panel) or not panel.is_inside_tree() or not panel.is_visible_in_tree() or panel.is_queued_for_deletion(): return
	if host._advancing or panel.get_meta("card_command_pending",false) or not source.get("ok",false) or not source_reader.is_valid() or source!=source_reader.call(host,source.actor_id): return
	if on_page.is_valid(): on_page.call(offset)

## 有来源的本局经历展示日期；只曾发现时不给当前人物添加经历记录。
static func build(host,parent: Control,panel: Control,view: Dictionary,source: Dictionary,source_reader: Callable,on_page: Callable) -> void:
	var section=VBoxContainer.new();section.name="InsightCardsSection";section.add_theme_constant_override("separation",10);parent.add_child(section)
	host._text(section,"见闻 · 人生经历",25)
	host._text(section,"见闻记录亲身经历；曾经发现的卡片可在此回看。",16)
	if not str(view.get("discovery_notice","")).is_empty(): host._text(section,view.discovery_notice,16)
	for row in view.get("cards",[]):
		var box=VBoxContainer.new();box.name="InsightCard_"+str(row.card_id);section.add_child(box)
		host._text(box,str(row.name),23)
		var state: String="尚未发现"
		if row.get("experienced_this_world",false): state="本局亲身经历"
		elif row.get("discovered",false): state="曾经发现 · 本局尚未经历"
		if row.get("discovered_free",false): state+=" · 自由创建记录"
		host._text(box,state,18).name="InsightState"
		if not str(row.get("description","")).is_empty(): host._text(box,row.description,17)
		if row.get("experienced_this_world",false) and row.get("acquired_tick")!=null:
			host._text(box,"本局经历：第 %d 日"%(int(row.acquired_tick)/2+1),15).name="InsightDate"
		elif not str(row.get("hint","")).is_empty(): host._text(box,row.hint,16)
		box.add_child(HSeparator.new())
	if view.get("cards",[]).is_empty(): host._text(section,"本情景尚无见闻卡片。",18)
	var offset: int=view.get("offset",0);var limit: int=view.get("limit",32)
	var pages=HBoxContainer.new();section.add_child(pages)
	var host_ref: WeakRef=weakref(host);var panel_ref: WeakRef=weakref(panel)
	if offset>0:
		var previous: Button=host._button(pages,"上一页见闻","insight_cards_page","",func(): _page(host_ref,panel_ref,source,source_reader,on_page,maxi(0,offset-limit)))
		previous.name="InsightCardsPrevious";previous.disabled=not on_page.is_valid()
	if view.get("next_offset")!=null:
		var next_offset: int=int(view.next_offset)
		var next: Button=host._button(pages,"下一页见闻","insight_cards_page","",func(): _page(host_ref,panel_ref,source,source_reader,on_page,next_offset))
		next.name="InsightCardsNext";next.disabled=not on_page.is_valid()
