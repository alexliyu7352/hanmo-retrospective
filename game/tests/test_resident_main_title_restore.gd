## 两个独立进程的真实Main标题继续门；仅写新测试profile，不迁移旧档或借合成百人目录。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Catalog=preload("res://core/content_catalog.gd")
const SlotCodec=preload("res://storage/resident_slot_codec.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var evidence: String=""
var phase: String=""
var directory: String=""
var profile: String=""
var catalog_path: String=""
var observed: Dictionary={}

## 延后到真实场景树可处理Window和worker完成时才执行。
func _initialize() -> void:
	call_deferred("_run")

## 任一断言失败进入退出码，依赖步骤不以空结果继续。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
	return value

## 只识别生产控件的语义标记，不制造候选或直接调用存储完成回调。
func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 等待有限同源frame和已接受操作排空；这里绝不自动发许可或提交命令。
func wait_idle(main: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if main._started and main._resident_mode and is_instance_valid(main._resident_adapter) and not main._advancing and not main._resident_frame_pending and main._storage_intent.is_empty() and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty(): return true
		await process_frame
	return false

## 标题列表只读物理槽；确认以前不得安装默认世界或任何原生候选。
func open_title_slots(main: Node) -> bool:
	if not check(not main._started,"Continue之前仍在标题且没有旧运行假设"): return false
	var accepted: Dictionary=main._open_slots("load")
	if not check(accepted.get("ok",false),"标题Continue接受原生槽列表："+str(accepted)): return false
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if main._started: return check(false,"列表等待不能提前发布默认世界")
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible:
			if not check(not main.get("_resident_bootstrap_loading") and not main._resident_mode and not is_instance_valid(main._resident_load_candidate),"标题列表尚未创建候选世界"): return false
			var caption: Button=button(main._slot_dialog,"slot_select","manual-1")
			if not check(caption!=null and "行旅存档" in caption.text and "内容与剧本待读取时核对" in caption.text and not "旧格式" in caption.text,"原生槽文案如实标注待读取核对，不误报旧格式或伪造内容元数据"): return false
			return check(main.kernel.snapshot().is_empty() and not main._resident_frame_pending and main.find_child("WorldMapCanvas",true,false)==null and main.storage.native._resident_profile,"空标题未创建世界或排frame，物理槽保持原生格式")
		await process_frame
	return check(false,"标题原生槽列表有界完成")

## 选择与确认都发真实按钮pressed，确认以前只读磁盘。
func confirm_slot(main: Node, slot: String) -> bool:
	var selected: Button=button(main._slot_dialog,"slot_select",slot)
	if not check(selected!=null and not selected.disabled,"真实列表中目标槽可选择："+slot): return false
	var before: Dictionary=bank_hashes()
	selected.pressed.emit()
	var confirm: Button=button(main._slot_dialog,"slot_confirm")
	if not check(confirm!=null and not confirm.disabled and bank_hashes()==before,"选择只解锁确认，不写磁盘"): return false
	confirm.pressed.emit()
	return true

## 全世界导出仅用于本冷验收摘要，不进入Main热路径或前端规则缓存。
func world_digest(main: Node) -> String:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false) and exported.get("json") is String,"真实worker冷审计完整世界"): return ""
	return exported.json.sha256_text()

## 摘要覆盖原银行及意外临时文件，失败不得改写任何一项。
func bank_hashes() -> Dictionary:
	var result: Dictionary={}
	if DirAccess.dir_exists_absolute(directory):
		for name in DirAccess.get_files_at(directory): result[name]=FileAccess.get_sha256(directory.path_join(name))
	return result

## 原bytes直接计算SHA；不能以包装自报摘要替代被测payload。
func digest(bytes: PackedByteArray) -> String:
	var context=HashingContext.new()
	if not check(context.start(HashingContext.HASH_SHA256)==OK and context.update(bytes)==OK,"测试输入原字节可计算摘要"): return ""
	return context.finish().hex_encode()

## 仅在自有目录创建此前不存在的夹具，保存原正常槽不覆盖。
func write_new(path: String, bytes: PackedByteArray) -> bool:
	if not check(path.begins_with(evidence+"/") and not FileAccess.file_exists(path),"测试输出是自有新路径："+path): return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"测试输出可创建"): return false
	file.store_buffer(bytes); file.close()
	return check(FileAccess.get_file_as_bytes(path)==bytes,"测试输出逐字节读回")

## 按已冻结visitor的string头定位外层目录摘要；不解析或改写任何玩法域。
func foreign_binding(bytes: PackedByteArray) -> PackedByteArray:
	var magic: PackedByteArray="HM-RESIDENT-CHECKPOINT/1\n".to_utf8_buffer()
	if not check(bytes.slice(0,magic.size())==magic and bytes.size()>magic.size()+64,"外层checkpoint版本符合已冻结格式"): return PackedByteArray()
	var payload: PackedByteArray=bytes.slice(magic.size(),bytes.size()-64)
	var field: PackedByteArray="artifact_sha256".to_utf8_buffer()
	var marker: PackedByteArray=PackedByteArray([4]); var length=PackedByteArray(); length.resize(8); length.encode_u64(0,field.size()); marker.append_array(length); marker.append_array(field)
	var found: int=-1
	# 外层catalog在domains之前；限定前1024字节，拒绝误改域中的同名字符串。
	for offset in range(mini(1024,payload.size()-marker.size()-8)):
		if payload.slice(offset,offset+marker.size())==marker:
			if not check(found<0,"外层catalog摘要字段只有一个"): return PackedByteArray()
			found=offset+marker.size()
	if not check(found>=0 and payload.decode_u64(found)==64 and found+8+64<payload.size(),"冻结目录binding为64字节摘要"): return PackedByteArray()
	var first: int=found+8; payload[first]=48 if payload[first]!=48 else 49
	var result: PackedByteArray=magic.duplicate(); result.append_array(payload); result.append_array(digest(payload).to_utf8_buffer())
	return result

## 负例由真实当前Alpha检查点派生：内层损坏、未来包装及checksum成立的外来目录绑定。
func make_failure_slots(bytes: PackedByteArray) -> bool:
	var corrupt: PackedByteArray=bytes.duplicate(); var midpoint: int=corrupt.size()>>1; corrupt[midpoint]=corrupt[midpoint]^1
	var damaged: Dictionary=SlotCodec.encode(corrupt,1,"manual-2")
	var valid_previous: Dictionary=SlotCodec.encode(bytes,1,"manual-3")
	var future_base: Dictionary=SlotCodec.encode(bytes,2,"manual-3")
	var foreign: PackedByteArray=foreign_binding(bytes)
	if not check(damaged.get("ok",false) and valid_previous.get("ok",false) and future_base.get("ok",false) and not foreign.is_empty(),"负例只派生包装或外层身份，不造第二玩法oracle"): return false
	var parsed: Dictionary=Strict.parse_text(future_base.text,SlotCodec.MAX_BYTES)
	if not check(parsed.get("ok",false),"未来包装夹具源可严格解析"): return false
	var header: Dictionary=parsed.value; header.format_version=2
	var foreign_text: Dictionary=SlotCodec.encode(foreign,1,"manual-4")
	if not check(foreign_text.get("ok",false) and SlotCodec.inspect_metadata(foreign_text.get("text","")).get("ok",false),"外来binding由真实包装器编码，物理checksum校验成立"): return false
	return write_new(directory.path_join("manual-2.A.hmcheckpoint"),damaged.text.to_utf8_buffer()) and write_new(directory.path_join("manual-3.A.hmcheckpoint"),valid_previous.text.to_utf8_buffer()) and write_new(directory.path_join("manual-3.B.hmcheckpoint"),JSON.stringify(header,"",true,true).to_utf8_buffer()) and write_new(directory.path_join("manual-4.A.hmcheckpoint"),foreign_text.text.to_utf8_buffer())

## 第一进程走当前已安装Alpha、一次启程和一个显式半日，再经真实Main保存。
func produce(main: Node) -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"生产进程profile从未存在，不碰玩家数据"): return false
	var installed: Dictionary=main.start_content("scenario.alpha_life",catalog_path)
	if not check(installed.get("ok",false) and await wait_idle(main),"已安装的当前Alpha经原Main冷安装并发布有限frame"): return false
	main._map_select("village.east")
	if not check(await wait_idle(main) and main._map_quote().get("ok",false),"实际默认Alpha路线可报价"): return false
	main._map_confirm()
	if not check(await wait_idle(main) and main.kernel.action_context().get("action_kind")=="travel","原Main实际启程"): return false
	main._perform("advance",{})
	if not check(await wait_idle(main) and main.kernel.source_identity().tick==1,"生产保存前只显式推进一半日"): return false
	var source: Dictionary=main.kernel.source_identity(); var state_sha: String=world_digest(main)
	var opened: Dictionary=main._open_slots("save")
	if not check(opened.get("ok",false),"原Main接受保存槽列表"): return false
	var deadline: int=Time.get_ticks_usec()+15000000
	while not is_instance_valid(main._slot_dialog) and Time.get_ticks_usec()<deadline: await process_frame
	if not check(is_instance_valid(main._slot_dialog),"真实保存窗口完成"): return false
	if not confirm_slot(main,"manual-1"): return false
	if not check(await wait_idle(main) and main.last_storage_result.get("ok",false) and main._save_generation==1,"原Main冷验证、CAS银行写入和读回成功"): return false
	if not check(main.kernel.source_identity()==source and world_digest(main)==state_sha,"保存不推进或替换世界"): return false
	var selected: Dictionary=main.storage.native.inspect_slot("manual-1")
	var decoded: Dictionary=SlotCodec.decode_payload(selected.get("text",""))
	if not check(selected.get("ok",false) and decoded.get("ok",false),"读回实际被选原生银行"): return false
	var original: Dictionary=bank_hashes()
	if not make_failure_slots(decoded.payload): return false
	for name in original:
		if not check(bank_hashes().get(name)==original[name],"构造负例后有效原银行保留："+name): return false
	var manifest: Dictionary={"profile":profile,"directory":directory,"producer_pid":OS.get_process_id(),"identity":source,"world_sha256":state_sha,"checkpoint_sha256":digest(decoded.payload),"catalog_path":catalog_path,"catalog_sha256":FileAccess.get_sha256(catalog_path),"banks":bank_hashes(),"valid_banks":original}
	observed.producer=manifest
	return write_new(evidence.path_join("title-manifest.json"),JSON.stringify(manifest,"",true,true).to_utf8_buffer())

## 取消和失败都必须释放未显示候选；等待期间每一帧都检查没有提前启动世界。
func wait_retired(main: Node, title_kernel_id: int) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	var candidates: Array[WeakRef]=[]
	while Time.get_ticks_usec()<deadline:
		if main._started or main._resident_mode: return check(false,"取消或失败不得激活默认世界")
		if main.kernel.get_instance_id()!=title_kernel_id or not main.kernel.snapshot().is_empty(): return check(false,"失败或取消必须保持原空标题Kernel")
		if is_instance_valid(main._resident_load_candidate): candidates.append(weakref(main._resident_load_candidate))
		var retired: bool=true
		for candidate in candidates:
			if candidate.get_ref()!=null: retired=false
		if retired and not is_instance_valid(main._resident_load_candidate) and not main.get("_resident_bootstrap_loading") and not main.storage._busy and main._storage_intent.is_empty():
			return check(main.find_child("WorldMapCanvas",true,false)==null,"未发布候选已退休，仍只显示空标题")
		await process_frame
	return check(false,"未发布候选在有界等待内退休")

## 原生冷检查与候选完整restore分别守住拒绝边界；UI不能退读别槽或建立默认新局。
func rejected_restore(main: Node, slot: String, code: String, expected_banks: Dictionary) -> bool:
	if not await open_title_slots(main): return false
	var title_kernel_id: int=main.kernel.get_instance_id()
	if not confirm_slot(main,slot): return false
	if not check(await wait_retired(main,title_kernel_id),"失败恢复退休候选："+slot): return false
	return check(main.last_storage_result.get("ok",true)==false and main.last_storage_result.get("code")==code and bank_hashes()==expected_banks,"真实拒绝码、标题与全部原银行保持："+code)

## 第二进程从空标题开始，先验取消和拒绝，再恢复第一进程原银行。
func consume(main: Node) -> bool:
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(evidence.path_join("title-manifest.json")))
	if not check(loaded.get("ok",false) and loaded.get("value") is Dictionary,"重启进程只读取第一进程落盘清单"): return false
	var manifest: Dictionary=loaded.value
	if not check(manifest.profile==profile and manifest.directory==directory and bank_hashes()==manifest.banks and manifest.catalog_path==catalog_path and FileAccess.get_sha256(catalog_path)==manifest.catalog_sha256,"重启沿同一实际目录和原银行，不修改目录身份"): return false
	if not check(not main._started and not main._resident_mode,"新进程Main起于空标题，无已安装resident世界"): return false
	if not await open_title_slots(main): return false
	var first: int=main.kernel.get_instance_id()
	await process_frame
	if not check(main.kernel.get_instance_id()==first and main.kernel.snapshot().is_empty() and not is_instance_valid(main._resident_load_candidate),"仅列出不创建隐藏候选或默认世界"): return false
	var cancel: Button=button(main._slot_dialog,"slot_cancel")
	if not check(cancel!=null,"真实列表可取消"): return false
	cancel.pressed.emit()
	if not check(await wait_retired(main,first) and bank_hashes()==manifest.banks,"取消退休候选且全部银行原字节不变"): return false
	if not await rejected_restore(main,"manual-2","CheckpointDigestMismatch",manifest.banks): return false
	if not await open_title_slots(main): return false
	var future_title: int=main.kernel.get_instance_id()
	var future: Button=button(main._slot_dialog,"slot_select","manual-3")
	var inspected: Dictionary=main.storage.native.inspect_slot("manual-3")
	if not check(future!=null and future.disabled and not inspected.get("ok",true) and inspected.get("code")=="UnsupportedSaveVersion","未来版本明确禁选，不能回退同槽较旧正常银行"): return false
	button(main._slot_dialog,"slot_cancel").pressed.emit()
	if not check(await wait_retired(main,future_title) and bank_hashes()==manifest.banks,"未来版本拒绝后取消仍保全原银行"): return false
	if not await rejected_restore(main,"manual-4","CheckpointCatalogMismatch",manifest.banks): return false
	if not await open_title_slots(main): return false
	var title_kernel_id: int=main.kernel.get_instance_id()
	if not confirm_slot(main,"manual-1"): return false
	if not check(await wait_idle(main) and main.last_storage_result.get("ok",false),"重启Continue真实restore后才进入完整frame"): return false
	var resumed: Dictionary=main.kernel.source_identity()
	if not check(main.kernel.get_instance_id()!=title_kernel_id and resumed.execution_epoch>1 and not is_instance_valid(main._resident_load_candidate),"确认后创建的确切内容候选仅在成功恢复后激活并取得新执行epoch"): return false
	if not check(resumed.world_id==manifest.identity.world_id and resumed.tick==manifest.identity.tick and resumed.revision==manifest.identity.revision and world_digest(main)==manifest.world_sha256,"跨进程保留worldID、tick、revision及完整原世界"): return false
	if not check(main._resident_mode and not main.get("_resident_bootstrap_loading") and main._selected_slot=="manual-1" and main._save_generation==1 and not main._resident_adapter.auto_running and bank_hashes()==manifest.banks,"只在恢复后激活Main，没有旧自动许可或槽写入"): return false
	observed.consumer={"pid":OS.get_process_id(),"title_kernel_instance":title_kernel_id,"restored":resumed,"world_sha256":manifest.world_sha256,"banks":bank_hashes()}
	return true

## 从原受信安装索引选择唯一当前Alpha，不改包身份或回退散装默认目录。
func select_installed_alpha() -> bool:
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.get("ok",false),"当前安装索引及内容全部验证成功"): return false
	var candidates: Array=[]
	for catalog in installed.catalogs:
		if catalog.data.scenarios.has("scenario.alpha_life"): candidates.append(catalog)
	if not check(candidates.size()==1 and candidates[0].get("catalog_path") is String,"已安装当前Alpha有唯一确切目录"): return false
	catalog_path=candidates[0].catalog_path
	return true

## 显式双阶段入口；只准user://落在本次证据根下，以免误用真实玩家profile。
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--title-phase="): phase=arg.trim_prefix("--title-phase=")
		elif arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=").simplify_path().trim_suffix("/")
	if not check(phase in ["produce","consume"] and evidence.is_absolute_path() and DirAccess.dir_exists_absolute(evidence),"需显式produce/consume及已创建的新证据根"): quit(2); return
	profile="resident-title-"+evidence.sha256_text().left(16)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile=") and not check(arg.trim_prefix("--profile=")==profile,"命令行不得改写隔离profile"): quit(2); return
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with(evidence+"/") and not FileAccess.file_exists(evidence.path_join(phase+"-report.json")),"运行数据及结果只在本次新测试根，禁止盲重放"): quit(2); return
	ProjectSettings.set_setting("development/native_resident_world_enabled",true)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"匹配当前生产源码的真实Bridge已注册"): quit(2); return
	var main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=false
	if select_installed_alpha():
		if phase=="produce": passed=await produce(main)
		else: passed=await consume(main)
	main.queue_free(); await process_frame
	var report: Dictionary={"ok":passed and failures==0,"phase":phase,"checks":checks,"failures":failures,"observed":observed,"scope":"Actual current-Alpha Main producer and fresh-process title restore; no historical migration or pixel acceptance; foreign fixture changes only checkpoint catalog binding"}
	var file=FileAccess.open(evidence.path_join(phase+"-report.json"),FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify(report,"",true,true)+"\n"); file.close()
	print("RESIDENT_MAIN_TITLE_PHASE=",phase," CHECKS=",checks," FAILURES=",failures," OK=",report.ok)
	quit(0 if report.ok else 1)
