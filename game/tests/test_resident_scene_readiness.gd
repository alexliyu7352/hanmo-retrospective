## 原Main/Backend/SceneHost的首次热点协议门；transport与纹理为明确夹具，不冒充原生玩法、真鼠标或像素验收。
extends SceneTree
const Protocol=preload("res://tests/test_native_resident_backend.gd")
const Host=preload("res://ui/scene_host.gd")
const Art=preload("res://ui/art_catalog.gd")
const Requests=preload("res://ui/resident_page_requests.gd")
const Adapter=preload("res://ui/resident_journey_adapter.gd")
var checks: int=0
var failures: int=0

## 只替换传输；生产Backend仍负责需求去重、来源绑定、排队、poll与页面发布。
class Wire extends Protocol.Wire:
	var projection: Dictionary={}
	var denied: bool=false
	var activity: Dictionary={}
	var needs_activity_input: bool=false
	var deny_advance: bool=false
	var challenge: Dictionary={"ok":true,"phase":"completed","result":{"score":60,"performance_bp":10000}}
	## 许可控制器接口仍被实际Adapter核验，本门从不创建自动run。
	func set_reading_paused(_run: Dictionary, _paused: bool) -> Dictionary: return {"ok":true}
	## 固定页面DTO使首次动态报价处于真实未缓存状态，没有GD玩法世界。
	func answer(id: String) -> Dictionary:
		match id:
			"screen_context":
				var value: Dictionary=super.answer(id)
				value.presentation_location={"kind":"Settlement","place_id":"place.grain"}; value.world_scene_legacy=false
				if not activity.is_empty(): value.study_activity=activity.duplicate(true)
				return value
			"action_context":
				var value: Dictionary=super.answer(id); value.action_id=activity.get("action_id",""); value.action_kind=activity.get("kind",""); return value
			"study_challenge": return challenge.duplicate(true)
			"continuation_state": return {"ok":true,"available":false}
			"journey_input_need": return {"ok":true,"needs_input":needs_activity_input}
			"action_manual_window": return {"ok":true,"manual":false}
			"scene_view": return projection.duplicate(true)
			"place_sidebar": return {"ok":true,"people":[],"queued_event_ids":[],"estate_ids":[],"operating_campaigns":[],"action_busy":false,"military_here":false}
		return super.answer(id)
	## 普通规则拒绝带同一revision；必须显示正常失败反馈，不能永久阻挡页面。
	func poll_live_operation(reference: Dictionary) -> Dictionary:
		var kind: String=pending.get("payload",{}).get("kind","")
		var reply: Dictionary=super.poll_live_operation(reference)
		if denied and kind=="move_local" and reply.get("ready",false) and reply.get("tag")=="preview":
			reply.result={"ok":false,"kind":"rule","code":"AccessDenied","revision":reply.source_identity.revision}
		return reply
	## 固定首页报价保持真实wire形状，普通advance拒绝不伪装缺读。
	func frame_seed() -> Dictionary:
		var value: Dictionary=super.frame_seed()
		if deny_advance:
			value.quotes[0].result={"ok":false,"kind":"rule","code":"ActionBlocked","revision":source.revision}
		return value

## 实际Backend仅通过既有测试接缝使用有限transport，其余行为逐字沿生产代码。
class Backend extends "res://ui/native_resident_backend.gd":
	var wire=Wire.new()
	## 创建唯一测试传输，不另建规则或提交路径。
	func _create_bridge() -> Object: return wire

## Main仅跳过音频/磁盘启动；render、process、场景检查和冻结提交均使用原方法。
class Main extends "res://ui/main.gd":
	var storage_requests: Array=[]
	## 手动驱动真实页面与异步边界，不运行音频和外部存储初始化。
	func _ready() -> void:
		set_process(false); page=VBoxContainer.new(); add_child(page); page.size=Vector2(1280,800)
	## 存储边界只记录列表请求；实际save意图、撤销读链和开槽守卫仍由Main负责。
	func _request_storage(intent: String, operation: String, args: Dictionary) -> Dictionary:
		_storage_intent=intent; storage_requests.append({"operation":operation,"args":args.duplicate(true)})
		return {"ok":true,"pending":true}

## 纯色小纹理只用于实例化真实HitArea，不是正式美术或像素验收材料。
class Registry extends RefCounted:
	var manifest: Dictionary={}
	var image: Texture2D=ImageTexture.create_from_image(Image.create(10,10,false,Image.FORMAT_RGBA8))
	## 清单副本遵守场景resolver的同摘要检查，不复写生产清单。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 唯一测试纹理不声称任何正式资产已恢复。
	func texture(_group: String, _id: String) -> Texture2D: return image
	## 标题关闭控件需要可查询的背景目录，不影响场景候选。
	func ids(_group: String) -> Array: return []

## 原候选保留当前observer、目标与revision，不能在点击时改用新位置。
func projection() -> Dictionary:
	var location: Dictionary={"settlement_id":"settlement.qingchuan","place_id":"place.grain","parent_place_id":""}
	var target: Dictionary={"kind":"place","id":"place.courtyard"}
	var passage: Dictionary={"target_ref":target,"location_ref":{"settlement_id":"settlement.qingchuan","place_id":"place.courtyard","parent_place_id":""}}
	var candidate: Dictionary={"id":"return","command_kind":"move_local","command_source":"local","target_ref":target,"params":{"place_id":"place.courtyard"},"expected_revision":7,"needs_preview":true,"enabled":false,"reason":"KernelPreviewRequired"}
	return {"ok":true,"view_kind":"city","observer_id":"actor.player","location_ref":location,"revision":7,"knowledge_tick":3,"visibility":"current","template_id":"fixture.grain","layers":{"background":[],"buildings":[passage],"furnishings":[],"actors":[],"effects":[],"foreground":[]},"people":[],"passage_targets":[passage],"hotspot_candidates":[candidate]}

## 结构完整的窄模板只绑定一个原通路，生产ScenePresentation与LayeredScene仍完整校验。
func registry() -> Registry:
	var value=Registry.new()
	var geometry: Dictionary={"logical_canvas":[100,100],"source_size":[10,10],"safe_crop":[0,0,10,10],"support_point":[0,0]}
	value.manifest={"scene_templates":{"fixture.grain":{"id":"fixture.grain","view_kind":"city","logical_canvas":[100,100],"ui_safe_areas":[],"layers":[
		{"id":"background","kind":"background","asset_id":"backgrounds/fixture","geometry":geometry,"anchor":[0,0],"after":[],"z_index":0},
		{"id":"exit","kind":"building","asset_id":"backgrounds/fixture","geometry":geometry,"anchor":[0,0],"target_ref":{"kind":"place","id":"place.courtyard"},"after":["background"],"z_index":1},
		{"id":"grain.return","kind":"hotspot","target_layer_id":"exit","rect":[10,10,20,20],"candidate_selector":{"command_kind":"move_local","params":{}},"after":["exit"],"z_index":2}]}}}
	return value

## 有限作者数据只支持实际Screen渲染；没有世界真值或额外业务规则。
func data() -> Dictionary:
	return {"texts":{},"required_capabilities":[],"scenarios":{"fixture":{"world_map_id":"map"}},"world_maps":{"map":{}},"places":{"place.grain":{"id":"place.grain","settlement_id":"settlement.qingchuan","parent_place":null,"passages":["place.courtyard"]},"place.courtyard":{"id":"place.courtyard","settlement_id":"settlement.qingchuan","parent_place":null,"passages":["place.grain"]}},"settlements":{"settlement.qingchuan":{"id":"settlement.qingchuan","entry_place":"place.courtyard"}},"roles":{},"props":{},"estates":{}}

## 原始错误累计到退出码，不因后续重试成功覆盖首次失败。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 所有真实节点先入树；旧完整页面在新有限读取期间保留可见但失去输入。
func fixture(denied: bool=false, activity: Dictionary={}, needs_input: bool=false, deny_advance: bool=false, plan_enabled: bool=false) -> Dictionary:
	Art.commit_content({"registry":registry()})
	var main=Main.new(); root.add_child(main)
	var backend=Backend.new(); main.add_child(backend); backend.set_process(false)
	backend.wire.projection=projection(); backend.wire.denied=denied
	backend.wire.activity=activity.duplicate(true); backend.wire.needs_activity_input=needs_input; backend.wire.deny_advance=deny_advance
	var content: Dictionary=data()
	if plan_enabled: content.scenarios.fixture.player_plan_profile="known_life_v1"
	check(backend.install({"ok":true,"data":content},{"player_actor_id":"actor.player","scenario_id":"fixture"}).ok,"实际Backend冷装配有限transport")
	var adapter=Adapter.new(); main.add_child(adapter); adapter.set_process(false)
	check(adapter.bind_context(backend.transport(),backend.source_identity(),0).ok,"实际Adapter绑定同一transport来源")
	main.kernel=backend; main._resident_adapter=adapter; main._resident_mode=true; main._started=true; main._data=data()
	backend.frame_ready.connect(main._resident_frame_ready); backend.page_reads_ready.connect(main._resident_page_reads_ready)
	backend.preview_ready.connect(main._resident_preview_ready); backend.failed.connect(main._resident_failed)
	var errors: Array=[]; backend.failed.connect(func(error): errors.append(error))
	var old=Host.new(); main.page.add_child(old); old.size=Vector2(1280,720)
	var configured: Dictionary=old.configure_view(projection(),data(),"place.grain","city",func(_kind,_params,_actor): return {"ok":true,"revision":7},backend.character_input)
	check(configured.get("mode")=="layered","旧完整页面是真实SceneHost与HitArea "+str(configured))
	old.command_requested.connect(main._scene_request.bind(old))
	check(backend.refresh(main._scene_counter).ok,"真实闭合frame开始，未预热动态热点报价")
	backend.poll()
	return {"main":main,"backend":backend,"wire":backend.wire,"old":old,"errors":errors}

## 只手动消费生产process/poll，不注入缓存或手工发ready信号。
func settle(f: Dictionary) -> void:
	for _index in range(32):
		f.main._resident_process(); f.backend.poll()
		if not f.backend.is_busy() and not f.backend.has_pending_reads() and not f.main._resident_frame_pending and f.main._resident_command.is_empty() and f.main._resident_pending_slot_mode.is_empty(): return
	check(false,"有限纯读链应在32次内排空")

## 计数真实transport命令包，不把Host信号或Main返回pending冒作已提交命令。
func commands(f: Dictionary) -> Array:
	return f.wire.calls.filter(func(packet): return packet.kind=="command")

## 释放未启动的音频/存储成员，避免孤儿掩盖退出日志。
func dispose(f: Dictionary) -> void:
	var main=f.main
	main._resident_mode=false
	main._ambience.music.free(); main._ambience.effect.free(); main._ambience.free()
	main.journey_view.free(); main.storage.free(); main._ui_font_scale.free(); main.free()

## 先断言真实控件停用，再等同源读取完成；首次有效点击应立即冻结唯一命令。
func first_click() -> void:
	var f: Dictionary=fixture(); var old_hit: Control=hit_area(f.old)
	check(f.main._advancing and not f.old._input_enabled and old_hit!=null and not old_hit.enabled,"页面pending同步关闭旧Host及真实HitArea")
	check(f.old.activate_hotspot("grain.return").get("code")=="SceneInputUnavailable" and commands(f).is_empty(),"pending旧热点不能发起预览或命令")
	settle(f)
	var host: Control=f.main.find_child("SceneHost",true,false)
	if not check(host!=null and host!=f.old and not f.main._advancing and f.errors.is_empty(),"全部纯读完成才发布新完整Main页面"): dispose(f); return
	var preview_packets: Array=f.wire.calls.filter(func(packet): return packet.kind=="preview")
	check(preview_packets.size()==1 and preview_packets[0].payload=={"actor_id":"actor.player","kind":"move_local","params":{"place_id":"place.courtyard"}},"有限候选只读一次，人物命令参数精确匹配原scene_view")
	check(commands(f).is_empty(),"页面预读从未提交命令")
	var result: Dictionary=host.activate_hotspot("grain.return")
	var frozen: Dictionary=f.main._resident_command.duplicate(true)
	check(result.ok and frozen.get("kind")=="move_local" and frozen.get("params")=={"place_id":"place.courtyard"} and frozen.get("request_seq")==4 and frozen.get("expected_revision")==7,"首次启用热点直接进入Main冻结提交，无失败后重试")
	host.activate_hotspot("grain.return"); f.main._resident_process(); f.main._resident_process()
	check(commands(f).size()==1 and commands(f)[0].payload==frozen,"重复点击只产生一次原封不动的完整命令")
	check(frozen.get("target_ref")=={"kind":"place","id":"place.courtyard"} and frozen.get("scene_epoch")==host._epoch,"冻结命令保持目标和Host代次")
	dispose(f)

## 普通拒绝仍发布完整页面，并沿真实Host反馈；失败不是一条等待中的命令。
func ordinary_denial() -> void:
	var f: Dictionary=fixture(true); settle(f)
	var host: Control=f.main.find_child("SceneHost",true,false)
	var rejected: Array=[]; host.input_rejected.connect(func(value): rejected.append(value))
	var result: Dictionary=host.activate_hotspot("grain.return")
	hit_area(host).activated.emit()
	check(not f.main._advancing and result.get("code")=="AccessDenied" and not rejected.is_empty() and commands(f).is_empty(),"普通规则拒绝显示原反馈且零提交")
	dispose(f)

## 读链发出但未完成时改变页面或来源，晚回执只能丢弃或读新页，不能变成晚提交。
func interrupted_reads() -> void:
	for mode in ["close","save","source","revision","scene"]:
		var f: Dictionary=fixture()
		for _index in range(16):
			f.main._resident_process()
			if f.wire.pending.get("kind")=="preview": break
			f.backend.poll()
		if not check(f.wire.pending.get("kind")=="preview",mode+"：真实热点pure preview确实在途"): dispose(f); continue
		var old_scene: int=f.main._scene_counter
		if mode=="close":
			f.old.clear_scene(); f.main._started=false; f.main._scene_counter+=1; f.backend.invalidate()
		elif mode=="save": f.main._save()
		elif mode in ["source","revision"]:
			var current: Dictionary=f.backend.source_identity()
			if mode=="source": current.execution_epoch+=1
			else: current.revision+=1
			check(f.backend.adopt_source(current).ok,mode+"：采用新来源")
		else: f.main._scene_counter+=1
		f.backend.poll()
		if mode=="save": settle(f)
		else: f.main._resident_process()
		check(commands(f).is_empty() and f.main._resident_command.is_empty(),mode+"：晚读回执不产生任何命令")
		if mode=="save": check(f.main._storage_intent=="save_list" and f.main.storage_requests.size()==1,"save：旧读取消后沿原Main到达存槽列表")
		if mode=="scene": check(f.main._scene_counter==old_scene+1 and f.old.is_inside_tree(),"scene：旧回调不能重建新代次页面")
		dispose(f)

## 无正式模板的旧场景不新增候选报价；只运行相同的既有场景读取。
func legacy_reads() -> void:
	var f: Dictionary=fixture(); f.backend.page_reads_ready.disconnect(f.main._resident_page_reads_ready)
	f.backend.invalidate(); f.wire.projection.erase("template_id")
	check(not Requests._scene_ready(f.main,"place.grain","city"),"旧场景仍等待原scene_view纯读")
	check(f.backend.begin_pending_read(f.main._scene_counter).ok,"旧场景只发出原scene_view查询")
	f.backend.poll()
	check(Requests._scene_ready(f.main,"place.grain","city"),"旧scene_view到达即可结束准备，无新增报价")
	check(f.wire.calls.filter(func(packet): return packet.kind=="preview").is_empty(),"无模板原场景不新增动态候选纯读")
	check(commands(f).is_empty(),"旧场景准备不提交命令")
	dispose(f)

## 按稳定元数据查找真实控件；Node名会规范化标点，不据此猜测热点身份。
func hit_area(host: Control) -> Control:
	for node in host.find_children("*","Control",true,false):
		if node.get_meta("hotspot_id","")=="grain.return": return node
	return null

## 真实HitArea的按下状态必须在停用时清空，重新启用不能消费旧抬起事件。
func pointer_fence() -> void:
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	check(host.configure_view(projection(),data(),"place.grain","city",func(_kind,_params,_actor): return {"ok":true,"revision":7},func(_actor): return {}).ok,"指针门使用实际Host配置")
	var hit: Control=hit_area(host)
	var events: Array=[]; hit.activated.connect(func(): events.append(true))
	var down=InputEventMouseButton.new(); down.button_index=MOUSE_BUTTON_LEFT; down.pressed=true; down.position=hit.size/2.0
	hit._gui_input(down); check(hit.pointer==-2,"真实HitArea记录本次鼠标按下")
	host.set_input_enabled(false); check(not hit.enabled and hit.pointer==-1,"停用清空真实HitArea按下状态")
	host.set_input_enabled(true); down.pressed=false; hit._gui_input(down)
	check(events.is_empty(),"重新启用后的旧抬起不能激活热点")
	host.clear_scene(); check(not hit.enabled,"关闭前先停用仍待释放的真实命中控件")
	host.free()

## 当前修业DTO沿原字段描述已接受课程；不预热、不改缓存，也不重复点击来规避未准备页。
func study_activity(paused: bool=false) -> Dictionary:
	return {"ok":true,"active":true,"kind":"study_skill","actor_id":"actor.player","action_id":"study:accepted","remaining_ticks":1,"completed_lessons":1,"paid":2,"reserved":2,"settlement_policy":"per_lesson_v1","xp_gained":3,"pause_supported":true,"paused":paused,"paused_ticks":0,"challenge_pending":false,"challenge_scopes":["study"]}

## 在真实页面首次出现预算按钮时停下，避免测试等待额外读链掩盖玩家首次点击空转。
func first_budget_button(f: Dictionary) -> Button:
	for _index in range(20):
		var button: Button=f.main.find_child("ActivityContinuationBegin",true,false)
		if button!=null: return button
		f.main._resident_process(); f.backend.poll()
	check(false,"有限活动页须发布实际预算确认控件")
	return null

## 只数实际transport接纳的许可，不把按钮点击、草案或自动运行标志当成成功。
func starts(f: Dictionary) -> Array:
	return f.wire.calls.filter(func(packet): return packet.kind=="begin_run")

## 原advance已在首页seed；活动页其他迟发报价同样不得令首次已启用确认静默返回。
func activity_first_confirmation() -> void:
	var f: Dictionary=fixture(false,study_activity())
	var button: Button=first_budget_button(f)
	if button==null: dispose(f); return
	print("ACTIVITY_FIRST_VISIBLE ",JSON.stringify({"disabled":button.disabled,"pending_reads":f.backend._pending_reads,"advance":f.backend.preview("advance"),"scene_epoch":f.main._scene_counter}))
	check(not button.disabled and not f.backend.has_pending_reads() and not f.backend.is_busy(),"首次已启用预算按钮发布前本页纯读已闭合")
	button.pressed.emit()
	check(starts(f).size()==1 and f.main._resident_adapter.auto_running,"第一次预算确认立即创建一个原生许可请求")
	button.pressed.emit()
	check(starts(f).size()==1,"重复确认不重复begin_run或补授半日")
	if starts(f).size()==1:
		var packet: Dictionary=starts(f)[0]
		check(packet.payload.actor_id=="actor.player" and packet.payload.action_id=="study:accepted" and packet.payload.max_half_days==1 and packet.payload.max_gross_outflow==0 and packet.source==f.backend.source_identity(),"首次确认保留原人物行动来源和本人预算")
	check(commands(f).is_empty() and f.wire.calls.filter(func(packet): return packet.kind=="grant").is_empty(),"页面准备与确认没有代发普通命令或首半日grant")
	dispose(f)

## 同一实际Main在读链排空后首次确认可启动；保留restore的隐藏地图标志以区分独立原因。
func activity_settled_confirmation() -> void:
	var f: Dictionary=fixture(false,study_activity()); f.main.journey_view.opened=true
	settle(f)
	var button: Button=f.main.find_child("ActivityContinuationBegin",true,false)
	var half_days: SpinBox=f.main.find_child("ActivityContinuationHalfDays",true,false)
	var money: SpinBox=f.main.find_child("ActivityContinuationMoney",true,false)
	if not check(button!=null and half_days!=null and money!=null,"排空后原预算控件仍可见"): dispose(f); return
	half_days.value=5; money.value=150
	print("ACTIVITY_SETTLED_GUARDS ",JSON.stringify({"source":f.backend.source_identity(),"context":f.backend.action_context(),"draft":f.main._activity_draft,"scene_epoch":f.main._scene_counter,"advancing":f.main._advancing,"pending_reads":f.backend.has_pending_reads(),"busy":f.backend.is_busy(),"needs_input":f.main._activity_continuation_needs_input(),"main_hold":f.main._resident_hold,"adapter_hold":f.main._resident_adapter._refresh_hold,"journey_opened":f.main.journey_view.opened}))
	button.pressed.emit()
	check(starts(f).size()==1 and f.main._resident_adapter.auto_running,"已排空读页且隐藏地图opened时首次预算确认仍启动原许可")
	if starts(f).size()==1: check(starts(f)[0].payload.max_half_days==5 and starts(f)[0].payload.max_gross_outflow==150,"实际SpinBox草案完整进入本次许可")
	dispose(f)

## 本页可见原题目、退出与加入安排入口都用相同有限纯读屏障，不能在发布后再补报价。
func activity_visible_quotes() -> void:
	var activity: Dictionary=study_activity(); activity.challenge_pending=true
	var f: Dictionary=fixture(false,activity,false,false,true)
	f.wire.challenge={"ok":true,"phase":"choosing","challenge_kind":"etiquette","round":1,"node_id":"lesson:1","topic":"测试原题面","options":[{"id":"first","text":"原选项一"},{"id":"second","text":"原选项二"}]}
	var button: Button=first_budget_button(f)
	if button==null: dispose(f); return
	check(button.disabled and not f.backend.has_pending_reads(),"当前题面未答时预算禁用，页面所有报价已齐")
	var requested: Array=[]
	for packet in f.wire.calls:
		if packet.kind=="preview": requested.append(packet.payload)
	for option in ["first","second"]:
		check({"actor_id":"actor.player","kind":"study_challenge_choose","params":{"action_id":"study:accepted","step_id":"lesson:1","option_id":option}} in requested,"本页练习原选项参数精确预读："+option)
	check({"actor_id":"actor.player","kind":"study_challenge_exit","params":{"action_id":"study:accepted"}} in requested,"本页退出原练习报价齐备")
	check({"actor_id":"actor.player","kind":"player_plan_add","params":{"kind":"continue_action","params":{"action_id":"study:accepted"},"authorized":false}} in requested,"仅已启用安排入口预读默认未授权的原continue_action草案")
	var exit_button: Button=f.main.find_child("ActivityChallengeExit",true,false)
	var plan_button: Button=f.main.find_child("PlayerPlanAdd",true,false)
	check(exit_button!=null and not exit_button.disabled and plan_button!=null and not plan_button.disabled,"练习退出和加入安排初次显示即可使用")
	button.pressed.emit()
	check(starts(f).is_empty() and commands(f).is_empty(),"未答题即使旧回调触发也不授许可或半日")
	dispose(f)

## 原普通拒绝、暂停恢复仍由Session显示和Main守卫，不被页面屏障改成成功许可。
func activity_rejections() -> void:
	var denied: Dictionary=fixture(false,study_activity(),false,true)
	var begin: Button=first_budget_button(denied)
	var advance: Button=denied.main.find_child("StudyAdvance",true,false)
	check(begin!=null and advance!=null and advance.disabled and advance.get_meta("activity_quote",{}).get("code")=="ActionBlocked" and denied.main._resident_page_error.is_empty(),"普通advance拒绝仍发布页面并保留原禁用反馈")
	if begin!=null: begin.pressed.emit()
	check(starts(denied).is_empty() and commands(denied).is_empty(),"普通规则拒绝不能创建预算许可")
	dispose(denied)
	var paused: Dictionary=fixture(false,study_activity(true)); paused.main._courses_open=true
	settle(paused)
	var resume: Button=paused.main.find_child("StudyResume",true,false)
	check(resume!=null and not resume.disabled and paused.main.find_child("ActivityContinuationBegin",true,false)==null and not paused.backend.has_pending_reads(),"暂停课程只预读当前恢复/结束按钮，不露出继续预算")
	check(starts(paused).is_empty() and commands(paused).is_empty(),"暂停页预读不会替玩家恢复课程")
	dispose(paused)

## 旧可见确认的source、scene、action及关闭/输入门保持原优先级；晚纯读也不能补发许可。
func activity_confirmation_fences() -> void:
	for mode in ["source","scene","action","close","input","read"]:
		var f: Dictionary=fixture(false,study_activity(),mode=="input")
		var begin: Button=first_budget_button(f)
		if begin==null: dispose(f); continue
		if mode=="source":
			var newer: Dictionary=f.backend.source_identity(); newer.execution_epoch+=1
			check(f.backend.adopt_source(newer).ok,"采用新的恢复来源")
		elif mode=="scene": f.main._scene_counter+=1
		elif mode=="close": f.main._started=false
		elif mode=="read":
			f.backend.demand_view("study_challenge",{"actor_id":"actor.player","action_id":"study:other"})
		if mode=="action": f.main._begin_activity_continuation("actor.player","study:other",f.backend.source_identity(),f.main._scene_counter)
		else: begin.pressed.emit()
		if mode=="read": settle(f)
		check(starts(f).is_empty() and commands(f).is_empty() and f.main._resident_command.is_empty(),mode+"：原守卫拒绝旧确认且无迟来自动提交")
		dispose(f)

## 页面准备中关闭或改来源只撤销纯读，未发布预算确认不能在晚回执后自动出现一次提交。
func activity_pending_cancellation() -> void:
	for mode in ["close","save","source","scene"]:
		var f: Dictionary=fixture(false,study_activity())
		for _index in range(20):
			f.main._resident_process()
			if f.wire.pending.get("kind")=="preview": break
			f.backend.poll()
		if not check(f.wire.pending.get("kind")=="preview" and f.main.find_child("ActivityContinuationBegin",true,false)==null,mode+"：本页报价在途时尚未发布确认入口"): dispose(f); continue
		if mode=="close": f.main._started=false; f.backend.invalidate()
		elif mode=="save": f.main._save()
		elif mode=="source":
			var newer: Dictionary=f.backend.source_identity(); newer.execution_epoch+=1
			check(f.backend.adopt_source(newer).ok,"在途时采用新来源")
		else: f.main._scene_counter+=1
		f.backend.poll()
		if mode=="save": settle(f)
		check(starts(f).is_empty() and commands(f).is_empty(),mode+"：迟来纯读不会创建许可、重放点击或提交命令")
		dispose(f)

## 固定引擎下仅报告协议/控件范围，保留无真实原生规则与图形验收的边界。
func _run() -> void:
	if not "--activity-only" in OS.get_cmdline_user_args():
		first_click(); ordinary_denial(); interrupted_reads(); legacy_reads(); pointer_fence()
	activity_first_confirmation(); activity_settled_confirmation(); activity_visible_quotes(); activity_rejections(); activity_confirmation_fences(); activity_pending_cancellation()
	await process_frame
	print("RESIDENT_SCENE_READINESS_CHECKS=",checks," FAILURES=",failures," SCOPE=actual Main/Backend/SceneHost controls; explicit transport and texture fixtures; no native world or GUI acceptance")
	quit(0 if failures==0 else 1)

## 资源与场景树完成初始化后再驱动控件。
func _initialize() -> void: call_deferred("_run")
