## 单一具名任职Main联合节点；新局与独立空标题续程共用实际控件，冷导出只作证据。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Strict=preload("res://core/strict_json.gd")
const Life=preload("res://tests/test_regional_life_player_flow.gd")
const Flow=preload("res://tests/test_named_membership_player_flow.gd")
var main
var rows: Array=[]
var failures: Array=[]
var receipts: Array=[]
var pending_receipts: Array=[]
var release_receipts: Array=[]
var after_pending: bool=false
var after_release: bool=false
var output: String=""
var case_root: String=""
var phase: String=""
var case_name: String=""
var profile: String=""
var directory: String=""
var binding_path: String=""
var binding: Dictionary={}
var ids: Dictionary={}
var manifest: Dictionary={}
var last_state: Dictionary={}
var saved_case_root: String=""
var saved_binding_path: String=""
var saved_source: Dictionary={}
var terminal_diagnostic: Dictionary={}

## check-only只解析；运行须由明确绑定的启动器在SceneTree就绪后开始。
func _initialize() -> void: call_deferred("run")

## 所有失败保留真实现场，不修改世界或以另一人物替换。
func check(ok: bool, label: String) -> bool:
	rows.append({"stage":label,"ok":ok})
	if not ok: failures.append(label); printerr("FAIL ",label)
	return ok

## 证据输出只写当前进程独占目录；原物理银行由产品菜单唯一写入。
func write_text(name: String, value: String) -> bool:
	var file=FileAccess.open(output.path_join(name),FileAccess.WRITE)
	if file==null: return check(false,"cannot_write_"+name)
	file.store_string(value); file.flush(); file.close(); return true

## 序列化原整数和有序结果，证据不能再喂入Factory或restore。
func write_json(name: String, value: Variant) -> bool:
	return write_text(name,JSON.stringify(value,"",false,true)+"\n")

## 原严格reader仅读来源绑定与旧证据，拒绝浮点化大整数。
func read_json(path: String) -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not check(parsed.get("ok",false) and parsed.get("value") is Dictionary,"strict_read_"+path.get_file()): return {}
	return parsed.value

## 排空同源有限页面读和已接受命令；等待SceneTree帧不等于授权世界半日。
func settled(host) -> bool:
	var deadline: int=Time.get_ticks_usec()+45000000
	var stable: int=0
	while Time.get_ticks_usec()<deadline and failures.is_empty():
		if host._started and host._resident_mode and not host._advancing and not host._resident_frame_pending and host._storage_intent.is_empty() and not host.kernel.is_busy() and not host.kernel.has_pending_reads() and not host._resident_adapter.is_busy() and host._resident_command.is_empty() and host._resident_quote.is_empty():
			stable+=1
			if stable>=3: return check(host._resident_page_error.is_empty(),"settled_without_page_error")
		else: stable=0
		await process_frame
	write_json("idle-failure.json",{"message":host._message,"page_error":host._resident_page_error})
	return check(false,"actual_main_idle_deadline")

## 只监听实际提交回执；比较去掉进程传输来源，保留全部业务结果与顺序。
func command_completed(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	receipts.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true)})
	var comparable: Dictionary={"kind":kind,"result":result.duplicate(true)}
	if after_pending: pending_receipts.append(comparable)
	if after_release: release_receipts.append(comparable)
	write_json("receipts.json",receipts)

## 原共同来源下取得已验证有效checkpoint；不写存档，不恢复、不推进世界。
func checkpoint(label: String) -> bool:
	var source: Dictionary=main.kernel.source_identity()
	var bridge=main.kernel.transport()
	var started: Dictionary=bridge.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(started.get("ok",false),label+"_checkpoint_accepted"): return false
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary=bridge.poll_live_operation(started.reference)
		if not reply.get("ok",false): return check(false,label+"_checkpoint_poll")
		if reply.get("ready",false):
			var result: Dictionary=reply.get("result",{})
			if not check(result.get("ok",false) and result.get("checkpoint_bytes") is PackedByteArray and reply.get("current_identity")==source,label+"_effective_checkpoint"): return false
			var bytes: PackedByteArray=result.checkpoint_bytes
			var file=FileAccess.open(output.path_join(label+".hmcheckpoint"),FileAccess.WRITE)
			if file==null: return check(false,label+"_checkpoint_write")
			file.store_buffer(bytes); file.close()
			return check(FileAccess.get_sha256(output.path_join(label+".hmcheckpoint"))==result.validation.candidate_sha256,label+"_checkpoint_actual_hash")
		await process_frame
	return check(false,label+"_checkpoint_deadline")

## 整态仅用于同一次节点资金/责任断言和冷热对照，不替换有限产品入口。
func record(label: String) -> bool:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false) and exported.get("json") is String,label+"_actual_world_export"): return false
	var parsed: Dictionary=Strict.parse_text(exported.json)
	if not check(parsed.get("ok",false),label+"_typed_integer_evidence"): return false
	last_state=parsed.value
	if not write_text(label+"-state.json",exported.json) or not write_json(label+"-source.json",main.kernel.source_identity()) or not await checkpoint(label): return false
	return await observe(label)

## 渲染模式截取真实已绘制viewport，截图等待不授半日；headless明确没有视觉证据。
func observe(label: String) -> bool:
	if DisplayServer.get_name()=="headless": return true
	var source: Dictionary=main.kernel.source_identity()
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image=root.get_texture().get_image()
	if not check(not image.is_empty() and image.save_png(output.path_join(label+".png"))==OK,label+"_actual_viewport_png"): return false
	write_json(label+"-visual.json",{"source":source,"pid":OS.get_process_id(),"display_server":DisplayServer.get_name(),"image_sha256":FileAccess.get_sha256(output.path_join(label+".png"))})
	return check(main.kernel.source_identity()==source,label+"_visual_observation_is_read_only")

## 只读当前隔离profile原文件；消费者不得改变银行或写新槽。
func bank_hashes() -> Dictionary:
	var result: Dictionary={}
	if DirAccess.dir_exists_absolute(directory):
		for name in DirAccess.get_files_at(directory): result[name]=FileAccess.get_sha256(directory.path_join(name))
	return result

## 本人到期工资与任务都读取真实域结果；未发生即失败，不能由旁白证明已付。
func verify_paid_contract() -> bool:
	if not await record("delivered"): return false
	var economy: Dictionary=last_state.get("local_economy",{})
	var bindings: Dictionary=economy.get("membership_wage_bindings",{})
	var matching: Array=bindings.values().filter(func(value): return value.actor_id==ids.invitee_actor_id)
	if not check(matching.size()==1,"one_real_member_wage_binding"): return false
	var wage: Dictionary=matching[0]
	var paid: int=0
	for payment in economy.get("payments",{}).values():
		if payment.obligation_id==wage.obligation_id:
			if not check(payment.from_wallet_id==wage.payer_wallet_id and payment.to_wallet_id==wage.payee_wallet_id,"original_employer_to_personal_wallet"): return false
			paid+=payment.amount
	var accrued: int=0
	for debt in economy.get("arrears",{}).values():
		if debt.obligation_id==wage.obligation_id:
			if not check(debt.original_due>=debt.paid_amount,"original_employer_debt_not_erased"): return false
			accrued+=debt.original_due
	if not check(wage.credited_periods>0 and accrued>0 and accrued>=paid,"real_periodic_wages_accrued_and_accounted"): return false
	if case_name=="funded" and not check(paid>0,"funded_branch_actual_wage_payment"): return false
	var contracts: Array=dynamic_records(last_state.get("dynamic_tasks",{}).get("contracts",[])).filter(func(value): return value.get("executor_actor_id",{}).get("value")==ids.invitee_actor_id)
	if case_name=="funded":
		return check(contracts.size()==1 and contracts[0].frozen_terms.agreed_quantity==4 and contracts[0].delivered_quantity==4 and contracts[0].paid_reward==8,"funded_task_actual_full_delivery_and_org_reward")
	if not check(contracts.size()==1,"one_original_shortage_contract"): return false
	var contract: Dictionary=contracts[0]
	if not check(contract.status==3 and contract.execution_stopped and contract.stop_reason=="expired" and contract.delivered_quantity==3 and contract.eligible_delivered_quantity==3 and contract.paid_reward==6 and contract.refunded_budget==19 and contract.refunded_reward==2 and contract.restitution_refs.is_empty(),"short_actual_expired_prorata_and_unused_resource_return"): return false
	var task_id: String=contract.current_task_id.get("value","")
	var task: Dictionary=last_state.tasks.get(task_id,{})
	if not check(task.get("status")=="failed" and task.get("late",false) and not task.get("settlement_id","").is_empty(),"expired_task_legally_settled"): return false
	for inventory in last_state.inventories.values():
		for lot in inventory.get("lots",[]):
			if not check(not (lot.owner.get("kind")=="task" and lot.owner.get("id")==task_id and lot.quantity>0),"no_original_task_cargo_before_lawful_departure"): return false
	return check(wage.credited_periods==13 and accrued==33 and paid==8 and accrued-paid==25 and wage.ended_tick==27,"short_original_episode_33_accrued_8_paid_25_owed")

## 充足路线等实际采购进度终态；每一步只有正常半日，未完成不能伪装成空闲。
func wait_delivery() -> bool:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		for row in Flow.visible_rows(main.page):
			if row.get("kind")=="membership_task" and row.get("actor_id")==ids.invitee_actor_id and row.get("agreed_quantity",0)>0 and row.get("delivered_quantity",0)==row.get("agreed_quantity",0): return await verify_paid_contract()
		if not await Life.wait_halfday(self,main,ids.deadline_tick): return false
	return check(false,"actual_delivery_deadline")

## 剩余路线在生产者与空标题消费者完全共用；第二保存边界在消费者只观察不重写。
func continue_from_pending(save_release: bool) -> bool:
	after_pending=true
	if case_name in ["recipient_accept","recipient_decline"]:
		var answer: String="accept" if case_name=="recipient_accept" else "decline"
		var result: Dictionary=await Flow.answer_as_player(self,main,ids,answer)
		if result.is_empty(): return false
		if answer=="accept":
			for tick in range(2):
				if not await Life.wait_halfday(self,main,ids.deadline_tick): return false
			if not await Flow.click_action(self,main,"organization_member_response",{"actor_id":ids.player_actor_id,"kind":"lawful_leave"}): return false
			if not await record("recipient-left"): return false
			var members: Array=last_state.organization.members.values().filter(func(value): return value.actor_id==ids.player_actor_id and value.get("episode_source")!=null)
			if not check(members.size()==1 and members[0].response=="departed","paid_up_player_actually_leaves"): return false
			var fractions: Array=last_state.local_economy.membership_terminal_fractions.values().filter(func(value): return value.member_episode_id==members[0].id)
			if not check(fractions.size()==1 and fractions[0].original_numerator==1 and fractions[0].denominator==2 and fractions[0].compensation_numerator==1 and fractions[0].integer_due==1,"one_credited_fraction_original_debt_and_compensation"): return false
		else:
			if not await record("recipient-declined"): return false
			if not check(last_state.organization.members.values().all(func(value): return value.actor_id!=ids.player_actor_id),"player_decline_never_joins"): return false
		return true
	if case_name=="deferred":
		if not await Life.click(self,main,"move_local",ids.market_place_id): return false
		for unit in range(2):
			if not await Life.open_merchant_trade(self,main,ids) or not await Life.sell_owned_grain(self,main,ids): return false
		if not await Life.click(self,main,"move_local",ids.membership_place_id): return false
		# 原条款期限已由本人获知；不会因为对方尚在旧采购地点而延长邀请。
		var expiry: int=manifest.pending.offer.expires_tick
		while main.kernel.source_identity().tick<expiry:
			if not await Life.wait_halfday(self,main,expiry): return false
		var visible: bool=false
		for row in Flow.visible_rows(main.page):
			if row.get("kind")=="membership_invitation" and row.get("id")==manifest.pending.offer.id and row.get("status")=="expired": visible=true
		# 原任务已公开指向粮行，沿已有通道实际去该处找人；不扩大远地人物查询。
		if not visible and not await Life.click(self,main,"move_local",ids.market_place_id): return false
		var terminal: Dictionary=await Flow.wait_for_answer(self,main,ids,manifest.pending.offer.id,"expired")
		if terminal.is_empty() or not await record("deferred-expired"): return false
		var offer: Dictionary=last_state.organization.membership_offers[manifest.pending.offer.id]
		if not check(offer.status=="expired" and offer.expires_tick==expiry and offer.member_episode_id.is_empty(),"deferred_invitation_really_expired_without_membership"): return false
		var prior: Dictionary=last_state.organization.membership_decisions.get(manifest.pending.offer.source_fact_id,{})
		if not check(prior.get("kind")=="defer" and prior.get("offer_id")==offer.id,"original_personal_defer_retained"): return false
		var task_id: String=manifest.pending.original_task.task_id
		var task: Dictionary=last_state.tasks.get(task_id,{})
		var contracts: Array=dynamic_records(last_state.dynamic_tasks.contracts).filter(func(value): return value.current_task_id.get("value")==task_id)
		if not check(contracts.size()==1 and task.get("status")=="failed" and task.get("late",false) and not task.get("settlement_id","").is_empty(),"deferred_old_task_legally_expired_and_settled"): return false
		var contract: Dictionary=contracts[0]
		return check(contract.status==3 and contract.stop_reason=="expired" and contract.delivered_quantity==1 and contract.frozen_terms.agreed_quantity==2 and contract.paid_reward==2 and contract.refunded_budget==19 and contract.refunded_reward==2 and contract.restitution_refs.is_empty(),"deferred_original_partial_delivery_and_refunds_preserved")
	if (await Flow.wait_for_answer(self,main,ids,manifest.pending.offer.id,"accepted")).is_empty(): return false
	if case_name=="funded" and not manifest.get("refusal_offer_id","").is_empty():
		if (await Flow.wait_for_answer(self,main,ids,manifest.refusal_offer_id,"declined")).is_empty(): return false
	if not await Flow.delegate_contract(self,main,ids): return false
	if case_name=="funded": return await wait_delivery()
	var release: Dictionary=await Flow.reach_release_bank(self,main,ids,"manual-2",save_release)
	if release.is_empty() or not await record("release"): return false
	if save_release:
		manifest.release=release; manifest.banks=bank_hashes(); manifest.release_world_sha256=FileAccess.get_sha256(output.path_join("release-state.json")); write_json("save-manifest.json",manifest)
	return await continue_from_release()

## 真实卖出原粮后继续原合同；旧债可以未清，但其原雇主和唯一义务不得消失。
func continue_from_release() -> bool:
	after_release=true
	var departure: Dictionary=await Flow.finish_old_commitment(self,main,ids)
	if departure.is_empty() or not await verify_paid_contract(): return false
	if not await record("departed"): return false
	var episodes: Array=last_state.organization.members.values().filter(func(value): return value.actor_id==ids.invitee_actor_id and value.get("episode_source")!=null)
	if not check(episodes.size()==1 and episodes[0].response=="departed","actual_member_departed_once"): return false
	var wage_rows: Array=last_state.local_economy.membership_wage_bindings.values().filter(func(value): return value.actor_id==ids.invitee_actor_id)
	if not check(wage_rows.size()==1 and wage_rows[0].ended_tick!=null,"future_wages_stopped_original_binding_retained"): return false
	if not await Flow.continue_ordinary_life(self,main,ids,departure): return false
	return true

## 原标题选择、实际邀请与菜单银行形成唯一来源；没有预置同意或快照安装。
func produce() -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"fresh_owned_profile"): return false
	var pending: Dictionary={}
	if case_name=="deferred": pending=await Flow.start_to_deferred(self,main,ids)
	elif case_name.begins_with("recipient"):
		if not await Life.start_selected(self,main,ids): return false
		while main.kernel.source_identity().tick<ids.deadline_tick and pending.is_empty():
			for row in Flow.visible_rows(main.page):
				if row.get("kind")=="membership_invitation" and row.get("status")=="pending": pending={"offer":row.duplicate(true),"source":main.kernel.source_identity(),"place_id":ids.membership_place_id}
			if pending.is_empty() and not await Life.wait_halfday(self,main,ids.deadline_tick): return false
	else: pending=await Flow.start_to_pending(self,main,ids)
	if not check(not pending.is_empty(),"real_initial_membership_boundary"): return false
	manifest={"format":"HM-NAMED-MEMBERSHIP-MAIN/1","source_commit":binding.source_commit,"producer_pid":OS.get_process_id(),"profile":profile,"directory":directory,"case":case_name,"binding_sha256":FileAccess.get_sha256(binding_path),"pack_set_digest":binding.pack_set_digest,"pending":pending.duplicate(true)}
	if case_name=="funded":
		var refuse_ids: Dictionary=ids.duplicate(true); refuse_ids.invitee_actor_id="society::actor.study_swordswoman"
		var refused: Dictionary=await Flow.invite_person(self,main,refuse_ids)
		if refused.is_empty(): return false
		manifest.refusal_offer_id=refused.offer.id
		manifest.pending.source=main.kernel.source_identity()
	if not await Life.save_via_menu(self,main,"manual-1") or not await record("pending"): return false
	manifest.pending.slot="manual-1"; manifest.pending_world_sha256=FileAccess.get_sha256(output.path_join("pending-state.json")); manifest.banks=bank_hashes()
	if not write_json("save-manifest.json",manifest): return false
	main.kernel.command_completed.connect(command_completed)
	if not await continue_from_pending(true): return false
	return check(bank_hashes()==manifest.banks,"producer_tail_preserves_saved_banks")

## 独立进程只恢复原物理槽；回执、全世界及有效checkpoint均与相同尾程逐项一致。
func consume(release: bool) -> bool:
	var producer: String=case_root.path_join("producer")
	manifest=read_json(producer.path_join("save-manifest.json")); var process=read_json(producer.path_join("process.json")); var launch=read_json(output.path_join("launch.json"))
	if not check(process.get("exit_code")==0 and process.finished_unix<launch.started_unix and manifest.producer_pid!=OS.get_process_id(),"producer_ended_before_independent_consumer"): return false
	if not check(manifest.profile==profile and manifest.directory==directory and manifest.binding_sha256==FileAccess.get_sha256(binding_path) and manifest.banks==bank_hashes(),"original_physical_banks_and_binding"): return false
	var label: String="release" if release else "pending"
	if release:
		if not await Flow.continue_release_bank(self,main,ids,manifest.release): return false
	elif not await Flow.continue_pending(self,main,ids,manifest.pending): return false
	if not await record("restored"): return false
	if not check(FileAccess.get_file_as_string(output.path_join("restored-state.json"))==FileAccess.get_file_as_string(producer.path_join(label+"-state.json")),"restored_whole_world_exact"): return false
	main.kernel.command_completed.connect(command_completed)
	if release:
		if not await continue_from_release(): return false
	elif not await continue_from_pending(false): return false
	if not await record("completed"): return false
	var expected: String="release-receipts.json" if release else "pending-receipts.json"
	var current: Array=release_receipts if release else pending_receipts
	if not check(JSON.stringify(current,"",false,true)+"\n"==FileAccess.get_file_as_string(producer.path_join(expected)),"remaining_formal_receipts_exact"): return false
	if not check(FileAccess.get_file_as_string(output.path_join("completed-state.json"))==FileAccess.get_file_as_string(producer.path_join("completed-state.json")),"continued_whole_world_exact"): return false
	if not check(FileAccess.get_file_as_bytes(output.path_join("completed.hmcheckpoint"))==FileAccess.get_file_as_bytes(producer.path_join("completed.hmcheckpoint")),"continued_effective_checkpoint_exact"): return false
	return check(bank_hashes()==manifest.banks,"consumer_did_not_rewrite_bank")

## 原失败生产者只提供已真实保存的bank；失败状态与全部原证据须原样保留。
func saved_release_source_valid() -> bool:
	var producer: String=saved_case_root.path_join("producer")
	var prior: Dictionary=read_json(producer.path_join("process.json")); var result: Dictionary=read_json(producer.path_join("result.json"))
	if prior.is_empty() or result.is_empty(): return false
	if not check(FileAccess.get_sha256(saved_binding_path)==saved_source.saved_binding_sha256 and saved_source.saved_binding_sha256==manifest.binding_sha256,"original_binding_matches_physical_bank"): return false
	if not check(prior.exit_code==saved_source.original_exit_code and result.get("ok",false)==saved_source.original_ok and not saved_source.original_ok,"original_helper_red_remains_red"): return false
	for name in prior.artifacts:
		if not check(FileAccess.get_sha256(producer.path_join(String(name)))==prior.artifacts[name],"original_artifact_retained_"+String(name)): return false
	return check(manifest.profile==profile and manifest.directory==directory and manifest.pack_set_digest==binding.pack_set_digest and bank_hashes()==manifest.banks,"original_release_banks_intact")

## 新参考及比较都只恢复原manual2；不重放邀请、委派前缀，不修写或重存原银行。
func continue_saved_release(compare: bool) -> bool:
	var producer: String=saved_case_root.path_join("producer")
	manifest=read_json(producer.path_join("save-manifest.json"))
	if manifest.is_empty() or not saved_release_source_valid(): return false
	var launch: Dictionary=read_json(output.path_join("launch.json"))
	if not check(saved_source.original_finished_unix<launch.started_unix,"original_producer_terminal_before_fresh_reference"): return false
	var reference: String=case_root.path_join("release-reference")
	if compare:
		var prior: Dictionary=read_json(reference.path_join("process.json")); var result: Dictionary=read_json(reference.path_join("result.json")); var proof: Dictionary=read_json(reference.path_join("saved-source.json"))
		if not check(prior.exit_code==0 and result.get("ok",false) and prior.finished_unix<launch.started_unix and proof==saved_source and prior.binding_sha256==FileAccess.get_sha256(binding_path),"same_source_successful_reference_terminated_before_comparison"): return false
		for name in prior.artifacts:
			if not check(FileAccess.get_sha256(reference.path_join(String(name)))==prior.artifacts[name],"reference_artifact_retained_"+String(name)): return false
	if not await Flow.continue_release_bank(self,main,ids,manifest.release) or not await record("restored-release"): return false
	if not check(FileAccess.get_file_as_string(output.path_join("restored-release-state.json"))==FileAccess.get_file_as_string(producer.path_join("release-state.json")),"original_release_world_restored_exactly"): return false
	main.kernel.command_completed.connect(command_completed)
	if not await continue_from_release() or not await record("completed"): return false
	if not write_json("release-receipts.json",release_receipts): return false
	if compare:
		if not check(FileAccess.get_file_as_string(output.path_join("completed-state.json"))==FileAccess.get_file_as_string(reference.path_join("completed-state.json")),"saved_release_continued_whole_world_exact"): return false
		if not check(FileAccess.get_file_as_bytes(output.path_join("completed.hmcheckpoint"))==FileAccess.get_file_as_bytes(reference.path_join("completed.hmcheckpoint")),"saved_release_effective_checkpoint_exact"): return false
		if not check(FileAccess.get_file_as_string(output.path_join("release-receipts.json"))==FileAccess.get_file_as_string(reference.path_join("release-receipts.json")),"saved_release_remaining_command_receipts_exact"): return false
	return saved_release_source_valid()

## 路线失败也只读保存空闲原世界；不清空失败、不等待忙碌世界或注入快照。
func preserve_failed_terminal() -> void:
	terminal_diagnostic={"attempted":false,"reason":"no_installed_world"}
	if not is_instance_valid(main) or not main._resident_mode: return
	var source: Dictionary=main.kernel.source_identity()
	var idle: bool=not main._advancing and not main._resident_frame_pending and main._storage_intent.is_empty() and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty()
	terminal_diagnostic={"attempted":false,"source":source,"reason":"native_world_busy" if not idle else "reading_actual_failed_terminal"}
	if not idle:
		write_json("failed-terminal-diagnostic.json",terminal_diagnostic); return
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	terminal_diagnostic={"attempted":true,"source":source,"ok":exported.get("ok",false),"code":exported.get("code",""),"original_failure_count":failures.size()}
	if exported.get("ok",false) and exported.get("json") is String:
		write_text("failed-terminal-state.json",exported.json)
		write_json("failed-terminal-source.json",source)
		terminal_diagnostic.source_unchanged=main.kernel.source_identity()==source
	write_json("failed-terminal-diagnostic.json",terminal_diagnostic)

## 只有显式源码和真实库摘要绑定齐备才实例化原Main，来源准备期不能运行玩法。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--phase="): phase=arg.trim_prefix("--phase=")
		elif arg.begins_with("--case="): case_name=arg.trim_prefix("--case=")
		elif arg.begins_with("--case-root="): case_root=arg.trim_prefix("--case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--binding="): binding_path=arg.trim_prefix("--binding=")
		elif arg.begins_with("--saved-case-root="): saved_case_root=arg.trim_prefix("--saved-case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--saved-binding="): saved_binding_path=arg.trim_prefix("--saved-binding=").simplify_path()
	if phase not in ["produce","consume-pending","consume-release","continue-release-reference","continue-release-compare"] or not case_root.is_absolute_path(): quit(2); return
	var continuing_saved: bool=phase in ["continue-release-reference","continue-release-compare"]
	if continuing_saved and (case_name!="short" or not saved_case_root.is_absolute_path() or not saved_binding_path.is_absolute_path() or saved_case_root==case_root): quit(2); return
	output=case_root.path_join("producer" if phase=="produce" else "release-reference" if phase=="continue-release-reference" else "release-comparison" if phase=="continue-release-compare" else phase)
	if not DirAccess.dir_exists_absolute(output) or FileAccess.file_exists(output.path_join("result.json")): quit(2); return
	binding=read_json(binding_path)
	if not check(binding.get("native_artifact_status")=="BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED" and binding.source_commit==binding.get("reviewed_source_commit") and FileAccess.get_sha256(binding.native_extension.path)==binding.native_extension.sha256,"explicit_actual_library_source_binding"): quit(2); return
	if not check(binding.cases.has(case_name),"declared_formal_case"): quit(2); return
	ids=binding.cases[case_name].duplicate(true); ids.pack_set_digest=binding.pack_set_digest
	if continuing_saved:
		saved_source=read_json(output.path_join("saved-source.json")); profile=saved_source.get("profile","")
		if saved_source.get("saved_case_root")!=saved_case_root or saved_source.get("saved_binding_path")!=saved_binding_path: quit(2); return
	else: profile="membership-"+case_root.sha256_text().left(12)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with((saved_case_root if continuing_saved else case_root)+"/") and (not continuing_saved or directory==saved_source.directory) and ClassDB.class_exists("HanmoResidentBridge"),"isolated_profile_and_native_registration"): quit(2); return
	root.size=Vector2i(1280,900); main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=false
	if continuing_saved: passed=await continue_saved_release(phase=="continue-release-compare")
	elif phase=="produce": passed=await produce()
	else: passed=await consume(phase=="consume-release")
	if passed and phase=="produce": passed=await record("completed")
	if not passed or not failures.is_empty(): preserve_failed_terminal()
	write_json("pending-receipts.json",pending_receipts); write_json("release-receipts.json",release_receipts)
	write_json("result.json",{"ok":passed and failures.is_empty(),"phase":phase,"case":case_name,"pid":OS.get_process_id(),"source_commit":binding.source_commit,"profile":profile,"rows":rows,"failures":failures,"manifest":manifest,"page_error":main._resident_page_error,"message":main._message,"saved_source":saved_source,"terminal_diagnostic":terminal_diagnostic,"scope":"Original producer remains failed; only exact saved-release continuation is proved." if continuing_saved else "Original actual Main route and independent continuation."})
	main.queue_free(); await process_frame
	print("NAMED_MEMBERSHIP_MAIN ",phase," ",case_name," OK=",passed and failures.is_empty()); quit(0 if passed and failures.is_empty() else 1)

## 动态Task序列保存key/record包装；这里只解析原导出形状，不能混当领域字典。
func dynamic_records(values: Array) -> Array:
	var result: Array=[]
	for value in values:
		if value is Dictionary and value.get("record") is Dictionary: result.append(value.record)
	return result
