## 真实Main、StorageService与Native槽位菜单短门；独立profile和正常v2输入，不替换世界规则。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Service=preload("res://storage/storage_service.gd")
const Native=preload("res://storage/native_storage.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0
var catalogs: Array=[]
var profiles: Array=[]

## 所有请求仍由原Service执行；只观察已接受的操作和独立参数副本。
class RecordingStorage extends Service:
	var accepted: Array=[]
	## 不替换Native结果或deferred完成时序，拒绝请求也不假报已执行。
	func request(operation: String, args: Dictionary={}) -> Dictionary:
		var result: Dictionary=super.request(operation,args)
		if result.ok: accepted.append({"operation":operation,"args":args.duplicate(true)})
		return result

## 场景树就绪后才建立实际Window与Storage节点。
func _init() -> void:
	call_deferred("run")

## 每项失败进入终态计数与进程退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
	return value

## 有界等待真实异步存储完成和控件释放，不推进世界时间。
func settle(host: Control) -> void:
	for unused in range(30):
		await process_frame
		if not host.storage._busy and host._storage_intent.is_empty():
			await process_frame
			return
	check(false,"真实Storage deferred应在30帧内结束")

## 只按生产按钮语义标记查找当前真实控件。
func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 观察实际人工槽候选，保存只建议第一个空槽，加载不凭空生成空记录。
func manual_buttons(host: Control) -> Array:
	var result: Array=[]
	for node in host.find_children("*","Button",true,false):
		if node.get_meta("command_kind","")=="slot_select": result.append(node.get_meta("target_id",""))
	return result

## 每个案例使用全新独立profile，保留所有银行与诊断供复核。
func mount(pack_index: int=0, profile: String=""):
	if profile.is_empty(): profile="slots-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	profiles.append(profile)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	var host=Main.new()
	host.storage.free(); host.storage=RecordingStorage.new()
	root.add_child(host)
	host.set_meta("test_profile",profile)
	if pack_index>=0: begin(host,pack_index)
	else: host.storage.configure({})
	return host

## 正常启动指定已安装剧本；没有构造或注入假Kernel状态。
func begin(host: Control, pack_index: int) -> bool:
	var catalog: Dictionary=catalogs[pack_index]
	return check(host.start_content(catalog.data.scenarios.keys()[0],catalog.catalog_path).get("ok",false),"真实Main/Kernel开启内容集："+catalog.pack_set_digest)

## 银行原字节摘要包括意外.tmp文件，菜单只读或失败不得改变任何一项。
func hashes(host: Control) -> Dictionary:
	var result: Dictionary={}
	var directory: String=host.storage.native.directory
	if not DirAccess.dir_exists_absolute(directory): return result
	for name in DirAccess.get_files_at(directory): result[name]=FileAccess.get_sha256(directory.path_join(name))
	return result

## 记录真实写请求数量，避免文件最终相同掩盖一次不该发生的写入。
func puts(host: Control) -> int:
	var count: int=0
	for entry in host.storage.accepted:
		if entry.operation=="put": count+=1
	return count

## 原Native与Codec生成可完整decode的正常v2输入；生成目录和UI实际profile分离。
func seed_slot(host: Control, slot: String, context: Dictionary, generation: int=1) -> Dictionary:
	var text: String=Codec.encode(context.state,generation,slot,context.catalog,context.registry)
	check(not text.is_empty() and Codec.inspect_metadata(text).get("metadata",{}).get("format_version")==2,"正常Kernel上下文编码v2："+slot)
	var native=Native.new(host.storage.native.directory+"-inputs",context.catalog,context.registry)
	var result: Dictionary=native.put(slot,text,generation-1)
	check(result.get("ok",false),"原Native在输入目录生成正常银行："+slot+" "+str(result.get("code","")))
	if result.get("ok",false):
		DirAccess.make_dir_recursive_absolute(host.storage.native.directory)
		var filename: String=slot+"."+result.suffix+".hmsave"
		check(DirAccess.copy_absolute(native.directory.path_join(filename),host.storage.native.directory.path_join(filename))==OK,"只复制正常v2原银行到独立UI profile："+slot)
	return result

## 实际list完成后才查找生产Window，列表和浏览不能写槽或改变世界。
func open_slots(host: Control, mode: String) -> Window:
	var state: Dictionary=host.kernel.snapshot()
	var before: Dictionary=hashes(host)
	var writes: int=puts(host)
	var accepted: Dictionary=host._open_slots(mode)
	check(accepted.get("ok",false),"接受真实选槽list请求："+mode+" "+str(accepted))
	await settle(host)
	var dialog=host._slot_dialog
	check(dialog is Window and dialog.visible and dialog.is_inside_tree() and not dialog.is_queued_for_deletion(),"list完成后当前真实Window可见，允许旧窗口待释放时引擎自动改名")
	check(host.kernel.snapshot()==state and hashes(host)==before and puts(host)==writes,"打开与浏览槽列表保持完整world/RNG和银行字节")
	var expected: Array=[]
	for index in range(1,11): expected.append("manual-"+str(index))
	var shown: Array=manual_buttons(host)
	var unique: Dictionary={}
	for id in shown: unique[id]=true
	check(shown.all(func(id): return id in expected) and shown.size()==unique.size(),"候选只用固定manual-1至manual-10且无重复")
	for row in host.storage.native.list_slots().slots:
		if not row.ok: continue
		var inspected: Dictionary=host.storage.native.inspect_slot(row.slot)
		var path: String=host.storage.native.directory.path_join(row.slot+"."+inspected.suffix+".hmsave")
		var modified: int=FileAccess.get_modified_time(path)
		check(row.get("modified_unix")==modified and modified>0,"列表保存时间精确来自已选银行文件："+row.slot)
		var displayed: Button=button(host,"slot_select",row.slot)
		var stamp: String=Time.get_datetime_string_from_unix_time(modified).replace("T"," ")+" UTC"
		check(displayed!=null and stamp in displayed.text,"UI显示同一真实文件保存时间而非游戏日期："+row.slot)
	return dialog

## 选择和确认分别触发真实pressed，选择本身不能提前持久化。
func select_slot(host: Control, slot: String) -> Button:
	var before: Dictionary=hashes(host)
	var writes: int=puts(host)
	var selected: Button=button(host,"slot_select",slot)
	if not check(selected!=null,"目标槽有真实按钮："+slot): return null
	selected.pressed.emit()
	await settle(host)
	check(hashes(host)==before and puts(host)==writes,"仅选择目标槽尚不写入："+slot)
	var confirm: Button=button(host,"slot_confirm")
	check(confirm!=null,"实际选择后可找到确认控件")
	return confirm

## 取消实际窗口并核对完整世界与银行未变化。
func cancel(host: Control) -> void:
	var before: Dictionary=hashes(host)
	var world: Dictionary=host.kernel.snapshot()
	var writes: int=puts(host)
	var target: Button=button(host,"slot_cancel")
	if check(target!=null,"实际菜单提供取消按钮"):
		target.pressed.emit(); await settle(host)
	check(hashes(host)==before and host.kernel.snapshot()==world and puts(host)==writes,"取消不写入、不推进world/RNG")

## 失败确认必须同时守住世界、银行和Service写请求三个边界。
func rejected_confirm(host: Control, confirm: Button, label: String) -> void:
	if confirm==null: return
	var world: Dictionary=host.kernel.snapshot()
	var before: Dictionary=hashes(host)
	var writes: int=puts(host)
	confirm.pressed.emit(); await settle(host)
	check(host.kernel.snapshot()==world and hashes(host)==before and puts(host)==writes,label+"保留完整world/RNG与所有银行且未请求put")

## 新局两个内容集分别选择新槽并保存；冷继续仍由选择槽的元信息精确恢复。
func check_new_and_two_packs() -> void:
	var host=mount()
	var first: Dictionary=host.kernel.save_context()
	await open_slots(host,"save")
	var confirm: Button=await select_slot(host,"manual-1")
	if confirm!=null:
		confirm.pressed.emit(); await settle(host)
	check(host.last_storage_result.get("ok",false) and puts(host)==1,"新槽确认只执行一次真实put")
	var one: Dictionary=Native.new(host.storage.native.directory,first.catalog,first.registry).get_slot("manual-1")
	check(one.get("ok",false) and one.generation==1 and Codec.decode(one.text,first.catalog,first.registry).state==first.state,"空槽expected0生成第一代且完整状态相同")
	var first_hashes: Dictionary=hashes(host)
	if not begin(host,1): host.queue_free(); return
	var second: Dictionary=host.kernel.save_context()
	var export_path: String=host.storage.native.directory+"-second-export.hmsave"
	var export_file=FileAccess.open(export_path,FileAccess.WRITE)
	export_file.store_string(Codec.encode(second.state,0,"export",second.catalog,second.registry)); export_file.close()
	check(host.import_snapshot(export_path).get("ok",false) and hashes(host)==first_hashes,"实际v2导入后尚未写本机槽")
	host._save(); await settle(host)
	check(is_instance_valid(host._slot_dialog) and host._slot_dialog.visible and hashes(host)==first_hashes,"保存入口打开列表且不自动覆盖manual-1")
	confirm=await select_slot(host,"manual-2")
	if confirm!=null: confirm.pressed.emit(); await settle(host)
	var two: Dictionary=Native.new(host.storage.native.directory,second.catalog,second.registry).get_slot("manual-2")
	check(two.get("ok",false) and two.generation==1 and Codec.decode(two.text,second.catalog,second.registry).state==second.state,"第二内容集保存到明确所选新槽")
	var kept: Dictionary=Native.new(host.storage.native.directory,first.catalog,first.registry).get_slot("manual-1")
	check(kept.get("text")==one.get("text") and first.catalog.pack_set_digest!=second.catalog.pack_set_digest,"不同内容集银行并存且第一槽原字节保持")
	var profile: String=host.get_meta("test_profile")
	host.queue_free(); await process_frame
	var cold=mount(-1,profile)
	var disk: Dictionary=hashes(cold)
	cold._continue(); await settle(cold)
	check(not cold._started and cold.kernel.snapshot().is_empty() and hashes(cold)==disk,"冷继续先列槽，未自动恢复manual-1")
	confirm=await select_slot(cold,"manual-2")
	if confirm!=null: confirm.pressed.emit(); await settle(cold)
	check(cold._started and cold.kernel.snapshot()==second.state and cold.kernel.content()==second.catalog,"确认第二槽按真实元信息链恢复正确pack和完整世界")
	check(hashes(cold)==disk and puts(cold)==0,"继续读取不写任何槽")
	cold._load(); await settle(cold)
	check(button(cold,"slot_select","manual-1")!=null and cold.kernel.snapshot()==second.state,"游戏内读档也先显示选择列表")
	await cancel(cold)
	cold.queue_free(); await process_frame

## list显示代次、世界revision与UI epoch都必须冻结，旧候选不能借新状态写入。
func check_stale_and_cancel() -> void:
	var host=mount()
	var context: Dictionary=host.kernel.save_context()
	seed_slot(host,"manual-1",context)
	await open_slots(host,"save")
	seed_slot(host,"manual-1",context,2)
	var confirm: Button=await select_slot(host,"manual-1")
	await rejected_confirm(host,confirm,"list后generation已变")
	await open_slots(host,"save")
	confirm=await select_slot(host,"manual-2")
	var state: Dictionary=host.kernel.snapshot()
	var place: String=host.kernel.content().data.places.keys().filter(func(id): return id.ends_with("::west.grain"))[0]
	var result: Dictionary=host.kernel.submit({"kind":"move_local","params":{"place_id":place},"actor_id":state.player_actor_id,"request_seq":state.request_sequences[state.player_actor_id]+1,"expected_revision":state.revision})
	check(result.ok and host.kernel.snapshot().revision!=state.revision,"正常Kernel move_local改变候选世界revision")
	await rejected_confirm(host,confirm,"世界revision改变")
	await open_slots(host,"save")
	confirm=await select_slot(host,"manual-2")
	state=host.kernel.snapshot()
	host._render()
	check(host.kernel.snapshot()==state,"同revision真实重绘只改变UI epoch")
	await rejected_confirm(host,confirm,"同世界重绘后旧UI候选")
	await open_slots(host,"save")
	confirm=await select_slot(host,"manual-2")
	var before: Dictionary=hashes(host)
	var world: Dictionary=host.kernel.snapshot()
	var writes: int=puts(host)
	var cancel_button: Button=button(host,"slot_cancel")
	if check(cancel_button!=null,"取消候选存在真实按钮"):
		cancel_button.pressed.emit()
		# 控件尚活着时送达迟来信号，不能以已释放节点代替回调冻结检查。
		if is_instance_valid(confirm): confirm.pressed.emit()
		await settle(host)
	check(hashes(host)==before and host.kernel.snapshot()==world and puts(host)==writes,"取消后迟来确认仍不写入")
	host.queue_free(); await process_frame

## 满十槽只展示候选；没有明确选择与确认就不得自动回收或覆盖任何槽。
func check_full() -> void:
	var host=mount()
	var context: Dictionary=host.kernel.save_context()
	for index in range(1,11): seed_slot(host,"manual-"+str(index),context)
	var before: Dictionary=hashes(host)
	host._save(); await settle(host)
	check(manual_buttons(host).size()==10 and hashes(host)==before and puts(host)==0,"十槽满时仍只列出十槽，不自动删除或覆盖")
	await cancel(host)
	host.queue_free(); await process_frame

## 只在隔离profile写明示负例；坏格式和不匹配内容必须保留原字节。
func raw_bank(host: Control, slot: String, text: String) -> void:
	DirAccess.make_dir_recursive_absolute(host.storage.native.directory)
	var file=FileAccess.open(host.storage.native.directory.path_join(slot+".B.hmsave"),FileAccess.WRITE)
	check(file!=null,"独立负例银行可写")
	if file!=null: file.store_string(text); file.close()

## 缺包、坏格式与真实v1派生封装都不能通过保存选槽绕过旧档decode保护。
func check_protected_inputs() -> void:
	var host=mount(1)
	var other: Dictionary=host.kernel.save_context()
	seed_slot(host,"manual-7",other)
	if not begin(host,0): host.queue_free(); return
	await open_slots(host,"save")
	var foreign: Button=button(host,"slot_select","manual-7")
	check(foreign!=null and foreign.disabled and "IncompatibleContent" in foreign.tooltip_text,"另一已安装内容集准确分类不兼容，不能误称缺包或损坏")
	await cancel(host)
	raw_bank(host,"manual-8","{broken json")
	var original: String=FileAccess.get_file_as_string("res://tests/original-v1.hmsave")
	var old: Dictionary=Strict.parse_text(original,Codec.MAX_BYTES).value
	var original_payload: String=old.payload_json
	# 仅重绑历史封装槽名/代次；原payload字节不变，明确属于派生拒绝fixture。
	old.save_id="manual-9"; old.generation=1
	raw_bank(host,"manual-9",JSON.stringify(old,"",true,true))
	check(old.payload_json==original_payload and Codec.inspect_metadata(JSON.stringify(old)).get("ok",false),"实际v1正文保留原字节且封装仍合法，不称物理损坏")
	var index_path: String="res://data/generated/installed-content.json"
	var original_index: String=FileAccess.get_file_as_string(index_path)
	var index: Dictionary=Strict.parse_text(original_index).value
	index.sets=index.sets.filter(func(entry): return entry.pack_set_digest!=other.catalog.pack_set_digest)
	var file=FileAccess.open(index_path,FileAccess.WRITE); file.store_string(JSON.stringify(index,"",true,true)); file.close()
	for slot in ["manual-7","manual-8","manual-9"]:
		await open_slots(host,"save")
		var confirm: Button=await select_slot(host,slot)
		await rejected_confirm(host,confirm,"不可覆盖的既有槽："+slot)
	await open_slots(host,"load")
	var missing_confirm: Button=await select_slot(host,"manual-7")
	await rejected_confirm(host,missing_confirm,"缺包读档")
	var missing_row: Button=button(host,"slot_select","manual-7")
	check(missing_row!=null and missing_row.disabled and "MissingPack" in missing_row.tooltip_text,"缺包加载沿元信息解析明确禁用并保留MissingPack诊断")
	await open_slots(host,"load")
	for slot in ["manual-7","manual-9"]:
		var row: Button=button(host,"slot_select",slot)
		check(row!=null and not "损坏" in row.text,"缺包与未知v1的UI槽描述不称损坏："+slot)
	await cancel(host)
	var listed: Dictionary=host.storage.native.list_slots()
	for entry in listed.slots:
		if entry.slot in ["manual-7","manual-9"]:
			check(entry.ok and not entry.get("physical_corruption",false),"缺包/真实v1列表保留元信息，不误标损坏："+entry.slot)
	file=FileAccess.open(index_path,FileAccess.WRITE); file.store_string(original_index); file.close()
	check(Catalog.installed_catalogs().get("ok",false),"隔离索引原字节恢复后两个内容集仍可验证")
	host.queue_free(); await process_frame

## 物理封装和包身份均合法但正文人物状态非法时，必须到确认后的完整decode拒写。
func check_semantic_decode() -> void:
	var host=mount()
	var context: Dictionary=host.kernel.save_context()
	var header: Dictionary=Strict.parse_text(Codec.encode(context.state,1,"manual-1",context.catalog,context.registry),Codec.MAX_BYTES).value
	var body: Dictionary=Strict.parse_text(header.payload_json).value
	body.actors[body.player_actor_id].hp=-1
	header.payload_json=JSON.stringify(body,"",true,true)
	header.payload_length=header.payload_json.to_utf8_buffer().size()
	header.payload_sha256=header.payload_json.sha256_text()
	var text: String=JSON.stringify(header,"",true,true)
	var decoded: Dictionary=Codec.decode(text,context.catalog,context.registry)
	check(Codec.inspect_metadata(text).get("ok",false) and not decoded.get("ok",false) and not decoded.get("code") in ["ChecksumMismatch","InvalidSaveHeader","MissingPack"],"明示负例物理合法且同包，非法hp只在完整decode拒绝")
	raw_bank(host,"manual-1",text)
	await open_slots(host,"save")
	var selected: Button=button(host,"slot_select","manual-1")
	check(selected!=null and not selected.disabled,"列表元信息通过，不能用禁用按钮掩盖正文复核")
	var confirm: Button=await select_slot(host,"manual-1")
	var reads: int=host.storage.accepted.size()
	await rejected_confirm(host,confirm,"旧槽非法正文的完整decode")
	check(host.storage.accepted.size()==reads+1 and host.storage.accepted[-1].operation=="inspect","确认真实复读同槽后到decode，尚未请求put")
	check(host.last_storage_result.get("code")==decoded.get("code"),"保留完整Codec的语义拒绝码："+str(decoded.get("code")))
	host.queue_free(); await process_frame

## 只执行这个新门；全部日志与实际profile留本地，无GUI或无关主题回归。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/slot_menu_isolated",false):
		printerr("FAIL: requires dedicated slot-selection fixture project"); quit(1); return
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.get("ok",false) and installed.get("catalogs",[]).size()==2,"真实安装索引包含两套完整可验证内容集："+str(installed.get("code",""))):
		quit(1); return
	catalogs=installed.catalogs
	catalogs.sort_custom(func(a,b): return a.pack_set.packs[0].id<b.pack_set.packs[0].id)
	var art: Dictionary=Art.prepare_content(catalogs[0])
	if not check(art.get("ok",false),"受信表现与tiny资源通过真实Art校验"):
		quit(1); return
	Art.commit_content(art)
	if "--decode-only" in OS.get_cmdline_user_args():
		await check_semantic_decode()
	elif "--classification-only" in OS.get_cmdline_user_args():
		await check_protected_inputs()
		await check_semantic_decode()
	else:
		await check_new_and_two_packs()
		await check_stale_and_cancel()
		await check_full()
		await check_protected_inputs()
		await check_semantic_decode()
	print("SLOT_SELECTION_MENU_CHECKS=",checks," FAILURES=",failures," PROFILES=",profiles)
	print("REAL_MAIN_NATIVE_KERNEL=true; OS_VISUAL=NOT_RUN; WEB=NOT_RUN")
	quit(0 if failures==0 else 1)
