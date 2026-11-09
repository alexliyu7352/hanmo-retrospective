## 有限旅行适配的窄门：只核输入裁剪、全批绑定、候选整数与失败，不替代独立原算法oracle。
extends SceneTree
const PROVIDER_PATH="res://core/backends/native_travel_schedule_provider.gd"
var checks: int=0
var failures: int=0
var provider_script: Script

## 仅模拟同步传输形状，不实现旅行算法；记录scope字符串以核适配调用时序。
class ProbeBridge extends RefCounted:
	var identity: Dictionary={}
	var scopes: Array=[]
	var malformed: bool=false
	## 接收真实有限编码并回显绑定；坏根用于观察适配是否明确传播失败。
	func propose_travel_schedule(bytes: Variant) -> Variant:
		var request: Dictionary=JSON.parse_string(bytes.get_string_from_utf8())
		scopes.append(request.scope_token)
		if malformed: return []
		return {"ok":true,"format":"HM-NATIVE-TRAVEL-PROPOSALS/1","scope_token":request.scope_token,"evaluated_count":request.actors.size(),"proposals":[],"build":identity.duplicate(true)}

## 延迟执行使所有断言都能打印并按失败数退出。
func _init() -> void: call_deferred("run")

## 每项边界保留明确说明，任一失败使进程返回非零。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 固定身份仅供输出绑定测试；真实编译身份由独立原生集成门提供。
func build() -> Dictionary:
	return {"profile":"native_travel_schedule","source_commit":"a".repeat(40),"source_sha256":"b".repeat(64),"core_source_sha256":"c".repeat(64),"dependency_lock_sha256":"d".repeat(64),"godot_cpp_commit":"e".repeat(40),"api_sha256":"f".repeat(64),"trimmed_api_sha256":"1".repeat(64),"interface_sha256":"2".repeat(64),"profile_sha256":"3".repeat(64),"compiler":"fixture","target":"Linux-x86_64","cpp_standard":"17","godot_target":"template_debug","cmake_configuration":"Debug","precision":"single","real_t_bits":32,"pointer_bits":64,"builtin_configuration":"float_64"}

## 所有计划人物均应进入请求，资格筛选留原生；不向适配夹具注入完整作者世界。
func source() -> Dictionary:
	var state: Dictionary={"tick":2147483648,"player_actor_id":"a_player","actors":{},"itineraries":{},"parties":{},"interactions":{},"wallets":{"secret":{"balance":999}},"facts":{"unrelated":{"private":"never-wire"}}}
	var data: Dictionary={"actors":{},"settlements":{"west":{"entry_place":"west.gate","private":"never-wire"}},"private":"never-wire"}
	for id in ["a_player","b_busy","c_dead","d_missing","e_ready"]:
		state.actors[id]={"action_id":"busy" if id=="b_busy" else "","alive":id!="c_dead","available":true,"location":{"kind":"Settlement","settlement_id":"west","place_id":"west.gate","private":"never-wire"},"private":"never-wire"}
		data.actors[id]={"travel_plan":{"destinations":["west","east","east"],"dwell_ticks":1,"private":"never-wire"}}
		if id!="d_missing": state.itineraries[id]={"index":0,"ready_tick":2147483649,"travelling":false,"private":"never-wire"}
	state.parties={"irrelevant":{"members":["outsider"]},"relevant":{"members":["e_ready","outsider"],"secret":4}}
	state.interactions={"outsider":{"context":{"provider_actor_id":"e_ready","private":"never-wire"},"dialogue":"never-wire"},"a_player":{"context":{"provider_actor_id":"other"}},"irrelevant":{"context":{"provider_actor_id":"other"},"private":"never-wire"}}
	return {"state":state,"data":data}

## 输出只描述已有请求中的人物、目的地及游标，故意不重算原算法资格。
func result(request: Dictionary) -> Dictionary:
	return {"ok":true,"format":"HM-NATIVE-TRAVEL-PROPOSALS/1","scope_token":request.scope_token,"evaluated_count":request.actors.size(),"build":build(),"proposals":[{"actor_id":"b_busy","kind":"travel","params":{"destination":"east"},"next_index":2},{"actor_id":"e_ready","kind":"travel","params":{"destination":"west"},"next_index":1}]}

## 适配不筛资格、不保存源、只保协议路径，十进制字符串覆盖完整int64而非世界MAX_TICK。
func input_checks() -> Dictionary:
	var input: Dictionary=source(); var before: Dictionary=input.duplicate(true)
	var collected: Dictionary=provider_script.collect_request(input.state,input.data,"1")
	if not check(collected.get("ok",false),"有限候选请求生成"): return {}
	var request: Dictionary=collected.request
	check(input==before,"适配读取不修改世界或作者数据")
	check(request.keys().size()==7 and request.actors.size()==5,"只传协议根与全部原授权计划人物")
	check(request.tick=="2147483648" and request.actors[0].itinerary.ready_tick=="2147483649","候选tick与ready_tick可超过已提交MAX_TICK")
	check(request.actors[3].itinerary==null,"缺行程不提前过滤")
	check(request.actors[1].action_id=="busy" and request.actors[2].alive==false,"忙碌死亡不提前过滤")
	check(request.parties==[{"id":"relevant","members":["e_ready","outsider"]}],"仅保相关队伍及其真实完整成员顺序")
	check(request.interactions==[{"actor_id":"outsider","provider_actor_id":"e_ready"},{"actor_id":"a_player","provider_actor_id":"other"}],"仅保相关会话key与provider事实")
	check(not collected.bytes.get_string_from_utf8().contains("never-wire") and not collected.bytes.get_string_from_utf8().contains("wallets"),"全世界及开放extra未进入wire")
	check(request.actors[0].destinations==["west","east","east"],"原目的地重复与顺序不改写")
	request.actors[0].destinations.clear(); request.parties[0].members.clear()
	check(input==before,"输出深改不别名原作者或成员")
	input.state.tick=9223372036854775807; input.state.itineraries.a_player.index=-9223372036854775808
	input.state.itineraries.a_player.ready_tick=9223372036854775807
	var extremes: Dictionary=provider_script.collect_request(input.state,input.data,"2")
	check(extremes.get("ok",false) and extremes.request.tick=="9223372036854775807" and extremes.request.actors[0].itinerary.index=="-9223372036854775808" and extremes.request.actors[0].itinerary.ready_tick=="9223372036854775807","完整int64字符串无JSON精度丢失或提前索引门")
	return provider_script.collect_request(before.state,before.data,"bound").request

## 后项畸形、顺序、引用和绑定均在返回首项前失败；输出只复制完整通过的纯提案。
func output_checks(request: Dictionary) -> void:
	var good: Dictionary=result(request); var wanted: Dictionary=good.duplicate(true)
	var accepted: Dictionary=provider_script.validate_result(good,request,build())
	check(accepted.get("ok",false) and accepted.proposals==good.proposals,"完整输出通过，不在GD重复资格算法")
	if accepted.get("ok",false): accepted.proposals[0].params.destination="changed"
	check(good==wanted,"返回不别名原生返回值")
	for mode in ["scope","count","float_count","format","build","build_type","partial_build","array","late","order","duplicate","unknown","missing_itinerary","destination","index","negative_bound","float_index","extra","params_extra","failure_partial"]:
		var bad: Dictionary=good.duplicate(true)
		match mode:
			"scope": bad.scope_token="other"
			"count": bad.evaluated_count-=1
			"float_count": bad.evaluated_count=float(bad.evaluated_count)
			"format": bad.format="wrong"
			"build": bad.build.source_commit="9".repeat(40)
			"build_type": bad.build.pointer_bits=64.0
			"partial_build": bad.build.erase("compiler")
			"array": bad.proposals={}
			"late": bad.proposals[1].kind="battle"
			"order": bad.proposals.reverse()
			"duplicate": bad.proposals[1]=bad.proposals[0].duplicate(true)
			"unknown": bad.proposals[1].actor_id="outside"
			"missing_itinerary": bad.proposals[1].actor_id="d_missing"
			"destination": bad.proposals[1].params.destination="outside"
			"index": bad.proposals[1].next_index=3
			"negative_bound": bad.proposals[1].next_index=-3
			"float_index": bad.proposals[1].next_index=1.0
			"extra": bad.proposals[1].extra="ignored?"
			"params_extra": bad.proposals[1].params.extra="ignored?"
			"failure_partial": bad.ok=false; bad.code="NativeTravelInjected"; bad.path="/injected"
		var rejected: Dictionary=provider_script.validate_result(bad,request,build())
		check(not rejected.get("ok",true) and rejected.get("code","").begins_with("NativeTravel") and not rejected.has("proposals"),"全批失败无部分提案："+mode)
	var negative: Dictionary=good.duplicate(true); negative.proposals[0].next_index=-2
	check(provider_script.validate_result(negative,request,build()).get("ok",false),"保Godot可访问的候选负索引，交旧尾Schema")
	var singleton: Dictionary=request.duplicate(true); singleton.actors[1].destinations=["east"]
	var single_result: Dictionary=good.duplicate(true); single_result.proposals=single_result.proposals.slice(0,1); single_result.proposals[0].next_index=-1
	check(not provider_script.validate_result(single_result,singleton,build()).get("ok",true),"单目的地next_index负1永非余数，整批拒绝")
	single_result.proposals[0].next_index=0
	check(provider_script.validate_result(single_result,singleton,build()).get("ok",false),"单目的地唯一合法next_index为0")
	var native_failure: Dictionary={"ok":false,"code":"NativeTravelDestinationIndex","path":"/actors/0/itinerary/index"}
	check(provider_script.validate_result(native_failure,request,build())==native_failure,"明确原生失败保码与路径")
	check(not provider_script.validate_result(good,request,{}).get("ok",true),"空build配置不接受任意成功")
	var minimum: Dictionary={}
	for field in ["profile","source_commit","source_sha256","core_source_sha256","dependency_lock_sha256","api_sha256"]: minimum[field]=build()[field]
	check(provider_script.validate_result(good,request,minimum).get("ok",false),"最小六键身份可绑定更完整的真实build")
	for value in [null,[],true,42,"not-a-result"]:
		check(provider_script.validate_result(value,request,minimum).get("code")=="NativeTravelInvalidResult","畸形原生根不能假成功："+str(typeof(value)))

## 旧Godot运行时证明负索引及同号余数语义；候选未过Schema，适配不可提前变窄。
func negative_index_checks() -> void:
	var input: Dictionary=source(); var state: Dictionary=input.state; var data: Dictionary=input.data
	state.player_actor_id="a_player"; state.interactions={}; state.parties={}
	state.itineraries.e_ready={"index":-2,"ready_tick":0,"travelling":false}
	var schedule: Script=load("res://core/travel_schedule.gd")
	var original: Array=schedule.proposals(state,data)
	check(original==[{"actor_id":"e_ready","kind":"travel","params":{"destination":"east"},"next_index":-1}],"原Schedule实测负数组index保留负next_index")
	var modulo_source: int=-2
	check(modulo_source%3==-2,"Godot负余数与C++同号余数一致")

## 真无扩展环境失败明确，错误配置均不借旧算法返回空数组。
func invocation_checks() -> void:
	var input: Dictionary=source(); var adapter=provider_script.new()
	check(adapter.propose(input.state,input.data).get("code")=="NativeTravelBuildConfiguration","默认无身份配置不能调用")
	adapter.configure({})
	check(adapter.propose(input.state,input.data).get("code")=="NativeTravelBuildConfiguration","错误配置调用时失败")
	var identity: Dictionary=build(); adapter.configure(identity); identity.clear()
	check(adapter.propose(input.state,input.data).get("code")=="NativeTravelBridgeUnavailable","缺真实扩展失败明确且配置已复制")

## 同步调用不缓存上次提案，配置变化释放Bridge但不能重置实例内scope。
func scope_checks() -> void:
	var input: Dictionary=source(); var adapter=provider_script.new(); var bridge=ProbeBridge.new()
	bridge.identity=build(); adapter.configure(build()); adapter._bridge=bridge
	check(adapter.propose(input.state,input.data)=={"ok":true,"proposals":[]},"真实适配同步收编完整成功批")
	check(adapter.propose(input.state,input.data).get("ok",false) and bridge.scopes==["1","2"],"每半日独立调用且scope单调，空批不缓存")
	adapter.configure(build())
	check(adapter._bridge==null,"显式重配置不复用旧Bridge")
	adapter._bridge=bridge
	check(adapter.propose(input.state,input.data).get("ok",false) and bridge.scopes==["1","2","3"],"重配置不回退本实例序号")
	bridge.malformed=true
	check(adapter.propose(input.state,input.data).get("code")=="NativeTravelInvalidResult","真实适配坏根不回退旧算法")
	bridge.malformed=false
	check(adapter.propose(input.state,input.data).get("ok",false) and bridge.scopes==["1","2","3","4","5"],"失败后重试走新的同步scope")
	adapter._call_seq=9223372036854775807
	check(adapter.propose(input.state,input.data).get("code")=="NativeTravelScopeExhausted" and bridge.scopes.size()==5,"序号耗尽明确失败而不溢出或调用Bridge")
	var missing=provider_script.new(); missing.configure(build()); missing._bridge=RefCounted.new()
	check(missing.propose(input.state,input.data).get("code")=="NativeTravelBridgeUnavailable","有对象但错API仍明确失败")

## 可绑定对象只用于验证Callable的真实引用释放，不参与任何旅行或原生计算。
func ownership_probe(_held: RefCounted) -> void: pass

## 独立调用帧释放Godot的Callable临时寄存器，再让外层观察真实弱引用。
func configure_bound_callable(adapter: RefCounted) -> Dictionary:
	var held: RefCounted=RefCounted.new(); var weak: WeakRef=weakref(held)
	var bad: Dictionary=build(); bad.compiler=Callable(self,"ownership_probe").bind(held)
	var expected: Dictionary=provider_script._check_build_configuration(bad)
	adapter.configure(bad)
	return {"weak":weak,"expected":expected}

## 错误配置也不能把对象或Callable跨调用持久化；错误码和路径保持原验证的首错。
func configuration_ownership_checks() -> void:
	var input: Dictionary=source(); var adapter=provider_script.new()
	var held: RefCounted=RefCounted.new(); var weak: WeakRef=weakref(held)
	var bad: Dictionary=build(); bad.unexpected={"nested":[held]}
	var expected: Dictionary=provider_script._check_build_configuration(bad)
	adapter.configure(bad); bad.clear(); held=null
	check(weak.get_ref()==null,"错误配置嵌套Object在调用configure后释放，provider不持世界引用")
	check(adapter.propose(input.state,input.data)==expected and adapter._call_seq==0,"对象配置仍在原首点返回同码同路径，不调用原生或消耗scope")
	var returned: Dictionary=adapter.propose(input.state,input.data); returned.path="mutated"
	check(adapter.propose(input.state,input.data)==expected,"保存的纯值配置错误不会被调用方改写")
	var callable_case: Dictionary=configure_bound_callable(adapter)
	check(callable_case.weak.get_ref()==null,"错误配置Callable绑定的RefCounted实际释放，不持久保存Callable")
	check(adapter.propose(input.state,input.data)==callable_case.expected and adapter._call_seq==0,"Callable配置错误先于缺库且保原首错")
	held=RefCounted.new(); weak=weakref(held); bad=build(); bad.source_commit=held
	expected=provider_script._check_build_configuration(bad)
	adapter.configure(bad); bad.clear(); held=null
	check(weak.get_ref()==null and adapter.propose(input.state,input.data)==expected,"必需身份字段的Object也不持有，并保精确字段错误路径")
	var bridge=ProbeBridge.new(); bridge.identity=build(); adapter.configure(build()); adapter._bridge=bridge
	check(adapter.propose(input.state,input.data)=={"ok":true,"proposals":[]} and bridge.scopes==["1"],"坏配置后恢复合法配置，原成功语义及scope不变")

## 不依赖预加载缺失资源，在尚无实现时得到可读的预期红门。
func run() -> void:
	if check(ResourceLoader.exists(PROVIDER_PATH),"已实现可加载provider叶"):
		provider_script=load(PROVIDER_PATH)
		var request: Dictionary=input_checks()
		if not request.is_empty(): output_checks(request)
		negative_index_checks()
		invocation_checks()
		scope_checks()
		configuration_ownership_checks()
	print("NATIVE_TRAVEL_ADAPTER_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
