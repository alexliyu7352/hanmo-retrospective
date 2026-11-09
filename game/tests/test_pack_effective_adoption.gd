## 新模式effective场景中正式两tick收养及完成后v2冷恢复；不直接生成身份或改写世界。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Family=preload("res://core/family_rules.gd")
const Schema=preload("res://core/state_schema.gd")
const Strict=preload("res://core/strict_json.gd")
var mode: String=""
var evidence: String=""
var checks: int=0
var failures: int=0
var fixture: Dictionary={}
var observed: Dictionary={}

## 读取独立准备器参数；证据文件只存在本轮临时目录，不使用玩家存档。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--mode="): mode=arg.trim_prefix("--mode=")
		if arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=")
	call_deferred("run")

## 每条失败均导致最终非零退出，不让提前结束冒充通过。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 打印正式错误并由调用者停止依赖它的后续步骤。
func require_ok(result: Dictionary, label: String) -> bool:
	check(result.get("ok",false),label)
	if not result.get("ok",false): printerr(result)
	return result.get("ok",false)

## 只读期望与作者合同；期望state不直接传给Kernel以制造测试世界。
func read_json(name: String) -> Dictionary:
	return Strict.parse_text(FileAccess.get_file_as_string(evidence.path_join(name)),Codec.MAX_BYTES).get("value",{})

## 仅在独立本地证据目录写源码之外的实际结果和v2文件。
func write_text(name: String, text: String) -> void:
	var file=FileAccess.open(evidence.path_join(name),FileAccess.WRITE)
	check(file!=null,"证据文件可写："+name)
	if file!=null: file.store_string(text); file.close()

## 以规范JSON保存实际对照结果，不改变历史捕获档或标注伪黄金。
func write_json(name: String, data: Dictionary) -> void:
	write_text(name,JSON.stringify(data,"",true,true))

## 只经当前revision/请求序号的正式命令入口办理移动、收养及等待。
func request_for(kernel, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":player,"request_seq":state.request_sequences[player]+1,"expected_revision":state.revision}

## 完成态检查真实创建来源、唯一家庭索引、钱包与占用；原型定义本身永不实例化。
func assert_completed(kernel, contract: Dictionary) -> void:
	var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	check(data.actors.has(fixture.prototype) and not state.actors.has(fixture.prototype) and not fixture.prototype in data.active_static_actor_ids,"prototype仍仅为完整定义，未被恢复或生成成初始真人")
	check(state.family.children.size()==1 and state.actors.size()==contract.original_actor_count+1,"只通过正式完成新增一位动态子女")
	var child: String=contract.child_actor_id
	check(child.begins_with("actor:") and not data.actors.has(child) and state.actors.has(child),"子女ID为真实动态actor实例，非作者原型ID")
	if not state.actors.has(child): return
	var actor: Dictionary=state.actors[child]
	var adoption: Dictionary=state.facts.get(contract.action_id,{})
	var creation: Dictionary=state.facts.get(child,{})
	check(adoption.get("kind")=="family_adoption" and adoption.get("success")==true and adoption.get("child_actor_id")==child and state.family.children[child]==contract.action_id,"完成事实与家庭children索引指向同一真实收养")
	check(creation.get("kind")=="actor_creation" and creation.get("source")==contract.action_id and creation.get("prototype_id")==fixture.prototype and actor.prototype_id==fixture.prototype,"动态创建事实追溯到实际收养行动及未激活原型")
	check(Identity.age_years(state,child)==fixture.child_age and actor.created_tick==contract.started_tick+fixture.duration and adoption.completed_tick==actor.created_tick,"6岁与创建日期来自正常两tick办理")
	check(state.wallets[contract.payer_wallet].balance==contract.payer_balance-fixture.fee and state.wallets[fixture.recipient_wallet].balance==contract.recipient_balance+fixture.fee,"唯一20费用从真实监护人钱包转到东区公钱包")
	check(state.wallets[contract.payer_wallet].reserved.is_empty() and not state.actions.has(contract.action_id) and state.actors[contract.guardian].action_id.is_empty() and state.actors[contract.provider].action_id.is_empty(),"完成后释放费用预留与双方占用")
	check(actor.wallet_id=="personal:"+child and state.wallets[actor.wallet_id].balance==0 and state.inventories[actor.inventory_id].lots.is_empty() and state.knowledge[child].is_empty(),"动态子女私产与知识独立，不复制监护人资产")
	check(not state.actors[fixture.deceased].alive,"初态已故人物没有随收养或恢复复活")
	check(Schema.validate(state,kernel.content(),kernel.save_context().registry).get("ok",false) and Family.validate_state(state,data).get("ok",false),"完成态通过同registry全Schema及完整Family合法性")

## 用原two_eras east初态的44岁玩家、真实入场与现有family角色完成正常两tick收养。
func create(catalog: Dictionary) -> void:
	var kernel=Kernel.new()
	if not require_ok(kernel.new_game(fixture.scenario,catalog.catalog_path),"正式新包effective east场景开局"): finish(); return
	var initial: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	check(data.world_maps.keys()==["adopt::map.east"] and not data.places.has("adopt::west.office") and data.scenarios[fixture.scenario].start_date.year==204,"使用第二年代和独立东图effective enabled/initial视图")
	check(Identity.age_years(initial,initial.player_actor_id)==fixture.guardian_age and initial.actors.size()==3,"真实44岁与三名初始人物来自east初态")
	check(data.actors.has(fixture.prototype) and not initial.actors.has(fixture.prototype),"开始前原型仅定义未实例化")
	if not require_ok(kernel.submit(request_for(kernel,"move_local",{"place_id":fixture.place})),"正式move_local走通东广场到东官署"): finish(); return
	var before: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(before,data,fixture.role)
	var params: Dictionary={"context":context,"display_name":"阿宁"}
	var quote: Dictionary=kernel.preview("family_adopt",params)
	if not require_ok(quote,"现有family角色正常收养报价"): finish(); return
	check(quote.duration==fixture.duration and quote.cost==fixture.fee and kernel.snapshot()==before,"原家庭合同两tick费20且报价纯只读")
	var request: Dictionary=request_for(kernel,"family_adopt",params)
	var result: Dictionary=kernel.submit(request)
	if not require_ok(result,"正式family_adopt开始"): finish(); return
	var pending: Dictionary=kernel.snapshot(); var guardian: String=pending.player_actor_id
	var action_id: String=pending.actors[guardian].action_id
	var contract: Dictionary={"action_id":action_id,"guardian":guardian,"provider":context.provider_actor_id,"payer_wallet":before.actors[guardian].wallet_id,"payer_balance":before.wallets[before.actors[guardian].wallet_id].balance,"recipient_balance":before.wallets[fixture.recipient_wallet].balance,"original_actor_count":before.actors.size(),"started_tick":pending.tick,"request":request,"result":result}
	check(contract.payer_balance==100 and contract.recipient_balance==1700,"支付资产来自唯一新增作者钱包与原east初态")
	check(not action_id.is_empty() and pending.facts.get(action_id,{}).get("kind")=="family_adoption_pending" and pending.actors.size()==before.actors.size(),"开始只冻结pending事实，未提前创建动态真人")
	check(pending.wallets[contract.payer_wallet].balance==100 and pending.wallets[contract.payer_wallet].reserved.get(action_id)==20 and pending.wallets[fixture.recipient_wallet].balance==1700,"开始仅预留20，双方余额未转移")
	check(pending.actors[guardian].action_id==action_id and pending.actors[contract.provider].action_id==action_id,"开始占用真实监护人与当值办理人")
	if not require_ok(kernel.advance_one_tick(),"正常推进第一个tick"): finish(); return
	var waiting: Dictionary=kernel.snapshot()
	check(waiting.actions.get(action_id,{}).get("remaining_ticks")==1 and waiting.actors.size()==before.actors.size() and not waiting.actors.has(fixture.prototype),"等待中仍只有pending占用，原型未复活")
	check(Schema.validate(waiting,kernel.content(),kernel.save_context().registry).get("ok",false) and Family.validate_state(waiting,data).get("ok",false),"等待态通过完整Schema/Family")
	if not require_ok(kernel.advance_one_tick(),"正常推进第二个tick完成"): finish(); return
	var completed: Dictionary=kernel.snapshot()
	check(completed.family.children.size()==1,"真实完成应有且仅有一个孩子")
	if completed.family.children.size()!=1: finish(); return
	contract.child_actor_id=completed.family.children.keys()[0]
	assert_completed(kernel,contract)
	check(kernel.submit(request)==result and kernel.snapshot()==completed,"完成后重试原收养请求不再创建身份或收费")
	var saved: Dictionary=kernel.save_context()
	var text: String=Codec.encode(saved.state,1,"effective-adoption",saved.catalog,saved.registry)
	var metadata: Dictionary=Codec.inspect_metadata(text)
	check(metadata.get("ok",false) and metadata.get("metadata",{}).get("format_version")==2,"真实收养完成态写出完整v2封装")
	write_text("completed-v2.hmsave",text)
	write_json("expected.json",{"state":saved.state,"catalog":saved.catalog,"contract":contract,"save_sha256":text.sha256_text()})
	observed={"started_tick":contract.started_tick,"completed_tick":completed.tick,"child_actor_id":contract.child_actor_id,"creation_source":action_id,"next_instance_id":completed.next_instance_id,"prototype_active":completed.actors.has(fixture.prototype)}
	finish()

## 新进程只从实际v2恢复，通过原catalog/registry与单次Snapshot后终验，不创建默认世界。
func restore(installed: Dictionary) -> void:
	var expected: Dictionary=read_json("expected.json")
	var text: String=FileAccess.get_file_as_string(evidence.path_join("completed-v2.hmsave"))
	check(text.sha256_text()==expected.save_sha256,"完成档原字节SHA一致")
	var kernel=Kernel.new()
	check(kernel.snapshot().is_empty() and kernel.content().is_empty(),"独立恢复进程没有默认世界")
	var metadata: Dictionary=Codec.inspect_metadata(text)
	if not require_ok(metadata,"正式metadata物理检查"): finish(); return
	var registered: Dictionary=kernel.prepare_restore_registry()
	if not require_ok(registered,"受信registry准备"): finish(); return
	var matched: Dictionary=Resolver.resolve(metadata.metadata,installed.catalogs,registered.registry)
	if not require_ok(matched,"metadata精确匹配installed原目录"): finish(); return
	var selection: Dictionary=Codec.inspect_restore_selection(text)
	if not require_ok(selection,"真实正文候选founder选择"): finish(); return
	var effective: Dictionary=Snapshot.resolve(matched.catalog,selection.scenario_id,selection.founder_actor_id)
	if not require_ok(effective,"单次effective east场景投影"): finish(); return
	var decoded: Dictionary=Codec.decode(text,effective.catalog,registered.registry)
	if not require_ok(decoded,"Codec完整同registry语义校验"): finish(); return
	if not require_ok(kernel.restore_with_catalog(decoded.state,effective.catalog,registered.registry),"Kernel终验原子恢复"): finish(); return
	check(kernel.snapshot()==expected.state and kernel.content()==expected.catalog,"真实动态子女完成档冷恢复完整state/catalog等值")
	check(kernel.snapshot().rng==expected.state.rng and kernel.snapshot().next_instance_id==expected.state.next_instance_id,"冷恢复RNG/next实例ID严格保持")
	assert_completed(kernel,expected.contract)
	check(kernel.submit(expected.contract.request)==expected.contract.result and kernel.snapshot()==expected.state,"重启后原收养请求仍幂等，身份与钱不重复")
	check(FileAccess.get_file_as_string(evidence.path_join("completed-v2.hmsave")).sha256_text()==expected.save_sha256,"读取与幂等重试未改原v2文件")
	observed={"completed_tick":kernel.snapshot().tick,"child_actor_id":expected.contract.child_actor_id,"next_instance_id":kernel.snapshot().next_instance_id,"prototype_active":kernel.snapshot().actors.has(fixture.prototype)}
	finish()

## 两个模式重新读取正式installed集，只接受固定作者包锁，不运行任何旧family长链。
func run() -> void:
	if not mode in ["create","restore"] or not evidence.is_absolute_path():
		printerr("FAIL: isolated mode/evidence required"); quit(1); return
	fixture=read_json("fixture.json")
	var installed: Dictionary=Catalog.installed_catalogs()
	if not require_ok(installed,"真实compose→stage安装集校验"): finish(); return
	check(installed.catalogs.size()==1 and installed.catalogs[0].pack_set_digest==fixture.pack_set_digest,"使用固定newpack精确锁")
	if installed.catalogs.size()!=1: finish(); return
	if mode=="create": create(installed.catalogs[0])
	else: restore(installed)

## 只记录本轮实际短链结果；档案与日志仅本地，成长/继任/OS均未执行。
func finish() -> void:
	var result: Dictionary={"mode":mode,"process_id":OS.get_process_id(),"checks":checks,"failures":failures,"observed":observed}
	write_json(mode+"-result.json",result)
	print("PACK_EFFECTIVE_ADOPTION ",JSON.stringify(result,"",true,true))
	quit(0 if failures==0 else 1)
