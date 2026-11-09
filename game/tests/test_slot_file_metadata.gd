## 真实银行文件时间只是展示元信息；generation、物理损坏白名单与错误首序不受mtime影响。
extends SceneTree
const Native=preload("res://storage/native_storage.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const SOURCE="res://tests/fixtures/legacy-original.hmsave"
const SOURCE_SHA="153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690"
var checks: int=0
var failures: int=0
var folder: String=""
var source: Dictionary={}

## 引擎准备好后运行独立profile，仅测试银行物理封装与文件元信息。
func _init() -> void:
	call_deferred("run")

## 明确记录每个失败，不把脚本提前退出算作通过。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 原v1只派生槽身份与代号；正文、checksum、规则和格式版本保持真实原件值。
func envelope(slot: String, generation: int) -> String:
	var header: Dictionary=source.duplicate(true)
	header.save_id=slot; header.generation=generation
	return JSON.stringify(header,"",true,true)

## 向本进程专属目录写真实银行，不触碰原始捕获档或用户槽。
func bank(slot: String, suffix: String, text: String) -> String:
	var path: String=folder.path_join(slot+"."+suffix+".hmsave")
	var file=FileAccess.open(path,FileAccess.WRITE)
	check(file!=null,"可写独立银行"+slot+suffix)
	if file!=null: file.store_string(text); file.close()
	return path

## 真实文件mtime直接比对展示字段，不从封装generation或其他银行推算时间。
func time_is(result: Dictionary, expected: int, label: String) -> void:
	check(result.has("modified_unix") and typeof(result.get("modified_unix"))==TYPE_INT and result.get("modified_unix")==expected,label)

## 所有现存银行原字节保留，inspect/list从不更新时间或持久化世界。
func hashes() -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(folder): result[name]=FileAccess.get_sha256(folder.path_join(name))
	return result

## 构造mtime与generation相反的真实文件，再覆盖未知版本、资源限制和物理损坏的原错误边界。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated fixture project"); quit(1); return
	check(FileAccess.get_sha256(SOURCE)==SOURCE_SHA,"读取真实v1来源SHA，不制造历史黄金")
	if FileAccess.get_sha256(SOURCE)!=SOURCE_SHA: finish(); return
	source=Strict.parse_text(FileAccess.get_file_as_string(SOURCE),Codec.MAX_BYTES).value
	folder=OS.get_user_data_dir().path_join("profiles/a2-slot-file-time-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	check(DirAccess.make_dir_recursive_absolute(folder)==OK,"建立独立profile")
	var first: String=bank("manual-1","A",envelope("manual-1",9))
	# FileAccess以整秒报告mtime，跨过一秒只用于建立明确反向时间，不运行世界。
	await create_timer(1.1).timeout
	var second: String=bank("manual-1","B",envelope("manual-1",8))
	var first_time: int=FileAccess.get_modified_time(first)
	var second_time: int=FileAccess.get_modified_time(second)
	check(first_time>0 and second_time>first_time,"较低generation银行有严格较新的真实mtime")
	var store=Native.new(folder)
	var inspected: Dictionary=store.inspect_slot("manual-1")
	check(inspected.get("ok",false) and inspected.get("generation")==9 and inspected.get("suffix")=="A" and not inspected.get("recovered",true),"仍按最高generation选择旧mtime银行")
	time_is(inspected,first_time,"inspect透传选中银行真实modified_unix")
	check(inspected.get("metadata",{}).get("format_version")==1 and not inspected.get("metadata",{}).has("scenario_id") and not inspected.get("physical_corruption",false),"v1没有剧本元字段仍是合法物理封装，不标损坏")
	bank("manual-2","A","")
	var recovered_path: String=bank("manual-2","B",envelope("manual-2",4))
	inspected=store.inspect_slot("manual-2")
	check(inspected.get("ok",false) and inspected.get("recovered",false) and inspected.get("corrupt_generations")==[{"suffix":"A","code":"InvalidSaveHeader"}],"明确损坏仍按原白名单恢复，损坏描述不猜代号或时间")
	time_is(inspected,FileAccess.get_modified_time(recovered_path),"损坏回读只附实际选中好银行时间")
	bank("manual-3","A",envelope("manual-3",1))
	var future_path: String=bank("manual-3","B",'{"magic":"HM-SAVE","format_version":999,"opaque":"future"}')
	inspected=store.inspect_slot("manual-3")
	check(inspected.get("code")=="UnsupportedSaveVersion" and inspected.get("physical_corruption")==false and not inspected.has("text"),"未知格式继续停止，mtime不允许降代")
	time_is(inspected,FileAccess.get_modified_time(future_path),"未知格式只附实际失败银行文件时间")
	bank("manual-4","A",envelope("manual-4",2)); bank("manual-4","B",envelope("manual-4",2)+"\n")
	inspected=store.inspect_slot("manual-4")
	check(inspected.get("code")=="GenerationConflict" and inspected.get("physical_corruption")==false,"同代冲突原错误首序不变")
	time_is(inspected,0,"冲突没有选中银行，文件时间明确未知0")
	bank("manual-5","A","")
	inspected=store.inspect_slot("manual-5")
	check(inspected.get("code")=="CorruptSlot" and inspected.get("physical_corruption")==true,"全坏槽仍明确物理损坏")
	time_is(inspected,0,"全坏槽没有选中银行，不猜文件时间")
	time_is(store.inspect_slot("manual-6"),0,"不存在槽文件时间为0")
	time_is(store.inspect_slot("manual-1\n"),0,"非法槽不读取文件且时间为0")
	var deep_path: String=bank("manual-7","A","[".repeat(34)+"0"+"]".repeat(34))
	inspected=store.inspect_slot("manual-7")
	check(inspected.get("code")=="TooDeep" and inspected.get("physical_corruption")==false,"资源限制保持原错误且不许可回退")
	time_is(inspected,FileAccess.get_modified_time(deep_path),"资源限制只附实际文件时间")
	var mismatch_path: String=bank("manual-8","A",envelope("manual-9",3))
	inspected=store.inspect_slot("manual-8")
	check(inspected.get("code")=="SlotIdentityMismatch" and inspected.get("physical_corruption")==false,"错槽仍优先明确拒绝")
	time_is(inspected,FileAccess.get_modified_time(mismatch_path),"错槽错误有证据的文件时间")
	var before: Dictionary=hashes()
	var listed: Dictionary=store.list_slots(); var rows: Dictionary={}
	for row in listed.get("slots",[]): rows[row.slot]=row
	check(listed.get("ok",false) and rows.size()==7 and not rows.has("manual-6"),"列表保留七个实际存在槽并忽略缺档")
	time_is(rows.get("manual-1",{}),first_time,"list透传物理选中高代时间，不使用较新mtime")
	time_is(rows.get("manual-3",{}),FileAccess.get_modified_time(future_path),"list透传未来格式失败银行时间")
	time_is(rows.get("manual-4",{}),0,"list冲突时间保持未知0")
	check(rows.get("manual-1",{}).get("ok",false) and rows.get("manual-3",{}).get("code")=="UnsupportedSaveVersion" and rows.get("manual-3",{}).get("physical_corruption")==false,"列表既有成功/错误与物理分类不漂移")
	check(hashes()==before and FileAccess.get_modified_time(first)==first_time and FileAccess.get_modified_time(second)==second_time and FileAccess.get_sha256(SOURCE)==SOURCE_SHA,"inspect/list不改银行原字节/mtime或真实捕获原件")
	finish()

## 所有档与日志仅留独立本地profile，提交只有测试源码。
func finish() -> void:
	print("SLOT_FILE_METADATA_CHECKS=",checks," FAILURES=",failures," PROFILE=",folder)
	quit(0 if failures==0 else 1)
