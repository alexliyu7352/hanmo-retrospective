## 独立profile的真实双银行物理选代；不创建世界、不修改原档，也不宣称v2完整恢复。
extends SceneTree
const Native=preload("res://storage/native_storage.gd")
const Codec=preload("res://core/save_codec.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const FIXTURE_ROOT="res://../tests/fixtures/saves/a2/"
var checks: int=0
var failed: int=0
var profile: String=""
var folders: Array[String]=[]
var source: Dictionary={}
var catalog: Dictionary={}

## 延后执行，使用当前进程专属profile，绝不进入玩家发行槽目录。
func _init() -> void:
	call_deferred("_run")

## 所有失败都计数并导致非零退出，不把提前结束算通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 原v1只在内存派生测试封装，不把这些变体标为历史原始捕获。
func sample(generation: int, mode: String="plain") -> String:
	var header: Dictionary=source.duplicate(true)
	header.generation=generation; header.save_id="manual-1"
	if mode=="engine" or mode=="rules":
		var body: Dictionary=Strict.parse_text(header.payload_json).value
		if mode=="engine":
			header.engine_build="unknown-future-engine"; header.rng_backend="godot:unknown-future-engine"
			body.engine_build=header.engine_build; body.rng_backend=header.rng_backend
		else:
			header.ruleset_id="unknown-future-rules"; body.ruleset_id=header.ruleset_id
		header.payload_json=JSON.stringify(body,"",true,true)
		header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	elif mode=="checksum": header.payload_sha256="0".repeat(64)
	elif mode=="other_slot": header.save_id="manual-2"
	elif mode=="v2_missing_pack":
		var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(FIXTURE_ROOT+"resolver-contract.json")).value.catalog
		header.format_version=2; header.content_id=fixture.content_id; header.content_version=fixture.content_version; header.content_digest=fixture.content_digest
		header.pack_set=fixture.pack_set.duplicate(true); header.pack_set_digest=fixture.pack_set_digest
		header.scenario_id="scenario::start"; header.player_actor_id="people::player"
		var registered: Dictionary=Registry.build(Builtin.descriptors()); header.rule_modules=[]
		var ids: Array=registered.modules.keys(); ids.sort()
		for id in ids: header.rule_modules.append({"id":id,"version":registered.modules[id].version})
		header.payload_json="opaque v2 test body, not a restored world"
		header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	return JSON.stringify(header,"",true,true)

## 每个case是新目录和真实A/B文件；空字符串同样写为真实零字节文件。
func store_case(label: String, banks: Dictionary):
	var directory: String=profile.path_join(label)
	check(DirAccess.make_dir_recursive_absolute(directory)==OK,"创建独立case目录："+label)
	folders.append(directory)
	for suffix in banks:
		var file=FileAccess.open(directory.path_join("manual-1."+suffix+".hmsave"),FileAccess.WRITE)
		check(file!=null,"创建银行文件："+label+"/"+suffix)
		if file==null: continue
		file.store_string(banks[suffix]); file.close()
	return Native.new(directory,catalog)

## 通过文件SHA记录完整原字节；读失败或语义失败后必须逐文件保持一致。
func hashes(store) -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(store.directory): result[name]=FileAccess.get_sha256(store.directory.path_join(name))
	return result

## inspect和兼容get都不得猜旧代；阻塞只返回实际错误而非CorruptSlot。
func expect_stop(store, code: String, label: String) -> void:
	var before: Dictionary=hashes(store)
	for result in [store.inspect_slot("manual-1"),store.get_slot("manual-1")]:
		check(not result.get("ok",false) and result.get("code")==code and not result.has("text"),label+" code="+str(result.get("code",""))+" ok="+str(result.get("ok",false)))
	check(hashes(store)==before,"阻塞后原银行字节未动："+label)

## 只清理由本测试创建的目录/文件，不递归操作任何用户存档根。
func cleanup() -> void:
	for directory in folders:
		for filename in DirAccess.get_files_at(directory): DirAccess.remove_absolute(directory.path_join(filename))
		DirAccess.remove_absolute(directory)
	DirAccess.remove_absolute(profile)

## 真实文件覆盖物理损坏、未知格式、资源限制、同代冲突以及最新代语义错误不回退。
func _run() -> void:
	profile=OS.get_user_data_dir().path_join("profiles/a2-slot-inspection-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	var raw: String=FileAccess.get_file_as_string(FIXTURE_ROOT+"v1-4f16935-family-after-adoption.hmsave")
	var original_hash: String=raw.sha256_text()
	source=Strict.parse_text(raw,Codec.MAX_BYTES).value
	catalog=Catalog.read_prepared("res://data/generated/alpha_life.hmcatalog.json")
	check(catalog.ok,"真实匹配alpha目录通过正式运行校验")
	if not catalog.ok:
		quit(1); return
	var older: String=sample(1); var newest: String=sample(2)
	var empty=store_case("empty",{})
	check(empty.inspect_slot("manual-1").get("code")=="NotFound","没有银行明确NotFound")
	for slot in ["../manual-1","manual-1\n","manual-1\r\n","manual-01","manual-11"]:
		check(empty.inspect_slot(slot).get("code")=="InvalidSlot" and not Native.valid_slot(slot),"槽命名全串匹配且不可路径穿越："+slot.c_escape())
	for banks in [{"A":older,"B":newest},{"A":newest,"B":older},{"B":newest}]:
		var store=store_case("ordered-"+str(folders.size()),banks); var before: Dictionary=hashes(store)
		var result: Dictionary=store.inspect_slot("manual-1")
		check(result.get("ok",false) and result.get("text")==newest and result.get("generation")==2 and result.get("snapshot_generation")==2 and result.get("metadata",{}).get("generation")==2 and result.get("recovered")==false,"按generation选最高原text，不按银行字母")
		check(store.get_slot("manual-1").get("text")==newest,"旧get_slot成功签名仍保留")
		check(hashes(store)==before,"成功读取后银行原字节不变")
	var same=store_case("same-generation-identical",{"A":newest,"B":newest})
	var repeated: Dictionary=same.inspect_slot("manual-1")
	check(repeated.get("ok",false) and repeated.get("suffix")=="A","同代同字节可确定选A")
	var conflict=store_case("same-generation-conflict",{"A":newest,"B":newest+"\n"})
	expect_stop(conflict,"GenerationConflict","同代不同原字节拒绝猜赢家")
	var conflict_before: Dictionary=hashes(conflict)
	check(conflict.put("manual-1",sample(3),2).get("code")=="GenerationConflict" and hashes(conflict)==conflict_before,"同代冲突也不能通过put覆盖")
	for broken in ["",sample(2,"checksum"),newest.substr(0,newest.length()-10)]:
		var store=store_case("corrupt-"+str(folders.size()),{"A":older,"B":broken}); var before: Dictionary=hashes(store)
		var result: Dictionary=store.inspect_slot("manual-1")
		check(result.get("ok",false) and result.get("text")==older and result.get("generation")==1 and result.get("recovered")==true and result.get("corrupt_generations",[]).size()==1,"仅明确物理损坏才选可读旧代并说明")
		if result.get("ok",false) and result.get("corrupt_generations",[]).size()==1:
			var damaged: Dictionary=result.corrupt_generations[0]
			check(damaged.size()==2 and damaged.get("suffix")=="B" and damaged.has("code") and not damaged.has("generation"),"坏封装只报告银行和错误，不猜损坏代号")
		check(store.get_slot("manual-1").get("text")==older and hashes(store)==before,"坏代回读不删除或覆盖原两银行")
	var corrupt_both=store_case("corrupt-both",{"A":"","B":sample(2,"checksum")})
	expect_stop(corrupt_both,"CorruptSlot","所有存在代均明确物理损坏")
	var future: String=FileAccess.get_file_as_string(FIXTURE_ROOT+"synthetic-unknown-future-envelope.hmsave")
	for banks in [{"A":older,"B":future},{"A":future,"B":newest}]:
		expect_stop(store_case("unknown-"+str(folders.size()),banks),"UnsupportedSaveVersion","未知future无法安全取得generation时停止")
	var deep: String="[".repeat(34)+"0"+"]".repeat(34)
	expect_stop(store_case("depth-limit",{"A":older,"B":deep}),"TooDeep","资源限制不是物理损坏许可")
	var oversized=store_case("size-limit",{"A":older})
	var large=FileAccess.open(oversized.directory.path_join("manual-1.B.hmsave"),FileAccess.WRITE)
	check(large!=null,"创建隔离的超限稀疏银行")
	if large!=null:
		large.seek(Codec.MAX_BYTES); large.store_8(0); large.close()
		expect_stop(oversized,"SaveSizeLimit","超限文件只检查长度并停止，不回读另一代")
	expect_stop(store_case("slot-identity",{"A":older,"B":sample(2,"other_slot")}),"SlotIdentityMismatch","合法封装属于另一槽时不能回退掩盖")
	for mode in ["engine","rules"]:
		var semantic: String=sample(2,mode)
		var store=store_case("semantic-"+mode,{"A":older,"B":semantic}); var before: Dictionary=hashes(store)
		var inspected: Dictionary=store.inspect_slot("manual-1")
		check(inspected.get("ok",false) and inspected.get("generation")==2 and inspected.get("text")==semantic and inspected.get("recovered")==false,"最新代"+mode+"不兼容仍保持物理选中")
		var code: String="IncompatibleEngine" if mode=="engine" else "UnsupportedSaveVersion"
		check(store.get_slot("manual-1").get("code")==code,"旧get只decode最新选中代，不读旧代掩盖"+mode)
		check(store.put("manual-1",sample(3),2).get("code")==code and hashes(store)==before,"语义不可读最新代不能通过put被覆盖")
		var listed: Dictionary=store.list_slots(); var entries: Array=listed.get("slots",[])
		check(listed.get("ok",false) and entries.size()==1 and entries[0].get("ok",false) and entries[0].get("metadata",{}).get("generation")==2,"列表只inspect，显示不兼容最新代身份")
	var missing=store_case("missing-pack",{"A":older,"B":sample(2,"v2_missing_pack")}); var missing_before: Dictionary=hashes(missing)
	check(Codec.decode(older,catalog).ok,"缺包case的旧代确实可由当前目录decode")
	var selected: Dictionary=missing.inspect_slot("manual-1")
	check(selected.get("ok",false) and selected.get("generation")==2 and selected.get("recovered")==false,"旧代可读也不得在缺包前预先降代")
	if selected.get("ok",false):
		check(Resolver.resolve(selected.metadata,[],Registry.build(Builtin.descriptors())).get("code")=="MissingPack" and hashes(missing)==missing_before,"物理选代后resolver明确缺包且原银行不变")
	check(FileAccess.get_file_as_string(FIXTURE_ROOT+"v1-4f16935-family-after-adoption.hmsave").sha256_text()==original_hash,"来源历史原档未被改写")
	cleanup()
	print("STORAGE_SLOT_INSPECTION_CHECKS=",checks," FAILURES=",failed)
	print("M3_FULL_RESTORE=NOT_RUN; V2_SAVE_WRITE=NOT_RUN; UI_CONTINUE=NOT_RUN")
	quit(0 if failed==0 else 1)
