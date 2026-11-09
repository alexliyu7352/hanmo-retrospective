## 共同底座的有限真实Main门：正常native启动分支、现地方近况控件及只读DTO，不冒充正式选包发行或GUI像素验收。
extends "../resident_delivery_node/package_entry.gd"
var original_world: String=""
var original_source: Dictionary={}
var bank_before: PackedByteArray

## 真实worker只读检查点用于前后比较；此门不安装导出世界或修改产品存档。
func checkpoint() -> PackedByteArray:
	var source: Dictionary=main.kernel.source_identity()
	var bridge=main.kernel.transport()
	var accepted: Dictionary=bridge.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(accepted.get("ok",false),"checkpoint_accepted"): return PackedByteArray()
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary=bridge.poll_live_operation(accepted.reference)
		if not reply.get("ok",false): check(false,"checkpoint_poll"); return PackedByteArray()
		if reply.get("ready",false):
			var result: Dictionary=reply.get("result",{})
			if not check(result.get("ok",false) and result.get("checkpoint_bytes") is PackedByteArray and reply.get("current_identity")==source,"checkpoint_same_source"): return PackedByteArray()
			return result.checkpoint_bytes
		await process_frame
	check(false,"checkpoint_timeout"); return PackedByteArray()

## 只采实际显示的行；控件树不从fixture自行拼一页以冒充原生消费。
func row_texts(node: Node) -> Array:
	var values: Array=[]
	if node is Label and str(node.name).begins_with("WorldObservationRow"): values.append(node.text)
	for child in node.get_children(): values.append_array(row_texts(child))
	return values

## 开关现有近况入口不推进半日、不产生任何命令；过期页回调仅忽略。
func exercise_page() -> bool:
	if not await settled(): return false
	main.kernel.command_completed.connect(observed_command)
	original_source=main.kernel.source_identity(); original_world=world(); bank_before=await checkpoint()
	if not check(not bank_before.is_empty() and main.kernel.world_observation_enabled(),"actual_registry_world_installed"): return false
	if not press(main.page,"dynamic_open") or not await settled(): return false
	var section: Control=main.page.find_child("WorldObservationSection",true,false)
	if not check(section!=null and section.is_visible_in_tree(),"real_main_observation_section"): return false
	var place: String=main.kernel.presentation_location().get("place_id","")
	var view: Dictionary=main.kernel.world_observation(place)
	var rows: Array=row_texts(section)
	if not check(view.get("version")==1 and view.get("identity")==original_source and rows.size()==view.get("rows",[]).size() and not rows.is_empty(),"visible_rows_from_actual_same_source_dto"): return false
	if not write("actual-view.json",encode(view)) or not write("actual-visible-text.json",encode(rows)): return false
	var previous: Dictionary=main.get_meta("shared_world_observation_draft",{}).duplicate(true)
	var old_source: Dictionary=previous.source.duplicate(true); old_source.identity.execution_epoch-=1
	var close: Button=control(main.page,"command_kind","dynamic_close")
	main.DynamicWorldPanel._select(main,close,{"invalid":true},old_source,main._scene_counter)
	if not check(previous==main.get_meta("shared_world_observation_draft",{}),"old_epoch_callback_cannot_replace_page"): return false
	if not press(main.page,"dynamic_close") or not await settled(): return false
	if not check(not main._dynamic_open and main.page.find_child("WorldObservationSection",true,false)==null,"return_closes_observation_controls"): return false
	if not press(main.page,"dynamic_open") or not await settled(): return false
	var bank_after: PackedByteArray=await checkpoint()
	return check(world()==original_world and main.kernel.source_identity()==original_source and bank_after==bank_before and commands.is_empty(),"open_return_reopen_and_stale_callback_are_pure")

## 从正常已prepare作者目录调用实际Main内容入口；Schema、整数及引用规则由原Catalog统一装载。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2: quit(2); return
	var catalog_script: Script=load("res://core/content_catalog.gd")
	var prepared: Dictionary=catalog_script.read_prepared(args[0]); output=args[1]
	DirAccess.make_dir_recursive_absolute(output)
	var version: Dictionary=Engine.get_version_info()
	var ready: bool=check(version.major==4 and version.minor==7 and version.patch==2 and str(version.hash).begins_with("ed1daf0bf") and DisplayServer.get_name()=="headless","fixed_engine_headless_only")
	ready=ready and check(prepared.get("ok",false) and ClassDB.class_exists("HanmoResidentBridge"),"real_sdk_bridge_and_explicit_fixture")
	if ready:
		var scene: PackedScene=load("res://main.tscn"); main=scene.instantiate(); root.size=Vector2i(1280,900); root.add_child(main)
		var installed: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",args[0],{},"society::actor.apprentice")
		write("native-start-result.json",encode({"content_id":prepared.get("content_id"),"content_digest":prepared.get("content_digest"),"schema_sha256":prepared.get("schema_sha256"),"result":installed,"message":main._message}))
		ready=check(installed.get("ok",false),"main_normal_author_content_start")
		if ready: ready=await exercise_page()
	write("result.json",encode({"ok":ready,"checks":checks,"failures":failures,"commands":commands,"actual_main_scene":is_instance_valid(main),"actual_sdk_bridge":ClassDB.class_exists("HanmoResidentBridge"),"scope":"normal author content start and existing place controls","catalog_path":args[0],"content_id":prepared.get("content_id"),"content_digest":prepared.get("content_digest"),"schema_sha256":prepared.get("schema_sha256"),"same_input_as_native_bank":false,"formal_title_pack_installation":false,"gui_pixels_accepted":false,"engine":version,"pid":OS.get_process_id()}))
	quit(0 if ready else 1)
