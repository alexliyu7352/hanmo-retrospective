## 青川实际行动银行的独立Main消费者；只沿标题继续和当前活控件完成合法后续。
extends "../formal_appearance_main_node/main_entry.gd"

var restored_world: String = ""
var restored_source: Dictionary = {}
var continuation_steps: Array = []

## 逐条保留产品命令信号和当前本人上下文，不构造命令或更改回执。
func observed_command(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	commands.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true),"context":main.kernel.command_context().duplicate(true)})
	write("commands.json",encode(commands))

## 冷查询直接经过公开worker只读边界，等待原ticket，不授予运行许可。
func cold_query(query_name: String) -> Dictionary:
	var bridge = main.kernel.transport()
	var source: Dictionary = main.kernel.source_identity()
	var accepted: Dictionary = bridge.enqueue_live_operation({"kind":"query","source":source,"payload":{"query_id":query_name,"args":{}}})
	if not check(accepted.get("ok",false),query_name+"_accepted"): return {}
	var deadline: int = Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary = bridge.poll_live_operation(accepted.reference)
		if not reply.get("ok",false): check(false,query_name+"_poll"); return {}
		if reply.get("ready",false):
			if not check(reply.get("current_identity")==source and reply.get("result",{}).get("ok",false),query_name+"_read_only"): return {}
			return reply.result
		await process_frame
	check(false,query_name+"_timeout"); return {}

## 产品物理codec解开真实银行供独立native oracle；不以JSON重建typed checkpoint。
func extract_bank() -> bool:
	var codec: Script = load("res://storage/resident_slot_codec.gd")
	var bank: String = str(binding.original_bank)
	var decoded: Dictionary = codec.decode_payload(FileAccess.get_file_as_string(bank))
	if not check(decoded.get("ok",false) and decoded.get("payload") is PackedByteArray,"actual_product_bank_codec_accepts_payload"): return false
	var file = FileAccess.open(output.path_join("bank-payload.hmc"),FileAccess.WRITE)
	if file==null: return check(false,"bank_payload_write")
	file.store_buffer(decoded.payload); file.close()
	if decoded.has("continuation_bytes"):
		file=FileAccess.open(output.path_join("bank-continuation.hmc"),FileAccess.WRITE)
		if file==null: return check(false,"bank_continuation_write")
		file.store_buffer(decoded.continuation_bytes); file.close()
	return write("bank-codec.json",encode({"bank_sha256":FileAccess.get_sha256(bank),"payload_sha256":FileAccess.get_sha256(output.path_join("bank-payload.hmc")),"has_continuation":decoded.has("continuation_bytes")}))

## 从恢复的权威本人检查Director隔离，原始全态保持只读。
func actor_boundary(state: Dictionary, label: String) -> bool:
	var actor_id: String = binding.player_actor_id
	return check(state.get("player_actor_id")==actor_id and main.kernel.command_context().get("actor_id")==actor_id,label+"_current_actor") and check(not state.director.schedules.has(actor_id) and not state.director.plans.has(actor_id),label+"_no_npc_director_player_plan")

## 当前规划页选择真实防御，轮次自然推进至作者终点；胜负完全由核心决定。
func continue_battle() -> bool:
	var count: int = 0
	while count<64:
		var view = main.page.find_child("PersonalBattleScreen",true,false)
		if not check(view!=null and view.is_visible_in_tree(),"live_personal_battle_page"): return false
		var battle: Dictionary = view.battle.duplicate(true)
		if not check(battle.controller_id==binding.player_actor_id and view.page_input.actor_id==binding.player_actor_id,"player_controls_actual_personal_battle"): return false
		continuation_steps.append({"battle":battle,"context":main.kernel.command_context().duplicate(true),"source":main.kernel.source_identity().duplicate(true)})
		if not write("steps.json",encode(continuation_steps)): return false
		if battle.phase=="result":
			if not write("battle-result.json",encode(view.page_input)): return false
			if not press(main.page,"battle_close",battle.id) or not await settled(): return false
			return check(main.page.find_child("PersonalBattleScreen",true,false)==null,"actual_battle_result_closed_once")
		if not check(battle.phase=="planning","legal_planning_phase"): return false
		var raw: String=world(); var context: Dictionary=main.kernel.command_context().duplicate(true)
		if not press(main.page,"battle_action","defend") or not await settled(): return false
		if not check(world()==raw and main.kernel.command_context()==context,"legal_draft_changes_no_world_or_request_sequence"): return false
		if not press(main.page,"battle_submit",battle.id) or not await settled(): return false
		count+=1
		if not actor_boundary(JSON.parse_string(world()),"round_"+str(count)): return false
	return check(false,"actual_battle_author_boundary_not_reached")

## 本人页仅核当前名称、经验投影和归属；不复制旧只读导航矩阵。
func inspect_profile() -> bool:
	var raw: String=world(); var source: Dictionary=main.kernel.source_identity().duplicate(true)
	if not press(main.page,"system_menu") or not press(main.page,"self_profile_open") or not await settled(): return false
	var page: Node=main.page.find_child("SelfProfileScreen",true,false)
	if not check(page!=null and page.is_visible_in_tree(),"actual_post_action_self_profile"): return false
	var profile: Dictionary=main.kernel.self_profile()
	if not check(profile.get("actor_id")==binding.player_actor_id,"profile_belongs_to_current_person") or not write("actual-profile.json",encode(profile)): return false
	var close: Node=main.page.find_child("SelfProfileClose",true,false)
	if not check(close is Button and close.is_visible_in_tree() and not close.disabled,"visible_profile_close"): return false
	close.pressed.emit()
	if not await settled(): return false
	return check(world()==raw and main.kernel.source_identity()==source,"profile_read_does_not_settle_again")

## 新进程空标题读取实际行动后银行，原字节一致且许可停住，随后完成真实合法后续。
func consume() -> bool:
	if not extract_bank() or not press(main.page,"continue_dynamic") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main.kernel.content().get("pack_set_digest")==binding.pack_set_digest,"normal_continue_exact_catalog"): return false
	main.kernel.command_completed.connect(observed_command)
	restored_world=world(); restored_source=main.kernel.source_identity().duplicate(true)
	var state: Variant=JSON.parse_string(restored_world)
	if not check(state is Dictionary and state.tick>0,"actual_action_bank_not_old_tick_zero"): return false
	if not check(restored_world==FileAccess.get_file_as_string(binding.producer.path_join("saved-world.json")),"complete_world_exact_original_bytes"): return false
	if not check(encode(state.actors[binding.player_actor_id].appearance)==FileAccess.get_file_as_string(binding.producer.path_join("saved-appearance.json")),"complete_appearance_exact_original_encode_bytes"): return false
	if not actor_boundary(state,"restored") or not write("restored-source.json",encode(restored_source)): return false
	var continuation: Dictionary=await cold_query("continuation_state")
	if continuation.is_empty() or not write("restored-continuation.json",encode(continuation)): return false
	await create_timer(1.4).timeout
	if not await settled(): return false
	if not check(world()==restored_world and main.kernel.source_identity()==restored_source and commands.is_empty() and not main._resident_adapter.auto_running,"restored_permit_remains_stopped_no_tick_or_payment"): return false
	if binding.boundary=="personal_battle_planning":
		if not await continue_battle(): return false
	else:
		return check(false,"producer_needs_explicit_supported_legal_continuation")
	if not await inspect_profile(): return false
	var final_raw: String=world(); var final_state: Dictionary=JSON.parse_string(final_raw)
	if not actor_boundary(final_state,"final") or not write("final-world.json",final_raw) or not write("final-source.json",encode(main.kernel.source_identity())): return false
	if not check(encode(final_state.actors[binding.player_actor_id].appearance)==FileAccess.get_file_as_string(binding.producer.path_join("saved-appearance.json")),"battle_keeps_same_complete_person_appearance"): return false
	return write("commands.json",encode(commands)) and write("continuation-summary.json",encode({"boundary":binding.boundary,"restored_tick":state.tick,"final_tick":final_state.tick,"battle_rounds":continuation_steps.size(),"command_count":commands.size(),"auto_running":main._resident_adapter.auto_running}))
