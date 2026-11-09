## 地方近况保原业务操作，并以同源有限观察页展示已见人物、货源和本人责任。
extends RefCounted
const Existing=preload("res://ui/dynamic_world_panel.gd")
const META="shared_world_observation_draft"

## 旧Main入口仍使用原近况草稿；新分页仅保存同代只读游标，不建立第二世界或知识表。
static func prepare_draft(host) -> Dictionary:
	return Existing.prepare_draft(host)

## 世界、人物、revision或当前位置变化时丢弃旧游标，不能把上一页继续到另一世界。
static func _draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var place: String=host.kernel.presentation_location().get("place_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("place_id")!=place:
		draft={"source":source,"place_id":place,"cursor":{}}
	host.set_meta(META,draft.duplicate(true))
	return draft

## 新页沿原异步读取队列等待；旧profile不请求新能力，失败不被当成空消息成功。
static func prepare(host) -> bool:
	if not Existing.prepare(host): return false
	if not host.kernel.world_observation_enabled(): return true
	var draft: Dictionary=_draft(host)
	var view: Dictionary=host.kernel.world_observation(draft["place_id"],draft["cursor"])
	if view.get("code")=="NativeAsyncRequired": return false
	if view.get("ok",true)==false: host._resident_page_error=view; return false
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]:
		host._resident_page_error={"ok":false,"code":"StaleWorldObservation"}; return false
	return true

## 静态姓名仅解析DTO已给出的合法身份；没有目录名字时用普通称谓，不显示内部ID。
static func _name(host, table: String, id: String, fallback: String) -> String:
	var record: Dictionary=host._data.get(table,{}).get(id,{})
	return host._name(record) if not record.is_empty() else fallback

## 每行保观察时刻语义；旧供给数字不是当前可花实物，单步结算也不冒整项事业完成。
static func _describe(host, row: Dictionary) -> String:
	match row.get("kind",""):
		"actor":
			return "%s · %s"%[_name(host,"actors",row.get("actor_id",""),"一位在此的人"),"正在行动" if row.get("action_occupied",false) else "暂时无法接待" if not row.get("available",false) else "现在此处"]
		"supply":
			return "%s · %s：半日%d观察时有%d份；半日%d获知%s"%[_name(host,"organizations",row.get("organization_id",""),"供货方"),_name(host,"items",row.get("item_id",""),"物资"),row.get("observed_tick",0),row.get("quantity",0),row.get("learned_tick",0),"（消息已过时）" if row.get("stale",false) else ""]
		"arrear":
			return "%s · 半日%d获知尚欠你%d钱，到期半日%d"%[_name(host,"organizations",row.get("organization_id",""),"付款方"),row.get("learned_tick",0),row.get("outstanding",0),row.get("due_tick",0)]
		"commitment":
			return "已接事务 · %s，期限半日%d"%["正在清算" if row.get("status")=="settling" else "仍需履行",row.get("due_tick",0)]
	return ""

## 旧控件、已关闭页面和过期读取源不能改变当前页的只读游标。
static func _select(host, button: Button, cursor: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source: return
	draft["cursor"]=cursor.duplicate(true); host.set_meta(META,draft.duplicate(true)); host._render()

## 在原真实近况滚动体内追加只读段，沿原返回按钮退出，不新增修改世界的捷径。
static func build(host, stage: Control) -> void:
	Existing.build(host,stage)
	if not host.kernel.world_observation_enabled(): return
	var draft: Dictionary=_draft(host)
	var view: Dictionary=host.kernel.world_observation(draft["place_id"],draft["cursor"])
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]: return
	var panel: Control=stage.find_child("DynamicWorldPanel",true,false)
	if panel==null: return
	var scroll: ScrollContainer=panel.get_node("DynamicWorldScroll")
	var body: Control=scroll.get_child(0)
	var section=VBoxContainer.new(); section.name="WorldObservationSection"; section.add_theme_constant_override("separation",12); body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"所见事务",22)
	for row in view.get("rows",[]):
		var text: String=_describe(host,row)
		if text.is_empty(): continue
		var label: Label=host._text(section,text,18); label.name="WorldObservationRow"
	if view.get("rows",[]).is_empty(): host._text(section,"此刻没有更多已知事务。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft["cursor"].is_empty():
		var first: Button=host._button(navigation,"从头查看","world_observation_first","",func(): pass,host._advancing)
		first.pressed.connect(_select.bind(host,first,{},draft["source"],host._scene_counter))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多所见","world_observation_next","",func(): pass,host._advancing)
		next.pressed.connect(_select.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],host._scene_counter))
