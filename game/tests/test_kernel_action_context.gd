## 真Main控制时序oracle及现有采购单步真链；前者明确使用出口替身，后者经完整Schema，不含存读验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Policy=preload("res://ui/activity_policy.gd")
const Schema=preload("res://core/state_schema.gd")
const ORACLE="f767d6a8c1c1bd8f2b429d280899cff4ac1f1a9d7ba32924e0eac54625d12376"
const CASES=["apply_failure","apply_short","apply_manual","apply_manual_done","apply_advance_poison","apply_plan","apply_trade","finish_present","finish_empty","map_empty","map_quote","map_confirm","toggle_blocked","toggle_running_empty","toggle_resume","toggle_denied","step_off","step_busy_poison","step_storage_poison","step_empty_poison","step_blocked","step_advance","rest_off","rest_wrong","rest_start","process_off","process_busy","process_continue","process_complete","process_failure"]
var checks: int=0
var failures: int=0

## 规则出口只记录确定的测试结果；新查询委托同backend真实Kernel，缺API不伪造成功。
class ProbeKernel extends "res://core/world_kernel.gd":
	var events: Array=[]
	var reads: int=0
	var receipt_reads: int=0
	var basic_reads: int=0
	var manual_reads: int=0
	var deny: bool=false
	var change_on_preview: bool=false
	var advance_mode: String="keep"
	var trade_case: bool=false
	## 原snapshot读点归一为current；成功trade新增的专属收据读取独立计数而不假称消除。
	func snapshot() -> Dictionary:
		reads+=1
		if trade_case and basic_reads>0: receipt_reads+=1
		else: events.append("current")
		return super.snapshot()
	## 基础读取计数后调用真实新门面，不能预读两个窗口。
	func action_context() -> Dictionary:
		basic_reads+=1; events.append("current")
		var actual=load("res://core/world_kernel.gd").new(); actual._backend=_backend
		return actual.call("action_context") if actual.has_method("action_context") else {"ok":false,"code":"ProbeMissingActionContext"}
	## 保原needs查询发生时点；实际值交唯一核心实现。
	func action_needs_input() -> bool:
		events.append("needs")
		var actual=load("res://core/world_kernel.gd").new(); actual._backend=_backend
		return actual.call("action_needs_input") if actual.has_method("action_needs_input") else false
	## manual次数另验；原静态UI调用不伪造记录，原不可触碰facts另用反例锁定。
	func action_manual_window() -> bool:
		manual_reads+=1
		var actual=load("res://core/world_kernel.gd").new(); actual._backend=_backend
		return actual.call("action_manual_window") if actual.has_method("action_manual_window") else false
	## 报价替身只观察命令种类；反例在第一次报价后换行动以锁定confirm第二次新读取。
	func preview(kind: String,params: Dictionary={},actor_id: String="") -> Dictionary:
		events.append(["preview",kind,params.duplicate(true),actor_id])
		if change_on_preview: _state.actions.A.kind="work"; change_on_preview=false
		return {"ok":not deny,"code":"ProbeDenied" if deny else "","revision":_state.revision}
	## 明确模拟规则已返回的结果和后状态，验证Main读取在advance前后，而非测试领域推进。
	func advance_one_tick() -> Dictionary:
		events.append("advance")
		if advance_mode=="failure": return {"ok":false,"code":"ProbeDenied"}
		_state.tick+=1; _state.revision+=1
		if advance_mode=="complete": _state.actors.p.action_id=""
		if advance_mode=="replace": _state.actions.A.kind="travel"
		return {"ok":true,"details":{"completed_actions":{"A":{"success":true}}}}

## 保留真实本地自动授权行为，只观察启动/停止/再次调度，不创建额外世界时钟。
class ProbeJourney extends "res://ui/journey_view.gd":
	var events: Array=[]
	## 启动仍由原JourneyView设置标志并单次调度。
	func start_auto() -> void: events.append("start_auto"); super.start_auto()
	## 停止仍清原授权，不补发任何世界步骤。
	func stop_auto() -> void: events.append("stop_auto"); super.stop_auto()
	## 离树测试没有实际Timer；这里只核调度请求时点。
	func schedule_next() -> void: events.append("schedule"); super.schedule_next()

## 实际Main逻辑离树运行；仅重绘/命令出口替身，便于精确观察原条件与调用顺序。
class ProbeMain extends "res://ui/main.gd":
	var events: Array=[]
	## 不运行整屏渲染或其额外快照。
	func _render() -> void: events.append("render")
	## 命令仍保原种类/参数；是否支付或推进由其他领域门覆盖。
	func _perform(kind: String,params: Dictionary={}) -> void:
		events.append(["perform",kind,params.duplicate(true)])
		if kind=="resume_travel": kernel._state.actions.A.params.paused=false
		if kind=="pause_travel" and not kernel._state.actors.p.action_id.is_empty(): kernel._state.actions.A.params.paused=true
	## 原state型helper计数，仍调用原真实领域判定。
	func _travel_needs_input(state: Dictionary) -> bool:
		events.append("needs"); return super._travel_needs_input(state)

## 短行动排队门只确认原deferred被安排，实际_finish_short另以未覆盖的ProbeMain直接验证。
class DeferredMain extends ProbeMain:
	## 防止等待帧时把排队测试变成第二次领域推进。
	func _finish_short() -> void: events.append("deferred")

## 等场景树消息队列可用后执行。
func _init() -> void: call_deferred("_run")

## 每个失败参与退出码；引擎诊断独立保留。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只提供控制器实际读取字段；poison容器用于证明旧短路未读取它们，不冒充完整世界。
func fixture(kind: String) -> Dictionary:
	var events: Array=[]; var main=DeferredMain.new() if kind.begins_with("apply_") else ProbeMain.new(); var kernel=ProbeKernel.new()
	kernel.events=events; main.events=events
	kernel._state={"player_actor_id":"p","revision":7,"tick":0,"actors":{"p":{"id":"p","action_id":"A"}},"actions":{"A":{"id":"A","kind":"travel","actor_id":"p","params":{"paused":false}}},"facts":{},"battles":{},"armies":{},"sieges":{},"encounters":{}}
	main.kernel=kernel; main._started=true; main.journey_view.free(); main.journey_view=ProbeJourney.new(); main.journey_view.events=events
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	if kind in ["apply_short","finish_present","apply_trade","apply_plan"]: kernel._state.actions.A.kind="work"
	if kind in ["apply_manual","apply_manual_done","apply_advance_poison"]:
		kernel._state.actions.A.kind="study_skill"
		kernel._state.facts.A={"id":"A","phase":"started","actor_id":"p","terms":{"challenge":{"mode":"manual"}},"challenge":{"phase":"closed" if kind=="apply_manual_done" else "choosing"}}
	if kind=="apply_advance_poison": kernel._state.facts=false
	if kind in ["finish_empty","toggle_running_empty","step_empty_poison"]: kernel._state.actors.p.action_id=""
	if kind in ["toggle_blocked","step_blocked"]: kernel._state.battles.b={"kind":"personal","phase":"result","units":{"p":{}}}
	if kind in ["toggle_resume","toggle_denied"]: kernel._state.actions.A.params.paused=true
	if kind=="toggle_denied": kernel.deny=true
	if kind=="map_confirm": kernel.change_on_preview=true
	if kind in ["map_quote","map_confirm"]: main.journey_view.selected="destination"
	if kind=="toggle_running_empty" or kind.begins_with("step_"): main.journey_view.auto_running=true
	if kind=="step_off": main.journey_view.auto_running=false
	if kind=="step_busy_poison": main._advancing=true
	if kind=="step_storage_poison": main._storage_intent="save"
	if kind.begins_with("step_") and kind.ends_with("poison"): kernel._state.battles=false; kernel._state.sieges=false; kernel._state.encounters=false
	if kind.begins_with("rest_") or kind.begins_with("process_"): kernel._state.actions.A.kind="long_rest"
	if kind=="rest_off": main._rest_auto=true
	if kind=="rest_wrong": kernel._state.actions.A.kind="work"
	if kind.begins_with("process_"): main._rest_auto=kind!="process_off"
	if kind=="process_busy": main._storage_intent="save"
	if kind=="process_complete": kernel.advance_mode="complete"
	if kind=="process_failure": kernel.advance_mode="failure"
	if kind=="apply_plan": kernel.advance_mode="replace"
	if kind=="apply_trade": kernel.trade_case=true; kernel._state.actions.A.params.receipt={"fixture":"old receipt"}
	return {"main":main,"kernel":kernel,"events":events}

## 每个case执行真正目标方法；只过滤新增加且另断言的trade展示专属读取。
func run_case(kind: String,checking: bool) -> Dictionary:
	var f: Dictionary=fixture(kind); var main=f.main; var kernel=f.kernel
	if kind.begins_with("apply_"):
		var command: String={"apply_failure":"work","apply_advance_poison":"advance","apply_plan":"military_plan","apply_trade":"trade"}.get(kind,"work")
		main._apply_command_result(command,{"ok":kind!="apply_failure","code":"ProbeDenied","details":{"action_id":"A"}})
		await process_frame
	elif kind.begins_with("finish_"): main._finish_short()
	elif kind.begins_with("map_"):
		if kind=="map_confirm": main._map_confirm()
		else: main._map_quote()
	elif kind.begins_with("toggle_"): main._toggle_auto_travel()
	elif kind.begins_with("step_"): main._auto_step()
	elif kind.begins_with("rest_"): main._toggle_rest_auto()
	else: main._process(.21)
	if checking:
		check(kernel.reads==(1 if kind=="apply_trade" else 0),"仅成功trade展示保snapshot "+kind+"/"+str(kernel.reads))
		check(kernel.manual_reads==(1 if kind in ["apply_short","apply_manual","apply_manual_done","apply_trade"] else 0),"manual只在原短路条件通过后读取 "+kind)
	var result: Dictionary={"events":f.events.duplicate(true),"state":kernel._state.duplicate(true),"auto":main.journey_view.auto_running,"rest":main._rest_auto,"advancing":main._advancing,"message":main._message,"receipt":main._trade_receipt.duplicate(true),"selected":main.journey_view.selected,"via":main.journey_view.via.duplicate(),"refresh":main._rest_refresh_elapsed}
	main.free(); return result

## 新基础读取只接触所需字段；两个窗口单独运行并保原短路与手动题面语义。
func leaf_checks() -> void:
	var source=load("res://core/views/action_context.gd")
	check(source.query({})=={"ok":false,"code":"NoWorld"},"基础空世界拒绝")
	var f: Dictionary=fixture("step_busy_poison"); var state: Dictionary=f.kernel._state
	state.facts=false
	var before: Dictionary=state.duplicate(true); seed(822); var next_random: int=randi(); seed(822)
	var value: Dictionary=source.query(state); var keys: Array=value.keys(); keys.sort()
	check(value.ok and keys==["action_id","action_kind","actor_id","ok","revision","travel_paused"],"基础六键不预计算窗口")
	check(value.action_kind=="travel" and not value.travel_paused and state==before and randi()==next_random,"原未触碰battle/fact容器仍不读且无副作用")
	value.action_kind="forged"; check(source.query(state).action_kind=="travel","基础返回值隔离")
	state.actions.A.actor_id="q"; state.actions.A.participants=["q","p"]
	check(source.query(state).ok and source.query(state).actor_id=="p" and source.travelling(state),"共享行动读本人占用，不把发起人当唯一参与者")
	state.actions.A.id="different"
	check(source.query(state)=={"ok":false,"code":"InvalidActionContext"},"当前行动ID绑定仍检查")
	state.actors.p.action_id=""; state.actions=false
	check(source.query(state).ok and source.query(state).action_kind=="","空闲不读actions容器")
	f.main.free()
	for cause in ["personal","military","siege","encounter"]:
		f=fixture("step_advance"); state=f.kernel._state
		if cause in ["personal","military"]:
			state.battles.b={"kind":cause,"phase":"result","units":{"p":{}},"participant_actor_ids":["p"]}
			state.sieges=false; state.encounters=false
		elif cause=="siege":
			state.armies.G={"status":"active","member_actor_ids":["p"]}
			state.sieges.S={"phase":"planning","armies":{"G":{}},"withdrawn_army_ids":[]}; state.encounters=false
		else: state.encounters.E={"status":"pending","participants":["p"]}
		before=state.duplicate(true)
		check(source.needs_input(state) and state==before,"按需输入原顺序/占用边界 "+cause)
		f.main.free()
	f=fixture("apply_manual_done"); state=f.kernel._state
	check(source.manual_window(state,"p") and Policy.manual_window(state,"p"),"题面完成仍保手动课窗口，UI兼容同算法")
	state.facts.A.phase="completed"; check(not source.manual_window(state,"p"),"结课后旧事实不挡未来时钟")
	f.main.free()

## 使用真实当前序号提交现有正式采购命令；NPC提交仍经过同一核心资格及交易边界。
func send_actual(kernel,kind: String,params: Dictionary={},actor_id: String="") -> Dictionary:
	var context: Dictionary=kernel.command_context(actor_id)
	if not context.ok: return context
	return kernel.submit({"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})

## 现有作者内容的实际accept_offer写者与完整Schema；仅换合法当前岗位任职，不造新活动或存读协议。
func actual_chain_checks() -> void:
	for player_is_provider in [false,true]:
		var label: String="玩家经办" if not player_is_provider else "玩家为非发起参与者"
		var kernel=Kernel.new()
		if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"真实采购启动 "+label): continue
		if not check(send_actual(kernel,"move_local",{"place_id":"west.office"}).ok,"真实入署 "+label): continue
		var player: String=kernel._state.player_actor_id
		var initiator: String="actor.chen" if player_is_provider else player
		var provider: String=player if player_is_provider else "actor.chen"
		if player_is_provider:
			var assigned: Dictionary=kernel._state.duplicate(true); assigned.roles["role.office"].actor_id=player
			if not check(Schema.validate(assigned,kernel._catalog,kernel._registry).ok,"完整Schema允许当前玩家任职岗位"): continue
			kernel._state=assigned
		if not check(send_actual(kernel,"inspect",{"prop_id":"notice.grain"},initiator).ok,"真实来源发现 "+label): continue
		var accepted: Dictionary=send_actual(kernel,"accept_offer",{"offer_id":"offer.grain","context":{"place_id":"west.office","provider_actor_id":provider,"role_id":"role.office"}},initiator)
		if not check(accepted.ok,"真实共享行动提交 "+label+str(accepted)): continue
		var before: Dictionary=kernel._state.duplicate(true); var action_id: String=before.actors[player].action_id
		check(Schema.validate(before,kernel._catalog,kernel._registry).ok and before.actions[action_id].actor_id==initiator and player in before.actions[action_id].participants,"真实writer与完整占用Schema "+label)
		var source=load("res://core/views/action_context.gd"); var value: Dictionary=source.query(before)
		check(value.ok and value.actor_id==player and value.action_id==action_id and value.action_kind=="accept_offer","真实共享行动有限查询 "+label)
		if not kernel.has_method("action_context"): continue
		seed(392); var expected_random: int=randi(); seed(392)
		check(kernel.call("action_context")==value and not kernel.call("action_needs_input") and not kernel.call("action_manual_window") and kernel._state==before and randi()==expected_random,"真实三门面无改写与RNG副作用 "+label)
		var f: Dictionary=fixture("finish_present"); f.main.kernel=kernel; f.main._advancing=true
		f.main._finish_short()
		var after: Dictionary=kernel._state
		check(after.tick==before.tick+1 and after.actors[player].action_id.is_empty() and after.actors[initiator].action_id.is_empty() and after.tasks.values()[0].status=="active","真实Main短行动只推进一次并释放所有参与者 "+label)
		check(kernel.call("action_context").action_id=="" and not f.main._advancing and f.events==["render"],"真实短行动结束后的当前读取及重绘 "+label)
		f.main.free()

## 摘要固定真实旧Main控制结果/顺序，正常执行不重录。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 先完整记录原方法，再核新同源读取；被毒化的未触碰容器若提前读取会产生引擎错误而失败。
func _run() -> void:
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var outputs: Dictionary={}
	for kind in CASES: outputs[kind]=await run_case(kind,capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE)
		file.store_string(JSON.stringify({"main_sha256":FileAccess.get_sha256("res://ui/main.gd"),"policy_sha256":FileAccess.get_sha256("res://ui/activity_policy.gd"),"sha256":digest(outputs),"outputs":outputs},"\t",true,true)+"\n"); file.close()
		print("OLD_ACTION_CONTROL_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE,"原Main命令/读点/调度/状态完整oracle保持")
		for name in ["action_context","action_needs_input","action_manual_window"]: check(Kernel.new().has_method(name),"正式Kernel按需入口 "+name)
		if FileAccess.file_exists("res://core/views/action_context.gd"): leaf_checks()
		actual_chain_checks()
	print("KERNEL_ACTION_CONTEXT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
