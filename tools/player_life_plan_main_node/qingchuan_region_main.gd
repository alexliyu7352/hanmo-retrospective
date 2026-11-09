## 青川1.6两条正式经历的恢复节点；复用同一Main、原银行和稳定等待，不冒称失落原回执。
extends "../activity_continuation_main_node/activity_continuation_main.gd"
const Region=preload("res://tests/test_qingchuan_region_player_flow.gd")
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")
const P="society::"
var ids: Dictionary={}
var contract: Dictionary={}
var course_boundary: Dictionary={}
var activity_begin_observation: int=0

## 区域分支统一只取当前可见控件，基类预算仍操作原ActivityContinuation入口。
func click(kind: String, target: String="") -> bool:
	var diagnose: bool=kind=="activity_begin" and phase=="consume-auto" and action_case=="course_first"
	if diagnose:
		activity_begin_observation+=1
		record_activity_begin_fields("before")
	var result: bool=await Region.click(self,main,kind,target)
	if diagnose: record_activity_begin_fields("after")
	return result

## 原确认前后只抄当前字段与真实信号绑定，不预览、不请求新页面，也不改变原等待或断言。
func record_activity_begin_fields(label: String) -> void:
	var current: Button=Region.button(main.page,"activity_begin")
	var button_fields: Dictionary={"found":current!=null}
	if current!=null:
		var connections: Array=[]
		for entry in current.pressed.get_connections():
			var callback: Callable=entry.callable
			connections.append({"method":str(callback.get_method()),"object_id":callback.get_object_id(),"bound_arguments":callback.get_bound_arguments().duplicate(true),"flags":entry.flags})
		button_fields={"found":true,"path":str(current.get_path()),"disabled":current.disabled,"visible_in_tree":current.is_visible_in_tree(),"pressed_connections":connections}
	var main_fields: Dictionary={"scene_counter":main._scene_counter,"source":main.kernel.source_identity(),"activity_draft":main._activity_draft.duplicate(true),"resident_hold":main._resident_hold.duplicate(true),"started":main._started,"resident_mode":main._resident_mode,"advancing":main._advancing,"storage_intent":main._storage_intent,"resident_pending_slot_mode":main._resident_pending_slot_mode,"player_plan_pending_start":main._player_plan_pending_start.duplicate(true),"resident_short_action":main._resident_short_action,"resident_step_after_plan":main._resident_step_after_plan,"journey_view_opened":main.journey_view.opened}
	var adapter_fields: Dictionary={"refresh_hold":main._resident_adapter._refresh_hold.duplicate(true),"job":main._resident_adapter._job.duplicate(true),"auto_running":main._resident_adapter.auto_running,"is_busy":main._resident_adapter.is_busy()}
	var kernel_fields: Dictionary={"has_pending_reads":main.kernel.has_pending_reads(),"is_busy":main.kernel.is_busy()}
	write_json("activity-begin-%02d-"%activity_begin_observation+label+".json",{"boundary":label,"monotonic_usec":Time.get_ticks_usec(),"button":button_fields,"main":main_fields,"adapter":adapter_fields,"kernel":kernel_fields,"recorded_command_receipts":receipts.size()})

## 留存准确世界、原外貌和当前会话；所有导出仅为证据，不作为产品恢复输入。
func record(label: String) -> bool:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	var original: Dictionary=await Region.appearance(self,main,ids)
	if not check(exported.get("ok",false) and not original.is_empty(),label+"_world_and_appearance"): return false
	return write_text(label+"-state.json",exported.json) and write_json(label+"-appearance.json",original) and write_json(label+"-interaction.json",main.kernel.interaction_view()) and write_json(label+"-source.json",main.kernel.source_identity())

## 逐字段保留类型、缺键及原值差异；只诊断，不归一化或删除字段以改变原全等断言。
func exact_differences(expected: Variant, actual: Variant, path: String="") -> Array:
	var differences: Array=[]
	if typeof(expected)!=typeof(actual):
		return [{"path":path,"kind":"type","expected_type":type_string(typeof(expected)),"actual_type":type_string(typeof(actual)),"expected":expected,"actual":actual}]
	if expected is Dictionary:
		var keys: Array=expected.keys()
		for key in actual:
			if not expected.has(key): keys.append(key)
		keys.sort()
		for key in keys:
			var child: String=path+"/"+str(key).replace("~","~0").replace("/","~1")
			if not expected.has(key): differences.append({"path":child,"kind":"unexpected_key","actual":actual[key]})
			elif not actual.has(key): differences.append({"path":child,"kind":"missing_key","expected":expected[key]})
			else: differences.append_array(exact_differences(expected[key],actual[key],child))
	elif expected is Array:
		if expected.size()!=actual.size(): differences.append({"path":path,"kind":"array_size","expected":expected.size(),"actual":actual.size()})
		for index in range(mini(expected.size(),actual.size())): differences.append_array(exact_differences(expected[index],actual[index],path+"/"+str(index)))
	elif expected!=actual:
		differences.append({"path":path,"kind":"value","expected":expected,"actual":actual})
	return differences

## 同正式初态分别承接松门长约或县仓短约，行动先后由原世界产生不同结果。
func produce() -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"fresh_regional_profile"): return false
	if not await Region.start(self,main,ids) or not await Region.introduction(self,main): return false
	main.kernel.command_completed.connect(command_completed)
	if action_case=="fulfill":
		if not await Region.travel(self,main,P+"settlement.sword_yard",24): return false
		contract=await Region.accept(self,main,P+"role.sect_supply",24)
		if contract.is_empty(): return false
		if not await Region.travel(self,main,P+"settlement.qingchuan",contract.deadline_tick): return false
		if not await click("move_local",P+"place.grain") or not await Region.purchase(self,main,contract): return false
		if not await Region.travel(self,main,P+"settlement.sword_yard",contract.deadline_tick) or not await Region.report(self,main,contract): return false
		var observed: Dictionary=await Region.outcome(self,main,contract,"topic.sect.outcome",false)
		if observed.is_empty() or not write_json("outcome-before-save.json",observed): return false
	else:
		contract=await Region.accept(self,main,P+"role.city_supply",4)
		if contract.is_empty(): return false
		course_boundary=await Region.course(self,main)
		if course_boundary.is_empty(): return false
	if not await Region.save(self,main) or not await record("saved"): return false
	return write_json("save-manifest.json",{"format":"HM-QINGCHUAN-REGION-MAIN/1","evidence_origin":"NEW_RECOVERY_RUN_NOT_ORIGINAL_RESTORED_PROOF","chapter_profile":"qingchuan_region","producer_pid":OS.get_process_id(),"case":action_case,"profile":profile,"directory":directory,"binding_sha256":FileAccess.get_sha256(binding_path),"banks":bank_hashes(),"source":main.kernel.source_identity(),"contract":contract,"course":course_boundary,"interaction":main.kernel.interaction_view()})

## 消费者先逐字核原全世界和外貌，再显式继续原对话或原剩余课程；原银行保持不变。
func consume() -> bool:
	var manifest: Dictionary=read_json(case_root.path_join("producer/save-manifest.json"))
	contract=manifest.contract; course_boundary=manifest.course
	if not check(bank_hashes()==manifest.banks,"same_original_regional_physical_banks"): return false
	if not await Region.restore(self,main,ids): return false
	if not check(main.kernel.transport().debug_export_state().json==FileAccess.get_file_as_string(case_root.path_join("producer/saved-state.json")),"fresh_title_exact_whole_world"): return false
	var original: Dictionary=await Region.appearance(self,main,ids)
	var saved: Dictionary=read_json(case_root.path_join("producer/saved-appearance.json"))
	var actual_interaction: Dictionary=main.kernel.interaction_view().duplicate(true)
	# 外貌原JSON逐字节比较保留裁切浮点和所有描述字段；Strict解码只用于原差异诊断。
	var actual_raw: PackedByteArray=(JSON.stringify(original,"",false,true)+"\n").to_utf8_buffer()
	var saved_raw: PackedByteArray=FileAccess.get_file_as_bytes(case_root.path_join("producer/saved-appearance.json"))
	var raw_bytes_equal: bool=actual_raw==saved_raw
	var comparison: Dictionary={"expected_appearance":saved,"actual_appearance":original,"expected_interaction":manifest.interaction,"actual_interaction":actual_interaction,"saved_source":manifest.source,"restored_source":main.kernel.source_identity(),"appearance_equal":original==saved,"raw_bytes_equal":raw_bytes_equal,"interaction_equal":actual_interaction==manifest.interaction,"appearance_differences":exact_differences(saved,original),"interaction_differences":exact_differences(manifest.interaction,actual_interaction)}
	if not write_json("restore-appearance-actual.json",original) or not write_json("restore-interaction-actual.json",actual_interaction) or not write_json("restore-exact-comparison.json",comparison): return false
	if not check(raw_bytes_equal and main.kernel.interaction_view()==manifest.interaction,"fresh_title_exact_appearance_resources_and_interaction"): return false
	var source: Dictionary=main.kernel.source_identity(); await create_timer(1.4).timeout
	if not check(source==main.kernel.source_identity() and not main._resident_adapter.auto_running,"restored_idle_does_not_grant_halfdays"): return false
	if not await record("restored"): return false
	main.kernel.command_completed.connect(command_completed)
	if action_case=="fulfill":
		if not await Region.close_outcome(self,main): return false
	else:
		if not await Region.finish_course(self,main,course_boundary) or not await Region.expired_contract(self,main,contract): return false
		var observed: Dictionary=await Region.outcome(self,main,contract,"topic.city.outcome",true)
		if observed.is_empty() or not write_json("expired-outcome.json",observed): return false
		if not await Region.close_outcome(self,main): return false
	if not await record("completed"): return false
	return check(bank_hashes()==manifest.banks,"consumer_keeps_original_physical_banks")

## 显式profile才允许这两条恢复经历；实际helper及原库身份同时留入新回执。
func run() -> void:
	var chapter: String=""
	var attempt: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--phase="): phase=arg.trim_prefix("--phase=")
		elif arg.begins_with("--case="): action_case=arg.trim_prefix("--case=")
		elif arg.begins_with("--chapter-profile="): chapter=arg.trim_prefix("--chapter-profile=")
		elif arg.begins_with("--case-root="): case_root=arg.trim_prefix("--case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--binding="): binding_path=arg.trim_prefix("--binding=")
		elif arg.begins_with("--attempt="): attempt=arg.trim_prefix("--attempt=")
	if chapter!="qingchuan_region" or phase not in ["produce","consume-auto"] or action_case not in ["fulfill","course_first"] or not case_root.is_absolute_path(): quit(2); return
	if not attempt.is_empty() and (not attempt.is_valid_identifier() or phase=="produce"): quit(2); return
	output=case_root.path_join(("producer" if phase=="produce" else phase)+("-"+attempt if not attempt.is_empty() else ""))
	if not DirAccess.dir_exists_absolute(output) or FileAccess.file_exists(output.path_join("result.json")): quit(2); return
	binding=read_json(binding_path); ids=binding.cases[action_case]
	if not check(binding.get("chapter_profile")==chapter and binding.get("native_artifact_status")=="BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED" and binding.get("source_commit")==binding.get("reviewed_source_commit") and FileAccess.get_sha256(binding.native_extension.path)==binding.native_extension.sha256,"reviewed_regional_source_and_library"): quit(2); return
	profile="qingchuan-region-"+case_root.sha256_text().left(12); ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with(case_root+"/") and ClassDB.class_exists("HanmoResidentBridge"),"owned_regional_profile_real_native"): quit(2); return
	root.size=Vector2i(1280,900); main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=await produce() if phase=="produce" else await consume()
	write_json("receipts.json",receipts)
	var actual_helpers: Dictionary={}
	for name in binding.drivers:
		actual_helpers[name]={"path":binding.drivers[name].path,"sha256":FileAccess.get_sha256(binding.drivers[name].path)}
	write_json("result.json",{"ok":passed and failures.is_empty(),"phase":phase,"chapter_profile":chapter,"case":action_case,"pid":OS.get_process_id(),"source_commit":binding.source_commit,"native_source_commit":binding.native_source_commit,"helpers":actual_helpers,"profile":profile,"rows":rows,"failures":failures,"page_error":main._resident_page_error,"message":main._message,"actual_main_scene":true,"actual_native_bridge":true,"gui_pixels_accepted":false,"gui_mouse_evidence":false,"evidence_origin":"NEW_RECOVERY_RUN_NOT_ORIGINAL_RESTORED_PROOF"})
	main.queue_free(); await process_frame
	print("QINGCHUAN_REGION_MAIN ",action_case," ",phase," OK=",passed and failures.is_empty()); quit(0 if passed and failures.is_empty() else 1)
