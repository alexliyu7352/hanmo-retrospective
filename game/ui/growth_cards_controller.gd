## 卡片页与跨局记录的本域宿主接线；中央只提供有限查询、身份回调和返回场所入口。
extends RefCounted
const Profile=preload("res://core/card_discovery_profile.gd")
const Screen=preload("res://ui/growth_cards_screen.gd")
var profile: RefCounted
var _loaded: bool=false
var _panel: Control

## 默认使用独立图鉴路径；测试路径不触碰玩家世界银行。
func _init(path: String="user://profiles/card_discovery_v1.json") -> void:
	profile=Profile.new(path)

## 损坏或不可读图鉴只报告自身失败，不修改世界结果，也不以空记录覆盖旧文件。
func _ensure_profile() -> Dictionary:
	if _loaded: return {"ok":true}
	var result: Dictionary=profile.load_profile(); _loaded=result.get("ok",false); return result

## 成功世界提交后有限分页观察本人取得；所有页与同一人物、世界及revision绑定再保存。
func sync_after_commit(query_cards: Callable, source_context: Callable, creation_mode: String) -> Dictionary:
	return _sync_after_commit(query_cards,source_context,creation_mode,false)

## 见闻使用同一跨局图鉴，只接受当前本人、完整同源的有限经历投影。
func sync_insights_after_commit(query_cards: Callable, source_context: Callable, creation_mode: String) -> Dictionary:
	return _sync_after_commit(query_cards,source_context,creation_mode,true)

## 收齐全部同源页后才观察；中途换世界或某页失败不保存部分发现。
func _sync_after_commit(query_cards: Callable, source_context: Callable, creation_mode: String, insights: bool) -> Dictionary:
	var loaded: Dictionary=_ensure_profile()
	if not loaded.ok: return loaded
	var source: Dictionary=source_context.call().duplicate(true)
	if not source.get("ok",false) or not source.get("actor_id") is String or source.actor_id.is_empty(): return {"ok":false,"code":"CardSourceUnavailable"}
	var identity: Dictionary={}
	if insights:
		for key in ["world_id","execution_epoch","revision","tick"]: identity[key]=source.get(key)
	var offset: int=0; var rows: Array=[]
	for _page in range(65):
		if source!=source_context.call(): return {"ok":false,"code":"CardSourceChanged"}
		var result: Variant=query_cards.call({"actor_id":source.actor_id,"offset":offset,"limit":64,"discovered":[]})
		if not result is Dictionary: return {"ok":false,"code":"InvalidCardPage"}
		if not result.get("ok",true):
			if result.get("code","")==("InsightCardsUnavailable" if insights else "CardsUnavailable") and offset==0: return {"ok":true,"supported":false}
			return result
		if result.get("actor_id")!=source.actor_id or not result.get("cards") is Array or result.cards.size()>64 or result.get("offset")!=offset: return {"ok":false,"code":"InvalidCardPage"}
		if insights and result.get("source_identity")!=identity: return {"ok":false,"code":"CardSourceChanged"}
		rows.append_array(result.cards)
		if rows.size()>4096: return {"ok":false,"code":"DiscoveryLimit"}
		if result.get("next_offset")==null:
			if source!=source_context.call(): return {"ok":false,"code":"CardSourceChanged"}
			var observation: Dictionary={"actor_id":source.actor_id,"cards":rows}
			if insights: observation["source_identity"]=identity
			var observed: Dictionary=profile.observe(observation,creation_mode)
			if not observed.ok: return observed
			if profile.needs_save():
				var saved: Dictionary=profile.save_profile()
				if not saved.ok: return saved
			return {"ok":true,"supported":true,"changed":observed.changed}
		if not (result.next_offset is int or result.next_offset is float) or result.next_offset!=offset+result.cards.size() or result.next_offset<=offset: return {"ok":false,"code":"InvalidCardPage"}
		offset=int(result.next_offset)
	return {"ok":false,"code":"DiscoveryLimit"}

## 只查一个有限页；关闭/翻页保持原世界，图鉴故障仍可阅读当前本领。
func page_view(query_cards: Callable, offset: int=0) -> Dictionary:
	var loaded: Dictionary=_ensure_profile()
	var result: Variant=query_cards.call({"offset":offset,"limit":32,"discovered":profile.discovered_ids()})
	if not result is Dictionary or not result.get("ok",true) or not result.get("cards") is Array: return result if result is Dictionary else {"ok":false,"code":"InvalidCardPage"}
	var view: Dictionary=profile.decorate(result)
	if not loaded.ok: view["discovery_notice"]="跨局发现记录暂不可用；本局掌握仍可查看。"
	return view

## 同步宿主可直接翻页；原生Main传入页选择回调，让新需求先经原异步prepare再重绘。
func open(host, parent: Control, query_cards: Callable, on_close: Callable, offset: int=0, on_page: Callable=Callable()) -> Dictionary:
	var view: Dictionary=page_view(query_cards,offset)
	if not view.get("ok",true) or not view.get("cards") is Array: return view
	return open_view(host,parent,view,on_close,func(next_offset: int):
		if on_page.is_valid(): on_page.call(next_offset)
		else: open(host,parent,query_cards,on_close,next_offset))

## Main先经同一读屏障取得本领与人物卡，再在原收藏面板一次挂载。
func open_view(host, parent: Control, view: Dictionary, on_close: Callable, on_page: Callable, on_actor_page: Callable=Callable(), on_insight_page: Callable=Callable()) -> Dictionary:
	if not view.get("ok",true) or not view.get("cards") is Array: return view
	close()
	_panel=Screen.build(host,parent,view,func(): close(); on_close.call(),on_page,on_actor_page,on_insight_page)
	return {"ok":true}

## 页面退出后立即脱离树，旧闭包不能在同帧继续提交或翻页。
func close() -> void:
	if is_instance_valid(_panel):
		if _panel.get_parent()!=null: _panel.get_parent().remove_child(_panel)
		_panel.queue_free()
	_panel=null
