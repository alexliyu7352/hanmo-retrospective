## 战略军令注册的真实回调、事务回滚和旧源码轨迹；不把六命令局部验收当作全军事迁移。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const F=preload("res://tests/companion_fixture.gd")
const Location=preload("res://core/world_location.gd")
var checks: int=0
var failures: int=0
var content_path: String=""
var write_trace: String=""
var compare_trace: String=""
var trace: Array=[]
var guards: Array=[]
var mustered: Dictionary={}
var moving: Dictionary={}

## 探针只包装真实发行Callable；故障在真正写入之后发生，不替换规则成功路径。
class TestKernel extends "res://core/world_kernel.gd":
	var calls: Dictionary={}
	var mutation: String=""
	var target_kind: String=""
	var failure: String=""
	var mutated: bool=false
	var journey_mutated: bool=false
	## 逐实际军令包装回调，缺命令与错误gate仍交生产启动锚拒绝。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for row in rows:
			if row.id=="tasks" and mutation=="foreign_gate":
				for command in row.commands:
					if command.kind==target_kind: command.gate_id="campaign_travel_redirect"; command.preview=foreign_preview
			if row.id!="military": continue
			for command in row.commands.duplicate():
				if command.kind==target_kind and mutation=="asset_gate": command.gate_id="campaign_travel_redirect"
				if not command.kind in Campaign.KINDS: continue
				if command.kind==target_kind and mutation=="missing": row.commands.erase(command); continue
				if command.kind==target_kind and mutation=="gate": command.gate_id="unaccepted_gate"
				command.preview=observed_preview.bind(command.preview)
				command.apply=observed_apply.bind(command.apply)
		return rows
	## 其他模块误写保留gate名时必须仍被公共busy拦住，哨兵只用于证明没有越过。
	func foreign_preview(_state: Dictionary,_data: Dictionary,_actor: String,_kind: String,_params: Dictionary) -> Dictionary:
		calls.foreign_gate=calls.get("foreign_gate",0)+1
		return {"ok":false,"code":"WrongDomainGateEntered"}
	## 重复纯预览仍调用唯一真实报价，并记录真实命中而非descriptor存在。
	func observed_preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,run: Callable) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return run.call(state,data,actor,kind,params)
	## 原apply先实际改军令或公粮，再可注入硬失败验证候选丢弃。
	func observed_apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary,run: Callable) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=run.call(state,data,actor,kind,params,offer)
		mutated=state!=before
		return {"ok":false,"code":"InjectedCampaignApply"} if failure=="apply" and result.ok else result
	## 原行旅已经占用真人并改变空间后才硬失败；规则不能提交半个军令。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		return {"ok":false,"code":"InjectedCampaignJourney"} if kind=="travel" and failure=="journey" and result.ok else result
	## 在共同协调出口之后注错，覆盖余程已经替换及最终Schema拒绝的整候选回滚。
	func _apply(state: Dictionary,kind: String,params: Dictionary,actor: String,offer: Dictionary) -> Dictionary:
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=super._apply(state,kind,params,actor,offer)
		if kind in Campaign.KINDS and result.ok:
			var previous_id: String=before.actors[actor].action_id
			if not previous_id.is_empty() and before.actions.has(previous_id) and state.actions.has(previous_id): journey_mutated=before.actions[previous_id].params!=state.actions[previous_id].params
			if failure=="coordinated": mutated=state!=before; return {"ok":false,"code":"InjectedCampaignCoordination"}
			if failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result

## 明确传入旧轨迹路径；写期望不能覆盖已有文件，也不自动生成当前代码的假基线。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): content_path=arg.trim_prefix("--military-content=")
		if arg.begins_with("--write-trace="): write_trace=arg.trim_prefix("--write-trace=")
		if arg.begins_with("--compare-trace="): compare_trace=arg.trim_prefix("--compare-trace=")
	call_deferred("run_checks")

## 所有失败都决定退出码，不能静默跳过关键场景后报告零失败。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 全部起点使用真实作者军事目录；不得直接注入军团、行程或军需余额。
func fresh(mutation: String="",target_kind: String=""):
	var k=TestKernel.new(); k.mutation=mutation; k.target_kind=target_kind
	if not check(not content_path.is_empty() and k.new_game("scenario.alpha_life",content_path).ok,"正式目录创建战略测试世界"): return null
	return k

## 通过正式Codec搬运已经实际产生的稳定状态；恢复失败不能继续依赖流程。
func restored(state: Dictionary):
	var k=fresh()
	if k==null: return null
	var bytes: String=Codec.encode(state,0,"registered-campaign",k.content())
	var decoded: Dictionary=Codec.decode(bytes,k.content())
	if not check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==state,"实际Codec恢复完整原世界"): return null
	return k

## 输入沿正式请求序列与当前revision建立，失败重试复用原输入以验证无部分提交。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var s: Dictionary=k.snapshot(); var actor: String=s.player_actor_id
	return {"kind":kind,"params":params.duplicate(true),"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision}

## 保存完整输入、结果、状态与HM-SAVE字节；没有字段归一化或资产容差。
func record(k,chain: String,label: String,input: Dictionary,result: Dictionary,resume: bool=false) -> void:
	var state: Dictionary=k.snapshot()
	trace.append({"chain":chain,"label":label,"input":input.duplicate(true),"result":result.duplicate(true),"state":state,"save_bytes":Codec.encode(state,0,"registered-campaign",k.content()),"resume":resume})

## 真实submit前重复预览，成功后相同请求重放必须保持完整世界与收据。
func send(k,kind: String,params: Dictionary={},chain: String="main",resume: bool=false) -> bool:
	var before: Dictionary=k.snapshot(); var quote: Dictionary=k.preview(kind,params)
	if not check(k.preview(kind,params)==quote and k.snapshot()==before,"重复纯预览 "+chain+"/"+kind): return false
	var input: Dictionary=request(k,kind,params); var result: Dictionary=k.submit(input)
	if not check(result.ok,"真实命令 "+chain+"/"+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot()
	if not check(k.submit(input)==result and k.snapshot()==after,"相同请求完整幂等 "+chain+"/"+kind): return false
	record(k,chain,kind,input,result,resume)
	return true

## 沿原地图有限推进到城口；每半日也保存完整独立旧轨迹。
func arrive(k,city: String) -> bool:
	for _half in range(40):
		var state: Dictionary=k.snapshot()
		if Location.resolve(state,state.player_actor_id).get("settlement_id")==city: return true
		if not send(k,"advance"): return false
	return check(false,"真实军旅行程在有限半日内抵达 "+city)

## 真实任官、受令、装运、交付、回营和报告；主线之外从实际行军状态分别返程与撤令。
func journey() -> bool:
	var k=fresh()
	if k==null: return false
	if not send(k,"office_accept",{"office_id":"office.military.a","context":F.context(k,"role.military.a")}) or not send(k,"advance"): return false
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":F.context(k,"role.military.a")}): return false
	var order: String=k.snapshot().campaigns.keys()[0]
	if not send(k,"army_form",{"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":160,"money":100}) or not send(k,"advance"): return false
	mustered=k.snapshot()
	if not send(k,"campaign_depart",{"campaign_id":order},"main",true) or not send(k,"advance",{},"main",true): return false
	moving=k.snapshot()
	if not check(not moving.actors[moving.player_actor_id].action_id.is_empty(),"代表节点仍在真实军旅行程中"): return false
	if not arrive(k,"village.east") or not send(k,"campaign_deliver",{"campaign_id":order}): return false
	var delivered: Dictionary=k.snapshot(); var repeated: Dictionary=k.submit(request(k,"campaign_deliver",{"campaign_id":order}))
	if not check(not repeated.ok and k.snapshot()==delivered,"不同请求也不能重复交货"): return false
	if not send(k,"campaign_return",{"campaign_id":order},"main",true) or not arrive(k,"city.west"): return false
	var army: String=k.snapshot().campaigns[order].army_id
	if not send(k,"army_return",{"army_id":army}) or not send(k,"advance"): return false
	if not send(k,"campaign_report",{"campaign_id":order,"context":F.context(k,"role.military.a")}): return false
	if write_trace.is_empty():
		for kind in Campaign.KINDS:
			if kind=="campaign_cancel": continue
			check(k.calls.get(kind+":preview",0)>0 and k.calls.get(kind+":apply",0)>0,"实际注册回调命中 "+kind)
	for kind in ["campaign_return","campaign_cancel"]:
		var branch=restored(moving)
		if branch==null: return false
		var action_id: String=moving.actors[moving.player_actor_id].action_id
		record(branch,kind,"restored",{}, {"ok":true})
		if not send(branch,kind,{"campaign_id":order},kind,true): return false
		check(branch.snapshot().actors[moving.player_actor_id].action_id==action_id,"途中重定向保留原行旅行动 "+kind)
		if not send(branch,"advance",{},kind): return false
		if write_trace.is_empty(): check(branch.calls.get(kind+":preview",0)>0 and branch.calls.get(kind+":apply",0)>0,"旅途守卫也走注册回调 "+kind)
	return true

## 直接比较已实际形成状态的公共守卫；缺能力的途中特例保持原错误优先级。
func guard_vectors() -> void:
	for state_name in ["mustered","moving"]:
		var k=restored(mustered if state_name=="mustered" else moving)
		if k==null: return
		var original: Dictionary=k._catalog.data
		for capability in [true,false]:
			k._catalog.data=original.duplicate(true)
			if not capability: k._catalog.data.required_capabilities.erase("military")
			for kind in Campaign.KINDS:
				var state: Dictionary=k.snapshot(); var order: String=state.campaigns.keys()[0]
				var params: Dictionary={"campaign_id":order,"template_id":"order.military.a","context":F.context(k,"role.military.a")}
				var result: Dictionary=k._preview(state,kind,params,state.player_actor_id)
				check(state==k.snapshot(),"守卫报价不写实际稳定状态")
				guards.append({"state":state_name,"military_capability":capability,"kind":kind,"result":result})
				if state_name=="moving" and not kind in ["campaign_return","campaign_cancel"]: check(result.get("code")=="ActorBusy","非重定向命令仍在busy后 "+kind)
				elif not capability and state_name=="mustered": check(result.get("code")=="MilitaryUnavailable","正常路径能力先于领域 "+kind)
				elif state_name=="moving": check(result.ok,"真实途中返程不被公共busy或新增能力门拒绝 "+kind)
				check(k._preview(state,kind,params,"actor.missing").get("code")=="UnknownActor","未知人物仍最先拒绝 "+kind)
		k._catalog.data=original
	var busy=restored(mustered)
	if busy!=null and check(busy.submit(request(busy,"wait")).ok,"真实等待占用夹具"):
		for kind in ["campaign_return","campaign_cancel"]: check(busy.preview(kind,{"campaign_id":mustered.campaigns.keys()[0]}).get("code")=="ActorBusy","非travel占用不能绕行 "+kind)

## 每个原领域kind缺失或gate变质都必须被实际new_game拒绝，并保留原世界与旧registry。
func coverage() -> void:
	for kind in Campaign.KINDS:
		for mutation in ["missing","gate"]:
			var k=fresh()
			if k==null: return
			var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
			k.mutation=mutation; k.target_kind=kind
			var result: Dictionary=k.new_game("scenario.alpha_life",content_path)
			check(result.get("code")==("MissingBuiltinCommand" if mutation=="missing" else "InvalidBuiltinCommand") and k.snapshot()==before and k.content()==catalog,"真实启动锚拒绝 "+mutation+"/"+kind)
			check(k.preview("office_accept",{"office_id":"office.military.a","context":F.context(k,"role.military.a")}).ok,"失败启动保留已接受注册表")

	for kind in Assets.KINDS:
		var k=fresh()
		if k==null: return
		var before: Dictionary=k.snapshot(); k.mutation="asset_gate"; k.target_kind=kind
		check(k.new_game("scenario.alpha_life",content_path).get("code")=="InvalidBuiltinCommand" and k.snapshot()==before,"人产误标战略gate启动拒绝 "+kind)
	var foreign=fresh()
	if foreign!=null:
		var before: Dictionary=foreign.snapshot(); var accepted: Dictionary=foreign._registry.duplicate(true)
		foreign.mutation="foreign_gate"; foreign.target_kind="cancel_task"
		check(foreign.new_game("scenario.alpha_life",content_path).get("code")=="InvalidBuiltinCommand" and foreign.snapshot()==before and foreign._registry==accepted,"默认完整命令锚先拒绝其他owner错gate且保留原世界")
		foreign.mutation=""
		var decoded: Dictionary=Codec.decode(Codec.encode(moving,0,"registered-campaign",foreign.content()),foreign.content())
		if check(decoded.ok and foreign.restore(decoded.state).ok,"其他模块gate误标夹具恢复真实行军"):
			# 先合法启动和恢复，再模拟受信注册被误写，以单独覆盖运行期owner守卫而不绕开启动验收。
			foreign._registry.commands.cancel_task.gate_id="campaign_travel_redirect"
			foreign._registry.commands.cancel_task.preview=foreign.foreign_preview
			check(foreign.preview("cancel_task",{}).get("code")=="ActorBusy" and foreign.calls.get("foreign_gate",0)==0,"其他owner同名gate不能提前调用报价")

## 军令与真实行旅均改变之后的失败必须回滚；重试同一请求不能新增第二行程。
func rollback() -> void:
	for mode in ["apply","journey","coordinated","tail"]:
		var k=restored(mustered)
		if k==null: return
		var before: Dictionary=k.snapshot(); var input: Dictionary=request(k,"campaign_depart",{"campaign_id":before.campaigns.keys()[0]})
		k.failure=mode
		var failed: Dictionary=k.submit(input)
		check(not failed.ok and k.snapshot()==before,"出征 "+mode+" 失败回滚完整状态/历史/序列")
		if mode=="apply" or mode=="coordinated": check(k.mutated,"故障发生在真实领域写入之后 "+mode)
		k.failure=""
		var retried: Dictionary=k.submit(input); var after: Dictionary=k.snapshot()
		var action_id: String=after.actors[after.player_actor_id].action_id
		check(retried.ok and after.actions.size()==before.actions.size()+1 and after.actions.has(action_id) and after.actions[action_id].kind=="travel" and after.actions[action_id].actor_id==after.player_actor_id and after.player_actor_id in after.actions[action_id].participants,"原请求重试只为统帅新增一个行程，不漏计既有NPC行动 "+mode)
		for id in before.actions: check(after.actions.get(id)==before.actions[id],"即时军令重试保持原NPC行动 "+id)
		check(k.submit(input)==retried and k.snapshot()==after,"成功重试的同一请求再重放仍完整幂等 "+mode)
	for kind in ["campaign_return","campaign_cancel"]:
		var k=restored(moving)
		if k==null: return
		var before: Dictionary=k.snapshot(); var input: Dictionary=request(k,kind,{"campaign_id":before.campaigns.keys()[0]})
		k.failure="coordinated"
		check(not k.submit(input).ok and k.mutated and k.journey_mutated and k.snapshot()==before,"途中余程已替换后故障仍回滚 "+kind)
		k.failure=""
		check(k.submit(input).ok and k.snapshot().actors[before.player_actor_id].action_id==before.actors[before.player_actor_id].action_id,"重试不分裂已有行程 "+kind)

## 用旧进程实际HM-SAVE恢复标记节点，并重放其下一输入，完整结果和状态须与旧轨迹一致。
func restore_old(rows: Array) -> void:
	var covered: Array=[]
	for index in range(rows.size()-1):
		var row: Dictionary=rows[index]; var next: Dictionary=rows[index+1]
		if not row.resume or row.chain!=next.chain or next.input.is_empty(): continue
		var k=fresh()
		if k==null: return
		var decoded: Dictionary=Codec.decode(row.save_bytes,k.content())
		if not check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==row.state,"旧进程真实存档恢复 "+row.chain+"/"+row.label): continue
		var result: Dictionary=k.submit(next.input)
		check(result==next.result and k.snapshot()==next.state,"旧存档下一步精确延续 "+row.chain+"/"+row.label)
		covered.append(row.chain+"/"+row.label)
	check(covered.size()>=4,"去程、返程与撤令至少四个真实待办点延续")

## 捕获仅用于冻结旧源码；新注册模式同时验证旧对照与新增错误路径，不默认为全量通过。
func run_checks() -> void:
	if not check(journey(),"六命令代表流程不能静默跳过"): finish(); return
	guard_vectors()
	var current: Dictionary={"trace":trace,"guards":guards}
	if not write_trace.is_empty():
		if not check(not FileAccess.file_exists(write_trace) and not FileAccess.file_exists(write_trace+".meta.json"),"旧期望不允许覆盖"): finish(); return
		var file=FileAccess.open(write_trace,FileAccess.WRITE)
		if check(file!=null,"旧轨迹文件可写"): file.store_string(JSON.stringify(current,"",true,true)); file.close()
		var meta=FileAccess.open(write_trace+".meta.json",FileAccess.WRITE)
		if check(meta!=null,"旧进程身份可写"): meta.store_string(JSON.stringify({"pid":OS.get_process_id(),"script_sha256":FileAccess.get_file_as_string("res://tests/test_registered_campaign.gd").sha256_text()})); meta.close()
		finish(); return
	if not compare_trace.is_empty():
		var old: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(compare_trace),67108864)
		if check(old.ok,"独立旧轨迹能解析"):
			check(old.value==current,"旧源码与注册新路的完整状态/结果/存档/守卫相等")
			restore_old(old.value.trace)
		var meta: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(compare_trace+".meta.json"))
		check(meta.ok and meta.value.pid!=OS.get_process_id() and meta.value.script_sha256==FileAccess.get_file_as_string("res://tests/test_registered_campaign.gd").sha256_text(),"同一脚本来自独立旧引擎进程")
	coverage(); rollback(); finish()

## 只报告当前实际执行断言，缺少旧轨迹参数时不宣称独立兼容对照已运行。
func finish() -> void:
	print("REGISTERED_CAMPAIGN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
