## 独立测试工程中的真实标题继续/文件导入；完整Host/UI与规则核心，不替换生产实现。
extends SceneTree
const Host=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
const Native=preload("res://storage/native_storage.gd")
const SaveFiles=preload("res://storage/save_files.gd")
const Strict=preload("res://core/strict_json.gd")
const Art=preload("res://ui/art_catalog.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const LEGACY_SHA="153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690"
const LEGACY_LOCK="13d450c74a6c9cc647c62cc5f93a42789a187f8e000f5b0584bc1ba6bf51f892"
const LEGACY_DIGEST="f0017626a226b2c1b322b55af128d722943e3d9da086ee3cdb1e5fbe99bc4516"
const EXPECTED_LOCK="9a3c82803e0f8effc26f061f24f82fb933a48b77e10b0b40c412ba6ace844a23"
var checks: int=0
var failures: int=0
var hosts: Array=[]
var profile: String=""
var directory: String=""
var completed_operations: Array=[]

## 只计数真正new_game并调用原实现；没有伪造Kernel结果、状态、规则或存储。
class CountedKernel extends Kernel:
	var new_game_calls: int=0
	## 任何隐含默认开局都被记录，随后仍执行实际生产入口。
	func new_game(scenario_id: String="scenario.two_stories", content_path: String="res://data/generated/two_stories.hmcatalog.json", appearance_draft: Dictionary={}, player_actor_id: String="") -> Dictionary:
		new_game_calls+=1
		return super.new_game(scenario_id,content_path,appearance_draft,player_actor_id)

## 原Host只加默认启动入口计数，保留_ready、所有节点、渲染、信号与恢复实现。
class CountedHost extends Host:
	var default_start_calls: int=0
	## 旧的默认上下文路径若被误调用，计数后继续实际行为，不掩盖缺陷。
	func _start_default_content() -> Dictionary:
		default_start_calls+=1
		return super._start_default_content()

## 等引擎准备好后构建真实节点和独立profile。
func _init() -> void:
	call_deferred("run")

## 所有失败均记录并造成最终非零退出。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok:
		failures+=1
		printerr("FAIL: ",label)

## 返回真实控件的语义按钮，测试按信号进入实际Host回调。
func button(node: Node, kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 只观察真实StorageService发出的完成信号，不替代结果或短接异步回调。
func stored(operation: String, result: Dictionary) -> void:
	completed_operations.append({"operation":operation,"ok":result.get("ok",false)})

## 加入完整Host节点之前替换为只计数的真实Kernel子类。
func cold_host():
	var host=CountedHost.new()
	host.kernel=CountedKernel.new()
	root.add_child(host); hosts.append(host)
	host.storage.completed.connect(stored)
	return host

## 有界等待真实deferred存储回调，不忙等或固定延长轨迹。
func settle(host) -> void:
	for unused in range(30):
		if host._storage_intent.is_empty() and not host.storage._busy: return
		await process_frame
	check(false,"真实存储回调应在30帧内结束")

## 银行完整原字节用SHA核对，失败不允许覆盖或新增临时文件。
func slot_hashes() -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(directory): result[name]=FileAccess.get_sha256(directory.path_join(name))
	return result

## 仅在显式隔离工程中改临时fixture或拷贝的索引，测试不会写共享staging。
func write_bytes(path: String, bytes: PackedByteArray) -> void:
	var file=FileAccess.open(path,FileAccess.WRITE)
	check(file!=null,"隔离测试路径可写："+path)
	if file!=null: file.store_buffer(bytes); file.close()

## 比较全state/catalog（包含RNG和身份）以及真实银行字节，不能只比一个计数。
func unchanged(host, before: Dictionary, hashes: Dictionary, label: String) -> void:
	check(host.kernel.snapshot()==before.state and host.kernel.content()==before.catalog,label+"保留完整state/catalog/RNG")
	check(slot_hashes()==hashes,label+"保留原银行字节")
	check(host.kernel.new_game_calls==0 and host.default_start_calls==0,label+"未经过默认开局")

## 真正安装产物写v2后，经标题继续与文件导入恢复；失败路径只在本工程副本造反例。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: test_restore_menu requires an explicitly isolated generated-content copy"); quit(1); return
	if "--legacy-only" in OS.get_cmdline_user_args():
		await run_legacy(); return
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==1,"真实P安装索引及文件摘要通过")
	if not installed.get("ok",false) or installed.get("catalogs",[]).size()!=1: finish(); return
	var original: Dictionary=installed.catalogs[0]
	check(original.pack_set_digest==EXPECTED_LOCK,"使用指定已验证真实9a3c内容集")
	var art: Dictionary=Art.prepare_content(original)
	check(art.get("ok",false),"真实受信presentation.schema与tiny表现资源通过")
	if not art.get("ok",false): printerr(art); finish(); return
	Art.commit_content(art)
	var source=Kernel.new()
	var started: Dictionary=source.new_game(original.data.scenarios.keys()[0],original.catalog_path)
	check(started.get("ok",false),"源世界来自真正Kernel.new_game，不是伪state")
	if not started.get("ok",false): printerr(started); finish(); return
	var context: Dictionary=source.save_context()
	profile="a2-restore-menu-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory="user://profiles/"+profile+"/saves"
	var native=Native.new(directory,context.catalog,context.registry)
	var text: String=Codec.encode(context.state,1,"manual-1",context.catalog,context.registry)
	check(native.put("manual-1",text,0).get("ok",false),"实际Native写出独立profile的v2银行")
	var hashes: Dictionary=slot_hashes()
	var host=cold_host()
	await process_frame
	check(host.kernel.snapshot().is_empty() and not host._started and host.find_child("TitleStage",true,false)!=null,"冷Host构建真实标题节点且未创建世界")
	var continue_button: Button=button(host,"continue")
	check(continue_button!=null,"标题存在真实继续按钮")
	if continue_button==null: finish(); return
	continue_button.pressed.emit()
	var active_ticket: int=host.storage._request_id
	var busy_result: Dictionary=host._request_storage("import","import",{})
	check(busy_result.get("code")=="StorageBusy" and host._storage_intent=="load" and host._continuing,"已有Main intent拒绝第二发起时保留原load与continuing")
	check(host.storage._busy and host.storage._request_id==active_ticket and completed_operations.is_empty(),"已有Main intent拒绝不改原Service票据或伪造完成")
	continue_button.pressed.emit()
	await settle(host)
	check(completed_operations.size()==1 and completed_operations[0].operation=="inspect","重复点击只完成一次Native inspect及真实回调")
	check(host.last_storage_result.get("ok",false) and host._started and host._save_generation==1,"冷标题继续真实恢复并报告正确generation")
	check(host.kernel.snapshot()==context.state and host.kernel.content()==context.catalog,"继续后完整世界、有效目录、RNG与存档上下文相等")
	check(host.kernel.new_game_calls==0 and host.default_start_calls==0,"继续过程中默认启动及Kernel.new_game调用均为零")
	check(host.find_child("WorldStatus",true,false)!=null and host.find_child("PlaceStage",true,false)!=null,"成功后真实UI显示世界节点")
	check(slot_hashes()==hashes,"读取未修改银行或创建临时档")
	var exported: String=directory.path_join("source-export.hmsave")
	check(SaveFiles.write_world(context.state,exported,context.catalog,context.registry).get("ok",false),"真实SaveFiles生成实际v2导入文件")
	hashes=slot_hashes()
	var importer=cold_host()
	await process_frame
	check(importer.kernel.snapshot().is_empty() and not importer._started,"文件导入前第二Host也没有默认世界")
	var imported: Dictionary=importer.import_snapshot(exported)
	check(imported.get("ok",false) and importer._started and importer.kernel.snapshot()==context.state,"import_snapshot读取实际v2文件并完整恢复")
	check(importer.kernel.new_game_calls==0 and importer.default_start_calls==0 and importer._save_generation==-1,"导入不经过默认开局且不假称已保存本机槽")
	check(importer.find_child("WorldStatus",true,false)!=null and importer._message.begins_with("已导入旅途"),"实际UI显示导入成功与恢复后的世界")
	var before: Dictionary=importer.kernel.save_context()
	unchanged(importer,before,hashes,"成功导入读取")
	var invalid: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(exported),Codec.MAX_BYTES).value
	invalid.pack_set_digest="0".repeat(64)
	var bad_path: String=directory.path_join("bad-content.hmsave")
	write_bytes(bad_path,JSON.stringify(invalid,"",true,true).to_utf8_buffer()); hashes=slot_hashes()
	var rejected: Dictionary=importer.import_snapshot(bad_path)
	check(not rejected.get("ok",false) and rejected.get("code")=="PackDigestMismatch" and importer._started and importer._message.begins_with("导入失败"),"坏内容身份明确失败，UI不误报成功")
	unchanged(importer,before,hashes,"坏内容导入")
	var index_path: String="res://data/generated/installed-content.json"
	var index_bytes: PackedByteArray=FileAccess.get_file_as_bytes(index_path)
	var empty_index: Dictionary=Strict.parse_text(index_bytes.get_string_from_utf8()).value; empty_index.sets=[]
	write_bytes(index_path,JSON.stringify(empty_index,"",true,true).to_utf8_buffer())
	rejected=importer.import_snapshot(exported)
	check(not rejected.get("ok",false) and rejected.get("code")=="MissingPack" and importer._message.begins_with("导入失败"),"缺包导入明确MissingPack，不能回默认内容")
	unchanged(importer,before,hashes,"缺包导入")
	var missing=cold_host()
	await process_frame
	button(missing,"continue").pressed.emit()
	await settle(missing)
	write_bytes(index_path,index_bytes)
	check(not missing.last_storage_result.get("ok",false) and missing.last_storage_result.get("code")=="MissingPack" and not missing._started and missing.kernel.snapshot().is_empty(),"冷标题缺包继续失败且保持未启动")
	check(missing._message.begins_with("读档失败") and missing.default_start_calls==0 and missing.kernel.new_game_calls==0,"缺包标题正确报错，无默认开局或成功文案")
	check(slot_hashes()==hashes,"缺包继续不改变原槽字节")
	check(FileAccess.get_file_as_bytes(index_path)==index_bytes and Catalog.installed_catalogs().get("ok",false),"隔离安装索引原字节已恢复且再次验证成功")
	unchanged(importer,before,hashes,"全部失败后")
	var busy_host=cold_host()
	await process_frame
	busy_host.storage.configure({})
	var completed_before: int=completed_operations.size()
	var listed: Dictionary=busy_host.storage.request("list",{})
	check(listed.get("ok",false),"真实Service接受list请求")
	check(busy_host.storage._busy and busy_host._storage_intent.is_empty(),"真实list回调尚在途而Main无intent")
	button(busy_host,"continue").pressed.emit()
	check(busy_host.last_storage_result.get("code")=="StorageBusy" and busy_host._storage_intent.is_empty() and not busy_host._continuing,"continue被Service拒绝后撤销本次intent并复位continuing")
	check(busy_host.storage._busy and busy_host.storage._request_id==listed.request_id and completed_operations.size()==completed_before,"拒绝continue未冒充list完成或改变原票据")
	await settle(busy_host)
	check(completed_operations.size()==completed_before+1 and completed_operations.back().operation=="list","原list随后只完成一次且操作身份保留")
	check(busy_host.last_storage_result.get("code")=="StorageBusy" and busy_host.kernel.snapshot().is_empty() and busy_host.kernel.content().is_empty() and not busy_host._started and busy_host._storage_intent.is_empty() and not busy_host._continuing,"原list完成被Main忽略，不误加载世界或悬挂状态")
	check(busy_host.default_start_calls==0 and busy_host.kernel.new_game_calls==0,"Service忙拒绝全过程没有默认开局")
	check(slot_hashes()==hashes,"Service忙拒绝及原list完成均不改银行字节")
	finish()

## 真4f16935原档经六canonical目录冷导入；继续仅重绑副本槽名，正文、规则与版本不改。
func run_legacy() -> void:
	var source_path: String="res://tests/fixtures/legacy-original.hmsave"
	check(FileAccess.get_sha256(source_path)==LEGACY_SHA,"真实4f16935捕获档原字节SHA匹配，未伪造历史版本")
	if FileAccess.get_sha256(source_path)!=LEGACY_SHA: finish(); return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(source_path)
	var text: String=bytes.get_string_from_utf8()
	var inspected: Dictionary=Codec.inspect_metadata(text)
	check(inspected.get("ok",false) and inspected.get("metadata",{}).get("format_version")==1,"真实v1封装与payload原字节checksum通过")
	if not inspected.get("ok",false): printerr(inspected); finish(); return
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==6,"P六header tiny固定映射由真实installed索引逐文件校验")
	if not installed.get("ok",false): printerr(installed); finish(); return
	var registry_result: Dictionary=Kernel.new().prepare_restore_registry()
	check(registry_result.get("ok",false),"使用正式Kernel准备的受信规则registry")
	if not registry_result.get("ok",false): finish(); return
	var registry: Dictionary=registry_result.registry
	var matched: Dictionary=Resolver.resolve(inspected.metadata,installed.catalogs,registry)
	check(matched.get("ok",false) and matched.get("catalog",{}).get("pack_set_digest")==LEGACY_LOCK,"旧Alpha三元组精确选archive canonical 13d450而非同id/version的当前Alpha")
	if not matched.get("ok",false): printerr(matched); finish(); return
	var original: Dictionary=matched.catalog
	check(original.content_id=="foundation.alpha_life" and original.content_version=="0.7.0" and original.content_digest==LEGACY_DIGEST,"旧Alpha完整内容身份保持原捕获值")
	var selection: Dictionary=Codec.inspect_restore_selection(text)
	check(selection.get("ok",false),"正式Codec从旧档规则迁移读取候选founder")
	if not selection.get("ok",false): printerr(selection); finish(); return
	var effective: Dictionary=Snapshot.resolve(original,selection.scenario_id,selection.founder_actor_id)
	check(effective.get("ok",false),"原canonical目录按真实旧档founder作有效视图")
	if not effective.get("ok",false): printerr(effective); finish(); return
	var decoded: Dictionary=Codec.decode(text,effective.catalog,registry)
	check(decoded.get("ok",false),"正式Codec对真实旧档迁移并完整Schema/Family校验作为基准")
	if not decoded.get("ok",false): printerr(decoded); finish(); return
	var expected: Dictionary=decoded.state
	var art: Dictionary=Art.prepare_content(original)
	check(art.get("ok",false),"canonical绑定的tiny表现通过真实Art校验，仅节点边界证据")
	if not art.get("ok",false): printerr(art); finish(); return
	Art.commit_content(art)
	profile="a2-legacy-menu-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory="user://profiles/"+profile+"/saves"
	check(DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(directory))==OK,"真实旧档只复制到独立native profile")
	var import_path: String=directory.path_join("04-after-adoption.hmsave")
	write_bytes(import_path,bytes)
	write_bytes(directory.path_join("manual-1.A.hmsave"),bytes)
	var hashes: Dictionary=slot_hashes()
	var importer=cold_host()
	await process_frame
	check(importer.kernel.snapshot().is_empty() and not importer._started,"旧档导入从真实冷标题进入")
	var imported: Dictionary=importer.import_snapshot(import_path)
	check(imported.get("ok",false) and importer._started and importer._message.begins_with("已导入旅途"),"冷Main.import_snapshot直接读取逐字原v1并成功恢复")
	if not imported.get("ok",false): printerr(imported); finish(); return
	check(importer.kernel.snapshot()==expected and importer.kernel.content()==effective.catalog,"原v1导入整态/RNG/身份与正式Codec基准及canonical有效目录等值")
	check(importer.default_start_calls==0 and importer.kernel.new_game_calls==0 and importer._save_generation==-1,"原v1导入无默认newgame且不冒称已写本机槽")
	check(slot_hashes()==hashes and FileAccess.get_sha256(import_path)==LEGACY_SHA,"原v1导入只读，所有槽及原封装字节不变")
	var host=cold_host()
	await process_frame
	button(host,"continue").pressed.emit()
	await settle(host)
	check(host.last_storage_result.get("code")=="SlotIdentityMismatch" and not host._started and host.kernel.snapshot().is_empty(),"真实原save_id不能冒充manual-1，冷继续明确拒绝错槽")
	check(slot_hashes()==hashes,"错槽拒绝不改真实原封装")
	# 仅原text中的封装槽名改变；不经encode、不改规则/format/version/payload或checksum。
	var needle: String='"save_id":"04-after-adoption"'
	check(text.count(needle)==1,"原件有且仅有一个明确的封装槽名可重绑")
	if text.count(needle)!=1: finish(); return
	var slot_text: String=text.replace(needle,'"save_id":"manual-1"')
	var before_header: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	var rebound_header: Dictionary=Strict.parse_text(slot_text,Codec.MAX_BYTES).value
	rebound_header.save_id=before_header.save_id
	check(rebound_header==before_header,"继续fixture仅重绑save_id，原payload字节/校验和/版本/代号完整相等")
	write_bytes(directory.path_join("manual-1.A.hmsave"),slot_text.to_utf8_buffer())
	hashes=slot_hashes()
	var completed_before: int=completed_operations.size()
	button(host,"continue").pressed.emit()
	await settle(host)
	check(completed_operations.size()==completed_before+1 and completed_operations.back().operation=="inspect","真实继续经Native.inspect与StorageService完成回调")
	check(host.last_storage_result.get("ok",false) and host._started and host._save_generation==decoded.generation,"冷Main继续保留原v1 generation并成功恢复")
	if not host.last_storage_result.get("ok",false): printerr(host.last_storage_result); finish(); return
	check(host.kernel.snapshot()==expected and host.kernel.content()==effective.catalog,"槽名派生v1继续整态/RNG/身份仍与原件Codec基准完全一致")
	check(host.kernel.snapshot()==importer.kernel.snapshot() and host.kernel.content()==importer.kernel.content(),"真实原v1导入与槽名派生继续得到完全相同世界与目录")
	check(host.default_start_calls==0 and host.kernel.new_game_calls==0,"旧档继续与先前错槽拒绝均未调用默认开局")
	check(host.find_child("WorldStatus",true,false)!=null and host.find_child("PlaceStage",true,false)!=null,"旧档恢复进入真实完整世界UI节点")
	check(slot_hashes()==hashes and FileAccess.get_sha256(source_path)==LEGACY_SHA and FileAccess.get_sha256(import_path)==LEGACY_SHA,"继续只读银行，原捕获文件及逐字导入副本SHA始终未变")
	print("LEGACY_ORIGINAL_SHA=",LEGACY_SHA," CANONICAL_LOCK=",LEGACY_LOCK," SLOT_FIXTURE=SAVE_ID_ONLY_REBOUND")
	finish()

## 释放真实UI节点；诊断捕获只留独立本地profile，不进入提交或用户目录。
func finish() -> void:
	for host in hosts:
		if is_instance_valid(host): host.queue_free()
	print("RESTORE_MENU_CHECKS=",checks," FAILURES=",failures," PROFILE=",profile)
	print("FULL_UI_NODES=true; VISUAL_ACCEPTANCE=NOT_RUN; WEB=NOT_RUN")
	quit(0 if failures==0 else 1)
