## 真实声明包work开始/等待/完成的v2跨进程恢复；状态只来自正式Kernel与保存恢复链。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Strict=preload("res://core/strict_json.gd")
var mode: String=""
var evidence: String=""
var checks: int=0
var failures: int=0
var fixture: Dictionary={}
var observations: Dictionary={}

## 显式证据目录由独立准备器提供，不读取正式玩家槽或修改主工程。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--mode="): mode=arg.trim_prefix("--mode=")
		if arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=")
	call_deferred("run")

## 每条断言失败均计入退出码，不以写出日志或档案代替真实通过。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 保留正式失败对象，调用方须停止依赖该结果的后续步骤。
func require_ok(result: Dictionary, label: String) -> bool:
	check(result.get("ok",false),label)
	if not result.get("ok",false): printerr(result)
	return result.get("ok",false)

## 只读准备器或无恢复对照写出的JSON，不把这些期望值直接注入世界。
func read_json(name: String) -> Dictionary:
	return Strict.parse_text(FileAccess.get_file_as_string(evidence.path_join(name)),Codec.MAX_BYTES).get("value",{})

## 只写独立证据目录；这些新生成档案不是历史黄金，也不纳入源码提交。
func write_text(name: String, text: String) -> void:
	var file=FileAccess.open(evidence.path_join(name),FileAccess.WRITE)
	check(file!=null,"独立证据可写："+name)
	if file!=null: file.store_string(text); file.close()

## 保存原始规范JSON便于另一进程比较完整state/catalog，不只比余额或几个计数。
func write_json(name: String, value: Dictionary) -> void:
	write_text(name,JSON.stringify(value,"",true,true))

## 构造当前合法网关请求；重试时另外使用原请求原字节，不重新生成序号。
func request_for(kernel, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":player,"request_seq":state.request_sequences[player]+1,"expected_revision":state.revision}

## 每个阶段保存真实v2与独立对照state/catalog，记录实际tick和remaining而非假定自动推进语义。
func save_point(kernel, name: String, generation: int, contract: Dictionary) -> void:
	var context: Dictionary=kernel.save_context()
	var text: String=Codec.encode(context.state,generation,"activity-"+name,context.catalog,context.registry)
	var metadata: Dictionary=Codec.inspect_metadata(text)
	check(not text.is_empty() and metadata.get("ok",false) and metadata.get("metadata",{}).get("format_version")==2,"阶段真实完整v2编码："+name)
	write_text(name+".hmsave",text)
	write_json(name+"-expected.json",{"state":context.state,"catalog":context.catalog,"save_sha256":text.sha256_text()})
	var wallet: Dictionary=context.state.wallets[contract.provider_wallet]
	observations[name]={"tick":context.state.tick,"remaining_ticks":context.state.actions.get(contract.action_id,{}).get("remaining_ticks",0),"reserved":wallet.reserved.get(contract.action_id,0),"next_instance_id":context.state.next_instance_id,"rng":context.state.rng}

## 活动过程中成本仅扣一次、工资保持真实预留；完成后唯一转账并释放双方占用。
func assert_activity(state: Dictionary, contract: Dictionary, completed: bool) -> void:
	var player: Dictionary=state.wallets[contract.player_wallet]
	var provider: Dictionary=state.wallets[contract.provider_wallet]
	var expected_wage: int=fixture.wage if completed else 0
	check(player.balance==contract.player_balance-fixture.cost+expected_wage,"玩家实际余额只含一次成本/工资")
	check(provider.balance==contract.provider_balance+fixture.cost-expected_wage,"雇主实际余额承担同一笔工资")
	check(player.balance+provider.balance==contract.player_balance+contract.provider_balance,"真实资金总额守恒")
	if completed:
		check(not state.actions.has(contract.action_id) and not provider.reserved.has(contract.action_id),"完成只释放该真实行动及工资预留")
		check(state.actors[contract.player].action_id.is_empty() and state.actors[contract.provider].action_id.is_empty(),"完成后双方占用释放")
	else:
		check(state.actions.has(contract.action_id) and provider.reserved.get(contract.action_id)==fixture.wage,"未完成工资按真实action.id全额预留")
		check(state.actors[contract.player].action_id==contract.action_id and state.actors[contract.provider].action_id==contract.action_id,"未完成双方仍属于同一行动")

## 无恢复对照只执行一次短work；保存三个真实时点以及最后请求供独立进程重放幂等检查。
func create_reference(catalog: Dictionary) -> void:
	var kernel=Kernel.new()
	if not require_ok(kernel.new_game(fixture.scenario,catalog.catalog_path),"正式包目录真实Kernel新局"): finish(); return
	var before: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	var context: Dictionary=Interaction.for_role(before,data,fixture.role)
	var params: Dictionary={"activity":"work","context":context}
	var quote: Dictionary=kernel.preview("service",params)
	if not require_ok(quote,"真实注册service报价"): finish(); return
	check(quote.duration==fixture.duration and quote.cost==fixture.cost and quote.wage==fixture.wage,"作者3 tick/7成本/80工资进入正式规则")
	check(kernel.snapshot()==before,"报价不改state/RNG")
	var begin_request: Dictionary=request_for(kernel,"service",params)
	var begin_response: Dictionary=kernel.submit(begin_request)
	if not require_ok(begin_response,"真实提交开始work"): finish(); return
	var started: Dictionary=kernel.snapshot(); var action_id: String=started.actors[started.player_actor_id].action_id
	check(not action_id.is_empty() and started.actions.has(action_id),"开始后有真实未完成行动")
	if action_id.is_empty() or not started.actions.has(action_id): finish(); return
	var terms: Dictionary=started.actions[action_id].params.terms
	var contract: Dictionary={"player":started.player_actor_id,"provider":context.provider_actor_id,"player_wallet":before.actors[before.player_actor_id].wallet_id,"provider_wallet":terms.wallet_id,"action_id":action_id,"player_balance":before.wallets[before.actors[before.player_actor_id].wallet_id].balance,"provider_balance":before.wallets[terms.wallet_id].balance,"begin_request":begin_request,"begin_response":begin_response,"start_tick":started.tick,"start_remaining":started.actions[action_id].remaining_ticks}
	check(contract.start_remaining>=2 and contract.start_remaining<=fixture.duration,"真实开始后余时足以覆盖独立等待点")
	assert_activity(started,contract,false)
	save_point(kernel,"start",1,contract)
	if not require_ok(kernel.advance_one_tick(),"无恢复对照等待一个实际tick"): finish(); return
	var waiting: Dictionary=kernel.snapshot()
	check(waiting.tick==started.tick+1 and waiting.actions.has(action_id) and waiting.actions.get(action_id,{}).get("remaining_ticks")==contract.start_remaining-1,"等待点保留动作并按实际tick推进")
	assert_activity(waiting,contract,false)
	save_point(kernel,"waiting",2,contract)
	for unused in range(fixture.duration):
		if not kernel.snapshot().actions.has(action_id): break
		contract.completion_request=request_for(kernel,"advance")
		contract.completion_response=kernel.submit(contract.completion_request)
		if not require_ok(contract.completion_response,"无恢复对照有界完成"): finish(); return
	var completed: Dictionary=kernel.snapshot()
	check(not completed.actions.has(action_id) and completed.tick==contract.start_tick+contract.start_remaining,"实际总余时结束后恰好完成")
	assert_activity(completed,contract,true)
	save_point(kernel,"completed",3,contract)
	write_json("contract.json",contract)
	check(kernel.submit(contract.begin_request)==contract.begin_response and kernel.submit(contract.completion_request)==contract.completion_response,"完成后原开始/最终推进请求返回原结果")
	check(kernel.snapshot()==completed,"无恢复对照重复请求不双结算、不增加ID或消耗RNG")
	finish()

## 冷进程按metadata→Resolver→selection→Snapshot一次→Codec→Kernel终验恢复，不建立默认世界。
func restore_point(installed: Dictionary) -> void:
	var expected: Dictionary=read_json(mode+"-expected.json")
	var contract: Dictionary=read_json("contract.json")
	var text: String=FileAccess.get_file_as_string(evidence.path_join(mode+".hmsave"))
	check(text.sha256_text()==expected.save_sha256,"读取当前阶段原v2文件SHA一致")
	var kernel=Kernel.new()
	check(kernel.snapshot().is_empty() and kernel.content().is_empty(),"独立冷进程没有默认新局")
	var metadata: Dictionary=Codec.inspect_metadata(text)
	if not require_ok(metadata,"正式metadata物理封装检查"): finish(); return
	var prepared: Dictionary=kernel.prepare_restore_registry()
	if not require_ok(prepared,"正式Kernel受信registry准备"): finish(); return
	var matched: Dictionary=Resolver.resolve(metadata.metadata,installed.catalogs,prepared.registry)
	if not require_ok(matched,"同一原metadata精确选择installed catalog"): finish(); return
	var selection: Dictionary=Codec.inspect_restore_selection(text)
	if not require_ok(selection,"正式正文候选founder选择"): finish(); return
	var effective: Dictionary=Snapshot.resolve(matched.catalog,selection.scenario_id,selection.founder_actor_id)
	if not require_ok(effective,"原目录单次Snapshot有效视图"): finish(); return
	var decoded: Dictionary=Codec.decode(text,effective.catalog,prepared.registry)
	if not require_ok(decoded,"同registry完整Codec语义校验"): finish(); return
	if not require_ok(kernel.restore_with_catalog(decoded.state,effective.catalog,prepared.registry),"真实Kernel终验原子恢复"): finish(); return
	check(kernel.snapshot()==expected.state and kernel.content()==expected.catalog,"冷恢复完整state/catalog等于无恢复对照")
	check(kernel.snapshot().rng==expected.state.rng and kernel.snapshot().next_instance_id==expected.state.next_instance_id,"冷恢复RNG与next实例ID严格不变")
	assert_activity(kernel.snapshot(),contract,mode=="completed")
	observations[mode]={"tick":kernel.snapshot().tick,"remaining_ticks":kernel.snapshot().actions.get(contract.action_id,{}).get("remaining_ticks",0),"reserved":kernel.snapshot().wallets[contract.provider_wallet].reserved.get(contract.action_id,0),"next_instance_id":kernel.snapshot().next_instance_id,"rng":kernel.snapshot().rng}
	for unused in range(fixture.duration):
		if not kernel.snapshot().actions.has(contract.action_id): break
		if not require_ok(kernel.advance_one_tick(),"恢复后以同一正式命令有界续跑"): finish(); return
	var final_expected: Dictionary=read_json("completed-expected.json")
	var completed: Dictionary=kernel.snapshot()
	check(completed==final_expected.state and kernel.content()==final_expected.catalog,"同命令续跑最终完整state/catalog等于无恢复对照")
	check(completed.rng==final_expected.state.rng and completed.next_instance_id==final_expected.state.next_instance_id,"续跑最终RNG与next实例ID等值")
	assert_activity(completed,contract,true)
	check(kernel.submit(contract.begin_request)==contract.begin_response and kernel.submit(contract.completion_request)==contract.completion_response,"跨进程后重复原开始/完成请求仍返回原结果")
	check(kernel.snapshot()==completed,"跨进程重复请求不双发工资、不再扣成本或改state/RNG/ID")
	check(FileAccess.get_file_as_string(evidence.path_join(mode+".hmsave")).sha256_text()==expected.save_sha256,"恢复与重复请求均未改原v2文件")
	finish()

## 只接受准备器的唯一真实安装集，四个模式都重新读取正式installed索引。
func run() -> void:
	if not mode in ["create","start","waiting","completed"] or not evidence.is_absolute_path():
		printerr("FAIL: explicit isolated mode/evidence required"); quit(1); return
	fixture=read_json("fixture.json")
	var installed: Dictionary=Catalog.installed_catalogs()
	if not require_ok(installed,"真实compose→stage installed信任链"): finish(); return
	check(installed.catalogs.size()==1 and installed.catalogs[0].pack_set_digest==fixture.pack_set_digest,"只使用准备器固定的精确两包锁")
	if installed.catalogs.size()!=1: finish(); return
	if mode=="create": create_reference(installed.catalogs[0])
	else: restore_point(installed)

## 本地记录实际进程与阶段结果；仅此三tick门，不宣称动态收养/长期家族/OS已验。
func finish() -> void:
	var result: Dictionary={"mode":mode,"process_id":OS.get_process_id(),"checks":checks,"failures":failures,"observations":observations}
	write_json(mode+"-result.json",result)
	print("PACK_ACTIVITY_RESTORE ",JSON.stringify(result,"",true,true))
	quit(0 if failures==0 else 1)
