## 真NativeStorage的常驻物理银行门；validation是显式绑定夹具，不冒充worker语义验收。
extends SceneTree
const Native=preload("res://storage/native_storage.gd")
const Codec=preload("res://storage/resident_slot_codec.gd")
const LegacyCodec=preload("res://core/save_codec.gd")
const Kernel=preload("res://core/world_kernel.gd")
var checks: int=0
var failures: int=0
var case_root: String=""
var artifacts: Array[String]=[]

## 只在真实临时文件已写出后改变本测试旧银行；随后仍调用唯一原生读者和原子写尾。
class PriorDriftStore extends "res://storage/native_storage.gd":
	var armed: bool=false
	var injected: bool=false
	var replacement: String=""
	## 复现异步确认后的同generation字节漂移，不给生产添加测试钩子或第二套写算法。
	func _read_bank(slot: String, suffix: String) -> Dictionary:
		if armed and not injected and FileAccess.file_exists(directory.path_join(slot+".A.hmcheckpoint.tmp")):
			var file=FileAccess.open(directory.path_join(slot+".B.hmcheckpoint"),FileAccess.WRITE)
			if file!=null:
				file.store_string(replacement); file.flush(); file.close(); injected=true
		return super._read_bank(slot,suffix)

## 在真实事件循环内运行，任何脚本/断言错误都不能产生通过结论。
func _init() -> void:
	call_deferred("_run")

## 保留所有自建案例文件供复核；测试不删除或修改输入资源和玩家原槽。
func check(condition: bool, label: String) -> bool:
	checks+=1
	if not condition:
		failures+=1
		printerr("FAIL: ",label)
	return condition

## 每个案例必须是此前不存在的新目录，禁止复用已有银行作为写入目标。
func new_case(name: String) -> String:
	var path: String=case_root.path_join(name)
	if not check(not DirAccess.dir_exists_absolute(path),"案例目录此前不存在："+name): return ""
	if not check(DirAccess.make_dir_recursive_absolute(path)==OK,"创建独立案例："+name): return ""
	artifacts.append(path)
	return path

## 夹具写入仅接受本次新建案例的直接子路径，并核实真实落盘字节。
func fixture_write(path: String, text: String) -> bool:
	if not check(path.get_base_dir() in artifacts,"写入限于本次测试目录"): return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"打开自有夹具文件"): return false
	file.store_string(text); file.flush()
	var error: Error=file.get_error(); file.close()
	return check(error==OK and FileAccess.get_file_as_string(path)==text,"夹具原字节写回一致")

## 记录所有实际文件哈希，拒绝路径必须保留原A/B与旁边的旧格式哨兵。
func hashes(path: String) -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(path): result[name]=FileAccess.get_sha256(path.path_join(name))
	return result

## 这些小payload不是世界；只用真实codec生成可物理读取的不同二进制包装。
func envelope(label: String, generation: int, slot: String="manual-1") -> String:
	var encoded: Dictionary=Codec.encode(("physical-fixture-only:"+label).to_utf8_buffer(),generation,slot)
	check(encoded.get("ok",false),"编码物理夹具："+label)
	return encoded.get("text","")

## 模拟中央已绑定的三项摘要，仅验证writer接口，不断言任何checkpoint语义成立。
func binding_fixture(candidate: String, previous: String="") -> Dictionary:
	var next: Dictionary=Codec.inspect_metadata(candidate)
	var old: Dictionary=Codec.inspect_metadata(previous) if not previous.is_empty() else {}
	return {"ok":true,"candidate_sha256":next.metadata.payload_sha256,"previous_sha256":old.metadata.payload_sha256 if not old.is_empty() else "","previous_text_sha256":previous.sha256_text() if not previous.is_empty() else ""}

## 统一断言真实拒绝与全部既存字节保全；只在案例开始后读取该案例自身文件。
func expect_preserved(store, candidate: String, generation: int, validation: Dictionary, code: String="") -> void:
	var before: Dictionary=hashes(store.directory)
	var result: Dictionary=store.put_resident("manual-1",candidate,generation,validation)
	check(not result.get("ok",false) and (code.is_empty() or result.get("code")==code),"写入明确拒绝："+code+" "+str(result.get("code")))
	check(hashes(store.directory)==before,"拒绝后全部已有文件原字节保持")

## 第一写与覆盖使用唯一原bank writer；受信语义绑定之前不能借旧put/get绕过。
func check_writes(one: String, two: String, three: String) -> void:
	var path: String=new_case("writes")
	if path.is_empty(): return
	var store=Native.new(path,{},null,true)
	fixture_write(path.path_join("manual-1.B.hmsave"),"legacy sentinel: never inspect or overwrite")
	var before: Dictionary=hashes(path)
	check(store.get_slot("manual-1").get("code")=="NativeValidationRequired","旧get在resident模式拒绝伪语义读取")
	check(store.put("manual-1",one,0).get("code")=="NativeValidationRequired" and hashes(path)==before,"旧put不能绕过worker绑定")
	var empty: Dictionary=store.inspect_slot("manual-1")
	check(empty.get("code")=="NotFound" and empty.get("generation")==0,"旧.hmsave不占用常驻物理命名空间")
	var first: Dictionary=store.put_resident("manual-1",one,0,binding_fixture(one))
	if not check(first.get("ok",false),"第一写真实成功："+str(first)): return
	check(first.generation==1 and first.suffix=="B" and first.text==one,"首代进入原算法B银行且读回原文")
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmcheckpoint"))==one,"第一代真实文件存在")
	check(not FileAccess.file_exists(path.path_join("manual-1.B.hmcheckpoint.tmp")),"成功提升不留下已提升临时文件")
	var second: Dictionary=store.put_resident("manual-1",two,1,binding_fixture(two,one))
	if not check(second.get("ok",false),"第二写真实成功："+str(second)): return
	check(second.generation==2 and second.suffix=="A" and second.text==two,"覆盖进入另一银行并读回新代")
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmcheckpoint"))==one and FileAccess.get_file_as_string(path.path_join("manual-1.A.hmcheckpoint"))==two,"两代原字节同时保留")
	check(not FileAccess.file_exists(path.path_join("manual-1.A.hmcheckpoint.tmp")),"另一银行提升完成后临时路径消失")
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmsave"))=="legacy sentinel: never inspect or overwrite","旧格式哨兵未受写入影响")
	var listed: Dictionary=store.list_slots()
	check(listed.get("ok",false) and listed.slots.size()==1 and listed.slots[0].generation==2,"物理列表只返回真实常驻最高代")
	expect_preserved(store,two,1,binding_fixture(two,one),"GenerationConflict")
	expect_preserved(store,three,2,{"ok":false,"code":"InvalidCachedResult"},"InvalidCachedResult")
	expect_preserved(store,three,2,{})
	for field in ["candidate_sha256","previous_sha256","previous_text_sha256"]:
		var wrong: Dictionary=binding_fixture(three,two); wrong[field]="0".repeat(64)
		expect_preserved(store,three,2,wrong)
	expect_preserved(store,envelope("wrong-slot",3,"manual-2"),2,binding_fixture(envelope("wrong-slot",3,"manual-2"),two))
	expect_preserved(store,envelope("wrong-generation",4),2,binding_fixture(envelope("wrong-generation",4),two))
	var corrupt: Dictionary=JSON.parse_string(three); corrupt.payload_sha256="0".repeat(64)
	expect_preserved(store,JSON.stringify(corrupt),2,binding_fixture(three,two))
	check(not store.put_resident("../outside",three,2,binding_fixture(three,two)).get("ok",false),"路径穿越仍由原固定槽规则拒绝")

## 只有明确物理损坏能恢复旧银行；未知版本和同代异文阻止覆盖，不能伪装成空槽。
func check_selection(one: String, two: String) -> void:
	var path: String=new_case("corrupt-one")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one)
	fixture_write(path.path_join("manual-1.A.hmcheckpoint"),"{broken")
	fixture_write(path.path_join("manual-1.A.hmcheckpoint.tmp"),two)
	var store=Native.new(path,{},null,true); var before: Dictionary=hashes(path)
	var selected: Dictionary=store.inspect_slot("manual-1")
	check(selected.get("ok",false) and selected.get("recovered",false) and selected.text==one and selected.generation==1,"坏新银行只恢复明确可读旧代；临时文件不参选")
	check(hashes(path)==before,"物理选代只读全部原文件")
	path=new_case("unsupported-version")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one)
	var future: Dictionary=JSON.parse_string(two); future.format_version=99
	fixture_write(path.path_join("manual-1.A.hmcheckpoint"),JSON.stringify(future))
	store=Native.new(path,{},null,true); before=hashes(path); selected=store.inspect_slot("manual-1")
	check(not selected.get("ok",false) and selected.get("code")=="UnsupportedSaveVersion" and not selected.get("physical_corruption",false),"未来版本停止选代，不许可损坏回退")
	expect_preserved(store,two,1,binding_fixture(two,one),"UnsupportedSaveVersion")
	check(hashes(path)==before,"未来格式和旧代字节全部保留")
	path=new_case("same-generation-conflict")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one)
	fixture_write(path.path_join("manual-1.A.hmcheckpoint"),one+"\n")
	store=Native.new(path,{},null,true); before=hashes(path); selected=store.inspect_slot("manual-1")
	check(selected.get("code")=="GenerationConflict" and not selected.get("physical_corruption",false),"同generation不同原文拒绝而非任选一个")
	check(hashes(path)==before,"同代冲突没有改写文件")

## 同代文本漂移和临时写失败均不得提升新候选；最后复核必须在真实tmp产生之后执行。
func check_failed_publication(one: String, two: String) -> void:
	var path: String=new_case("pre-write-drift")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one+"\n")
	var store=Native.new(path,{},null,true)
	expect_preserved(store,two,1,binding_fixture(two,one))
	path=new_case("temp-open-failed")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one)
	check(DirAccess.make_dir_recursive_absolute(path.path_join("manual-1.A.hmcheckpoint.tmp"))==OK,"自有同名目录使FileAccess临时打开确定失败")
	store=Native.new(path,{},null,true)
	expect_preserved(store,two,1,binding_fixture(two,one),"StorageWriteFailed")
	path=new_case("after-temp-drift")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),one)
	var drifting=PriorDriftStore.new(path,{},null,true)
	drifting.replacement=one+"\n"; drifting.armed=true
	var result: Dictionary=drifting.put_resident("manual-1",two,1,binding_fixture(two,one))
	check(drifting.injected,"漂移确实发生在真实tmp写出以后")
	check(not result.get("ok",false),"rename前再次复核拒绝同代文本漂移")
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmcheckpoint"))==one+"\n","拒绝没有覆盖本次模拟外部写者的最新旧代")
	check(not FileAccess.file_exists(path.path_join("manual-1.A.hmcheckpoint")),"漂移后候选没有提升成新committed银行")
	check(drifting.inspect_slot("manual-1").get("text")==one+"\n","未完成tmp不替代仍可读旧银行")

## 当前Alpha正式新state经原Codec编码成v1，确认默认三参共享写尾；不声称历史迁移。
func check_legacy_tail() -> void:
	var catalog_path: String="res://data/generated/alpha_life.hmcatalog.json"
	var catalog_hash: String=FileAccess.get_sha256(catalog_path)
	var source=Kernel.new(); var started: Dictionary=source.new_game("scenario.alpha_life",catalog_path)
	if not check(started.get("ok",false),"当前真实Alpha正式开局："+str(started.get("code",""))): return
	var context: Dictionary=source.save_context(); var before: Dictionary=source.snapshot()
	var one: String=LegacyCodec.encode(context.state,1,"manual-1",context.catalog,context.registry)
	var two: String=LegacyCodec.encode(context.state,2,"manual-1",context.catalog,context.registry)
	if not check(not one.is_empty() and not two.is_empty(),"同一真实状态生成两代旧格式封装"): return
	check(JSON.parse_string(one).format_version==1,"新fixture明确是原Codec的v1，不是假装历史原档")
	var path: String=new_case("legacy-default-write")
	if path.is_empty(): return
	fixture_write(path.path_join("manual-1.B.hmcheckpoint"),"resident sentinel: legacy writer must ignore")
	fixture_write(path.path_join("source-current-v1.hmsave"),one)
	var store=Native.new(path,context.catalog,context.registry)
	var first: Dictionary=store.put("manual-1",one,0)
	if not check(first.get("ok",false) and first.get("text")==one and first.get("generation")==1,"默认三参真实首写经过共享尾并回读"): return
	var second: Dictionary=store.put("manual-1",two,1)
	if not check(second.get("ok",false) and second.get("text")==two and second.get("generation")==2,"默认三参真实覆盖经过共享尾并回读"): return
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmsave"))==one and FileAccess.get_file_as_string(path.path_join("manual-1.A.hmsave"))==two,"旧路径两银行和原扩展完整保留")
	check(not FileAccess.file_exists(path.path_join("manual-1.A.hmsave.tmp")) and not FileAccess.file_exists(path.path_join("manual-1.B.hmsave.tmp")),"旧路径成功提升不留已提升tmp")
	check(store.get_slot("manual-1").get("text")==two,"旧get仍完整语义decode最高代")
	check(FileAccess.get_file_as_string(path.path_join("manual-1.B.hmcheckpoint"))=="resident sentinel: legacy writer must ignore","旧写者没有接触常驻扩展哨兵")
	check(FileAccess.get_sha256(catalog_path)==catalog_hash and source.snapshot()==before,"原目录字节与真实源世界没有改写")

## 用进程/单调时钟专属目录执行有限物理门，所有测试文件保留下来，不清理玩家数据。
func _run() -> void:
	var parent: String=OS.get_user_data_dir().path_join("resident-slot-tests")
	var legacy_only: bool=false
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--evidence-root="): parent=arg.trim_prefix("--evidence-root=")
		elif arg=="--legacy-only": legacy_only=true
	case_root=parent.path_join("case-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	if legacy_only:
		check_legacy_tail()
	else:
		var one: String=envelope("one",1); var two: String=envelope("two",2); var three: String=envelope("three",3)
		check_writes(one,two,three)
		check_selection(one,two)
		check_failed_publication(one,two)
	print("RESIDENT_NATIVE_STORAGE_CHECKS=",checks," FAILURES=",failures," ROOT=",case_root," MODE=", "legacy-only" if legacy_only else "resident", " SCOPE=shared physical writer; no native core/GUI acceptance")
	quit(0 if failures==0 else 1)
