## M0命令上下文首片：核纯标量隔离及真实Kernel提交/冷恢复；不冒称原生后端或场景按钮接线已完成。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const LEAF="res://core/backends/legacy_command_context.gd"
var Context
var checks: int=0
var failures: int=0

## 只统计真实门面调用，所有返回值仍来自原Kernel，不用替身提供成功规则。
class ObservedKernel extends "res://core/world_kernel.gd":
	var context_calls: int=0
	var snapshot_calls: int=0
	var commands: Array=[]
	## 查询继续进入原生产委托，计数只用于确认Main已停止取命令用全快照。
	func command_context(actor_id: String="") -> Dictionary:
		context_calls+=1
		return super.command_context(actor_id)
	## 结果收尾仍允许原有快照，不能把新增门面夸大成整个UI已去快照。
	func snapshot() -> Dictionary:
		snapshot_calls+=1
		return super.snapshot()
	## 捕获的是提交前原请求副本，真正执行与失败均交原submit。
	func submit(value: Dictionary) -> Dictionary:
		commands.append(value.duplicate(true))
		return super.submit(value)
	## 每个入口案例独立清空计数，不重置任何世界字段。
	func reset_observations() -> void:
		context_calls=0; snapshot_calls=0; commands=[]

## 只关闭本测试不验收的画面重建；实际_perform和_apply_command_result完全使用生产方法。
class QuietMain extends "res://ui/main.gd":
	var renders: int=0
	## 无GUI门只计重绘请求，不加载大美术或启动音频与存储。
	func _render() -> void:
		renders+=1

## 延迟加载新叶，缺实现时产生明确断言而非解析失败。
func _init() -> void:
	call_deferred("_run")

## 每次真实断言都计数，失败必须导致非零退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只构造本查询读取的原字段，附带不可被读取过程推进的随机/实例快照。
func fixture() -> Dictionary:
	return {"player_actor_id":"actor.player","actors":{"actor.player":{"alive":false,"available":false,"action_id":"busy"},"actor.npc":{}},"revision":7,"request_sequences":{"actor.player":2,"actor.npc":8},"tick":13,"next_instance_id":91,"rng":{"world":"unchanged"}}

## 返回值只含标量且没有身份或领域状态副本；读取不能预留请求或判断人物忙碌。
func projection() -> void:
	check(Context.query({})=={"ok":false,"code":"NoWorld"},"尚未开局返回原NoWorld")
	var state: Dictionary=fixture(); var before: Dictionary=state.duplicate(true)
	var expected: Dictionary={"ok":true,"actor_id":"actor.player","revision":7,"next_request_seq":3}
	var first: Dictionary=Context.query(state)
	check(first==expected,"默认人物的精确四键标量投影，无伪world_id")
	check(Context.query(state)==first and state==before,"重复读取不分配序号、实例或随机")
	first.actor_id="forged"; first.revision=99; first.next_request_seq=100
	check(Context.query(state)==expected and state==before,"改返回值不写回世界，也不污染后续查询")
	check(Context.query(state,"actor.npc")=={"ok":true,"actor_id":"actor.npc","revision":7,"next_request_seq":9},"指定人物沿自身序号而非玩家序号")
	check(Context.query(state,"missing")=={"ok":false,"code":"UnknownActor"},"未知人物显式拒绝，不替换成玩家")
	state.player_actor_id="actor.npc"
	check(Context.query(state).actor_id=="actor.npc" and Context.query(state).next_request_seq==9,"当前玩家变更后不复用旧默认人物")
	state=fixture(); state.request_sequences["actor.player"]=2147483647
	check(Context.query(state).next_request_seq==2147483648,"合法序号上界仍按旧构造加一，实际命令裁决留给submit")
	state.revision=2147483647
	check(Context.query(state).revision==2147483647,"合法revision上界不被只读查询改写")

## 坏字段类型先拒绝再比较或相加，所有失败均保持输入原样。
func malformed() -> void:
	for field in ["player_actor_id","actors","revision","request_sequences"]:
		var state: Dictionary=fixture(); state.erase(field)
		var before: Dictionary=state.duplicate(true)
		check(Context.query(state)=={"ok":false,"code":"InvalidCommandContext"} and state==before,"缺查询字段结构化拒绝 "+field)
	for field in ["player_actor_id","actors","request_sequences"]:
		for value in [null,false,0,0.5,[],"bad"]:
			if field=="player_actor_id" and value is String: continue
			var state: Dictionary=fixture(); state[field]=value
			var before: Dictionary=state.duplicate(true)
			check(Context.query(state)=={"ok":false,"code":"InvalidCommandContext"} and state==before,"坏容器或人物类型结构化拒绝 "+field+"/"+str(value))
	for field in ["revision","sequence"]:
		for value in [null,false,true,0.0,0.5,"0",[],{},-1,2147483648]:
			var state: Dictionary=fixture()
			if field=="revision": state.revision=value
			else: state.request_sequences["actor.player"]=value
			var before: Dictionary=state.duplicate(true)
			check(Context.query(state)=={"ok":false,"code":"InvalidCommandContext"} and state==before,"坏计数不能混型运算 "+field+"/"+str(value))
	var state: Dictionary=fixture(); state.request_sequences.erase("actor.player")
	check(Context.query(state)=={"ok":false,"code":"InvalidCommandContext"},"人物缺自己的序号不可默认为0")
	state=fixture(); state.player_actor_id="missing"
	check(Context.query(state)=={"ok":false,"code":"UnknownActor"},"坏默认人物不可被当前任意actor替代")

## 所有正式命令字段均来自一次查询；不在真正submit前悄悄刷新旧revision。
func command(context: Dictionary, kind: String, params: Dictionary={}) -> Dictionary:
	return {"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}

## 原tiny作者经过正式prepare/stage；这里不用直造事实或绕过Schema的运行世界。
func fresh():
	var kernel=Kernel.new()
	var result: Dictionary=kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json")
	check(result.ok,"正式tiny目录真实开局 "+str(result))
	return kernel

## 真门面与旧快照构造逐字对照，命令失败/成功/重放/过期仍由既有提交入口裁决。
func kernel_contract() -> void:
	var empty=Kernel.new()
	if not check(empty.has_method("command_context"),"真实Kernel必须提供command_context，不用测试替身代接线"): return
	check(empty.command_context()=={"ok":false,"code":"NoWorld"},"真实未开局门面NoWorld")
	var kernel=fresh()
	if kernel.snapshot().is_empty(): return
	var before: Dictionary=kernel.snapshot(); var context: Dictionary=kernel.command_context()
	check(context=={"ok":true,"actor_id":before.player_actor_id,"revision":before.revision,"next_request_seq":before.request_sequences[before.player_actor_id]+1},"门面保持旧快照命令标量")
	check(kernel.command_context()==context and kernel.snapshot()==before,"门面重复查询保持完整状态")
	check(kernel.command_context("actor.chen").actor_id=="actor.chen","门面显式人物不会强换成玩家")
	check(kernel.command_context("missing").get("code")=="UnknownActor" and kernel.snapshot()==before,"门面坏人物不写入状态")
	context.revision=123; context.actor_id="forged"
	check(kernel.snapshot()==before,"门面结果由调用者改写也不影响状态")
	context=kernel.command_context()
	var invalid: Dictionary=kernel.submit(command(context,"move_local",{"place_id":"missing"}))
	check(not invalid.ok and kernel.snapshot()==before and kernel.command_context()==context,"真实失败命令保留世界与请求序号")
	var original: Dictionary=command(context,"move_local",{"place_id":"west.office"})
	var result: Dictionary=kernel.submit(original)
	if not check(result.ok,"原正式MoveLocal成功 "+str(result)): return
	var after: Dictionary=kernel.snapshot()
	check(after.tick==before.tick and after.revision==before.revision+1 and after.actors[before.player_actor_id].location.place_id=="west.office","原零半日移动与revision语义保持")
	check(kernel.command_context().next_request_seq==context.next_request_seq+1,"提交成功后才推进下一序号")
	check(kernel.submit(original)==result and kernel.snapshot()==after,"相同请求返回原结果且不再次执行")
	var stale: Dictionary=command(context,"move_local",{"place_id":"west.street"}); stale.request_seq+=1
	check(kernel.submit(stale).get("code")=="StalePreview" and kernel.snapshot()==after,"新序号配旧revision仍拒绝，不自动刷新")
	var upper: Dictionary=after.duplicate(true); upper.request_sequences[upper.player_actor_id]=2147483647
	check(kernel.submit(command(Context.query(upper),"move_local",{"place_id":"west.street"})).get("code")=="InvalidCommand" and kernel.snapshot()==after,"查询上界不抢原submit的InvalidCommand首错")
	var identity: Dictionary=kernel.command_context()
	check(not kernel.new_game("missing","res://data/generated/procurement.hmcatalog.json").ok and kernel.command_context()==identity and kernel.snapshot()==after,"失败新局保留当前门面来源")
	cold_restore(kernel)

## 用真实Codec与冷恢复注册表续接同一状态，未来命令结果逐字相同；不写Native文件或旧槽。
func cold_restore(kernel) -> void:
	var saved: Dictionary=kernel.save_context()
	var text: String=Codec.encode(saved.state,0,"m0-command-context",saved.catalog,saved.registry)
	if not check(not text.is_empty(),"原保存Codec编码实际世界"): return
	var decoded: Dictionary=Codec.decode(text,saved.catalog,saved.registry)
	if not check(decoded.ok,"原保存Codec解码 "+str(decoded.get("code",""))): return
	var restored=Kernel.new(); var prepared: Dictionary=restored.prepare_restore_registry()
	if not check(prepared.ok and restored.restore_with_catalog(decoded.state,saved.catalog,prepared.registry).ok,"不new_game的实际冷恢复"): return
	check(restored.command_context()==kernel.command_context() and restored.snapshot()==kernel.snapshot(),"冷恢复上下文和完整状态一致")
	var first: Dictionary=kernel.submit(command(kernel.command_context(),"move_local",{"place_id":"west.street"}))
	var second: Dictionary=restored.submit(command(restored.command_context(),"move_local",{"place_id":"west.street"}))
	check(first.ok and first==second and kernel.snapshot()==restored.snapshot(),"恢复后下一真实命令结果和状态完全相同")

## 实际Main留在场景树外，原自建Node收为子节点以完整释放；不运行_ready或触发存储读写。
func main_for(kernel) -> QuietMain:
	var host=QuietMain.new(); host.kernel=kernel
	host._started=not kernel.snapshot().is_empty()
	host._data=kernel.content().get("data",{})
	for node in [host.journey_view,host.storage,host._ui_font_scale,host._ambience]: host.add_child(node)
	host._ambience.add_child(host._ambience.music); host._ambience.add_child(host._ambience.effect)
	return host

## 调用真实Main入口，核原请求、短动作收尾、忙碌早返及无世界安全错误，不冒称OS图形输入验收。
func main_contract() -> void:
	var kernel=ObservedKernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"真实Main使用正式tiny开局"): return
	var host: QuietMain=main_for(kernel)
	print("M0_MAIN_PATHS ",JSON.stringify({"user_data_dir":OS.get_user_data_dir(),"computed_native_directory":host.storage.native.directory,"native_operations":0}))
	var before: Dictionary=kernel.snapshot(); kernel.reset_observations()
	var expected: Dictionary={"kind":"move_local","params":{"place_id":"west.office"},"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	host._perform("move_local",{"place_id":"west.office"})
	check(kernel.commands==[expected] and kernel.context_calls==1,"真实Main只读一次新上下文并提交原命令标量")
	check(kernel.snapshot_calls==1,"命令组装不再读全快照，原结果收尾仍有一次")
	check(kernel.snapshot().actors[before.player_actor_id].location.place_id=="west.office" and host._message.is_empty() and host.renders==1,"原成功结果收尾与实际位置生效")
	var after: Dictionary=kernel.snapshot(); kernel.reset_observations()
	host._perform("move_local",{"place_id":"missing"})
	check(kernel.context_calls==1 and kernel.commands.size()==1 and kernel.snapshot()==after and not host._message.is_empty(),"真实失败Main命令不消耗版本或序号并显示原因")
	host._scene_counter=7
	check(host._topic_request("role.office",before.player_actor_id,after.revision,6).get("code")=="StaleSceneRequest","未改岗位入口仍拒绝旧scene_epoch")
	check(host._topic_request("role.office",before.player_actor_id,before.revision,7).get("code")=="StalePreview","未改岗位入口仍拒绝旧revision，不刷新请求")
	for blocked in ["advance","storage"]:
		kernel.reset_observations(); host._advancing=blocked=="advance"; host._storage_intent="load" if blocked=="storage" else ""
		host._rest_auto=true; host.journey_view.auto_running=true
		host._perform("move_local",{"place_id":"west.street"})
		check(kernel.context_calls==0 and kernel.commands.is_empty() and kernel.snapshot_calls==0,"原忙碌或存储早返不取上下文 "+blocked)
		check(not host._rest_auto and not host.journey_view.auto_running,"原非advance先停止自动授权的顺序保留 "+blocked)
	host.free()
	var empty=ObservedKernel.new(); var title: QuietMain=main_for(empty); empty.reset_observations()
	title._perform("wait")
	check(empty.context_calls==1 and empty.commands.is_empty() and empty.snapshot_calls==0 and title._message.contains("NoWorld") and title.renders==1,"无世界查询错误只显示返回，不进入解引用世界的结果收尾")
	title.free()

## 纯叶模式明确标注，默认总门必须包含真实Kernel，缺接线不能减项称通过。
func _run() -> void:
	if check(ResourceLoader.exists(LEAF),"命令上下文纯投影叶存在"):
		Context=load(LEAF)
		projection(); malformed()
		if not "--pure-only" in OS.get_cmdline_user_args(): kernel_contract(); main_contract()
	print("WORLD_COMMAND_CONTEXT_", "PURE" if "--pure-only" in OS.get_cmdline_user_args() else "KERNEL", "_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
