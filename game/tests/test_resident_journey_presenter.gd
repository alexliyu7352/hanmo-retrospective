## 同源有限页组帧与既有MapView/sidebar/Canvas合同；transport夹具不冒充真实core或Main验收。
extends SceneTree
const Presenter=preload("res://ui/resident_journey_presenter.gd")
const OriginalMap=preload("res://core/views/map_view.gd")
const OriginalSidebar=preload("res://core/views/scene_journey_sidebar.gd")
const Canvas=preload("res://ui/world_map_canvas.gd")
const Adapter=preload("res://ui/resident_journey_adapter.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 只返回明示的有限读页，完全不提供submit/advance/snapshot/export入口。
class Pages extends RefCounted:
	var answers: Dictionary={}
	var requests: Array=[]
	var pending: Dictionary={}
	var sequence: int=0
	var ready: bool=true
	var bad_next: bool=false
	var stale_on: String=""
	## 真实合同只允许一条普通query，控件或组帧器误发写请求即明确失败。
	func enqueue_live_operation(value: Dictionary) -> Dictionary:
		if value.kind!="query" or not pending.is_empty(): return {"ok":false,"code":"FixtureUnexpectedOperation"}
		sequence+=1; pending=value.duplicate(true); requests.append(value.duplicate(true))
		return {"ok":true,"reference":{"world_id":value.source.world_id,"execution_epoch":value.source.execution_epoch,"operation_id":sequence}}
	## 每次只消费一个原始ticket；可控制未完成或晚页版本失败来证明原子发布。
	func poll_live_operation(_reference: Dictionary) -> Dictionary:
		if not ready: return {"ok":true,"ready":false}
		var request: Dictionary=pending; pending={}
		var id: String=request.payload.query_id; var value: Dictionary
		var cursor: int=request.payload.args.get("cursor",0)
		if id in ["journey_route","journey_crossings","journey_companions"]:
			var key: String="points" if id=="journey_route" else "rows"
			var source: Dictionary=answers[id]; var rows: Array=source[key]
			value=source.duplicate(true); value[key]=rows.slice(cursor,mini(cursor+64,rows.size())).duplicate(true)
			value.cursor=cursor; value.total_rows=rows.size()
			if cursor+64<rows.size(): value.next_cursor=0 if bad_next else cursor+64
		else: value=answers[id].duplicate(true)
		var current: Dictionary=request.source.duplicate(true)
		if id==stale_on: current.revision+=1; value={"ok":false,"code":"StalePreview"}
		return {"ok":true,"ready":true,"tag":"query","source_identity":request.source.duplicate(true),"current_identity":current,"result":value}

## 等树完成初始化再挂真实控件，不开启场景或世界。
func _init() -> void: call_deferred("_run")

## 原视图对照失败与协议失败分别有具体标签。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 专用显示来源只含原视图读取字段，不声称通过整世界Schema。
func inputs(encounter: bool=false) -> Dictionary:
	var cells: Array=[]; var flags: Dictionary={}; var crossings: Array=[]
	for i in range(400): cells.append({"terrain":"plain"})
	for i in range(70):
		var id: String="gate."+str(i); cells[i]={"terrain":"plain","crossing":"bridge","blocked_flag":id}; flags[id]=(i%2)==0; crossings.append({"flag_id":id,"closed":flags[id]})
	cells[100]=cells[0].duplicate(true)
	var points: Array=[]
	for i in range(129): points.append([i,2])
	var data: Dictionary={"actors":{"player":{"id":"player","name_key":"player"}},"texts":{"player":"Player"},"settlements":{"town":{"id":"town","kind":"city","map_xy":[23,31]}},"routes":{},"scenarios":{"scenario":{"world_map_id":"map"}},"world_maps":{"map":{"id":"map","width":200,"height":200,"cell_size":10,"cells":cells}}}
	var state: Dictionary={"player_actor_id":"player","scenario_id":"scenario","revision":7,"tick":3,"actors":{"player":{"id":"player","location":{"kind":"Wilderness","map_id":"map","point":[0,2],"origin":"town"},"action_id":"travel"}},"actions":{"travel":{"kind":"travel","remaining_ticks":7,"params":{"terrain":true,"paused":true,"destination":"town","points":points}}},"parties":{"party":{"leader_id":"player","members":["player"],"terms":{}}},"armies":{},"encounters":{},"interactions":{},"flags":flags}
	var companions: Array=[]
	for i in range(67):
		var id: String="member:"+str(i); var label: String="" if i==0 else "Name "+str(i)
		state.actors[id]={"id":id}; data.actors[id]={"id":id,"name_key":id}; data.texts[id]=label
		state.parties.party.members.append(id); state.parties.party.terms[id]={"cost_per_tick":i+1}
		companions.append({"actor_id":id,"display_name":label,"cost_per_tick":i+1})
	var peer: Dictionary={}
	if encounter:
		state.encounters["encounter:1"]={"id":"encounter:1","status":"pending","participants":["player","member:2"],"leaders":["player","member:2"]}
		peer={"encounter_id":"encounter:1","provider_actor_id":"member:2","display_name":"Name 2"}
	var answers: Dictionary={"journey_location":{"kind":"Wilderness"},"journey_encounter":{"travel":{"active":true},"encounter":peer},"journey_companions":{"party_present":true,"rows":companions},"journey_logistics":{"ok":true,"logistics_quote":{}},"journey_travel":{"active":true,"paused":true,"remaining_ticks":7,"destination":"town"},"journey_map_position":{"world_map_id":"map","observer_position":[0,2]},"journey_route":{"active":true,"points":[[0,2]]+points.duplicate(true)},"journey_crossings":{"rows":crossings}}
	return {"state":state,"data":data,"answers":answers}

## 同一显示来源在整个收页期间保持完整world/epoch/revision/tick身份。
func identity() -> Dictionary:
	return {"world_id":"0123456789abcdef0123456789abcdef","execution_epoch":2,"revision":7,"tick":3}

## 构建实际Presenter，只有其边界transport是可控协议来源。
func fixture(encounter: bool=false) -> Dictionary:
	var source: Dictionary=inputs(encounter); var bridge=Pages.new(); bridge.answers=source.answers
	var presenter=Presenter.new(); root.add_child(presenter); presenter.set_process(false)
	var published: Array=[]; var errors: Array=[]
	presenter.frame_ready.connect(func(id,scene,map,side): published.append({"source":id,"scene_epoch":scene,"map_view":map,"sidebar":side}))
	presenter.failed.connect(func(error,_source,_scene): errors.append(error))
	check(presenter.bind_context(bridge,identity(),"player",11).ok,"绑定同源显示来源")
	return {"source":source,"bridge":bridge,"presenter":presenter,"published":published,"errors":errors}

## 有限测试轮询上限仅防损坏分页卡住；产品每帧一次，不在此执行世界。
func finish(f: Dictionary) -> void:
	for _i in range(40):
		if not f.presenter.is_busy(): return
		f.presenter.poll()
	check(false,"有限分页未在预期数量内结束")

## 与现有两个生产view逐值逐类型对照，期间不得发布任何部分flags/路线/同行。
func complete_frame() -> Dictionary:
	var f: Dictionary=fixture(); var p=f.presenter; var b=f.bridge
	check(p.refresh(identity(),11).ok,"开始有限收页")
	while p.is_busy():
		check(f.published.is_empty(),"最后一页前不发布部分画面")
		p.poll()
	var frame: Dictionary=p.frame(); var before: Dictionary=f.source.duplicate(true)
	check(frame.map_view==OriginalMap.query(f.source.state,f.source.data),"完整MapView值与原生产view一致")
	check(frame.sidebar==OriginalSidebar.journey(f.source.state,f.source.data),"完整sidebar与原生产view一致")
	check(typeof(frame.map_view.observer_position[0])==TYPE_FLOAT and typeof(frame.map_view.travel_overlay.points[0][0])==TYPE_INT,"显示Vector2浮点与权威路径整数区别保留")
	check(frame.map_view.crossing_closed.size()==70 and frame.map_view.travel_overlay.points.size()==130 and frame.sidebar.companions.size()==67,"全部页无截断且没有重复边界")
	check(b.requests[0].payload.query_id=="journey_location" and b.requests[1].payload.query_id=="journey_encounter" and b.requests[2].payload.query_id=="journey_companions" and b.requests[4].payload.query_id=="journey_logistics" and b.requests[5].payload.query_id=="journey_travel","保留原sidebar读取顺序")
	frame.map_view.crossing_closed.clear(); frame.sidebar.companions.clear()
	check(p.frame().map_view.crossing_closed.size()==70 and p.frame().sidebar.companions.size()==67 and f.source==before,"发布结果深拥有，不改来源")
	check(f.errors.is_empty() and f.published.size()==1,"成功只发一个完整frame")
	return f

## C会面存在时保留原早退，不触碰同行/军粮/行旅详细字段。
func encounter_frame() -> void:
	var f: Dictionary=fixture(true); f.presenter.refresh(identity(),11); finish(f)
	check(f.presenter.frame().sidebar==OriginalSidebar.journey(f.source.state,f.source.data),"会面sidebar原早退形状一致")
	var unwanted: bool=false
	for request in f.bridge.requests:
		if request.payload.query_id in ["journey_companions","journey_logistics","journey_travel"]: unwanted=true
	check(not unwanted and not f.presenter.frame().sidebar.party_present and f.presenter.frame().sidebar.companions.is_empty(),"会面不读取或泄露被早退的其它详情")
	f.presenter.free()

## 晚页失败或scene失效只撤销本批，不混入上一完整画面或继续发后续页。
func failed_and_stale(f: Dictionary) -> void:
	var previous: Dictionary=f.presenter.frame(); f.bridge.stale_on="journey_crossings"
	f.presenter.refresh(identity(),12); finish(f)
	check(f.presenter.frame()==previous and f.published.size()==1 and f.errors.back().code=="StalePreview","晚页源变化保留旧完整frame")
	f.bridge.stale_on=""; f.bridge.bad_next=true; f.presenter.refresh(identity(),13); finish(f)
	check(f.presenter.frame()==previous and f.errors.back().code=="InvalidJourneyPage","非前进cursor被拒，不能无限循环或发布截断")
	f.bridge.bad_next=false; f.bridge.ready=false; f.presenter.refresh(identity(),14)
	var count: int=f.bridge.requests.size(); f.presenter.invalidate(); f.bridge.ready=true; f.presenter.poll()
	check(not f.presenter.is_busy() and f.presenter.frame()==previous and f.bridge.requests.size()==count,"旧场景ticket只消费，不再追页或重绘")

## 实际现有Canvas消费新字典；只核控件位置与选择合同，不宣称完整Main/美术或像素验收。
func actual_canvas(f: Dictionary) -> void:
	var frame: Dictionary=f.presenter.frame(); var canvas=Canvas.new()
	canvas.data=f.source.data.duplicate(true); canvas.view=frame.map_view; canvas.terrain_points=frame.map_view.travel_overlay.points.duplicate(true)
	canvas.size=Vector2(800,600); root.add_child(canvas); canvas._layout()
	await process_frame; await process_frame
	check(canvas._markers.has(f.source.data.settlements.keys()[0]) and canvas._world_map.id==frame.map_view.world_map_id,"原Canvas接受完整地图DTO并建立实际据点按钮")
	var choices: Array=[]; canvas.point_selected.connect(func(point): choices.append(point))
	var point: Vector2=canvas.screen_point(Vector2(12,34))
	var down=InputEventMouseButton.new(); down.button_index=MOUSE_BUTTON_LEFT; down.pressed=true; down.position=point; canvas._gui_input(down)
	var up=InputEventMouseButton.new(); up.button_index=MOUSE_BUTTON_LEFT; up.pressed=false; up.position=point; canvas._gui_input(up)
	check(choices==[[12,34]],"原Canvas真实鼠标入口仍输出整数候选点")
	check(canvas.view==OriginalMap.query(f.source.state,f.source.data),"控件交互不改有限地图值")
	canvas.free()

## 实际扩展模式只读两个冻结真实来源（启程后/第一半日后），不执行第二套规则或推进半日。
func real_frames(oracle_root: String) -> void:
	check(ClassDB.class_exists("HanmoResidentBridge"),"真实Bridge已注册")
	if failures: return
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(oracle_root.path_join("oracle-index.json")))
	var catalog_bytes: PackedByteArray=FileAccess.get_file_as_bytes(oracle_root.path_join("expanded-catalog.json"))
	var catalog: Dictionary=Strict.parse_text(catalog_bytes.get_string_from_utf8())
	check(catalog.ok,"原显示视图冷读取真实作者目录")
	if failures: return
	for descriptor in [index.commands[0].after,index.commands[1].after]:
		var state_bytes: PackedByteArray=FileAccess.get_file_as_bytes(oracle_root.path_join(descriptor.file))
		var source_state: Dictionary=Strict.parse_text(state_bytes.get_string_from_utf8())
		check(source_state.ok,"原显示视图冷读取冻结真实状态")
		if failures: return
		var bridge: Object=ClassDB.instantiate("HanmoResidentBridge")
		var installed: Dictionary=bridge.install(catalog_bytes,state_bytes)
		check(installed.get("ok",false),"真实core安装完整来源")
		if failures: return
		var state: Dictionary=source_state.value; var actor: String=state.player_actor_id
		var p=Presenter.new(); root.add_child(p); p.set_process(false)
		var a=Adapter.new(); root.add_child(a); a.set_process(false)
		var errors: Array=[]; p.failed.connect(func(error,_source,_scene): errors.append(error)); a.failed.connect(func(error): errors.append(error))
		check(p.bind_context(bridge,installed.identity,actor,1).ok and a.bind_context(bridge,installed.identity,1).ok,"同一core上的controller/presenter绑定")
		# 第一来源没有待处理输入，仅建立停驻run来证明refresh hold阻止真实Timer；不授半日。
		var run: bool=descriptor==index.commands[0].after
		if run:
			check(a.start(actor,state.actors[actor].action_id,1).ok,"真实run停驻用于显示刷新")
			check(await wait_real(a),"真实begin完成")
		var hold: Dictionary=a.hold_for_refresh(installed.identity)
		check(hold.ok and p.refresh(installed.identity,1).ok,"真实查询期间持同源Timer hold")
		check(await wait_real(p),"真实全部有限页完成")
		var frame: Dictionary=p.frame()
		check(not frame.is_empty() and errors.is_empty(),"真实有限query被组帧器正确消费")
		if frame.is_empty(): p.free(); a.free(); return
		check(frame.map_view==OriginalMap.query(state,catalog.value.data),"真实map DTO与原生产view逐值一致")
		check(frame.sidebar==OriginalSidebar.journey(state,catalog.value.data),"真实sidebar DTO与原生产view逐值一致")
		check(a.release_refresh(hold.token).ok,"匹配完整frame可释放刷新hold")
		if run:
			check(a.auto_running and a._timer.wait_time==1.2,"真实刷新后从1.2秒重新计时")
			check(a.suspend("reading").ok and await wait_real(a),"阅读立即撤权并完成真实取消")
		var exported: Dictionary=bridge.debug_export_state()
		check(exported.get("ok",false) and exported.json.sha256_text()==descriptor.sha256,"组帧与阅读取消不改变完整真实世界")
		await actual_canvas({"presenter":p,"source":{"state":state,"data":catalog.value.data}})
		p.free(); a.free(); bridge=null

## 只轮询已接收的job；不拿全导出当等待条件，超时仅用于测试明确失败。
func wait_real(node: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+15000000
	while Time.get_ticks_usec()<deadline:
		node.poll()
		if not node.is_busy(): return true
		await process_frame
	return false

## 封闭首页组沿原query ID保留DTO；同源wrapper只由已校验外层回执的backend提供。
func seed_for(f: Dictionary, single_page: bool=false) -> Dictionary:
	var pages: Dictionary=f.source.answers.duplicate(true)
	if not pages.journey_encounter.encounter.is_empty():
		for key in ["journey_companions","journey_logistics","journey_travel"]: pages.erase(key)
	for key in ["journey_companions","journey_route","journey_crossings"]:
		if not pages.has(key): continue
		var field: String="points" if key=="journey_route" else "rows"
		var rows: Array=pages[key][field]
		if single_page: rows=rows.slice(0,mini(64,rows.size()))
		pages[key][field]=rows.slice(0,mini(64,rows.size()))
		pages[key].cursor=0; pages[key].total_rows=rows.size()
		if rows.size()>64: pages[key].next_cursor=64
	return {"source":identity(),"pages":pages}

## 原页验证和原排序只走一次；预载首页仅消除等待，续页仍经过真实transport合同。
func seeded_pages() -> void:
	var f: Dictionary=fixture(); var seed: Dictionary=seed_for(f)
	check(f.presenter.refresh_from_seed(identity(),21,seed).ok and f.published.is_empty(),"首页组只暂存，续页未齐不发布")
	seed.pages.journey_map_position.observer_position[0]=999
	finish(f)
	check(f.presenter.frame().map_view==OriginalMap.query(f.source.state,f.source.data) and f.presenter.frame().sidebar==OriginalSidebar.journey(f.source.state,f.source.data),"预载及续页组合与原生产投影完全相等且深拥有")
	var observed: Array=[]
	for request in f.bridge.requests: observed.append([request.payload.query_id,request.payload.args.cursor])
	check(observed==[["journey_companions",64],["journey_route",64],["journey_route",128],["journey_crossings",64]],"只请求缺失续页，保持原分支顺序")
	check(f.published.size()==1 and f.errors.is_empty() and not f.presenter.is_busy(),"预载完整组只发布一次且无遗留工作")
	var previous: Dictionary=f.presenter.frame(); f.bridge.stale_on="journey_crossings"
	f.presenter.refresh_from_seed(identity(),22,seed_for(f)); finish(f)
	check(f.presenter.frame()==previous and f.errors.back().code=="StalePreview","预载后晚页换源仍保留上一完整frame")
	f.bridge.stale_on=""; f.bridge.ready=false
	f.presenter.refresh_from_seed(identity(),23,seed_for(f))
	check(f.presenter.refresh_from_seed(identity(),24,seed_for(f)).code=="ResidentPresenterBusy","已有续页ticket时不能用新seed绕过忙状态")
	var count: int=f.bridge.requests.size(); f.presenter.invalidate(); f.bridge.ready=true; f.presenter.poll()
	check(f.bridge.requests.size()==count and not f.presenter.is_busy() and f.presenter.frame()==previous,"撤销预载批只排空旧续页，不接着消费其它首页")
	f.presenter.free()

## 单页种子无需发任何query；坏包/隐藏分支/末字段失败不能发布半帧或掩盖原错误。
func seeded_validation() -> void:
	var f: Dictionary=fixture(); var seed: Dictionary=seed_for(f,true)
	check(f.presenter.refresh_from_seed(identity(),31,seed).ok and f.published.size()==1 and f.bridge.requests.is_empty(),"所有首页已完整时同步原子发布，零额外query")
	var previous: Dictionary=f.presenter.frame()
	for mutation in ["source","missing","extra","wrapper"]:
		var bad: Dictionary=seed.duplicate(true)
		match mutation:
			"source": bad.source.tick+=1
			"missing": bad.pages.erase("journey_route")
			"extra": bad.pages.unrequested={}
			"wrapper": bad.unrequested=true
		check(f.presenter.refresh_from_seed(identity(),32,bad).code=="InvalidJourneySeed" and f.presenter.frame()==previous,"拒绝不完整或混源封闭首页组："+mutation)
	var bad: Dictionary=seed.duplicate(true); bad.pages.journey_companions.cursor=64
	check(f.presenter.refresh_from_seed(identity(),33,bad).code=="InvalidJourneyPage" and f.presenter.frame()==previous,"首页cursor仍由原分页验证拒绝")
	bad=seed.duplicate(true); bad.pages.journey_crossings.rows[-1].closed="false"
	check(f.presenter.refresh_from_seed(identity(),34,bad).code=="InvalidJourneyFrame" and f.presenter.frame()==previous,"最后一个预载字段无效也不发布前面成功部分")
	bad=seed.duplicate(true); bad.pages.journey_location={"ok":false,"code":"OriginalRuleFailure","path":"location"}
	var result: Dictionary=f.presenter.refresh_from_seed(identity(),35,bad)
	check(result==bad.pages.journey_location and f.errors.back()==result and f.presenter.frame()==previous,"原首页规则失败完整透传")
	check(f.bridge.requests.is_empty() and f.published.size()==1 and not f.presenter.is_busy(),"所有坏seed无世界请求/半帧/假忙")
	f.presenter.free()
	f=fixture(true); seed=seed_for(f,true)
	check(f.presenter.refresh_from_seed(identity(),36,seed).ok and f.bridge.requests.is_empty() and not f.presenter.frame().sidebar.party_present,"会面预载保留早退，不请求隐藏同行/军粮/行程")
	seed.pages.journey_companions={}
	check(f.presenter.refresh_from_seed(identity(),37,seed).code=="InvalidJourneySeed","会面包夹带已隐藏分支被明确拒绝")
	f.presenter.free()

## 同步发布/失败可触发宿主立即换源；旧调用不能清掉新批或返回新批的结果。
func seeded_reentrant() -> void:
	var f: Dictionary=fixture(); var seed: Dictionary=seed_for(f,true); var later: Dictionary=identity(); later.revision+=1
	var replacement: Dictionary=seed.duplicate(true); replacement.source=later
	f.presenter.frame_ready.connect(func(source,_scene,_map,_side):
		if source==identity(): f.presenter.refresh_from_seed(later,42,replacement))
	check(f.presenter.refresh_from_seed(identity(),41,seed).ok and f.presenter.frame().source==later and f.published.size()==2,"同步成功回调的新来源frame不被旧调用覆盖")
	f.presenter.free()
	f=fixture(); var repaired: Dictionary=seed_for(f,true)
	f.presenter.failed.connect(func(_error,_source,_scene): f.presenter.refresh_from_seed(identity(),44,repaired))
	var bad: Dictionary=repaired.duplicate(true); bad.pages.journey_map_position.observer_position=[1.0,2]
	check(f.presenter.refresh_from_seed(identity(),43,bad).code=="InvalidJourneyFrame" and f.published.size()==1 and f.presenter.frame().scene_epoch==44,"同步失败回调可修复刷新，旧返回仍保留原失败")
	f.presenter.free()

## 只运行受影响组帧与现有控件，不启动Main或读取整世界来实现有限查询。
func _run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()==2 and args[0]=="--real-bridge":
		await real_frames(args[1])
		print("RESIDENT_JOURNEY_PRESENTER_REAL_CHECKS=",checks," FAILURES=",failures," HALF_DAYS=0 MAIN_ACCEPTANCE=false")
		quit(0 if failures==0 else 1); return
	var f: Dictionary=complete_frame(); encounter_frame(); failed_and_stale(f); await actual_canvas(f); f.presenter.free()
	seeded_pages(); seeded_validation(); seeded_reentrant()
	print("RESIDENT_JOURNEY_PRESENTER_CHECKS=",checks," FAILURES=",failures," LIVE_CORE=false MAIN_ACCEPTANCE=false")
	quit(0 if failures==0 else 1)
