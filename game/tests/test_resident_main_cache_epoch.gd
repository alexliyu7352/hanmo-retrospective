## 真实Main同世界恢复的缓存/来源代次门；复用既有Alpha银行，只读且零推进。
extends "res://tests/test_resident_main_title_restore.gd"
var published_frames: Array=[]

## 记录真实有限frame的来源；不注入结果、不替换worker或改变Main发布时序。
func observe_frame(source: Dictionary, scene_epoch: int) -> void:
	published_frames.append({"source":source.duplicate(true),"scene_epoch":scene_epoch})

## 此唯一额外读通过同一真实transport；poll只等完成，不提交隐式推进。
func poll_query(bridge: Object, reference: Dictionary) -> Dictionary:
	var deadline: int=Time.get_ticks_usec()+15000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary=bridge.poll_live_operation(reference)
		if not reply.get("ok",false) or reply.get("ready",false): return reply
		await process_frame
	return {"ok":false,"code":"CacheEpochQueryTimeout"}

## 收集公开有限getter；它们不能触发原生读，也不是一个全态镜像。
func finite_views(backend: Node, actor_id: String) -> Dictionary:
	return {"command":backend.command_context(),"action":backend.action_context(),"map":backend.map_view(),"sidebar":backend.journey_sidebar(),"hud":backend.hud_view(),"interaction":backend.interaction_view(),"location":backend.presentation_location(),"character":backend.character_input(actor_id)}

## 先从标题实际恢复并暖cache，再经原Main对同一world执行第二次恢复。
func exercise_cache_epoch(main: Node, manifest: Dictionary) -> bool:
	if not await open_title_slots(main): return false
	if not confirm_slot(main,"manual-1"): return false
	if not check(await wait_idle(main) and main.last_storage_result.get("ok",false),"真实标题恢复已发布第一组有限cache"): return false
	var backend: Node=main.kernel; var bridge: Object=backend.transport()
	var source: Dictionary=backend.source_identity(); var actor_id: String=backend.command_context().get("actor_id","")
	if not check(not actor_id.is_empty() and world_digest(main)==manifest.world_sha256 and bank_hashes()==manifest.banks,"暖cache之前完整世界与原银行精确保留"): return false
	var warmed: Dictionary=finite_views(backend,actor_id)
	if not check(warmed.command.get("ok",false) and warmed.action.get("ok",false) and warmed.map.get("ok",false) and warmed.character.get("ok",false),"命令、行动、地图与选中人物确实来自已发布真实cache"): return false
	backend.frame_ready.connect(observe_frame)
	var params: Dictionary={"destination":"village.east","via":[]}
	var quoted: Dictionary=backend.preview_async("travel",params,actor_id,main._scene_counter)
	if not check(quoted.get("ok",false) and await wait_idle(main),"同一worker完成一个真实动态纯报价"): return false
	var quote: Dictionary=backend.preview("travel",params,actor_id)
	if not check(quote.has("ok") and quote.get("code")!="NativeAsyncRequired","动态报价确实入cache；普通规则拒绝同样是报价"): return false
	var query: Dictionary=bridge.enqueue_live_operation({"kind":"query","source":source,"payload":{"query_id":"command_context","args":{}}})
	if not check(query.get("ok",false),"原source仍可接纳真实有限读"): return false
	var read: Dictionary=await poll_query(bridge,query.reference)
	if not check(read.get("ok",false) and read.get("ready",false) and read.get("current_identity")==source and read.get("result")==warmed.command,"真实query与原cache一致，旧票据有确实成功的来源"): return false
	if not check(bridge.poll_live_operation(query.reference)==read,"下一操作前已完成JobRef可以重复读取"): return false
	var opened: Dictionary=main._open_slots("load")
	if not check(opened.get("ok",false),"同世界已暖cache时打开原Main加载窗口"): return false
	var deadline: int=Time.get_ticks_usec()+15000000
	while not is_instance_valid(main._slot_dialog) and Time.get_ticks_usec()<deadline: await process_frame
	if not check(is_instance_valid(main._slot_dialog),"同世界真实槽列表完成"): return false
	if not confirm_slot(main,"manual-1"): return false
	deadline=Time.get_ticks_usec()+30000000
	while backend.source_identity()==source and Time.get_ticks_usec()<deadline: await process_frame
	var current: Dictionary=backend.source_identity()
	if not check(current.world_id==source.world_id and current.revision==source.revision and current.tick==source.tick and current.execution_epoch>source.execution_epoch,"持久world/版本/半日相同但执行epoch已换"): return false
	if not check(main.kernel==backend and published_frames.is_empty(),"仍是原backend；恢复后尚未发布任何新有限frame"): return false
	var fenced: Dictionary=finite_views(backend,actor_id)
	for key in fenced:
		if not check(fenced[key].get("ok")==false and fenced[key].get("code")=="ResidentFrameUnavailable","旧epoch有限cache不可用："+str(key)): return false
	if not check(backend.preview("travel",params,actor_id).get("code")=="NativeAsyncRequired" and backend.preview("advance",{}).get("code")=="NativeAsyncRequired","动态报价和旧整帧固定报价均不会跨epoch重用"): return false
	var stale: Dictionary=bridge.enqueue_live_operation({"kind":"query","source":source,"payload":{"query_id":"character_input","args":{"actor_id":"private.denied"}}})
	if not check(not stale.get("ok",true) and stale.get("code")=="ResidentStaleReference","旧source在查询权限/参数前被真实transport代次门拒绝"): return false
	var expired: Dictionary=bridge.poll_live_operation(query.reference)
	if not check(not expired.get("ok",true) and expired.get("code")=="ResidentStaleReference","旧JobRef不可读；此项仅证明回收，不单独当epoch证据"): return false
	if not check(await wait_idle(main) and main.last_storage_result.get("ok",false),"成功恢复后原Main完成新source的有限frame"): return false
	if not check(published_frames.size()==1 and published_frames[0].source==current and backend.source_identity()==current,"只发布一组新代次frame，无旧来源回调混入"): return false
	if not check(finite_views(backend,actor_id)==warmed and backend.preview("travel",params,actor_id).get("code")=="NativeAsyncRequired","新cache由恢复世界重建，动态报价须重新请求"): return false
	if not check(world_digest(main)==manifest.world_sha256 and bank_hashes()==manifest.banks and not main._resident_adapter.auto_running,"两次恢复零推进、无旧授权，完整世界与所有银行保持"): return false
	observed={"before":source,"after":current,"frame_sources":published_frames,"world_sha256":manifest.world_sha256,"banks":bank_hashes(),"stale_source_code":stale.code,"old_job_code":expired.code}
	return true

## 使用已验证双进程门的只读银行；新报告单独命名，禁止覆盖或盲重跑。
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=").simplify_path().trim_suffix("/")
	if not check(evidence.is_absolute_path() and FileAccess.file_exists(evidence.path_join("title-manifest.json")) and not FileAccess.file_exists(evidence.path_join("cache-epoch-report.json")),"需既有真实title清单和未用的窄门报告名"): quit(2); return
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(evidence.path_join("title-manifest.json")))
	if not check(loaded.get("ok",false),"原清单完整可读"): quit(2); return
	var manifest: Dictionary=loaded.value; profile=manifest.profile; directory=manifest.directory
	if not check(ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")==directory and directory.begins_with(evidence+"/") and bank_hashes()==manifest.banks,"运行只指向同一自有银行目录，原字节未动"): quit(2); return
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile=") and not check(arg.trim_prefix("--profile=")==profile,"命令行不得切换银行profile"): quit(2); return
	ProjectSettings.set_setting("development/native_resident_world_enabled",true)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"真实匹配Bridge已注册"): quit(2); return
	var main=Main.new(); root.add_child(main)
	var selection: Dictionary=main._default_content_selection()
	var passed: bool=false
	if check(selection.scenario=="scenario.alpha_life" and FileAccess.get_sha256(selection.path)==manifest.catalog_sha256,"原当前Alpha目录身份未变"):
		passed=await exercise_cache_epoch(main,manifest)
	main.queue_free(); await process_frame
	var report: Dictionary={"ok":passed and failures==0,"checks":checks,"failures":failures,"observed":observed,"scope":"Real Main same-world restore source/cache fencing; zero advance, original case-02 banks read-only; old JobRef recycling is not itself proof of epoch rejection"}
	var file=FileAccess.open(evidence.path_join("cache-epoch-report.json"),FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify(report,"",true,true)+"\n"); file.close()
	print("RESIDENT_MAIN_CACHE_EPOCH_CHECKS=",checks," FAILURES=",failures," OK=",report.ok)
	quit(0 if report.ok else 1)
