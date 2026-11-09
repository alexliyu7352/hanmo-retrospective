## 岗位话题M0门面：实际Main与Kernel保留旧按钮身份、首错和预览版本，不运行GUI/存档矩阵。
extends "res://tests/test_world_command_context.gd"
var reported_paths: bool=false

## 跟踪真实生产入口；唯一故障开关只篡改成功预览的版本，专验第二次版本守卫。
class TopicKernel extends "res://core/world_kernel.gd":
	var contexts: Array=[]
	var snapshots: int=0
	var previews: Array=[]
	var commands: Array=[]
	var stale_quote: bool=false
	## 记录默认/指定人物参数，实际内容仍由原门面投影。
	func command_context(actor_id: String="") -> Dictionary:
		contexts.append(actor_id)
		return super.command_context(actor_id)
	## 区分命令组装与原结果尾部的快照，不改世界或返回值。
	func snapshot() -> Dictionary:
		snapshots+=1
		return super.snapshot()
	## 所有合法性仍来自实际preview；故障案例明确模拟成功报价的旧版本标记。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		previews.append({"kind":kind,"params":params.duplicate(true),"actor_id":actor_id})
		var result: Dictionary=super.preview(kind,params,actor_id)
		if stale_quote and result.ok: result.revision+=1
		return result
	## 保留原完整命令后仍调用生产submit，不替测试提供成功结果。
	func submit(value: Dictionary) -> Dictionary:
		commands.append(value.duplicate(true))
		return super.submit(value)
	## 清空观察记录不重置世界；故障开关由对应案例显式设置。
	func reset_observations() -> void:
		contexts=[]; snapshots=0; previews=[]; commands=[]

## 正式tiny世界经真实MoveLocal到官署，不手造交谈事实或替换占用。
func topic_fixture(at_office: bool=true) -> Dictionary:
	var kernel=TopicKernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"岗位测试正式tiny开局"): return {}
	if at_office:
		var moved: Dictionary=kernel.submit(command(kernel.command_context(),"move_local",{"place_id":"west.office"}))
		if not check(moved.ok,"真实移动到岗位所在地"): return {}
	var host=main_for(kernel); host._scene_counter=17
	if not reported_paths:
		print("M0_TOPIC_PATHS ",JSON.stringify({"user_data_dir":OS.get_user_data_dir(),"computed_native_directory":host.storage.native.directory,"native_operations":0}))
		reported_paths=true
	host._rest_auto=true; host.journey_view.auto_running=true
	var state: Dictionary=kernel.snapshot(); kernel.reset_observations()
	return {"kernel":kernel,"host":host,"before":state,"actor":state.player_actor_id,"revision":state.revision,"epoch":17}

## 每个拒绝保持世界、序号和原自动授权；观察只在计数核验后读取一次完整状态。
func rejected(f: Dictionary, result: Dictionary, code: String, query_count: int, preview_count: int, label: String) -> void:
	check(result.get("code")==code and not result.ok,label+"：原错误优先级")
	check(f.kernel.contexts==([""] if query_count==1 else []) and f.kernel.snapshots==0,label+"：默认玩家查询且无命令快照")
	check(f.kernel.previews.size()==preview_count and f.kernel.commands.is_empty(),label+"：守卫失败不提交")
	check(f.kernel.snapshot()==f.before and f.host._rest_auto and f.host.journey_view.auto_running,label+"：世界和自动授权不变")

## 实际岗位交谈沿同一命令进入真实会话；随后再点原按钮不能刷新旧revision。
func accepted_topic() -> void:
	var f: Dictionary=topic_fixture()
	if f.is_empty(): return
	var expected: Dictionary={"kind":"talk","params":{"role_id":"role.office"},"actor_id":f.actor,"request_seq":f.before.request_sequences[f.actor]+1,"expected_revision":f.revision}
	var result: Dictionary=f.host._topic_request("role.office",f.actor,f.revision,f.epoch)
	check(result.ok and result.get("kind")=="talk","真实岗位交谈提交成功")
	check(f.kernel.contexts==[""] and f.kernel.snapshots==1,"成功只用默认玩家context，保留结果尾部一次快照")
	check(f.kernel.previews==[{"kind":"talk","params":{"role_id":"role.office"},"actor_id":f.actor}] and f.kernel.commands==[expected],"预览及提交保持原人物/参数/版本，只有序号来自context")
	var after: Dictionary=f.kernel.snapshot()
	check(after.revision==f.revision+1 and after.tick==f.before.tick and after.request_sequences[f.actor]==expected.request_seq and after.interactions.has(f.actor),"真实会话与请求记账生效，不额外推进半日")
	check(not f.host._rest_auto and not f.host.journey_view.auto_running,"成功后仍停止自动授权")
	f.before=after; f.host._rest_auto=true; f.host.journey_view.auto_running=true; f.kernel.reset_observations()
	rejected(f,f.host._topic_request("role.office",f.actor,f.revision,f.epoch),"StalePreview",1,0,"原按钮重复点击")
	f.host.free()

## 页面和忙碌首门先于人物/版本，即使其它字段也过期仍保持原首错。
func early_guards() -> void:
	for reason in ["not_started","busy","storage","epoch"]:
		var f: Dictionary=topic_fixture()
		if f.is_empty(): continue
		if reason=="not_started": f.host._started=false
		if reason=="busy": f.host._advancing=true
		if reason=="storage": f.host._storage_intent="save"
		var epoch: int=f.epoch-1 if reason=="epoch" else f.epoch
		rejected(f,f.host._topic_request("role.office","actor.chen",-1,epoch),"StaleSceneRequest",0,0,reason)
		f.host.free()

## 即使NPC在真实世界存在也不是玩家按钮主体；人物错先于版本错，不询问按钮指定者的上下文。
func identity_and_revision() -> void:
	for actor_id in ["actor.chen","missing"]:
		var f: Dictionary=topic_fixture()
		if f.is_empty(): continue
		rejected(f,f.host._topic_request("role.office",actor_id,-1,f.epoch),"StaleSceneRequest",1,0,"冒充人物 "+actor_id)
		f.host.free()
	var f: Dictionary=topic_fixture()
	if f.is_empty(): return
	rejected(f,f.host._topic_request("role.office",f.actor,f.revision-1,f.epoch),"StalePreview",1,0,"玩家旧版本")
	f.host.free()

## 真实岗位预览拒绝直接传回，成功但版本错误也必须在停止自动操作与submit之前退出。
func preview_guards() -> void:
	for row in [[true,"missing"],[false,"role.office"]]:
		var f: Dictionary=topic_fixture(row[0])
		if f.is_empty(): continue
		var quote: Dictionary=f.kernel.preview("talk",{"role_id":row[1]},f.actor)
		check(not quote.ok,"实际岗位预览产生失败 "+str(quote.get("code","")))
		f.kernel.reset_observations()
		var result: Dictionary=f.host._topic_request(row[1],f.actor,f.revision,f.epoch)
		check(result==quote,"Main逐字保留原岗位报价失败")
		rejected(f,result,quote.get("code",""),1,1,"原岗位拒绝 "+str(row))
		f.host.free()
	var f: Dictionary=topic_fixture()
	if f.is_empty(): return
	f.kernel.stale_quote=true
	rejected(f,f.host._topic_request("role.office",f.actor,f.revision,f.epoch),"StalePreview",1,1,"实际成功预览的版本故障探针")
	f.host.free()

## 本门不调用父类存读检查，只执行本片岗位入口与失败合同；零断言或诊断不算通过。
func _run() -> void:
	accepted_topic(); early_guards(); identity_and_revision(); preview_guards()
	print("M0_TOPIC_COMMAND_CONTEXT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
