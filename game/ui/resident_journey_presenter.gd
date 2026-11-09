## 顺序收集同一已提交来源的有限页，再原子发布旧MapView/旅程侧栏；不拥有世界或提交命令。
extends Node
const MapProjection=preload("res://core/views/map_projection.gd")
signal frame_ready(source: Dictionary, scene_epoch: int, map_view: Dictionary, sidebar: Dictionary)
signal failed(error: Dictionary, source: Dictionary, scene_epoch: int)
var _bridge: Object
var _source: Dictionary={}
var _actor_id: String=""
var _scene_epoch: int=0
var _generation: int=0
var _active: bool=false
var _job: Dictionary={}
var _stage: Dictionary={}
var _published: Dictionary={}
var _page_total: int=-1
var _page_cursor: int=0
var _seed_pages: Dictionary={}
var _seed_mode: bool=false
var _seed_outcome: Dictionary={}

## 换世界前须消费旧ticket；新绑定清旧画面，不借旧revision为新世界生成显示。
func bind_context(bridge: Object, source: Dictionary, actor_id: String, scene_epoch: int=0) -> Dictionary:
	if is_busy(): return _error("ResidentPresenterBusy")
	if not is_instance_valid(bridge) or not bridge.has_method("enqueue_live_operation") or not bridge.has_method("poll_live_operation"): return _error("ResidentPresenterInvalidBridge")
	if not _valid_source(source) or actor_id.is_empty(): return _error("ResidentPresenterInvalidSource")
	_generation+=1; _bridge=bridge; _source=source.duplicate(true); _actor_id=actor_id; _scene_epoch=scene_epoch; _published={}
	return {"ok":true}

## 宿主先持有controller的同源refresh hold；本方法只查询，不暂停或恢复用户授权。
func refresh(source: Dictionary, scene_epoch: int) -> Dictionary:
	var guard: Dictionary=_refresh_guard(source)
	if not guard.is_empty(): return guard
	_begin_refresh(source,scene_epoch)
	return _issue("journey_location")

## 只接收backend已验证同源外层票据的封闭首页组；DTO本身不新增或猜测来源身份。
func refresh_from_seed(source: Dictionary, scene_epoch: int, seed: Dictionary) -> Dictionary:
	var guard: Dictionary=_refresh_guard(source)
	if not guard.is_empty(): return guard
	if seed.size()!=2 or not seed.get("source") is Dictionary or seed.source!=source or not seed.get("pages") is Dictionary: return _error("InvalidJourneySeed")
	var journey: Dictionary=seed.pages
	var encounter: Variant=journey.get("journey_encounter")
	if not encounter is Dictionary or not encounter.get("encounter") is Dictionary: return _error("InvalidJourneySeed")
	var expected: Array=["journey_location","journey_encounter","journey_map_position","journey_route","journey_crossings"]
	if encounter.encounter.is_empty(): expected.append_array(["journey_companions","journey_logistics","journey_travel"])
	if journey.size()!=expected.size(): return _error("InvalidJourneySeed")
	for key in expected:
		if not journey.get(key) is Dictionary: return _error("InvalidJourneySeed")
	_begin_refresh(source,scene_epoch)
	_seed_mode=true; _seed_pages=journey.duplicate(true)
	var outcome: Dictionary={"ok":true}; _seed_outcome=outcome
	_issue("journey_location")
	return outcome.duplicate(true)

## 两种入口共用世界/epoch与单调时钟守卫，不借预载页绕过旧在途ticket。
func _refresh_guard(source: Dictionary) -> Dictionary:
	if is_busy(): return _error("ResidentPresenterBusy")
	if not is_instance_valid(_bridge) or not _valid_source(source) or source.world_id!=_source.get("world_id") or source.execution_epoch!=_source.get("execution_epoch"): return _error("StaleWorld")
	if source.revision<_source.revision or source.tick<_source.tick: return _error("StalePreview")
	return {}

## 每批只保留新的有限暂存；失败前发布的旧完整frame仍保持独立。
func _begin_refresh(source: Dictionary, scene_epoch: int) -> void:
	_generation+=1; _source=source.duplicate(true); _scene_epoch=scene_epoch; _active=true
	_seed_pages={}; _seed_mode=false; _seed_outcome={}
	_stage={"map":{"ok":true,"revision":source.revision,"observer_id":_actor_id,"world_map_id":"","observer_position":[],"route_open":{},"crossing_closed":{},"road_progress":{},"travel_overlay":{}},"sidebar":{"ok":true,"actor_id":_actor_id,"revision":source.revision,"location":{},"dialogue_speaker":{},"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}}

## 场景/选择失效立即撤销本批发布资格，已接收的只读ticket仍需消费以释放唯一worker槽。
func invalidate() -> void:
	_generation+=1; _active=false; _stage={}
	_seed_pages={}; _seed_mode=false; _seed_outcome={}

## 旧完整画面保持其原source；读取者只得到深副本，不能看到半组新页或改写缓存。
func frame() -> Dictionary:
	return _published.duplicate(true)

## 单个查询在途或仍收集页时均视为忙；没有后台任意队列。
func is_busy() -> bool:
	return _active or not _job.is_empty()

## 每帧至多消费一个有限回执，再安排下一页；不会join、sleep或全态导出。
func poll() -> void:
	if _job.is_empty() or not is_instance_valid(_bridge): return
	var job: Dictionary=_job
	var reply: Dictionary=_bridge.poll_live_operation(job.reference.duplicate(true))
	if reply.get("ok",false) and not reply.get("ready",false): return
	_job={}
	if job.generation!=_generation or not _active: return
	if not reply.get("ok",false): _fail(reply); return
	if reply.get("tag")!="query" or reply.get("source_identity")!=job.source or not reply.get("result") is Dictionary: _fail(_error("InvalidJourneyFrame")); return
	var value: Dictionary=reply.result
	if value.has("ok") and not value.ok is bool: _fail(_error("InvalidJourneyFrame")); return
	if value.has("ok") and value.ok==false: _fail(value); return
	if reply.get("current_identity")!=_source: _fail(_error("StalePreview")); return
	var error: Dictionary=_consume(job.query_id,value)
	if not error.is_empty(): _fail(error)

## 签收接口只有query，小payload深复制；工作引用由真实Bridge创建。
func _issue(query_id: String, args: Dictionary={}) -> Dictionary:
	if _seed_mode and args.get("cursor",0)==0:
		if not _seed_pages.has(query_id):
			var missing: Dictionary=_error("InvalidJourneySeed"); _fail(missing); return missing
		var value: Dictionary=_seed_pages[query_id]; _seed_pages.erase(query_id)
		var error: Dictionary={}
		if value.has("ok") and not value.ok is bool: error=_error("InvalidJourneyFrame")
		elif value.get("ok")==false: error=value
		else: error=_consume(query_id,value)
		if not error.is_empty(): _fail(error); return error
		return {"ok":true}
	var accepted: Dictionary=_bridge.enqueue_live_operation({"kind":"query","source":_source.duplicate(true),"payload":{"query_id":query_id,"args":args.duplicate(true)}})
	if not accepted.get("ok",false): _fail(accepted); return accepted
	var reference: Variant=accepted.get("reference")
	if not reference is Dictionary or reference.get("world_id")!=_source.world_id or reference.get("execution_epoch")!=_source.execution_epoch or typeof(reference.get("operation_id"))!=TYPE_INT or reference.operation_id<1:
		var error: Dictionary=_error("ResidentPresenterInvalidReference"); _fail(error); return error
	_job={"query_id":query_id,"source":_source.duplicate(true),"reference":reference.duplicate(true),"generation":_generation}
	return accepted.duplicate(true)

## 分支顺序沿原侧栏：location→encounter；无会面才读companions→logistics→travel，再收地图。
func _consume(query_id: String, value: Dictionary) -> Dictionary:
	var side: Dictionary=_stage.sidebar
	var map: Dictionary=_stage.map
	match query_id:
		"journey_location":
			if not _text(value.get("kind")): return _error("InvalidJourneyFrame")
			var location: Dictionary={"kind":value.kind}
			for key in ["place_id","settlement_id"]:
				if value.has(key):
					if not _text(value[key]): return _error("InvalidJourneyFrame")
					location[key]=value[key]
			if value.kind=="Settlement" and (not location.has("place_id") or not location.has("settlement_id")): return _error("InvalidJourneyFrame")
			side.location=location; _issue("journey_encounter")
		"journey_encounter":
			if not value.get("travel") is Dictionary or not value.travel.get("active") is bool or not value.get("encounter") is Dictionary: return _error("InvalidJourneyFrame")
			side.travel={"active":value.travel.active}
			if not value.encounter.is_empty():
				for key in ["encounter_id","provider_actor_id","display_name"]:
					if not _text(value.encounter.get(key)): return _error("InvalidJourneyFrame")
				side.encounter=value.encounter.duplicate(true)
				if value.has("dialogue_speaker"):
					if not value.dialogue_speaker is Dictionary or not _text(value.dialogue_speaker.get("actor_id")) or not _text(value.dialogue_speaker.get("display_name")): return _error("InvalidJourneyFrame")
					side.dialogue_speaker=value.dialogue_speaker.duplicate(true)
				_issue("journey_map_position")
			else: _start_pages("journey_companions")
		"journey_companions":
			if not value.get("party_present") is bool: return _error("InvalidJourneyFrame")
			if _page_cursor==0: side.party_present=value.party_present
			elif side.party_present!=value.party_present: return _error("InvalidJourneyFrame")
			var error: Dictionary=_page(value,"rows")
			if not error.is_empty(): return error
			if not value.party_present and value.total_rows!=0: return _error("InvalidJourneyFrame")
			for row in value.rows:
				if not row is Dictionary or not _text(row.get("actor_id")) or not _text(row.get("display_name")) or typeof(row.get("cost_per_tick"))!=TYPE_INT: return _error("InvalidJourneyFrame")
				side.companions.append({"actor_id":row.actor_id,"display_name":row.display_name,"cost_per_tick":row.cost_per_tick})
			_next_page_or(value,"journey_companions","journey_logistics")
		"journey_logistics":
			if not value.get("logistics_quote") is Dictionary: return _error("InvalidJourneyFrame")
			var quote: Dictionary=value.logistics_quote
			if not quote.is_empty():
				if quote.get("ok")!=true: return _error("InvalidJourneyFrame")
				for key in ["available","required","mouth_count","full_ticks","shortage"]:
					if typeof(quote.get(key))!=TYPE_INT: return _error("InvalidJourneyFrame")
			side.logistics_quote=quote.duplicate(true); _issue("journey_travel")
		"journey_travel":
			if not value.get("active") is bool or value.active!=side.travel.active: return _error("InvalidJourneyFrame")
			if value.active:
				if not value.get("paused") is bool: return _error("InvalidJourneyFrame")
				if value.has("edge_id"):
					if not _text(value.edge_id) or typeof(value.get("progress"))!=TYPE_INT or value.progress<0: return _error("InvalidJourneyFrame")
					side.travel={"active":true,"edge_id":value.edge_id,"progress":value.progress,"paused":value.paused}
				else:
					if typeof(value.get("remaining_ticks"))!=TYPE_INT or not (_text(value.get("destination")) or _point(value.get("destination"))): return _error("InvalidJourneyFrame")
					side.travel={"active":true,"paused":value.paused,"remaining_ticks":value.remaining_ticks,"destination":value.destination.duplicate(true) if value.destination is Array else value.destination}
			_issue("journey_map_position")
		"journey_map_position":
			if not _text(value.get("world_map_id")): return _error("InvalidJourneyFrame")
			map.world_map_id=value.world_map_id
			var position: Vector2
			if value.has("road_position"):
				var road: Variant=value.road_position
				if not road is Dictionary or not _text(road.get("edge_id")) or typeof(road.get("progress"))!=TYPE_INT or typeof(road.get("length"))!=TYPE_INT or road.length<1 or road.progress<0 or road.progress>road.length or not road.get("raw") is bool or not road.get("points") is Array or road.points.size()<2 or road.points.size()>66: return _error("InvalidJourneyFrame")
				var line:=PackedVector2Array()
				for point in road.points:
					if not _point(point): return _error("InvalidJourneyFrame")
					line.append(Vector2(point[0],point[1]))
				var ratio: float=float(road.progress)/road.length
				position=MapProjection.along(line,ratio)
				if road.raw: map.road_progress={"edge_id":road.edge_id,"ratio":ratio}
			else:
				if not _point(value.get("observer_position")): return _error("InvalidJourneyFrame")
				position=Vector2(value.observer_position[0],value.observer_position[1])
			map.observer_position=[position.x,position.y]; _start_pages("journey_route")
		"journey_route":
			if not value.get("active") is bool: return _error("InvalidJourneyFrame")
			var road: bool=value.get("kind","")=="road"
			if value.has("kind") and not road: return _error("InvalidJourneyFrame")
			if road and (not value.active or not value.get("returning") is bool): return _error("InvalidJourneyFrame")
			if _page_cursor==0:
				map.travel_overlay=({"kind":"road","path":[],"returning":value.returning} if road else {"kind":"terrain","points":[]}) if value.active else {}
			elif value.active!=(not map.travel_overlay.is_empty()) or (value.active and road!=(map.travel_overlay.kind=="road")): return _error("InvalidJourneyFrame")
			if road and map.travel_overlay.returning!=value.returning: return _error("InvalidJourneyFrame")
			var error: Dictionary=_page(value,"path" if road else "points")
			if not error.is_empty(): return error
			if not value.active and value.total_rows!=0: return _error("InvalidJourneyFrame")
			if road:
				for id in value.path:
					if not _text(id): return _error("InvalidJourneyFrame")
					map.travel_overlay.path.append(id)
			else:
				for point in value.points:
					if not _point(point): return _error("InvalidJourneyFrame")
					map.travel_overlay.points.append(point.duplicate())
			if value.has("next_cursor"): _next_page_or(value,"journey_route","")
			else: _start_pages("journey_crossings")
		"journey_crossings":
			var road: bool=value.get("kind","")=="road"
			if value.has("kind") and not road: return _error("InvalidJourneyFrame")
			if road!=map.world_map_id.is_empty(): return _error("InvalidJourneyFrame")
			var error: Dictionary=_page(value,"rows")
			if not error.is_empty(): return error
			for row in value.rows:
				if road:
					if not row is Dictionary or not _text(row.get("route_id")) or not row.get("open") is bool or map.route_open.has(row.route_id): return _error("InvalidJourneyFrame")
					map.route_open[row.route_id]=row.open
				else:
					if not row is Dictionary or not _text(row.get("flag_id")) or not row.get("closed") is bool or map.crossing_closed.has(row.flag_id): return _error("InvalidJourneyFrame")
					map.crossing_closed[row.flag_id]=row.closed
			if value.has("next_cursor"): _next_page_or(value,"journey_crossings","")
			else: _publish()
		_: return _error("UnsupportedLiveQuery")
	return {}

## 首页才设集合总长；后续页必须连续、同总长且每页恰好取剩余的至多64行。
func _page(value: Dictionary, key: String) -> Dictionary:
	if typeof(value.get("cursor"))!=TYPE_INT or typeof(value.get("total_rows"))!=TYPE_INT or not value.get(key) is Array: return _error("InvalidJourneyPage")
	if value.cursor!=_page_cursor or value.total_rows<0 or value.cursor>value.total_rows: return _error("InvalidJourneyPage")
	if _page_total<0: _page_total=value.total_rows
	if value.total_rows!=_page_total or value[key].size()!=mini(64,value.total_rows-value.cursor): return _error("InvalidJourneyPage")
	var end: int=value.cursor+value[key].size()
	if end<value.total_rows:
		if typeof(value.get("next_cursor"))!=TYPE_INT or value.next_cursor!=end: return _error("InvalidJourneyPage")
	elif value.has("next_cursor"): return _error("InvalidJourneyPage")
	return {}

## 每组从0开始，不用前一组的游标拼接不同集合。
func _start_pages(query_id: String) -> void:
	_page_total=-1; _page_cursor=0; _issue(query_id,{"cursor":0})

## 只安排同源下一页；没有next_cursor才进入原消费者的下一个必要来源。
func _next_page_or(value: Dictionary, query_id: String, next_query: String) -> void:
	if value.has("next_cursor"):
		_page_cursor=value.next_cursor; _issue(query_id,{"cursor":_page_cursor})
	else: _issue(next_query)

## 所有页完成后一次替换旧完整frame，先发布内部状态再发同步信号，允许宿主重入换绑。
func _publish() -> void:
	if _seed_mode and not _seed_pages.is_empty(): _fail(_error("InvalidJourneySeed")); return
	_published={"source":_source.duplicate(true),"scene_epoch":_scene_epoch,"map_view":_stage.map.duplicate(true),"sidebar":_stage.sidebar.duplicate(true)}
	_stage={}; _active=false
	_seed_pages={}; _seed_mode=false; _seed_outcome={}
	frame_ready.emit(_published.source.duplicate(true),_scene_epoch,_published.map_view.duplicate(true),_published.sidebar.duplicate(true))

## 失败保留上一份完整frame的旧source；不发布部分crossings、路线或同行，也不自动重试写命令。
func _fail(error: Dictionary) -> void:
	if not _seed_outcome.is_empty(): _seed_outcome.clear(); _seed_outcome.merge(error.duplicate(true))
	_active=false; _stage={}
	_seed_pages={}; _seed_mode=false; _seed_outcome={}
	failed.emit(error.duplicate(true),_source.duplicate(true),_scene_epoch)

## 使用原DTO文本上限，合法空显示名允许，不以截断改变作者内容。
func _text(value: Variant) -> bool:
	return value is String and value.to_utf8_buffer().size()<=4096

## 保存路线只接受真整数点；屏幕位置的Vector2转换仅在指定展示分支执行。
func _point(value: Variant) -> bool:
	return value is Array and value.size()==2 and typeof(value[0])==TYPE_INT and typeof(value[1])==TYPE_INT

## 来源来自同core的有限身份，不接受浮点时钟或隐式缺省世界。
func _valid_source(value: Dictionary) -> bool:
	return value.get("world_id") is String and not value.world_id.is_empty() and typeof(value.get("execution_epoch"))==TYPE_INT and value.execution_epoch>0 and typeof(value.get("revision"))==TYPE_INT and value.revision>=0 and typeof(value.get("tick"))==TYPE_INT and value.tick>=0

## 本地组帧错误不伪造领域规则结果。
func _error(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## 活跃场景每帧只读有限已接受回执，不忙等worker。
func _process(_delta: float) -> void:
	poll()
