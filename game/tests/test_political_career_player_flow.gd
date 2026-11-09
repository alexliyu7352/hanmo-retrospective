## 正式政治情景的Main helper；由隔离stage传入已用真实城吏开启的新局，不创建替身世界。
extends RefCounted

## 参数为实际Main、输出证据绝对路径。需politics/1+political_fiscal/1同源原生及新prepared包。
## 调用：await load("res://tests/test_political_career_player_flow.gd").run(main, evidence_path)
static func run(main: Node, evidence_path: String) -> Dictionary:
	if not is_instance_valid(main) or not main.is_inside_tree() or not main._resident_mode or not main._started or not main._politics_enabled():
		return {"ok":false,"code":"PoliticalMainRequired"}
	var actor: String=main.kernel.command_context().get("actor_id","")
	if actor!="society::actor.city_steward": return {"ok":false,"code":"AuthoredCityStewardRequired"}
	var before: Dictionary=main.kernel.source_identity()
	var steps: Array=[]
	var receipts: Array=[]
	main.kernel.command_completed.connect(func(kind: String, receipt: Dictionary, source: Dictionary, current: Dictionary): receipts.append({"kind":kind,"result":receipt.duplicate(true),"source":source.duplicate(true),"current":current.duplicate(true)}))
	# 同一真实现任可把已有父Grant覆盖的空缺子职位委任本人；接受和到任仍分别点击。
	for choice in [{"kind":"political_set_tax","mode":"normal"},{"kind":"political_appoint","target_actor_id":actor},{"kind":"political_answer_appointment","accept":true},{"kind":"political_arrive"},{"kind":"political_set_tax","mode":"lenient"},{"kind":"political_dismiss"},{"kind":"political_set_tax","mode":"normal"}]:
		var selection_trace: Array=[]
		var button: Button=await _choice(main,choice,selection_trace)
		if button==null: return _save(evidence_path,{"ok":false,"code":"PoliticalChoiceNotAvailable","choice":choice,"steps":steps,"selection_trace":selection_trace,"diagnostic":_diagnostic(main)})
		var source: Dictionary=main.kernel.source_identity()
		var action: Dictionary=button.get_meta("politics_action",{}).duplicate(true)
		var receipt_count: int=receipts.size()
		button.pressed.emit()
		var completed: bool=await _changed(main,source)
		if not completed: return _save(evidence_path,{"ok":false,"code":"PoliticalCommandDidNotCommit","choice":choice,"steps":steps})
		if receipts.size()!=receipt_count+1 or receipts[-1].kind!=choice.kind or not receipts[-1].result.get("ok",false):
			return _save(evidence_path,{"ok":false,"code":"PoliticalReceiptMismatch","choice":choice,"receipts":receipts,"steps":steps})
		var current: Dictionary=main.kernel.source_identity()
		if current.get("tick")!=before.get("tick") or current.get("revision")!=source.get("revision")+1:
			return _save(evidence_path,{"ok":false,"code":"PoliticalZeroDurationMismatch","choice":choice,"source":source,"current":current,"steps":steps})
		steps.append({"choice":choice,"action":action,"before":source,"after":current,"receipt":receipts[-1].duplicate(true) if not receipts.is_empty() else {},"selection_trace":selection_trace})
	# 最后一次税策仍由原城吏父Office合法执行，子Office撤职不能让组织消失。
	var site: String=main.kernel.presentation_location().get("place_id","")
	var fiscal: Dictionary=await _fiscal(main,site)
	if not fiscal.get("ok",false) or fiscal.get("rows",[]).is_empty() or fiscal.rows[0].get("source_fact_id","").is_empty():
		return _save(evidence_path,{"ok":false,"code":"PoliticalPolicyFactMissing","steps":steps,"fiscal":fiscal})
	return _save(evidence_path,{"ok":true,"scenario_id":str(main.kernel.get("_scenario_id")),"actor_id":actor,"before":before,"after":main.kernel.source_identity(),"steps":steps,"fiscal":fiscal,"limits":"Main控件/真实命令局部；独立银行冷恢复、NPC与财政实收另验，不是GUI视觉验收。"})

## 只查真实控件的领域metadata；没有按钮时不能改世界补造可用命令。
static func _choice(main: Node, choice: Dictionary, trace: Array) -> Button:
	var deadline: int=Time.get_ticks_msec()+20000
	var expanded_nodes: Dictionary={}
	while Time.get_ticks_msec()<deadline:
		if not _idle(main):
			await main.get_tree().process_frame
			continue
		for candidate in main.find_children("*","Button",true,false):
			if candidate.is_queued_for_deletion() or candidate.disabled or not candidate.is_visible_in_tree(): continue
			if candidate.text=="核对职责与交接":
				var node_id: int=candidate.get_instance_id()
				if expanded_nodes.has(node_id): continue
				expanded_nodes[node_id]=true
				if trace.size()<40: trace.append({"node":candidate.get_instance_id(),"scene":main._scene_counter,"frame_pending":main._resident_frame_pending,"advancing":main._advancing,"before":main.get_meta("politics_terms_expanded",{}).duplicate(true)})
				candidate.pressed.emit()
				break
			var action: Dictionary=candidate.get_meta("politics_action",{})
			if action.get("command")!=choice.kind: continue
			var matches: bool=true
			for key in choice:
				if key!="kind" and action.get("params",{}).get(key)!=choice[key]: matches=false
			if matches: return candidate
		await main.get_tree().process_frame
	return null

## 失败只记录当前合法页及真实按钮；不读取或改写领域私有世界。
static func _diagnostic(main: Node) -> Dictionary:
	var buttons: Array=[]
	for button in main.find_children("*","Button",true,false):
		if button.has_meta("politics_action") or button.text in ["核对职责与交接","收起职责与交接"]:
			buttons.append({"text":button.text,"disabled":button.disabled,"visible":button.is_visible_in_tree(),"action":button.get_meta("politics_action",{})})
	var site: String=main.kernel.presentation_location().get("place_id","")
	return {"scene":main._scene_counter,"frame_pending":main._resident_frame_pending,"advancing":main._advancing,"expanded":main.get_meta("politics_terms_expanded",{}),"buttons":buttons,"page":main.kernel.politics_entries(site),"page_error":main._resident_page_error}

## 展开按钮刷新也走原异步frame；旧按钮尚未退休时不能在下一帧重复切回折叠。
static func _idle(main: Node) -> bool:
	return not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty()

## Main仍持唯一native工作槽；等待确切提交源和读取收尾，不自行轮转业务时钟。
static func _changed(main: Node, source: Dictionary) -> bool:
	var deadline: int=Time.get_ticks_msec()+20000
	while Time.get_ticks_msec()<deadline:
		if main.kernel.source_identity()!=source and _idle(main): return true
		await main.get_tree().process_frame
	return false

static func _fiscal(main: Node, site: String) -> Dictionary:
	var deadline: int=Time.get_ticks_msec()+20000
	while Time.get_ticks_msec()<deadline:
		var value: Dictionary=main.kernel.political_fiscal_entries(site)
		if value.get("code")!="NativeAsyncRequired": return value
		await main.get_tree().process_frame
	return {"ok":false,"code":"PoliticalFiscalReadTimeout"}

## 证据文件只写显式输出路径；目录/源摘要由调用它的正式stage一并冻结。
static func _save(path: String, result: Dictionary) -> Dictionary:
	if path.is_empty(): return {"ok":false,"code":"EvidencePathRequired","result":result}
	var file: FileAccess=FileAccess.open(path,FileAccess.WRITE)
	if file==null: return {"ok":false,"code":"EvidenceWriteFailed","result":result}
	file.store_string(JSON.stringify(result,"\t",false,true)+"\n")
	return result
