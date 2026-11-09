## 携物与护送层只消费S3本人实际货批/同行者，不查询远方库存或制造运输实体。
extends RefCounted
const META: StringName=&"map_transport_draft"
const Presence=preload("res://ui/world_map_presence.gd")

## 来源换代清掉翻页和名单；空间锚点仍来自原地图本人位置。
static func _draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source: draft={"source":source,"cursor":{},"page":{}}
	host.set_meta(META,draft.duplicate(true)); return draft

## 原Main页面读屏障准备同源有限页，未知能力保持可定位错误，不代替任务世界。
static func prepare(host) -> bool:
	if host.get("_resident_mode")!=true or not host.kernel.has_method("demand_view"): return true
	var draft: Dictionary=_draft(host); var args: Dictionary={}
	if not draft.cursor.is_empty(): args.cursor=draft.cursor.duplicate(true)
	var page: Dictionary=host.kernel.demand_view("task_travel_manifest",args)
	if page.get("code")=="NativeAsyncRequired": return false
	if page.get("ok",true)!=false:
		var checked: Dictionary=_validate(page,draft.source)
		if not checked.ok: page=checked
	draft.page=page.duplicate(true);host.set_meta(META,draft);return true

## 所有文字保原生字节上限；空显示名可回退通用称谓，身份必须非空。
static func _text_ok(value: Variant, required: bool=false) -> bool:
	return value is String and value.to_utf8_buffer().size()<=4096 and (not required or not value.is_empty())

## 验证本人完整身份与两种封闭行；实际lot逐行保留，不跨页合计数量。
static func _validate(page: Dictionary, source: Dictionary) -> Dictionary:
	if page.get("ok")!=true or page.get("version")!=1 or page.get("identity")!=source.identity or page.get("observer_actor_id")!=source.actor_id: return {"ok":false,"code":"StaleTaskTravelManifest"}
	if not page.get("rows") is Array or page.rows.size()>64 or not page.get("cursor") is int or page.cursor<0 or not page.get("scanned") is int or page.scanned<0 or page.scanned>256: return {"ok":false,"code":"InvalidTaskTravelManifest"}
	var seen: Dictionary={}
	for row in page.rows:
		if not row is Dictionary or not _text_ok(row.get("task_id"),true) or not _text_ok(row.get("display_name")) or not _text_ok(row.get("task_status"),true) or not row.get("observed_tick") is int or row.observed_tick!=source.identity.get("tick"): return {"ok":false,"code":"InvalidTaskTravelRow"}
		var key: String=""
		match row.get("kind"):
			"cargo":
				if not _text_ok(row.get("lot_id"),true) or not _text_ok(row.get("item_id"),true) or not _text_ok(row.get("item_name")) or row.get("cargo_kind") not in ["goods","message"] or not row.get("quantity") is int or row.quantity<=0 or not row.get("reserved") is bool: return {"ok":false,"code":"InvalidTaskTravelRow"}
				key="cargo:"+row.lot_id
			"escort":
				if not _text_ok(row.get("actor_id"),true) or row.actor_id==source.actor_id or not _text_ok(row.get("actor_name")) or row.get("objective_state") not in ["not_departed","travelling","arrived","failed"]: return {"ok":false,"code":"InvalidTaskTravelRow"}
				key="escort:"+row.task_id+"/"+row.actor_id
			_: return {"ok":false,"code":"InvalidTaskTravelRow"}
		if seen.has(key): return {"ok":false,"code":"InvalidTaskTravelRow"}
		seen[key]=true
	if page.has("next_cursor"):
		var cursor: Variant=page.next_cursor
		if not cursor is Dictionary or cursor.get("identity")!=source.identity or cursor.get("observer_actor_id")!=source.actor_id or not cursor.get("offset") is int or cursor.offset<0: return {"ok":false,"code":"InvalidTaskTravelCursor"}
	return {"ok":true}

## 翻页只转交所属域发回的完整游标，旧按钮不能借新身份读取另一页。
static func _page(host_ref: WeakRef, cursor: Dictionary, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	if not guard.call(): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or source.identity!=host.kernel.source_identity(): return
	draft.cursor=cursor.duplicate(true);draft.page={};draft.open=true;host.set_meta(META,draft);host._render()

## 离开地图释放这代清单；旧树退出不能删除已挂到新场景的同源页。
static func _exit(host_ref: WeakRef, source: Dictionary, scene_epoch: int) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	var draft: Dictionary=host.get_meta(META,{})
	if not host.journey_view.opened and draft.get("source")==source and draft.get("scene_epoch")==scene_epoch: host.remove_meta(META)

## 打开只持宿主/场景/面板弱引用，迟到回调不会先解码已经释放的lambda节点捕获。
static func _open(host_ref: WeakRef, stage_ref: WeakRef, panel_ref: WeakRef, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref();var stage=stage_ref.get_ref();var panel=panel_ref.get_ref()
	if host==null or stage==null or panel==null or not guard.call():return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:return
	_hide_other(host,stage,Presence.META,"WorldMapPresence");panel.show();draft.open=true;host.set_meta(META,draft)

## 收起只改变仍挂载的这代草稿，旧面板不能覆盖当前页面。
static func _close(host_ref: WeakRef, panel_ref: WeakRef, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref();var panel=panel_ref.get_ref()
	if host==null or panel==null or not guard.call():return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:return
	panel.hide();draft.open=false;draft.cursor={};draft.page={};host.set_meta(META,draft);host._render()

## 另一图层的迟到信号同样先核弱引用，再执行同源互斥展示。
static func _hide_for_presence(host_ref: WeakRef, stage_ref: WeakRef, source: Dictionary, guard: Callable) -> void:
	var host=host_ref.get_ref();var stage=stage_ref.get_ref()
	if host==null or stage==null or not guard.call():return
	if host.get_meta(META,{}).get("source")!=source:return
	_hide_other(host,stage,META,"WorldMapTransport")

## 同一画面只展开一份本人清单，不靠覆盖的面板截获另一层点击。
static func _hide_other(host, stage: Control, key: StringName, panel_name: String) -> void:
	var other=stage.get_node_or_null(panel_name)
	if other!=null: other.hide()
	var draft: Dictionary=host.get_meta(key,{})
	if not draft.is_empty(): draft.open=false;host.set_meta(key,draft)

## 标签只描述真实当前携物/护送；数量保留货批粒度，原任务办理仍在S3入口。
static func attach(host, stage: Control, canvas, guard: Callable) -> Control:
	if host.get("_resident_mode")!=true: return null
	var draft: Dictionary=host.get_meta(META,{})
	if draft.is_empty() or draft.get("source",{}).get("identity")!=host.kernel.source_identity(): return null
	var page: Dictionary=draft.get("page",{})
	if page.is_empty(): return null
	draft.scene_epoch=host._scene_counter;host.set_meta(META,draft)
	var host_ref: WeakRef=weakref(host);var stage_ref: WeakRef=weakref(stage)
	stage.tree_exiting.connect(_exit.bind(host_ref,draft.source,host._scene_counter))
	var failed: bool=page.get("ok",true)==false
	if not failed and page.rows.is_empty() and not page.has("next_cursor"): canvas.set_transport_available(false);return null
	canvas.set_transport_available(true)
	var panel=PanelContainer.new();panel.name="WorldMapTransport";stage.add_child(panel)
	var panel_ref: WeakRef=weakref(panel)
	panel.anchor_left=.375;panel.anchor_right=.705;panel.anchor_top=.225;panel.anchor_bottom=.625
	var body=VBoxContainer.new();panel.add_child(body);var heading=HBoxContainer.new();body.add_child(heading)
	host._text(heading,"携物与护送",21).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(heading,"收起","map_transport_close","",_close.bind(host_ref,panel_ref,draft.source,guard))
	if failed:
		host._text(body,"携行资料暂不可用",17);host._text(body,host._reason(page.get("code","InvalidTaskTravelManifest")),15)
	else:
		host._text(body,"半日%d · 本人实际携行"%page.identity.tick,15)
		var scroll=ScrollContainer.new();scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL;scroll.custom_minimum_size.y=80;body.add_child(scroll)
		var list=VBoxContainer.new();list.size_flags_horizontal=Control.SIZE_EXPAND_FILL;scroll.add_child(list)
		for row in page.rows:
			var text: String=row.display_name if not row.display_name.is_empty() else "受托事务"
			text+=" · "+{"active":"正在履行","ready":"可以回报","settling":"尚待清算","failed":"未能办妥","cancelled":"已经辞谢","expired":"期限已过","success":"已履约","late":"逾期办妥"}.get(row.task_status,"请依委托约定办理")
			if row.kind=="cargo": text+="\n%s%s × %d%s"%["受托信物 · " if row.cargo_kind=="message" else "受托物品 · ",row.item_name if not row.item_name.is_empty() else "物品",row.quantity," · 已预留" if row.reserved else ""]
			else: text+="\n护送 · "+(row.actor_name if not row.actor_name.is_empty() else "同行者")+" · "+{"not_departed":"尚未启程","travelling":"正在同行","arrived":"已到约定地点","failed":"护送结果待处理"}[row.objective_state]
			var label: Label=host._text(list,text,18);label.set_meta("map_transport_row",row.duplicate(true))
		if page.rows.is_empty(): host._text(list,"此页没有实际携物或护送。",17)
		var navigation=HBoxContainer.new();body.add_child(navigation)
		if not draft.cursor.is_empty(): host._button(navigation,"从头查看","map_transport_first","",_page.bind(host_ref,{},draft.source,guard))
		if page.has("next_cursor"): host._button(navigation,"继续查看","map_transport_next","",_page.bind(host_ref,page.next_cursor,draft.source,guard))
	panel.visible=draft.get("open",false)
	canvas.transport_requested.connect(_open.bind(host_ref,stage_ref,panel_ref,draft.source,guard))
	canvas.presence_requested.connect(_hide_for_presence.bind(host_ref,stage_ref,draft.source,guard))
	return panel
