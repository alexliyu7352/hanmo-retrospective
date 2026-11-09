## 人物卡在原收藏页内展示，跨局资格不授予本局操控权。
extends RefCounted
static func _page(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary,source_reader: Callable,on_page: Callable,offset: int) -> void:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(panel) or not panel.is_inside_tree() or not panel.is_visible_in_tree() or panel.is_queued_for_deletion(): return
	if host._advancing or panel.get_meta("card_command_pending",false) or not source.get("ok",false) or not source_reader.is_valid() or source!=source_reader.call(host,source.actor_id): return
	if on_page.is_valid(): on_page.call(offset)

static func build(host, parent: Control, panel: Control, view: Dictionary, source: Dictionary, source_reader: Callable, on_page: Callable) -> void:
	var section=VBoxContainer.new(); section.name="ActorCardsSection"; section.add_theme_constant_override("separation",10); parent.add_child(section)
	host._text(section,"人物卡 · 新局资格",25)
	host._text(section,"完成委托可取得人物卡；新开局时可选择已解锁人物，人物沿用该情景的真实身份。",16)
	if not str(view.get("unlock_notice","")).is_empty(): host._text(section,view.unlock_notice,16).name="ActorUnlockNotice"
	for row in view.get("cards",[]):
		var box=VBoxContainer.new(); box.name="ActorCard_"+str(row.card_id); box.set_meta("target_actor_id",row.get("target_actor_id","")); section.add_child(box)
		host._text(box,str(row.name),23)
		var state: String="尚未解锁"
		if row.get("unlocked",false): state="新局人物已解锁"+(" · 自由创建记录" if row.get("unlocked_free",false) else "")
		elif row.get("acquired") is Dictionary: state="本局条件已达成 · 跨局人物资格尚未保存"
		host._text(box,state,18).name="ActorUnlockState"
		var progress: String="本局已完成采购委托：%d / 2"%int(row.completed_count)
		if row.get("condition_kind")=="completed_envoy_round_trip_v1": progress="本局已完成交涉往返：%d / 1"%int(row.completed_count)
		host._text(box,progress,17).name="ActorUnlockProgress"
		if not str(row.get("description","")).is_empty(): host._text(box,row.description,17)
		var receipt: Dictionary=row.get("unlock_source",{})
		if row.get("unlocked",false) and receipt.has("acquired_tick"):
			host._text(box,"首次取得：该次人生第 %d 日"%(int(receipt.acquired_tick)/2+1),15)
		elif not str(row.get("hint","")).is_empty(): host._text(box,row.hint,16)
		box.add_child(HSeparator.new())
	if view.get("cards",[]).is_empty(): host._text(section,"本情景尚无人物卡。",18)
	var offset: int=view.get("offset",0); var limit: int=view.get("limit",32)
	var pages=HBoxContainer.new(); section.add_child(pages)
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	if offset>0:
		var previous: Button=host._button(pages,"上一页人物卡","actor_cards_page","",func(): _page(host_ref,panel_ref,source,source_reader,on_page,maxi(0,offset-limit)))
		previous.name="ActorCardsPrevious"; previous.disabled=not on_page.is_valid()
	if view.get("next_offset")!=null:
		var next_offset: int=int(view.next_offset)
		var next: Button=host._button(pages,"下一页人物卡","actor_cards_page","",func(): _page(host_ref,panel_ref,source,source_reader,on_page,next_offset))
		next.name="ActorCardsNext"; next.disabled=not on_page.is_valid()
