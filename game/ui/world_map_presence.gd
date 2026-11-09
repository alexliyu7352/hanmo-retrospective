## 同地人物图层只消费原生有限观察页；不读取人物总表，不推断远方位置或接待权限。
extends RefCounted
const META: StringName=&"map_presence_draft"

## 身份变化时清除只读翻页草稿；人物位置与世界时间仍以Kernel为准。
static func _draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source: draft={"source":source,"cursor":{},"page":{}}
	host.set_meta(META,draft.duplicate(true))
	return draft

## 只在原Main页面准备阶段排入同源读队列，渲染本身不请求或推进任何业务。
static func prepare(host) -> bool:
	if host.get("_resident_mode")!=true or not host.kernel.has_method("demand_view"): return true
	var draft: Dictionary=_draft(host)
	var args: Dictionary={}
	if not draft.cursor.is_empty(): args.cursor=draft.cursor.duplicate(true)
	var page: Dictionary=host.kernel.demand_view("map_presence",args)
	if page.get("code")=="NativeAsyncRequired": return false
	if page.get("ok",true)==false:
		# 缺新增显示能力不阻断旧地图旅行，图层保留明确不可用说明。
		draft.page=page.duplicate(true); host.set_meta(META,draft)
		return true
	var checked: Dictionary=_validate(page,draft.source)
	if not checked.ok:
		draft.page=checked; host.set_meta(META,draft)
		return true
	draft.page=page.duplicate(true); host.set_meta(META,draft)
	return true

## 页面只接受当前身份的至多64个真实ID，过期、重复或未知行不得成为地图标记。
static func _validate(page: Dictionary, source: Dictionary) -> Dictionary:
	if page.get("version")!=1 or page.get("identity")!=source.identity or page.get("observer_actor_id")!=source.actor_id: return {"ok":false,"code":"StaleMapPresence"}
	if not page.get("rows") is Array or page.rows.size()>64 or not page.get("cursor") is int or page.cursor<0: return {"ok":false,"code":"InvalidMapPresencePage"}
	var seen: Dictionary={}
	for row in page.rows:
		if not row is Dictionary or row.get("kind")!="actor" or not row.get("actor_id") is String or row.actor_id.is_empty() or row.actor_id==source.actor_id or seen.has(row.actor_id): return {"ok":false,"code":"InvalidMapPresenceRow"}
		if not row.get("display_name") is String or row.display_name.to_utf8_buffer().size()>4096 or row.actor_id.to_utf8_buffer().size()>4096 or not row.get("observed_tick") is int or row.observed_tick!=source.identity.get("tick"): return {"ok":false,"code":"InvalidMapPresenceRow"}
		seen[row.actor_id]=true
	if page.has("next_cursor") and (not page.next_cursor is Dictionary or page.next_cursor.get("identity")!=source.identity or page.next_cursor.get("observer_actor_id")!=source.actor_id): return {"ok":false,"code":"InvalidMapPresenceCursor"}
	return {"ok":true}

## 只保存原生发来的下一页游标；页面关闭、世界变代或旧按钮都不能翻动当前页面。
static func _page(host_ref: WeakRef, cursor: Dictionary, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	if not guard.call(): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or source.identity!=host.kernel.source_identity(): return
	draft.cursor=cursor.duplicate(true); draft.page={}; draft.open=true; host.set_meta(META,draft)
	host._render()

## 退出地图时丢弃页面数据；旧树退出不能清掉已经挂到新scene_epoch的同源新页。
static func _exit(host_ref: WeakRef, source: Dictionary, scene_epoch: int) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	var draft: Dictionary=host.get_meta(META,{})
	if not host.journey_view.opened and draft.get("source")==source and draft.get("scene_epoch")==scene_epoch: host.remove_meta(META)

## 延迟输入只持弱引用；旧面板释放后连lambda捕获解码都不能触碰已死节点。
static func _open(host_ref: WeakRef, panel_ref: WeakRef, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref();var panel=panel_ref.get_ref()
	if host==null or panel==null or not guard.call():return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:return
	panel.show();draft.open=true;host.set_meta(META,draft)

## 收起也必须核真实存活节点和当前草稿，不把旧捕获字典写回新页面。
static func _close(host_ref: WeakRef, panel_ref: WeakRef, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref();var panel=panel_ref.get_ref()
	if host==null or panel==null or not guard.call():return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:return
	panel.hide();draft.open=false;draft.cursor={};draft.page={};host.set_meta(META,draft);host._render()

## 标记绑在本人实际锚点旁，展开只读现场名单；姓名不变成未经preview的交谈命令。
static func attach(host, stage: Control, canvas, guard: Callable) -> Control:
	if host.get("_resident_mode")!=true: return null
	var draft: Dictionary=host.get_meta(META,{})
	if draft.is_empty() or draft.get("source",{}).get("identity")!=host.kernel.source_identity(): return null
	var page: Dictionary=draft.get("page",{})
	if page.is_empty(): return null
	draft.scene_epoch=host._scene_counter; host.set_meta(META,draft)
	var host_ref: WeakRef=weakref(host)
	stage.tree_exiting.connect(_exit.bind(host_ref,draft.source,host._scene_counter))
	var failed: bool=page.get("ok",true)==false
	canvas.set_presence_available(true)
	var panel=PanelContainer.new(); panel.name="WorldMapPresence"; stage.add_child(panel)
	var panel_ref: WeakRef=weakref(panel)
	panel.anchor_left=.375; panel.anchor_right=.705; panel.anchor_top=.225; panel.anchor_bottom=.625
	var body=VBoxContainer.new(); panel.add_child(body)
	var heading=HBoxContainer.new(); body.add_child(heading)
	host._text(heading,"此处人物",21).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(heading,"收起","map_presence_close","",_close.bind(host_ref,panel_ref,draft.source,guard))
	if failed:
		host._text(body,"此处人物资料暂不可用",17)
		host._text(body,host._reason(page.get("code","InvalidMapPresencePage")),15)
	else:
		host._text(body,"半日%d · 当前同地所见"%page.identity.tick,15)
		var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.custom_minimum_size.y=80; body.add_child(scroll)
		var list=VBoxContainer.new(); list.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(list)
		for row in page.rows:
			var label: Label=host._text(list,row.display_name if not row.display_name.is_empty() else "一位在此的人",18)
			label.set_meta("map_presence_actor_id",row.actor_id)
		if page.rows.is_empty(): host._text(list,"此刻未见其他人物。",17)
		var navigation=HBoxContainer.new(); body.add_child(navigation)
		if not draft.cursor.is_empty(): host._button(navigation,"从头查看","map_presence_first","",_page.bind(host_ref,{},draft.source,guard))
		if page.has("next_cursor"): host._button(navigation,"更多人物","map_presence_next","",_page.bind(host_ref,page.next_cursor,draft.source,guard))
	panel.visible=draft.get("open",false)
	canvas.presence_requested.connect(_open.bind(host_ref,panel_ref,draft.source,guard))
	return panel
