## 有限backend/presenter的协议门；transport为显式夹具，不冒充真实core、Main或存档验收。
extends SceneTree
const Backend=preload("res://ui/native_resident_backend.gd")
var checks: int=0
var failures: int=0

## 夹具只提供有界回执与单槽；任何全态读取、GD提交或并发普通请求都没有可调用入口。
class Wire extends RefCounted:
	var source: Dictionary={"world_id":"0123456789abcdef0123456789abcdef","execution_epoch":2,"revision":7,"tick":3}
	var calls: Array=[]
	var pending: Dictionary={}
	var sequence: int=0
	var ready: bool=true
	var installs: int=0
	var installed_catalog: String=""
	var installed_seed: String=""
	var fail_query: String=""
	var stale_query: String=""
	var command_result: Dictionary={"ok":true,"kind":"fixture-only"}
	var command_current: Dictionary={}
	var companions: Array=[]
	var options: Array=[]
	var seed_fault: String=""
	## 只记录冷边界实际字节，返回显式测试来源，不宣称任何seed通过规则校验。
	func install(catalog: PackedByteArray, seed: PackedByteArray) -> Dictionary:
		installs+=1; installed_catalog=catalog.get_string_from_utf8(); installed_seed=seed.get_string_from_utf8()
		return {"ok":true,"identity":source.duplicate(true),"initialization_count":1}
	## 同一槽仍有未消费回执时拒绝第二请求，记录原packet以检验没有抢先改写序号/params。
	func enqueue_live_operation(packet: Dictionary) -> Dictionary:
		if not pending.is_empty(): return {"ok":false,"code":"FixtureBusy"}
		sequence+=1; pending=packet.duplicate(true); calls.append(packet.duplicate(true))
		return {"ok":true,"reference":{"world_id":packet.source.world_id,"execution_epoch":packet.source.execution_epoch,"operation_id":sequence}}
	## 返回当前明确的有限页面；命令结果可由测试指定，但从不运行或假称玩法。
	func poll_live_operation(_reference: Dictionary) -> Dictionary:
		if not ready: return {"ok":true,"ready":false}
		var packet: Dictionary=pending; pending={}; var value: Dictionary={}; var current: Dictionary=packet.source.duplicate(true)
		if packet.kind=="query":
			var id: String=packet.payload.query_id
			value=answer(id)
			if id=="journey_companions" and not companions.is_empty():
				var cursor: int=packet.payload.args.cursor
				value={"party_present":true,"cursor":cursor,"total_rows":companions.size(),"rows":companions.slice(cursor,mini(cursor+64,companions.size())).duplicate(true)}
				if cursor+64<companions.size(): value.next_cursor=cursor+64
			if id=="interaction_view" and not options.is_empty(): value=interaction_page(packet.payload.args.cursor)
			if id==fail_query or id=="player_frame_seed" and not fail_query.is_empty(): value={"ok":false,"code":"UnsupportedLiveQuery"}
			if id==stale_query or id=="player_frame_seed" and not stale_query.is_empty(): current.revision+=1
		elif packet.kind=="preview": value={"ok":false,"code":"UnsupportedNativeCapability"} if packet.payload.kind=="rescue" else {"ok":true,"revision":packet.source.revision,"quoted":17}
		elif packet.kind=="command":
			value=command_result.duplicate(true)
			if not command_current.is_empty(): current=command_current.duplicate(true); source=current.duplicate(true)
		return {"ok":true,"ready":true,"tag":packet.kind,"source_identity":packet.source.duplicate(true),"current_identity":current,"result":value}
	## 这些值只表达被测协议，不是替代world/domain的生产默认值。
	func answer(id: String) -> Dictionary:
		match id:
			"player_frame_seed": return frame_seed()
			"command_context": return {"ok":true,"actor_id":"actor.player","revision":source.revision,"next_request_seq":4}
			"action_context": return {"ok":true,"actor_id":"actor.player","revision":source.revision,"action_id":"action:1","action_kind":"travel","travel_paused":false}
			"screen_context": return {"ok":true,"presentation_location":{"kind":"Wilderness"},"personal_battle_view":{"ok":true,"battle_id":""},"military_battle_page":{"ok":true,"active":false},"siege_page":{"ok":true,"siege_id":""},"study_activity":{"ok":true,"active":false},"world_scene_legacy":true}
			"hud_view": return {"ok":true,"actor_id":"actor.player","revision":source.revision,"day_index":1,"half_day":1,"display_name":"","balance":60,"hp":100}
			"journey_input_need": return {"ok":true,"needs_input":false}
			"interaction_view": return {"phase":"scene"}
			"journey_location": return {"kind":"Wilderness"}
			"journey_encounter": return {"travel":{"active":true},"encounter":{}}
			"journey_companions": return {"party_present":false,"cursor":0,"total_rows":0,"rows":[]}
			"journey_logistics": return {"ok":true,"logistics_quote":{}}
			"journey_travel": return {"active":true,"paused":false,"remaining_ticks":5,"destination":"town"}
			"journey_map_position": return {"world_map_id":"map","observer_position":[1,2]}
			"journey_route": return {"active":true,"cursor":0,"total_rows":2,"points":[[1,2],[2,3]]}
			"journey_crossings": return {"cursor":0,"total_rows":0,"rows":[]}
			"character_input": return {"ok":true,"actor_view":{"actor_id":"actor.player","display_name":"","age_years":20,"presentation_profile_id":"profile.player","portrait_kind":"hero","visibility_version":source.revision},"appearance_view":{"format_version":1,"generator_version":"legacy-portrait-v1","base_features":{},"attire_style":"legacy","legacy":true}}
		return {"ok":false,"code":"FixtureUnknownQuery"}
	## 首页与普通后续页沿同一个明确DTO夹具，不用前端规则推导真实世界。
	func interaction_page(cursor: int) -> Dictionary:
		var page: Dictionary={"cursor":cursor,"total_rows":options.size()}
		if cursor+64<options.size(): page.next_cursor=cursor+64
		return {"phase":"choice","options":options.slice(cursor,mini(cursor+64,options.size())).duplicate(true),"page":page}
	## 只模拟冻结的闭合wire，故意损坏字段用来证明前端整批拒绝。
	func frame_seed() -> Dictionary:
		var views: Dictionary={}
		for id in ["command_context","action_context","screen_context","hud_view","journey_input_need","interaction_view"]: views[id]=answer(id)
		if not options.is_empty(): views.interaction_view=interaction_page(0)
		var journey: Dictionary={}
		for id in ["journey_location","journey_encounter","journey_companions","journey_logistics","journey_travel","journey_map_position","journey_route","journey_crossings"]: journey[id]=answer(id)
		if not companions.is_empty():
			journey.journey_companions={"party_present":true,"cursor":0,"total_rows":companions.size(),"rows":companions.slice(0,mini(64,companions.size())).duplicate(true)}
			if companions.size()>64: journey.journey_companions.next_cursor=64
		var quotes: Array=[]
		for kind in ["advance","pause_travel","resume_travel","travel_wait","return","rescue","wait"]:
			quotes.append({"kind":kind,"actor_id":"actor.player","params":{},"result":{"ok":false,"code":"UnsupportedNativeCapability"} if kind=="rescue" else {"ok":true,"revision":source.revision,"quoted":17}})
		var result: Dictionary={"ok":true,"views":views,"journey":journey,"characters":{"actor.player":answer("character_input")},"quotes":quotes}
		match seed_fault:
			"bad_map": result.journey.journey_map_position.observer_position=[1.5,2]
			"extra_root": result.private_world={}
			"extra_character": result.characters["private.other"]=answer("character_input")
			"wrong_quote_actor": result.quotes[0].actor_id="private.other"
			"missing_quote": result.quotes.pop_back()
			"non_bool_quote": result.quotes[0].result.ok=1
		return result

## 产品唯一Bridge创建点在这里替换为明示transport夹具，backend与presenter仍是实际代码。
class Harness extends "res://ui/native_resident_backend.gd":
	var wire=Wire.new()
	## 只替换测试transport创建，不改变任何生产poll/缓存行为。
	func _create_bridge() -> Object: return wire

## 延后执行真实Node生命周期，但测试手工每轮只poll一次，不用时间等待假装完成。
func _init() -> void: call_deferred("_run")
## 断言失败累计到进程退出码，任何提前结束都不成为绿门。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 生产seed不保留在backend；测试只用少数标量确认冷serialize和来源接纳。
func seed_value() -> Dictionary:
	return {"player_actor_id":"actor.player","scenario_id":"scenario.alpha_life","float_tag":1.0,"large_tag":9223372036854775807,"private_seed_marker":"do-not-cache"}
## 最小作者目录仅支撑有限UI协议，不冒充经过真实Schema验证的内容。
func catalog_value() -> Dictionary:
	return {"ok":true,"content_id":"foundation.alpha_life","data":{"scenarios":{"scenario.alpha_life":{"world_map_id":"map"}},"world_maps":{"map":{}},"texts":{"one":"甲"}}}

## 实际后端信号只由生产poll/组帧触发；测试不手工发frame_ready充当完成。
func fixture() -> Dictionary:
	var backend=Harness.new(); root.add_child(backend); backend.set_process(false)
	var frames: Array=[]; var commands: Array=[]; var previews: Array=[]; var errors: Array=[]
	backend.frame_ready.connect(func(source,scene): frames.append({"source":source,"scene":scene}))
	backend.command_completed.connect(func(kind,result,source,current): commands.append({"kind":kind,"result":result,"source":source,"current":current,"observed":backend.source_identity()}))
	backend.preview_ready.connect(func(kind,params,result,source,scene): previews.append({"kind":kind,"params":params,"result":result,"source":source,"scene":scene}))
	backend.failed.connect(func(error): errors.append(error))
	check(backend.install(catalog_value(),seed_value()).get("ok",false),"冷安装唯一transport协议来源")
	return {"backend":backend,"wire":backend.wire,"frames":frames,"commands":commands,"previews":previews,"errors":errors}

## 测试步数有硬上界；缺少完成会报红，不跳过仍在途的工作。
func finish(f: Dictionary) -> void:
	for _index in range(40):
		if not f.backend.is_busy(): return
		f.backend.poll()
	check(false,"有限读链没有在40次poll内完成")

## 覆盖实际前端生命周期、单槽顺序和过期丢弃；不运行玩法或Native规则替身。
func _run() -> void:
	var f: Dictionary=fixture(); var backend=f.backend; var wire=f.wire
	check(backend.transport()==wire and wire.installs==1,"controller获得原唯一transport")
	check(wire.installed_seed.contains('"float_tag":1.0') and wire.installed_seed.contains('9223372036854775807'),"冷JSON不先把float/int64截断成普通数值")
	check(backend.install(catalog_value(),seed_value()).get("code")=="ResidentAlreadyInstalled" and wire.installs==1,"禁止第二次安装或另开core")
	var returned: Dictionary=backend.content(); returned.data.texts.one="changed"; check(backend.content().data.texts.one=="甲","内容副本不改作者目录")
	check(not backend.command_context().get("ok",false) and backend.action_needs_input(),"完整frame之前不伪造序号或自动授权")
	check(backend.refresh(11).ok,"开始闭合frame seed读")
	while backend.is_busy():
		check(f.frames.is_empty(),"最后一页之前不发布半组frame")
		backend.poll()
	check(f.frames.size()==1 and f.errors.is_empty(),"真实backend/presenter仅发布一次完整frame")
	check(wire.calls.size()==1 and wire.calls[0].kind=="query" and wire.calls[0].payload=={"query_id":"player_frame_seed","args":{}},"普通无后续页frame只跨一次worker边界，不重读首页或逐项quote")
	check(backend.command_context().next_request_seq==4 and backend.action_context().action_kind=="travel" and backend.hud_view().display_name=="","必要读由同源真实回执cache提供")
	check(backend.map_view().observer_position==[1.0,2.0] and backend.journey_sidebar().travel.remaining_ticks==5 and backend.interaction_view().phase=="scene","复用原presenter组帧，未另造地图规则")
	check(backend.presentation_location()=={"kind":"Wilderness"} and not backend.personal_battle_view().battle_id and not backend.study_activity().active,"screen只透传协议明确提供的有限子视图")
	check(backend.character_input("actor.player").get("ok",false) and not backend.character_input("private.other").get("ok",false),"只缓存原生可见选择人物，不在getter偷偷枚举其它ID")
	check(backend.preview("advance").get("quoted")==17 and backend.preview("rescue").get("code")=="UnsupportedNativeCapability" and f.previews.is_empty(),"固定控制的真实预览和拒绝同frame保留，不伪造许可或发动态预览信号")
	check(backend.scene_view("","world")=={"ok":true,"visibility":"current"} and not backend.scene_view("other","world").get("ok",false) and not backend.scene_view("","interior").get("ok",false),"legacy场景最小值只用于原生已核当前world分支")
	var calls: int=wire.calls.size(); var map: Dictionary=backend.map_view(); map.travel_overlay.points.clear(); backend.hud_view(); backend.command_context()
	check(wire.calls.size()==calls and backend.map_view().travel_overlay.points.size()==2,"同步getter无查询/全态复制，返回对象独立")
	check(backend.snapshot().get("code")=="UnsupportedNativeCapability" and backend.submit({}).get("code")=="NativeAsyncRequired","旧同步权威入口明确诊断，不fallback GD")
	var params: Dictionary={"destination":"town"}; wire.ready=false
	check(backend.preview_async("travel",params,"actor.player",11).ok,"预览沿单独pure job")
	params.destination="mutated"
	check(backend.submit_async({"kind":"advance"},11).get("code")=="ResidentBackendBusy","未消费预览期间不抢普通槽")
	wire.ready=true; backend.poll()
	check(f.previews.size()==1 and f.previews[0].params.destination=="town" and f.previews[0].scene==11 and wire.calls[-1].kind=="preview","预览保原params/source/scene且从不提交")
	check(backend.preview("travel",{"destination":"town"}).get("quoted")==17 and backend.preview("travel",{"destination":"other"}).get("code")=="NativeAsyncRequired","动态同步preview只读取精确原参数的已完成缓存")
	var raw: Dictionary={"actor_id":"actor.player","request_seq":1,"expected_revision":-20,"kind":"changed-replay","params":{"unrecognized":true}}
	wire.command_result={"ok":false,"code":"AlreadyApplied"}
	check(backend.submit_async(raw,11).ok,"旧请求/改payload仍交core，不在前端抢先拒绝")
	check(wire.calls[-1].payload==raw,"原命令字典和值全部保留")
	backend.poll(); check(f.commands.size()==1 and f.commands[0].result.code=="AlreadyApplied","正常规则拒绝通过command_completed保留")
	wire.command_result={"ok":true,"kind":"fixture-only"}; wire.command_current=wire.source.duplicate(true); wire.command_current.revision+=1
	check(backend.submit_async(raw,11).ok,"接纳下一测试command回执")
	backend.poll(); check(f.commands[-1].observed==wire.command_current and not backend.command_context().get("ok",false),"先采用真实current来源，再发command完成；旧cache不能混成新frame")
	check(backend.refresh(12).ok,"更新来源后显式刷新")
	finish(f); check(f.frames.size()==2 and backend.command_context().revision==wire.source.revision,"更新后所有cache同一新来源")
	wire.ready=false; check(backend.preview_async("travel",{},"actor.player",12).ok,"接纳将过期的只读预览")
	backend.invalidate(); wire.ready=true; backend.poll(); check(f.previews.size()==1 and not backend.is_busy(),"失效预览被消费但不发布过期信号")
	var old: Dictionary=backend.source_identity(); var newer: Dictionary=old.duplicate(true); newer.execution_epoch+=1; newer.revision=0; newer.tick=0
	check(backend.adopt_source(newer).ok and not backend.map_view().get("ok",false),"新epoch显式替换来源并清旧可见cache")
	check(backend.adopt_source(old).get("code")=="StaleWorld","旧完成不能回退当前epoch")
	var malformed: Dictionary=newer.duplicate(true); malformed.world_id="g".repeat(32)
	check(backend.adopt_source(malformed).get("code")=="InvalidResidentSource","来源必须是真32位hex，不接受任意同长字符串")
	backend.queue_free()
	var bad: Dictionary=fixture(); bad.wire.fail_query="hud_view"; check(bad.backend.refresh(1).ok,"启动缺能力负例")
	finish(bad); check(bad.frames.is_empty() and bad.errors.size()==1 and bad.errors[0].code=="UnsupportedLiveQuery" and not bad.backend.hud_view().get("ok",false),"缺必要query明确失败，不造空HUD或部分frame")
	bad.backend.queue_free()
	var stale: Dictionary=fixture(); stale.wire.stale_query="action_context"; check(stale.backend.refresh(1).ok,"启动来源漂移负例")
	finish(stale); check(stale.frames.is_empty() and stale.errors[-1].code=="StalePreview","晚读版本漂移不混入新画面")
	stale.backend.queue_free()
	var cancelled: Dictionary=fixture()
	for index in range(67): cancelled.wire.companions.append({"actor_id":"member:"+str(index),"display_name":"Name "+str(index),"cost_per_tick":index})
	check(cancelled.backend.refresh(1).ok,"启动seed后presenter续页失效负例")
	cancelled.backend.poll()
	cancelled.wire.ready=false; var queued: int=cancelled.wire.calls.size(); cancelled.backend.invalidate(); cancelled.wire.ready=true; cancelled.backend.poll()
	check(not cancelled.backend.is_busy() and cancelled.wire.calls.size()==queued and cancelled.frames.is_empty(),"presenter先invalidate再消费旧ticket，不给priority cancel后抢发下一query")
	cancelled.backend.queue_free()
	var roster: Dictionary=fixture()
	for index in range(67): roster.wire.companions.append({"actor_id":"member:"+str(index),"display_name":"Name "+str(index),"cost_per_tick":index})
	check(roster.backend.refresh(1).ok,"启动既有67人分页名单合同")
	finish(roster)
	var portraits: int=0
	for request in roster.wire.calls:
		if request.kind=="query" and request.payload.query_id=="character_input": portraits+=1
	check(roster.frames.size()==1 and roster.backend.journey_sidebar().companions.size()==67,"名单按原分页完整保留，没有新增18人上限")
	check(portraits==0 and roster.wire.calls.size()==2 and roster.wire.calls[1].payload=={"query_id":"journey_companions","args":{"cursor":64}},"seed已带self画像；67人名单只补缺少的第64游标页")
	roster.backend.queue_free()
	var dialogue: Dictionary=fixture()
	for index in range(67): dialogue.wire.options.append({"id":"choice:"+str(index),"text":"Option "+str(index)})
	check(dialogue.backend.refresh(1).ok,"接纳对白选项首页seed")
	finish(dialogue)
	check(dialogue.frames.size()==1 and dialogue.backend.interaction_view().options.size()==67 and dialogue.wire.calls.size()==2 and dialogue.wire.calls[1].payload=={"query_id":"interaction_view","args":{"cursor":64}},"对白仅补后一页，完整选项和同源地图一起发布")
	dialogue.backend.queue_free()
	var malformed_seed: Dictionary=fixture(); check(malformed_seed.backend.refresh(1).ok,"先建立待保留的完整frame")
	finish(malformed_seed); var preserved: Dictionary=malformed_seed.backend.map_view()
	for fault in ["extra_root","extra_character","wrong_quote_actor","missing_quote","non_bool_quote"]:
		malformed_seed.wire.seed_fault=fault
		check(malformed_seed.backend.refresh(2).ok,"发起闭合wire负例："+fault)
		finish(malformed_seed)
		check(malformed_seed.frames.size()==1 and malformed_seed.backend.map_view()==preserved,"拒绝坏seed但保留同源旧完整frame："+fault)
	check(malformed_seed.errors.size()==5,"每次坏seed只报告一次明确失败，不fallback旧分散查询")
	malformed_seed.backend.queue_free()
	for continuation in [false,true]:
		var reentry: Dictionary=fixture(); reentry.wire.seed_fault="bad_map"
		if continuation:
			for index in range(67): reentry.wire.options.append({"id":"choice:"+str(index),"text":"Option "+str(index)})
		var restarted: Array=[]
		reentry.backend.failed.connect(func(_error):
			if restarted.is_empty():
				reentry.wire.seed_fault=""
				restarted.append(reentry.backend.refresh(2)))
		check(reentry.backend.refresh(1).ok,"启动同步failed重入refresh负例，续页="+str(continuation))
		finish(reentry)
		check(restarted.size()==1 and restarted[0].get("ok",false) and reentry.errors.size()==1 and reentry.frames.size()==1 and reentry.frames[0].scene==2,"旧presenter错误返回不得清除failed处理器刚接纳的新批次")
		reentry.backend.queue_free()
	print("NATIVE_RESIDENT_BACKEND_PROTOCOL_CHECKS=",checks," FAILURES=",failures," SCOPE=explicit transport fixture, no real core/Main acceptance")
	quit(0 if failures==0 else 1)
