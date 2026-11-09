## 真实Main/Backend和继续控件的只读决定提示门；transport明确为夹具，不冒充原生规则或图形验收。
extends SceneTree
const Flow=preload("res://tests/test_resident_scene_readiness.gd")
const Controls=preload("res://ui/activity_continuation_controls.gd")
const Presenter=preload("res://ui/activity_decision_presenter.gd")
const Adapter=preload("res://ui/resident_journey_adapter.gd")
var checks: int=0
var failures: int=0

## 只提供中央冻结的有限DTO；原Backend仍核来源、排队及消费回执，不读取世界快照。
class Wire extends Flow.Wire:
	var decision_view: Dictionary={}
	## 其余现成协议字段保持原夹具，不添加替代玩法或提交入口。
	func answer(id: String) -> Dictionary:
		return decision_view.duplicate(true) if id=="continuation_state" else super.answer(id)

## 唯一测试接缝替换transport，产品缓存及同源失效逻辑保留原实现。
class Backend extends "res://ui/native_resident_backend.gd":
	var wire=Wire.new()
	## 固定有限传输不持有原生世界，不伪造实际玩法成功。
	func _create_bridge() -> Object: return wire

## 断言累计进入进程退出码，首个失败不会被后续成功掩盖。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 本人已知邀请字段与本章实例同形；地点明确是原邀请办理地点，不是对方实时位置。
func invitation() -> Dictionary:
	return {"kind":"membership_invitation","id":"membership_offer:39","actor_id":"actor.player","issuer_actor_id":"actor.clerk","display_name":"许主事","organization_id":"organization.county","source_fact_id":"fact:membership:39","status":"pending","known_place_id":"place.courtyard","known_place_name":"青川院前","expires_tick":25}

## 原Main只略过音频启动；真正Backend、Adapter、状态读取和可见继续控件共同入树。
func fixture(rows: Array, options: Dictionary={}) -> Dictionary:
	var main=Flow.Main.new(); root.add_child(main)
	var backend=Backend.new(); main.add_child(backend); backend.set_process(false)
	var kind: String=options.get("action_kind","study_skill")
	backend.wire.activity={"active":true,"kind":kind,"actor_id":"actor.player","action_id":"action:44","paused":false,"challenge_pending":false}
	var source: Dictionary=backend.wire.source.duplicate(true)
	var state: Dictionary={"actor_id":"actor.player","action_id":"action:44","source_identity":source.duplicate(true),"stop":options.get("stop","awaiting_input"),"max_half_days":5,"committed_half_days":0,"max_gross_outflow":150,"gross_outflow":0}
	if options.get("stale_state",false): state.source_identity.execution_epoch+=1
	backend.wire.decision_view={"ok":true,"available":true,"source_identity":source.duplicate(true),"continuation":state,"pending_decisions":rows.duplicate(true)}
	if options.get("stale_view",false): backend.wire.decision_view.source_identity.revision+=1
	if options.get("legacy",false): backend.wire.decision_view.erase("source_identity"); backend.wire.decision_view.erase("pending_decisions")
	check(backend.install({"ok":true,"data":{"texts":{},"scenarios":{"fixture":{"world_map_id":"map"}},"world_maps":{"map":{}}}},{"player_actor_id":"actor.player","scenario_id":"fixture"}).ok,"真实Backend安装有限transport")
	var adapter=Adapter.new(); main.add_child(adapter); adapter.set_process(false)
	check(adapter.bind_context(backend.transport(),source,0).ok,"真实Adapter绑定当前来源")
	main.kernel=backend; main._resident_adapter=adapter; main._resident_mode=true; main._started=true
	# 故意放入不该被提示读取的作者名字，证明未知地点不从静态目录补全。
	main._data={"places":{"place.secret":{"id":"place.secret","display_name":"未获知密室"}},"actors":{"actor.secret":{"id":"actor.secret","display_name":"秘密人物"}},"texts":{}}
	check(backend.refresh(0).ok,"实际首页缓存从wire发布")
	for _index in range(20):
		if not backend.is_busy(): break
		backend.poll()
	check(not backend.is_busy(),"首页有限读已完成")
	check(backend.continuation_state().get("code")=="NativeAsyncRequired" and backend.begin_pending_read(0).ok,"当前既有continuation_state只读链接纳一次")
	backend.poll()
	var before: String=JSON.stringify(backend.wire.decision_view)
	var call_count: int=backend.wire.calls.size()
	adapter.auto_running=options.get("running",false)
	Controls.render(main,main.page,kind)
	check(backend.wire.calls.size()==call_count and not backend.has_pending_reads() and backend.source_identity()==source and JSON.stringify(backend.wire.decision_view)==before,"实际控件只消费当前DTO，不补查远地或改来源/事实")
	return {"main":main,"backend":backend,"wire":backend.wire}

## 读取实际挂载Label而非对字符串函数做快照，保留界面信息可见性的意义。
func visible_text(main: Control) -> String:
	var values: PackedStringArray=[]
	for node in main.find_children("*","Label",true,false):
		if node.is_visible_in_tree(): values.append(node.text)
	return "\n".join(values)

## 只检查新提示没有命令控件；原预算继续按钮仍保留原Main守卫。
func assert_read_only(f: Dictionary) -> void:
	for packet in f.wire.calls: check(packet.kind=="query","提示过程只有既有query，没有preview/command/begin/grant")
	var details: Node=f.main.find_child("ActivityPendingDecisions",true,false)
	if details!=null: check(details.find_children("*","Button",true,false).is_empty(),"提示没有新增暂停/移动/接受/拒绝按钮")
	check(f.main._resident_command.is_empty() and not f.backend.is_busy(),"提示没有延期命令或等待提交")

## 保留实例中的原SourceFact标识，但不把完整世界或另一个人的邀请挂进提示。
func known_invitation() -> void:
	var f: Dictionary=fixture([invitation()]); var text: String=visible_text(f.main)
	check(text.contains("许主事") and text.contains("原邀请办理地点：青川院前") and text.contains("半日25"),"本人原邀请、来源人、原办理地点和期限可见")
	check(text.contains("暂停课程，保留进度与预留款") and text.contains("原通路") and text.contains("核对工资、职责与离开条款") and text.contains("恢复原课程"),"当前修业提示完整原办理路径，未代办任何一步")
	check(text.contains("是否在场"),"原邀请地点不被冒称对方此刻所在地")
	var row: Node=f.main.find_child("ActivityMembershipInvitation",true,false)
	check(row!=null and row.get_meta("decision_id")=="membership_offer:39" and row.get_meta("decision_source_fact_id")=="fact:membership:39","提示绑定实际邀请与原已知事实")
	assert_read_only(f); dispose(f)

## 他人、终态、到期、缺来源与无名行都不能因展示awaiting_input而泄漏。
func excluded_rows() -> void:
	var rows: Array=[]
	for fault in ["other","terminal","expired","missing_source","missing_name"]:
		var row: Dictionary=invitation(); row.id=fault; row.display_name="不应显示"+fault
		if fault=="other": row.actor_id="actor.secret"
		elif fault=="terminal": row.status="accepted"
		elif fault=="expired": row.expires_tick=3
		elif fault=="missing_source": row.source_fact_id=""
		else: row.display_name=""
		rows.append(row)
	rows.append("not-a-row")
	var f: Dictionary=fixture(rows); var text: String=visible_text(f.main)
	check(f.main.find_child("ActivityPendingDecisions",true,false)==null and not text.contains("不应显示") and not text.contains("秘密人物"),"只显示本人已知且仍需决定的邀请")
	check(text.contains("请先处理当前输入"),"其它合法awaiting_input保留原通用提示，不虚构任职原因")
	assert_read_only(f); dispose(f)

## 原事实没有可用地点名时明确未知，不能用目录中远地名称或邀请人去向填空。
func unknown_place() -> void:
	var row: Dictionary=invitation(); row.known_place_id="place.secret"; row.known_place_name=""
	var f: Dictionary=fixture([row]); var text: String=visible_text(f.main)
	check(text.contains("办理地点尚不清楚") and not text.contains("未获知密室"),"未知地点不从静态目录推断")
	check(text.contains("许主事") and text.contains("找到原邀请人"),"已知邀请人仍可见，办理仍沿原人物事项")
	assert_read_only(f); dispose(f)

## 不同停止原因、旧来源、旧保存协议或运行中都不展示不属于当前停点的详情。
func source_and_stop_guards() -> void:
	for option in [{"stale_view":true},{"stale_state":true},{"legacy":true},{"stop":"blocked"},{"running":true}]:
		var f: Dictionary=fixture([invitation()],option)
		check(f.main.find_child("ActivityPendingDecisions",true,false)==null and not visible_text(f.main).contains("许主事"),"当前来源/停点门："+str(option))
		assert_read_only(f); dispose(f)

## 行旅与休整复用只读邀请说明，不假称这些原行动存在课程暂停或恢复接口。
func other_action_paths() -> void:
	for kind in ["travel","long_rest"]:
		var f: Dictionary=fixture([invitation()],{"action_kind":kind}); var text: String=visible_text(f.main)
		check(text.contains("许主事") and text.contains("原邀请办理地点"),kind+"也能看见本人已知决定")
		check(not text.contains("暂停课程") and not text.contains("恢复原课程"),kind+"不借修业按钮描述不存在的功能")
		assert_read_only(f); dispose(f)

## 释放测试跳过启动的音频及存储成员，不留下孤儿影响退出证据。
func dispose(f: Dictionary) -> void:
	var main=f.main; main._resident_mode=false; main._resident_adapter.auto_running=false
	main._ambience.music.free(); main._ambience.effect.free(); main._ambience.free()
	main.journey_view.free(); main.storage.free(); main._ui_font_scale.free(); main.free()

## 固定引擎只报告本次有限DTO与实际控件范围，旧区域矩阵和真实GUI不在本门内。
func _run() -> void:
	known_invitation(); excluded_rows(); unknown_place(); source_and_stop_guards(); other_action_paths()
	await process_frame
	print("ACTIVITY_DECISION_GUIDANCE_CHECKS=",checks," FAILURES=",failures," SCOPE=actual Main/Backend/continuation controls with explicit finite transport fixture; no native world or GUI acceptance")
	quit(0 if failures==0 else 1)

## 引擎与场景树初始化后再运行真实控件生命周期。
func _initialize() -> void: call_deferred("_run")
