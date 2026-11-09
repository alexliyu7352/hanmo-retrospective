## 仅供测试的旧阶段观测与新图对照；原始字节和独立旧回执冻结后绝不重录期望。
extends RefCounted
const OLD_SHA="b51547967fcdd497928682e0a26e448b4a8aa66ab8ae1fd3a94c04bce39f2619"
static var evidence_dir: String=""
static var pristine: GDScript
static var mirror: GDScript
static var catalog: Dictionary={}
static var label: String=""
static var folder: String=""
static var errors: Array=[]
static var captures: Array=[]
static var signatures: Dictionary={}
static var observed: Array=[]
static var entered: Array=[]
static var current_mode: bool=false
static var current_cases: Array=[]
static var fixture_dir: String=""

## 每个脚本只装载明确给定的测试源码，解析失败保留错误而非跳过断言。
static func script(text: String) -> GDScript:
	var resource=GDScript.new(); resource.source_code=text
	if resource.reload()!=OK: errors.append("OracleScriptParse"); return null
	return resource

## 先核对冻结源码与仅移除标记的字节等价；禁止从当前新协调器生成旧期望。
static func prepare_capture(destination: String, source_dir: String) -> bool:
	folder=destination; evidence_dir=source_dir
	var original: String=FileAccess.get_file_as_string(evidence_dir.path_join("pristine_world_step.gd"))
	var instrumented: String=FileAccess.get_file_as_string(evidence_dir.path_join("instrumented_world_step.gd"))
	var stripped: String=""
	for line in instrumented.split("\n"):
		if not "# A14_ORACLE" in line: stripped+=line+"\n"
	stripped=stripped.trim_suffix("\n")
	if original.sha256_text()!=OLD_SHA or stripped!=original or FileAccess.get_file_as_string("res://core/world_step.gd")!=original:
		errors.append("OracleSourceMismatch"); return false
	pristine=script(original); mirror=script(instrumented)
	DirAccess.make_dir_recursive_absolute(folder)
	return pristine!=null and mirror!=null

## 仅替换旧Kernel的Step绑定并记录目录；其公共事务和全部尾部代码保持逐字一致。
static func kernel(adapter_path: String) -> GDScript:
	var source: String=FileAccess.get_file_as_string("res://core/world_kernel.gd")
	var anchor: String='const Step = preload("res://core/world_step.gd")'
	if source.count(anchor)!=1: errors.append("OracleKernelAnchor"); return null
	source=source.replace(anchor,'const Step = preload("'+adapter_path+'")')
	anchor='\tvar advanced: Dictionary = Step.advance(state,_catalog.data,_registry,hold)'
	if source.count(anchor)!=1: errors.append("OracleKernelAdvanceAnchor"); return null
	source=source.replace(anchor,'\tStep.catalog=_catalog\n'+anchor)
	return script(source)

## 记录有语义差异的有限边界，不保存每个平凡旅行半日或长寿命矩阵。
static func _signature(initial: Dictionary, result: Dictionary, rows: Array, hold: Array) -> String:
	var actions: Array=[]
	for action in initial.actions.values():
		var item: String=action.kind+":"+str(mini(action.remaining_ticks,2))
		if not item in actions: actions.append(item)
	actions.sort()
	var features: Array=[label,actions,not hold.is_empty(),result.get("code","")]
	if not rows.is_empty():
		var context: Dictionary=rows[-1].context
		features.append(context.get("contacts",[]).size()); features.append(context.get("military_contacts",[]).size())
		features.append(context.get("_battles",{}).get("completed_battle_ids",[]).size())
		features.append(not context.get("_battles",{}).get("actor_changes",[]).is_empty())
		var events: Array=[]
		for entry in context.get("_sieges",{}).get("events",[]): events.append(entry.event.get("kind",""))
		features.append(events); features.append(context.get("_sieges",{}).get("battle_requests",[]).size())
		features.append(context.get("_receipts",[]).size()); features.append(context.get("journey_requests",[]).size())
	return JSON.stringify(features)

## 同一输入分别运行无观测原函数与标记镜像；完整结果不等则拒绝收录且保持失败可见。
static func advance(state: Dictionary, data: Dictionary, registry: Dictionary, hold: Array=[]) -> Dictionary:
	if current_mode:
		var initial: Dictionary=state.duplicate(true); var before_hold: Array=hold.duplicate(true)
		var result: Dictionary=_run_observed(state,data,registry,hold)
		var signature: String=_signature(initial,result,observed,hold)
		if not signatures.has(signature):
			signatures[signature]=true
			current_cases.append({"label":label,"initial":initial,"data":data,"catalog":catalog,"hold":before_hold,"hold_after":hold.duplicate(),"entered":entered.duplicate(),"rows":observed.duplicate(true),"result":result.duplicate(true),"final":state.duplicate(true)})
		return result
	var initial: Dictionary=state.duplicate(true); var original: Dictionary=state.duplicate(true)
	var before_hold: Array=hold.duplicate(true); var before_data: Dictionary=data.duplicate(true); var before_registry: Dictionary=registry.duplicate(true)
	var expected: Dictionary=pristine.advance(original,data,registry,hold)
	var result: Dictionary=mirror.advance(state,data,registry,hold)
	if state!=original or result!=expected or hold!=before_hold or data!=before_data or registry!=before_registry: errors.append("InstrumentationChangedSemantics:"+label)
	var rows: Array=mirror._oracle_rows
	var signature: String=_signature(initial,result,rows,hold)
	if not signatures.has(signature):
		signatures[signature]=true
		var name: String="case-%03d.json"%captures.size()
		var record: Dictionary={"label":label,"signature":signature,"initial":initial,"data":data,"catalog":catalog,"hold":before_hold,"hold_after":hold,"entered":mirror._oracle_entered,"rows":rows,"result":result,"final":state,"pristine_equal":state==original and result==expected,"source_sha256":OLD_SHA}
		var file=FileAccess.open(folder.path_join(name),FileAccess.WRITE)
		if file==null: errors.append("OracleWrite:"+name)
		else: file.store_string(JSON.stringify(record,"",true,true)); file.close()
		captures.append({"file":name,"label":label,"signature":signature,"rows":rows.size(),"ok":result.ok})
	return result

## 生成Kernel仅沿原共享释放函数转发，不引入另一套资源规则。
static func release(state: Dictionary, action: Dictionary) -> void:
	if current_mode: load("res://core/world_step.gd").release(state,action)
	else: pristine.release(state,action)

## 收尾保留捕获入口和错误，不以有文件代替实际旧实现相等证明。
static func finish_capture() -> Dictionary:
	var result: Dictionary={"ok":errors.is_empty(),"source_sha256":OLD_SHA,"captures":captures,"errors":errors}
	var file=FileAccess.open(folder.path_join("index.json"),FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify(result,"\t",true,true)); file.close()
	return result

## 新图包装器只观测真实回调之后的候选与上下文，不改变返回值和执行次序。
static func observe(run: Callable, id: String, state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	entered.append(id)
	var result: Dictionary=run.call(state,data,context)
	if result.ok:
		var visible: Dictionary=context.duplicate(true); visible.erase("_actions")
		observed.append({"id":id,"state":state.duplicate(true),"context":visible})
	return result

## 对照逐阶段全状态、累加器、首错和最终状态；首个不同边界原样保留供定位。
static func compare(record: Dictionary, registry: Dictionary) -> Dictionary:
	var state: Dictionary=record.initial.duplicate(true); var data: Dictionary=record.data.duplicate(true); var hold: Array=record.hold.duplicate(true)
	var result: Dictionary=_run_observed(state,data,registry,hold)
	var difference: String=""
	if entered!=record.entered: difference="entered"
	elif observed.size()!=record.rows.size(): difference="row_count"
	else:
		for index in observed.size():
			var equal: bool=JSON.stringify(observed[index],"",true,true).sha256_text()==record._row_sha256[index] if record.has("_row_sha256") else observed[index]==record.rows[index]
			if not equal: difference=observed[index].id; break
	if difference.is_empty() and result!=record.result: difference="result"
	if difference.is_empty() and state!=record.final: difference="final"
	if difference.is_empty() and (hold!=record.hold_after or data!=record.data): difference="input_mutation"
	return {"ok":difference.is_empty(),"difference":difference,"rows":observed,"entered":entered,"result":result,"final":state}

## 从冻结清单逐文件核验压缩原始证据，严格JSON恢复整数而非浮点近似。
static func records() -> Array:
	if evidence_dir.is_empty(): return []
	var strict=load("res://core/strict_json.gd")
	var index: Dictionary=strict.parse_text(FileAccess.get_file_as_string(evidence_dir.path_join("accepted.json"))).get("value",{})
	var result: Array=[]
	var original: String=FileAccess.get_file_as_string(evidence_dir.path_join("pristine_world_step.gd"))
	var instrumented: String=FileAccess.get_file_as_string(evidence_dir.path_join("instrumented_world_step.gd"))
	var stripped: String=""
	for line in instrumented.split("\n"):
		if not "# A14_ORACLE" in line: stripped+=line+"\n"
	if original.sha256_text()!=OLD_SHA or stripped.trim_suffix("\n")!=original: return []
	if not index.get("ok",false) or index.get("source_sha256")!=OLD_SHA: return result
	var serialized: Dictionary=strict.parse_text(FileAccess.get_file_as_string(evidence_dir.path_join("row-serialization.json"))).get("value",{})
	if serialized.get("accepted_sha256")!=FileAccess.get_file_as_string(evidence_dir.path_join("accepted.json")).sha256_text(): return []
	for entry in index.captures:
		var bytes: PackedByteArray=FileAccess.get_file_as_bytes(evidence_dir.path_join(entry.file))
		if bytes.is_empty(): return []
		var digest=HashingContext.new(); digest.start(HashingContext.HASH_SHA256); digest.update(bytes)
		if digest.finish().hex_encode()!=entry.sha256: return []
		var text: String=bytes.decompress_dynamic(67108864,FileAccess.COMPRESSION_GZIP).get_string_from_utf8()
		var parsed: Dictionary=strict.parse_text(text,67108864)
		if not parsed.ok or not parsed.value.pristine_equal or parsed.value.source_sha256!=OLD_SHA: return []
		var hashes: Dictionary=serialized.get("files",{}).get(entry.file,{})
		if hashes.get("uncompressed_sha256")!=entry.uncompressed_sha256 or hashes.get("row_sha256",[]).size()!=parsed.value.rows.size(): return []
		for field in ["initial","data","hold"]:
			if JSON.stringify(parsed.value[field],"",true,true).sha256_text()!=hashes.get("field_sha256",{}).get(field): return []
		parsed.value._row_sha256=hashes.row_sha256
		result.append(parsed.value)
	return result

## 用原目录头包回已冻结作者数据，经正式new_game与restore重建，不修改运行字段。
static func restore_kernel(kernel_value, record: Dictionary) -> Dictionary:
	var strict=load("res://core/strict_json.gd")
	var names: Dictionary={"foundation.two_stories":"two_stories","foundation.lived_city":"lived_city","foundation.alpha_life":"alpha_life"}
	if not names.has(record.catalog.content_id): return {"ok":false,"code":"OracleUnknownCatalog"}
	var header: Dictionary=strict.parse_text(FileAccess.get_file_as_string("res://data/generated/"+names[record.catalog.content_id]+".hmcatalog.json")).value
	var original_keys: Array=header.catalog.keys()
	header.catalog=record.data.duplicate(true)
	# Catalog装载会补空可选表；写回作者头时移除原头没有的这些纯默认值。
	for key in header.catalog.keys():
		if not key in original_keys and header.catalog[key] is Dictionary and header.catalog[key].is_empty(): header.catalog.erase(key)
	header.content_id=record.catalog.content_id; header.content_version=record.catalog.content_version
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://phase-oracle-content-"+str(Time.get_ticks_usec())+".json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	if file==null: return {"ok":false,"code":"OracleContentWrite"}
	file.store_string(JSON.stringify(header,"",true,true)); file.close()
	var opened: Dictionary=kernel_value.new_game(record.initial.scenario_id,path)
	if not opened.ok: return opened
	if kernel_value.content()!=record.catalog: return {"ok":false,"code":"OracleCatalogMismatch"}
	return kernel_value.restore(record.initial)

## 差异另写独立诊断，不覆盖已经冻结的预期或原捕获文件。
static func write_difference(record: Dictionary, actual: Dictionary) -> void:
	var path: String="user://phase-difference-"+str(Time.get_ticks_usec())+".json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify({"label":record.label,"expected":record,"actual":actual},"",true,true)); file.close()
	printerr("PHASE_DIFFERENCE=",path)

## 为当前代码的事务与生命周期测试观测真实回调；这不是旧行为兼容证据。
static func _run_observed(state: Dictionary, data: Dictionary, registry: Dictionary, hold: Array) -> Dictionary:
	observed=[]; entered=[]
	var copy: Dictionary=registry.duplicate(true)
	for index in copy.phase_plan.hooks.size():
		var hook: Dictionary=registry.phase_plan.hooks[index]
		copy.phase_plan.hooks[index].run=func(candidate: Dictionary,content: Dictionary,context: Dictionary) -> Dictionary: return observe(hook.run,hook.id,candidate,content,context)
	return load("res://core/world_step.gd").advance(state,data,copy,hold)

## 测试生成脚本只写独立user目录；正常仓库测试无需迁移证据或Git历史。
static func _fixture_script(name: String, text: String) -> String:
	var path: String=fixture_dir.path_join(name)
	var file=FileAccess.open(path,FileAccess.WRITE)
	if file==null: errors.append("CurrentFixtureWrite"); return ""
	file.store_string(text); file.close()
	return path

## 复用现有合法命令夹具，展平测试继承以避开外部脚本继承解析器缺陷。
static func _fixture_suite(path: String, base: String="") -> void:
	label=path.get_file().get_basename()
	var text: String=FileAccess.get_file_as_string(path)
	if not base.is_empty():
		var parent: String=FileAccess.get_file_as_string(base)
		parent=parent.substr(0,parent.find("## 只报告当前已实际执行的集成断言"))
		parent=parent.replace("func run_checks()", "func _base_run_checks()")
		text=parent+"\n"+text.replace('extends "res://tests/test_military_campaign_world.gd"', "").replace("super.run_checks()", "_base_run_checks()")
	text=text.replace('extends SceneTree','extends RefCounted')
	for token in ['const Kernel=preload("res://core/world_kernel.gd")','const Kernel = preload("res://core/world_kernel.gd")']:
		text=text.replace(token,'const Kernel=preload("'+fixture_dir.path_join("kernel.gd")+'")')
	text=text.replace('call_deferred("run_checks")','pass').replace('call_deferred("_run")','pass').replace('quit(', '_oracle_quit(')
	text+='\n## 夹具失败保留给调用测试，不退出整个引擎。\nfunc _oracle_quit(code: int) -> void:\n\tif code: preload("res://tests/world_phase_oracle.gd").errors.append("CurrentFixtureFailed")\n'
	var resource=load(_fixture_script(path.get_file(),text))
	if resource==null or not resource.can_instantiate(): errors.append("CurrentFixtureParse:"+path); return
	var instance=resource.new()
	if instance.has_method("run_checks"): instance.run_checks()
	else: instance._run()

## 以当前正式夹具构造短流程覆盖，不加载任何旧状态或长寿命记录。
static func current_records() -> Array:
	current_mode=true; current_cases=[]; errors=[]; signatures={}
	fixture_dir=ProjectSettings.globalize_path("user://phase-current-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(fixture_dir)
	var adapted: GDScript=kernel("res://tests/world_phase_oracle.gd")
	if adapted==null: current_mode=false; return []
	_fixture_script("kernel.gd",adapted.source_code)
	_fixture_suite("res://tests/test_world_step.gd")
	_fixture_suite("res://tests/test_long_rest.gd")
	var commands=load("res://tests/companion_fixture.gd")
	label="family_adoption"
	var world=adapted.new()
	if not world.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok: errors.append("CurrentFamilyStart"); current_mode=false; return []
	if not commands.send(world,"move_local",{"place_id":"west.estate"}).ok or not commands.send(world,"family_adopt",{"context":commands.context(world,"role.estate_steward"),"display_name":"阿宁"}).ok: errors.append("CurrentFamilyCommand")
	if not world.advance_one_tick().ok or not world.advance_one_tick().ok: errors.append("CurrentFamilyAdvance")
	label="ordinary_contact"
	world=commands.fresh(true)
	if not commands.send(world,"travel",{"destination":"village.east"}).ok or not world.advance_one_tick().ok: errors.append("CurrentContactCommand")
	catalog=world.content()
	var registry: Dictionary=load("res://core/runtime/module_registry.gd").build(load("res://core/runtime/builtin_modules.gd").descriptors())
	if not advance(world.snapshot(),catalog.data,registry).ok: errors.append("CurrentContactAdvance")
	for kind in ["accept_offer","trade"]:
		world=adapted.new()
		if not world.new_game().ok: errors.append("CurrentRegisteredStart"); continue
		if kind=="accept_offer":
			if not commands.send(world,"move_local",{"place_id":"west.office"}).ok or not commands.send(world,"inspect",{"prop_id":"notice.grain"}).ok or not commands.send(world,"accept_offer",{"offer_id":"offer.grain","context":commands.context(world,"role.office")}).ok: errors.append("CurrentTaskCommand")
		else:
			if not commands.send(world,"move_local",{"place_id":"west.grain"}).ok or not commands.send(world,"trade",{"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":2,"payer_wallet_id":"wallet.player","context":commands.context(world,"role.merchant")}).ok: errors.append("CurrentTradeCommand")
		label="registered_hold:"+kind; catalog=world.content()
		if not advance(world.snapshot(),catalog.data,registry,[world.snapshot().player_actor_id]).ok: errors.append("CurrentRegisteredAdvance")
	var base: String=FileAccess.get_file_as_string("res://tests/test_military_campaign_world.gd")
	base=base.replace('extends SceneTree','extends RefCounted').replace('const Kernel=preload("res://core/world_kernel.gd")','const Kernel=preload("'+fixture_dir.path_join("kernel.gd")+'")').replace('call_deferred("run_checks")','pass')
	var base_path: String=_fixture_script("military_base.gd",base)
	_fixture_suite("res://tests/test_siege_timeout_return.gd",base_path)
	_fixture_suite("res://tests/test_siege_world.gd",base_path)
	current_mode=false
	return current_cases if errors.is_empty() else []
