## 正式prepared外交场景的实际Main按钮流程；参数为现有Main与独立证据文件。
extends RefCounted
const Political=preload("./test_political_career_player_flow.gd")

## 只选当代可见控件，展开来源明确的外交行；会面候选仍由原SceneHost提交。
static func _choice(main: Node, kinds: Array, target: String="") -> Button:
	var deadline: int=Time.get_ticks_msec()+20000
	var expanded: Dictionary={}
	while Time.get_ticks_msec()<deadline:
		if Political._idle(main):
			for button in main.find_children("*","Button",true,false):
				if button.is_queued_for_deletion() or button.disabled or not button.is_visible_in_tree(): continue
				if button.text=="核对条款与办理事项" and not expanded.has(button.get_instance_id()):
					expanded[button.get_instance_id()]=true;button.pressed.emit();break
				var kind: String=button.get_meta("command_kind","")
				if kind not in kinds: continue
				if not target.is_empty():
					var lawful_role: bool=false
					for person in main.kernel.place_sidebar().get("people",[]):
						if person.get("actor_id","")!=target: continue
						for topic in person.get("topics",[]):
							if topic.get("available",false) and button.get_meta("target_id","")==topic.get("role_id",""): lawful_role=true
					if not lawful_role: continue
				return button
		await main.get_tree().process_frame
	return null

static func _page(main: Node) -> Dictionary:
	var deadline: int=Time.get_ticks_msec()+20000
	while Time.get_ticks_msec()<deadline:
		if Political._idle(main):
			var page: Dictionary=main.kernel.diplomacy_entries(main.kernel.presentation_location().get("place_id",""))
			if page.get("code")!="NativeAsyncRequired": return page
		await main.get_tree().process_frame
	return {"ok":false,"code":"DiplomacyReadTimeout"}

## 点击后核原Backend的唯一命令回执，不能只把按钮按下或界面刷新当业务成功。
static func _press(main: Node, button: Button, receipts: Array, steps: Array) -> bool:
	if button==null: return false
	var kind: String=button.get_meta("command_kind","")
	var source: Dictionary=main.kernel.source_identity()
	var count: int=receipts.size()
	var action: Dictionary=button.get_meta("diplomacy_action",{}).duplicate(true)
	button.pressed.emit()
	if not await Political._changed(main,source): return false
	var starts_and_advances: bool=kind in ["wait","envoy_deliver"]
	var expected: int=2 if starts_and_advances else 1
	if receipts.size()!=count+expected or receipts[count].kind!=kind or not receipts[count].result.get("ok",false): return false
	if starts_and_advances and (receipts[count].current.get("tick")!=source.get("tick") or receipts[-1].kind!="advance" or not receipts[-1].result.get("ok",false) or receipts[-1].current.get("tick")!=source.get("tick")+1): return false
	steps.append({"kind":kind,"action":action,"source":source,"current":main.kernel.source_identity(),"receipts":receipts.slice(count)})
	return true

## 自任使者责任独立于条约同意；实际旅行/同场会面/一半日交接都沿原控件推进。
static func run(main: Node, evidence_path: String, expect_npc_reply: bool=false) -> Dictionary:
	if not is_instance_valid(main) or not main._resident_mode or not main._started or not main.kernel.diplomacy_enabled(): return Political._save(evidence_path,{"ok":false,"code":"ActualDiplomacyMainRequired"})
	var actor: String=main.kernel.command_context().get("actor_id","")
	if actor!="society::actor.city_steward": return Political._save(evidence_path,{"ok":false,"code":"ActualCityStewardRequired"})
	var receipts: Array=[];var steps: Array=[]
	main.kernel.command_completed.connect(func(kind: String, receipt: Dictionary, source: Dictionary, current: Dictionary): receipts.append({"kind":kind,"result":receipt.duplicate(true),"source":source.duplicate(true),"current":current.duplicate(true)}))
	for kind in ["diplomatic_propose_passage","diplomatic_commission_envoy","envoy_accept"]:
		if not await _press(main,await _choice(main,[kind]),receipts,steps): return Political._save(evidence_path,{"ok":false,"code":"DiplomacyInitialControlFailed","kind":kind,"steps":steps,"receipts":receipts,"page":await _page(main)})
	if await _choice(main,["envoy_cancel"])==null: return Political._save(evidence_path,{"ok":false,"code":"ActualAcceptedTermsNotExpandable","steps":steps})
	var accepted: Dictionary=await _page(main)
	var refusal: bool=false
	for row in accepted.get("rows",[]):
		if row.has("task_progress") and row.get("navigation_code","")=="InsufficientFunds": refusal=true
	var visible_reason: bool=false
	for label in main.find_children("*","Label",true,false):
		if "本人可用私钱不足" in label.text and label.is_visible_in_tree(): visible_reason=true
	if not refusal or not visible_reason: return Political._save(evidence_path,{"ok":false,"code":"ActualPrivateTravelRefusalNotVisible","page":accepted,"steps":steps,"receipts":receipts})
	# 原作者城吏工资第4半日到期；原等候控件真实推进，不能凭helper增添私钱。
	for half_day in range(4):
		if not await _press(main,await _choice(main,["wait"]),receipts,steps): return Political._save(evidence_path,{"ok":false,"code":"OriginalSalaryWaitFailed","half_day":half_day,"steps":steps,"receipts":receipts})
	for segment in range(16):
		if main.kernel.presentation_location().get("place_id","")=="society::place.sword_gate": break
		if not await _press(main,await _choice(main,["move_local","travel","advance"]),receipts,steps): return Political._save(evidence_path,{"ok":false,"code":"ActualDiplomaticNavigationMissing","segment":segment,"steps":steps,"receipts":receipts,"page":await _page(main)})
	if main.kernel.presentation_location().get("place_id","")!="society::place.sword_gate": return Political._save(evidence_path,{"ok":false,"code":"ActualDiplomaticDestinationNotReached","steps":steps})
	if not await _press(main,await _choice(main,["talk"],"society::actor.sect_steward"),receipts,steps): return Political._save(evidence_path,{"ok":false,"code":"ActualRepresentativeMeetingMissing","steps":steps,"receipts":receipts})
	var interaction: Dictionary=main.kernel.interaction_view()
	if interaction.get("phase") not in ["say","choice"]: return Political._save(evidence_path,{"ok":false,"code":"ActualRepresentativeContactPhaseMissing","interaction":interaction,"steps":steps})
	if not await _press(main,await _choice(main,["envoy_deliver"]),receipts,steps): return Political._save(evidence_path,{"ok":false,"code":"ActualEnvoyHandoverControlMissing","steps":steps,"receipts":receipts,"page":await _page(main)})
	# 作者交接只需1半日；原Main收到ActionStarted后自动提交一次advance，Native另保自然active bank。
	var active: Dictionary={"scope":"Main verifies original ActionStarted receipt; separate Native probe records the active bank","main_action_started_receipt":receipts[-2].duplicate(true)}
	var completed: Dictionary=await _page(main);var final: bool=false;var waiting_reply: bool=false
	var expected_text: String="对方已明确接受拟议条款，尚待必要认可与传达。" if expect_npc_reply else "提议已实际递交，等对方明确答复。"
	for row in completed.get("rows",[]):
		var progress: Dictionary=row.get("task_progress",{})
		if progress.get("status")=="success" and not progress.get("delivery_fact_id","").is_empty() and not progress.get("final_fact_id","").is_empty() and not progress.has("action_id"): final=true
		if row.get("text")==expected_text: waiting_reply=true
	if not final or not waiting_reply: return Political._save(evidence_path,{"ok":false,"code":"ActualDeliveryAndPendingReplyNotShown","steps":steps,"page":completed})
	return Political._save(evidence_path,{"ok":true,"scenario_id":"diploreply::scenario.qingchuan_diplomatic_response" if expect_npc_reply else "civicecon::scenario.qingchuan_diplomacy","explicit_npc_response":expect_npc_reply,"actor_id":actor,"steps":steps,"receipts":receipts,"accepted":accepted,"active":active,"completed":completed,"source":main.kernel.source_identity(),"limits":"真实Main按钮和回执；递交完成及对方明确接受仍待认可，不产生条约效力。此门不代表像素视觉验收。" if expect_npc_reply else "真实Main按钮和回执；自任无需返报的递交完成，尚无对方答复或条约效力。此门不代表像素视觉验收。"})
