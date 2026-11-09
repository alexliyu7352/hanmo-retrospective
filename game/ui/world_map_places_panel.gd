## 可收起的地点检索，仅发出稳定作者ID；关闭、翻页与输入不改变世界或行程草案。
extends PanelContainer
signal destination_selected(id: String)
const PlaceIndex=preload("res://ui/world_map_place_index.gd")
var data: Dictionary={}
var _candidates: Array=[]
var _offset: int=0
var _query: LineEdit
var _items: ItemList
var _summary: Label
var _previous: Button
var _next: Button

## 复用地图主题和键盘标准行为，结果单击或回车明确选点。
func _ready() -> void:
	name="WorldMapPlaces"
	var column=VBoxContainer.new(); add_child(column)
	var top=HBoxContainer.new(); column.add_child(top)
	_summary=Label.new(); _summary.size_flags_horizontal=Control.SIZE_EXPAND_FILL; top.add_child(_summary)
	var close=Button.new(); close.text="收起"; close.set_meta("command_kind","map_search_close"); top.add_child(close); close.pressed.connect(hide)
	_query=LineEdit.new(); _query.name="PlaceQuery"; _query.placeholder_text="输入地点名称"; column.add_child(_query)
	_query.text_changed.connect(func(_text: String): _offset=0; _refresh())
	_items=ItemList.new(); _items.name="PlaceResults"; _items.size_flags_vertical=Control.SIZE_EXPAND_FILL; _items.custom_minimum_size.y=120; column.add_child(_items)
	_items.item_clicked.connect(func(index: int,_at: Vector2,button: int):
		if button==MOUSE_BUTTON_LEFT: _choose(index))
	_items.item_activated.connect(_choose)
	_query.text_submitted.connect(func(_text: String):
		if _items.item_count==1: _choose(0)
		elif _items.item_count>0: _items.grab_focus(); _items.select(0))
	var paging=HBoxContainer.new(); column.add_child(paging)
	_previous=Button.new(); _previous.text="上一页"; paging.add_child(_previous)
	_next=Button.new(); _next.text="下一页"; paging.add_child(_next)
	_previous.pressed.connect(func(): _offset=maxi(0,_offset-PlaceIndex.PAGE_SIZE); _refresh())
	_next.pressed.connect(func(): _offset+=PlaceIndex.PAGE_SIZE; _refresh())
	_refresh(); hide()

## 密集标记传入有限ID集合，普通查找清除集合；不读取其他内容包或人物。
func open_candidates(ids: Array=[]) -> void:
	_candidates=ids.duplicate(); _offset=0; _query.text=""; _refresh(); show(); _query.grab_focus()

## 每页最多32行，空结果保持可读，长名称保留完整提示。
func _refresh() -> void:
	if _items==null: return
	var page: Dictionary=PlaceIndex.query(data,_query.text,_candidates,_offset)
	_offset=page.offset; _items.clear()
	for row in page.rows:
		var kind: String={"city":"城邑","village":"村落"}.get(row.kind,"地点")
		var index: int=_items.add_item(row.label+" · "+kind)
		_items.set_item_metadata(index,row.id); _items.set_item_tooltip(index,row.label+" · "+row.id)
	_summary.text=("重叠地点" if not _candidates.is_empty() else "查找地点")+" · %d处"%page.total
	if page.total==0: _summary.text+=" · 无匹配"
	_previous.disabled=_offset==0; _next.disabled=not page.has_more

## 只有明确选择才离开查找；行元数据存原ID，不从名称反推或写入坐标。
func _choose(index: int) -> void:
	if index<0 or index>=_items.item_count: return
	var id: String=_items.get_item_metadata(index)
	destination_selected.emit(id)
