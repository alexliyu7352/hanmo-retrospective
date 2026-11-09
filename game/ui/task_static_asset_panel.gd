## 本人已经办理的实物明细；只翻页读取原凭据，不替代任务结酬或继产资格核验。
extends RefCounted
const QUERY="task_static_assets"
const META="task_static_asset_page"

## 分页与批次选择同完整来源绑定，控制者/存档/推进变化后从原首批重取。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0,"receipt_id":"","lot_cursor":0}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _args(draft: Dictionary) -> Dictionary:
	return {"cursor":draft.cursor,"receipt_id":draft.receipt_id,"lot_cursor":draft.lot_cursor}

static func _same(page: Dictionary, draft: Dictionary) -> bool:
	return page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id and page.get("cursor")==draft.cursor and page.get("receipt_id")==draft.receipt_id and page.get("lot_cursor")==draft.lot_cursor

## 所有细节经原异步预读；旧回答不能套到新人物或另一项批次选择上。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	return _same(page,draft)

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	return page.get("ok",false) and _same(page,draft) and (not page.get("rows",[]).is_empty() or draft.cursor>0)

## 明细分两级展开，原货物名称取已安装目录；批次/容器标识只作原件追溯。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false):
		if page.get("code") not in ["TaskStaticAssetProfileRequired","NativeAsyncRequired"]: host._text(parent,"本人实物明细暂不能查看："+host._reason(page.get("code","Unknown")),15)
		return
	if not _same(page,draft): return
	for row in page.get("rows",[]):
		var box=VBoxContainer.new();box.name="TaskStaticAssetRow";parent.add_child(box)
		host._text(box,row.get("display_name","")+" · "+row.get("stage_text",""),17)
		host._text(box,"第 %d 半日 · 实际移交 %d 批 · 原行囊遗留 %d 批"%[row.get("observed_tick",0),row.get("transferred_batches",0),row.get("retained_batches",0)],14)
		host._text(box,row.get("explanation",""),14)
		if draft.receipt_id!=row.receipt_id:
			_button(host,box,"查看原批次",draft.cursor,row.receipt_id,0,draft)
		else:
			for lot in page.get("lots",[]):
				var label: String="已移交" if lot.get("kind")=="transferred" else "当时仍留"
				var item: Dictionary=host._data.items.get(lot.get("item_id",""),{})
				host._text(box,label+" · "+host._name(item)+" ×"+str(lot.get("quantity",0)),15)
				host._text(box,"原批次："+lot.get("source_lot_id","")+" · 原容器："+lot.get("from_inventory_id",""),13)
				if lot.get("kind")=="transferred": host._text(box,"交至："+lot.get("to_inventory_id","")+" · 新批次："+lot.get("destination_lot_id",""),13)
			if draft.lot_cursor>0: _button(host,box,"批次首批",draft.cursor,row.receipt_id,0,draft)
			if page.has("next_lot_cursor"): _button(host,box,"更多原批次",draft.cursor,row.receipt_id,int(page.next_lot_cursor),draft)
			_button(host,box,"收起原批次",draft.cursor,"",0,draft)
	if draft.cursor>0: _button(host,parent,"实物明细首批",0,"",0,draft)
	if page.has("next_cursor"): _button(host,parent,"更多本人实物明细",int(page.next_cursor),"",0,draft)

static func _button(host, parent: Control, text: String, cursor: int, receipt: String, lot_cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"task_static_asset_page",receipt,func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),cursor,receipt,lot_cursor,draft.duplicate(true),host._scene_counter))

## 同来源但已经翻页的旧回调也拒绝；这里只变本地草稿，不产生任何业务命令。
static func _page(host_ref: WeakRef, control_ref: WeakRef, cursor: int, receipt: String, lot_cursor: int, captured: Dictionary, epoch: int) -> void:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return
	if not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree() or control.disabled: return
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return
	if host.kernel.source_identity()!=captured.source or host.kernel.command_context().get("actor_id")!=captured.actor_id or host.get_meta(META,{})!=captured: return
	if cursor<0 or lot_cursor<0 or (receipt.is_empty() and lot_cursor!=0): return
	var next: Dictionary=captured.duplicate(true);next.cursor=cursor;next.receipt_id=receipt;next.lot_cursor=lot_cursor
	host.set_meta(META,next);host._render()
