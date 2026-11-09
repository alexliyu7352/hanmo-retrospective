## 正式治理情景只用Main真实控件：本人签发、受领，再沿原活动页逐半日推进。
extends RefCounted
const Political=preload("./test_political_career_player_flow.gd")

## 需正常prepared新情景、同源三治理模块与真实城吏；不是无参默认测试或视觉验收。
static func run(main: Node, evidence_path: String) -> Dictionary:
	if not is_instance_valid(main) or not main._resident_mode or not main.kernel.politics_governance_enabled(): return {"ok":false,"code":"ActualGovernanceMainRequired"}
	var actor: String=main.kernel.command_context().get("actor_id","")
	if actor!="society::actor.city_steward": return {"ok":false,"code":"AuthoredCityStewardRequired"}
	var before: Dictionary=main.kernel.source_identity()
	var steps: Array=[];var receipts: Array=[]
	main.kernel.command_completed.connect(func(kind: String, receipt: Dictionary, source: Dictionary, current: Dictionary): receipts.append({"kind":kind,"result":receipt.duplicate(true),"source":source.duplicate(true),"current":current.duplicate(true)}))
	for choice in [{"kind":"political_commission_governance","executor_actor_id":actor},{"kind":"governance_accept"}]:
		var selection: Array=[]
		var button: Button=await Political._choice(main,choice,selection)
		if button==null: return Political._save(evidence_path,{"ok":false,"code":"GovernanceChoiceMissing","choice":choice,"steps":steps,"diagnostic":Political._diagnostic(main)})
		var source: Dictionary=main.kernel.source_identity();var count: int=receipts.size()
		var action: Dictionary=button.get_meta("politics_action",{}).duplicate(true)
		button.pressed.emit()
		if not await Political._changed(main,source): return Political._save(evidence_path,{"ok":false,"code":"GovernanceCommandDidNotCommit","choice":choice,"steps":steps,"receipts":receipts})
		var started: bool=choice.kind=="governance_accept"
		var expected_count: int=2 if started else 1
		if receipts.size()!=count+expected_count or receipts[count].kind!=choice.kind or not receipts[count].result.get("ok",false): return Political._save(evidence_path,{"ok":false,"code":"GovernanceCommandReceiptMissing","choice":choice,"steps":steps,"receipts":receipts})
		if receipts[count].current.get("tick")!=source.get("tick"): return Political._save(evidence_path,{"ok":false,"code":"GovernanceAcceptanceAdvancedClock"})
		if started and (receipts[-1].kind!="advance" or not receipts[-1].result.get("ok",false) or receipts[-1].source.get("tick")!=source.get("tick") or receipts[-1].current.get("tick")!=source.get("tick")+1): return Political._save(evidence_path,{"ok":false,"code":"OriginalActionStartAdvanceMismatch","receipts":receipts})
		steps.append({"action":action,"source":source,"after":main.kernel.source_identity(),"receipts":receipts.slice(count)})
	# 原Main对ActionStarted另推进第1半日；剩余5次仍由原活动页明确点击，合计6次真实advance。
	var open: Button=await _button(main,"growth","")
	if open==null: return Political._save(evidence_path,{"ok":false,"code":"ActualActivityRouteMissing","steps":steps})
	open.pressed.emit()
	for remaining in range(5,0,-1):
		var advance: Button=await _button(main,"advance","GrowthAdvance")
		if advance==null: return Political._save(evidence_path,{"ok":false,"code":"ActualHalfDayButtonMissing","remaining":remaining,"steps":steps})
		var labels: Array=main.find_children("GrowthActionStatus","Label",true,false)
		if labels.size()!=1 or not ("剩余 %d 个半日"%remaining) in labels[0].text: return Political._save(evidence_path,{"ok":false,"code":"ActualActionCountdownMismatch","remaining":remaining,"steps":steps})
		var source: Dictionary=main.kernel.source_identity();var count: int=receipts.size()
		advance.pressed.emit()
		if not await Political._changed(main,source) or receipts.size()!=count+1 or receipts[-1].kind!="advance" or not receipts[-1].result.get("ok",false): return Political._save(evidence_path,{"ok":false,"code":"GovernanceHalfDayReceiptMissing","remaining":remaining,"steps":steps,"receipts":receipts})
		if main.kernel.source_identity().get("tick")!=source.get("tick")+1: return Political._save(evidence_path,{"ok":false,"code":"GovernanceHalfDayClockMismatch"})
		steps.append({"remaining_before":remaining,"source":source,"after":main.kernel.source_identity(),"receipt":receipts[-1]})
	var close: Button=await _button(main,"growth_close","GrowthClose")
	if close==null: return Political._save(evidence_path,{"ok":false,"code":"ActualActivityReturnMissing","steps":steps})
	close.pressed.emit()
	var page: Dictionary=await _page(main)
	var complete: bool=false;var support: bool=false
	for row in page.get("rows",[]):
		if row.get("kind")=="governance_task" and str(row.get("text","")).contains("已办妥"): complete=true
		if row.get("kind")=="governance_jurisdiction" and not str(row.get("text","")).contains("本辖区民心：50。"): support=true
	if not page.get("ok",false) or not complete or not support: return Political._save(evidence_path,{"ok":false,"code":"ActualGovernanceFinalPageMissing","steps":steps,"page":page})
	# 公库实际支付120后，再次真实委任仍可签发，但原受领控制须解释可用钱不足。
	var selection: Array=[]
	var issue: Button=await Political._choice(main,{"kind":"political_commission_governance","executor_actor_id":actor},selection)
	if issue==null: return Political._save(evidence_path,{"ok":false,"code":"SecondRealCommissionUnavailable","steps":steps})
	var source: Dictionary=main.kernel.source_identity();var receipt_count: int=receipts.size()
	issue.pressed.emit()
	if not await Political._changed(main,source) or receipts.size()!=receipt_count+1 or not receipts[-1].result.get("ok",false): return Political._save(evidence_path,{"ok":false,"code":"SecondRealCommissionDidNotCommit","steps":steps})
	steps.append({"source":source,"after":main.kernel.source_identity(),"receipt":receipts[-1]})
	var blocked: Button=await _disabled_accept(main)
	var explained: bool=false
	if blocked!=null:
		for label in blocked.get_parent().find_children("*","Label",true,false):
			if label.is_visible_in_tree() and label.text.contains("公库可用钱不足"): explained=true
	if blocked==null or not explained: return Political._save(evidence_path,{"ok":false,"code":"InsufficientPublicFundsControlMissing","steps":steps,"page":await _page(main)})
	var blocked_action: Dictionary=blocked.get_meta("politics_action",{}).duplicate(true)
	return Political._save(evidence_path,{"ok":true,"scenario_id":"civicecon::scenario.qingchuan_governance","actor_id":actor,"before":before,"after":main.kernel.source_identity(),"steps":steps,"receipts":receipts,"page":page,"blocked_acceptance":blocked_action,"limits":"真实Main控件/9份命令回执；Native另核实际资产及三域原子出口，不代表像素视觉验收。"})

## 只选当前可见可用的原按钮；等待页面/报价齐备，不调用私有业务代替点击。
static func _button(main: Node, kind: String, node_name: String) -> Button:
	var deadline: int=Time.get_ticks_msec()+20000
	while Time.get_ticks_msec()<deadline:
		if Political._idle(main):
			for button in main.find_children("*","Button",true,false):
				if not button.is_queued_for_deletion() and not button.disabled and button.is_visible_in_tree() and button.get_meta("command_kind","")==kind and (node_name.is_empty() or str(button.name)==node_name): return button
		await main.get_tree().process_frame
	return null

static func _page(main: Node) -> Dictionary:
	var deadline: int=Time.get_ticks_msec()+20000
	while Time.get_ticks_msec()<deadline:
		if Political._idle(main):
			var page: Dictionary=main.kernel.politics_governance(main.kernel.presentation_location().get("place_id",""))
			if page.get("code")!="NativeAsyncRequired": return page
		await main.get_tree().process_frame
	return {"ok":false,"code":"GovernanceFinalPageTimeout"}

## 展开当前责任后只寻找真实禁用按钮；不把DTO禁用直接当可见控件已正确渲染。
static func _disabled_accept(main: Node) -> Button:
	var deadline: int=Time.get_ticks_msec()+20000
	var expanded: Dictionary={}
	while Time.get_ticks_msec()<deadline:
		if Political._idle(main):
			for button in main.find_children("*","Button",true,false):
				if button.is_queued_for_deletion() or not button.is_visible_in_tree(): continue
				if button.text=="核对职责与交接" and not expanded.has(button.get_instance_id()):
					expanded[button.get_instance_id()]=true;button.pressed.emit();break
				var action: Dictionary=button.get_meta("politics_action",{})
				if action.get("command")=="governance_accept" and action.get("code")=="InsufficientReliefFunds" and button.disabled: return button
		await main.get_tree().process_frame
	return null
