## 精确旧军政提案oracle；复用原动态本人军权链，不写native协议或复制军事决策。
extends "res://tests/test_dynamic_actor_schedule.gd"
const OLD_SOURCE="a4fa824f308b5b33bd602a7286dae19219dde50f"
var oracle_path: String=""
var record_mode: bool=false
var rows: Array=[]
var captured: Dictionary={}
var native_probe=preload("res://tests/native_oracle_probe.gd").new()

## 本门只记录原链指定状态，不运行父测试的Codec往返，也不计恢复通过断言。
func restore_same(k, label: String) -> bool:
	capture_tail(k)
	rows.append({"label":"actual_checkpoint/"+label,"state":k.snapshot()})
	return true

## 完整旧数组及排序单独冻结；候选只读，不让输出别名反写原state/data。
func proposal_case(label: String, state: Dictionary, data: Dictionary) -> Array:
	var before: Dictionary=state.duplicate(true); var authored: Dictionary=data.duplicate(true)
	var result: Array=MilitarySchedule.proposals(state,data)
	check(state==before and data==authored,label+"旧查询只读")
	var compared: Dictionary=native_probe.compare(label,state,data,result)
	check(compared.ok,label+"已选provider边界："+str(compared if not compared.ok else "ok"))
	rows.append({"label":label,"ids":Behavior.scheduled_ids(state,data,"military_plan"),"campaign_order":state.campaigns.keys(),"proposals":result.duplicate(true)})
	return result

## 复用父链的真实查询点，捕获完整批次而非只取目标NPC的部分答案。
func military_request(k, actor_id: String) -> Dictionary:
	capture_tail(k)
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var result: Dictionary=super.military_request(k,actor_id)
	proposal_case("actual_request/"+str(rows.size())+"/"+actor_id,state,data)
	if not captured.has("initial"):
		captured.initial=true
		initial_boundaries(state,data,actor_id)
	if result.get("kind")=="army_form" and not captured.has("accepted"):
		captured.accepted=true
		campaign_boundaries(state,data,actor_id,result.params.order_id)
	if result.get("kind")=="campaign_return" and not captured.has("army"):
		captured.army=true
		army_boundaries(state,data,actor_id,result.params.campaign_id)
	return result

## 原父类捕获的军事候选按发生序仅记录一次，保留完整原批次、quote与候选状态。
func capture_tail(k) -> void:
	var key: int=k.get_instance_id()
	var offset: int=captured.get(key,0)
	for index in range(offset,k.military_witness.size()):
		var witness: Dictionary=k.military_witness[index]
		check(witness.unchanged,"实际军事尾部原提案查询只读")
		var compared: Dictionary=native_probe.compare("actual_candidate/"+str(index),witness.state,k.content().data,witness.proposals)
		check(compared.ok,"实际军事候选完整原数组："+str(compared if not compared.ok else "ok"))
		rows.append({"label":"actual_candidate/"+str(index),"witness":witness.duplicate(true)})
	captured[key]=k.military_witness.size()

## 原军事尾部已保存实际候选与真实quote；只取同一记录，不替调度器提前选择命令。
func military_witness(k, actor_id: String, kind: String) -> Dictionary:
	capture_tail(k)
	var result: Dictionary=super.military_witness(k,actor_id,kind)
	if not result.is_empty():
		proposal_case("actual_tail/"+kind,result.state,k.content().data)
		rows.append({"label":"actual_tail_witness/"+kind,"witness":result.duplicate(true)})
	return result

## 原静态军官不变，过滤和早退候选明确不作为可保存或新玩法状态。
func initial_boundaries(state: Dictionary,data: Dictionary,actor: String) -> void:
	for mode in ["player","dead","unavailable","busy","personal_battle","military_battle","appointment_history"]:
		var s: Dictionary=state.duplicate(true)
		match mode:
			"player":s.player_actor_id=actor
			"dead":s.actors[actor].alive=false
			"unavailable":s.actors[actor].available=false
			"busy":s.actors[actor].action_id="pure.busy"
			"personal_battle":s.battles["pure.battle"]={"kind":"personal","phase":"result","units":{actor:{}}}
			"military_battle":s.battles["pure.battle"]={"kind":"military","phase":"planning","participant_actor_ids":[actor]}
			"appointment_history":s.offices[data.actors[actor].military_plan.office_id].appointment_id="pure.history"
		var actual: Array=proposal_case("pure_initial/"+mode,s,data)
		check(not _proposed(actual,actor),"pure_initial/"+mode+"确实排除目标本人")
	walk_boundaries(state,data,actor)

## 最后匹配campaign覆盖含closed；原已受令本人提供来源，改值只服务分支对照。
func campaign_boundaries(state: Dictionary,data: Dictionary,actor: String,campaign_id: String) -> void:
	for mode in ["last_closed","last_accepted","authority_disabled","foreign_holder","unknown_status"]:
		var s: Dictionary=state.duplicate(true); var current: Dictionary=s.campaigns[campaign_id]
		match mode:
			"last_closed","last_accepted":
				var extra: Dictionary=current.duplicate(true);extra.id="pure.last";extra.status="closed" if mode=="last_closed" else "accepted"
				s.campaigns[extra.id]=extra
				if mode=="last_accepted":current.status="closed"
			"authority_disabled":s.offices[current.office_id].enabled=false
			"foreign_holder":s.offices[current.office_id].holder_actor_id=s.player_actor_id
			"unknown_status":current.status="pure.unknown"
		var actual: Array=proposal_case("pure_campaign/"+mode,s,data)
		var selected: Dictionary=for_actor(actual,actor)
		if mode in ["last_closed","unknown_status"]:check(selected.is_empty(),mode+"确实无提案")
		if mode=="last_accepted":check(selected.get("kind")=="army_form" and selected.params.order_id=="pure.last","最后匹配项覆盖前面的closed军令")
		if mode in ["authority_disabled","foreign_holder"]:check(selected.get("kind")=="campaign_cancel",mode+"原已受令本人仍提出取消")

## 已真实形成军团后隔离原operating/返程/围城优先分支，不提交这些纯候选。
func army_boundaries(state: Dictionary,data: Dictionary,actor: String,campaign_id: String) -> void:
	for mode in ["mustered","operating_deliver","operating_occupy","operating_finished","operating_unknown","siege_active","siege_result_npc","siege_result_player","returning","returning_no_camp","report_absent"]:
		var s: Dictionary=state.duplicate(true);var d: Dictionary=data.duplicate(true);var c: Dictionary=s.campaigns[campaign_id]
		s.offices[c.office_id].enabled=true;s.offices[c.office_id].holder_actor_id=actor
		c.status="operating";c.objective_result={}
		match mode:
			"mustered":c.status="mustered"
			"operating_deliver":d.military_orders[c.template_id].objective.kind="deliver_supply"
			"operating_occupy":d.military_orders[c.template_id].objective.kind="occupy"
			"operating_finished":c.objective_result={"kind":"pure.completed"}
			"operating_unknown":d.military_orders[c.template_id].objective.kind="pure.unknown"
			"siege_active","siege_result_npc","siege_result_player":
				s.sieges["pure.siege"]={"phase":"active" if mode=="siege_active" else "result","armies":{c.army_id:{}},"withdrawn_army_ids":[]}
				if mode=="siege_result_player":s.armies[c.army_id].member_actor_ids.append(s.player_actor_id)
			"returning":c.status="returning"
			"returning_no_camp":
				c.status="returning"
				for camp in d.garrisons.values():camp.organization_id="pure.foreign"
				for stronghold in s.strongholds.values():stronghold.owner_organization_id="pure.foreign"
			"report_absent":
				c.status="awaiting_report"
				for role in s.roles.values():role.enabled=false
		var actual: Array=proposal_case("pure_army/"+mode,s,d)
		var expected: Dictionary={"mustered":"campaign_depart","operating_deliver":"campaign_deliver","operating_occupy":"siege_start","operating_finished":"campaign_return","operating_unknown":"","siege_active":"","siege_result_npc":"siege_close","siege_result_player":"","returning":"army_return","returning_no_camp":"","report_absent":""}
		check(for_actor(actual,actor).get("kind","")==expected[mode],"pure_army/"+mode+"确实命中原目标分支")

## 只按永久actor_id取本行结果，断言不用显示姓名或部分批次替代完整oracle。
func for_actor(proposals: Array, actor: String) -> Dictionary:
	for proposal in proposals:
		if proposal.actor_id==actor:return proposal
	return {}

## 原place集合构成纯BFS候选，保持邻接数组原序；不改正式作者拓扑。
func walk_boundaries(state: Dictionary,data: Dictionary,actor: String) -> void:
	var nodes: Array=["west.street","west.office","west.tavern","west.estate"]
	for mode in ["first_office","first_tavern","cycle","unreachable","same","missing_target","foreign_interior","wilderness","nonsettlement_same_place"]:
		var s: Dictionary=state.duplicate(true);var d: Dictionary=data.duplicate(true)
		s.actors[actor].location={"kind":"Settlement","settlement_id":"city.west","place_id":nodes[0]}
		d.places[nodes[0]].passages=[nodes[1],nodes[2]]
		d.places[nodes[1]].passages=[nodes[0],nodes[3]]
		d.places[nodes[2]].passages=[nodes[0],nodes[3]]
		d.places[nodes[3]].passages=[nodes[1],nodes[2]]
		var target: String=nodes[3]
		if mode=="first_tavern":d.places[nodes[0]].passages.reverse()
		if mode=="cycle":d.places[nodes[1]].passages=[nodes[2],nodes[0]]
		if mode=="unreachable":d.places[nodes[1]].passages=[nodes[0]];d.places[nodes[2]].passages=[nodes[0]]
		if mode=="same":target=nodes[0]
		if mode=="missing_target":target="pure.absent"
		if mode=="foreign_interior":target="east.home";s.actors[actor].location.place_id=nodes[1]
		if mode=="wilderness":s.actors[actor].location={"kind":"Wilderness","map_id":"pure.map","point":[0,0]}
		if mode=="nonsettlement_same_place":s.actors[actor].location.kind="pure.kind";target=nodes[0]
		var before: Dictionary=s.duplicate(true);var original: Dictionary=d.duplicate(true)
		var result: Dictionary=MilitarySchedule.at_or_walk(s,d,actor,target,"office_accept",{"office_id":"office.military.a"})
		check(s==before and d==original,"pure_walk/"+mode+"原查询只读")
		rows.append({"label":"pure_walk/"+mode,"proposal":result})
		var expected: Dictionary={"first_office":{"kind":"move_local","params":{"place_id":"west.office"}},"first_tavern":{"kind":"move_local","params":{"place_id":"west.tavern"}},"cycle":{"kind":"move_local","params":{"place_id":"west.tavern"}},"unreachable":{},"same":{"kind":"office_accept","params":{"office_id":"office.military.a"}},"missing_target":{},"foreign_interior":{"kind":"move_local","params":{"place_id":"west.street"}},"wilderness":{"kind":"travel","params":{"destination":"city.west"}},"nonsettlement_same_place":{"kind":"office_accept","params":{"office_id":"office.military.a"}}}
		check(result==expected[mode],"pure_walk/"+mode+"确实覆盖原邻接与位置优先级")
		var office: String=d.actors[actor].military_plan.office_id
		d.roles[d.offices[office].appointing_role_id].place_id=target
		var batch: Array=proposal_case("pure_walk_batch/"+mode,s,d)
		var selected: Dictionary=for_actor(batch,actor).duplicate(true);selected.erase("actor_id")
		var leaf: Dictionary=MilitarySchedule.at_or_walk(s,d,actor,target,"office_accept",{"office_id":office,"context":{"place_id":target,"role_id":d.offices[office].appointing_role_id,"provider_actor_id":s.roles[d.offices[office].appointing_role_id].actor_id}})
		check(selected==leaf,"pure_walk_batch/"+mode+"完整出口与原叶相同")

## 显式首次捕获与普通比对分开；这里只跑原军事本人短链及本门纯候选。
func run_checks() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--oracle="):oracle_path=arg.trim_prefix("--oracle=")
		if arg=="--record":record_mode=true
	if not check(ProjectSettings.get_setting("application/config/native_military_schedule_isolated",false) and not path.is_empty() and not oracle_path.is_empty(),"显式隔离工程与旧oracle参数"):finish();return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not check(parsed.get("ok",false) and parsed.get("value",{}).has("military"),"原军事作者索引"):finish();return
	if record_mode and not check(FileAccess.get_sha256("res://core/military_schedule.gd")=="d7e80dfe46f18d999d9dadd920465059572670c72c804889372a2b9eb926cab3","首次oracle只能使用精确旧算法字节"):finish();return
	var initialized: Dictionary=native_probe.initialize(OS.get_cmdline_user_args())
	if not check(initialized.ok,"provider模式前置："+str(initialized)):finish();return
	check_military(parsed.value.military.catalog)
	if record_mode and failures==0:
		if check(not FileAccess.file_exists(oracle_path),"旧oracle只允许首次新建"):
			var file=FileAccess.open(oracle_path,FileAccess.WRITE)
			file.store_string(JSON.stringify({"format":"HM-MILITARY-OLD-ORACLE/1","source_commit":OLD_SOURCE,"rows":rows},"\t",true,true)+"\n");file.close()
	elif not record_mode:
		var oracle: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(oracle_path),16777216)
		check(oracle.get("ok",false) and oracle.value.get("source_commit")==OLD_SOURCE and oracle.value.get("rows")==rows,"完整旧提案数组、顺序、实际状态和候选见证一致")
	finish()

## 只报告实际执行的军事旧行为门；不把source oracle准备宣称为native通过。
func finish() -> void:
	var ended: Dictionary=native_probe.finish()
	check(ended.ok,"独立provider证据："+str(ended))
	print("NATIVE_MILITARY_OLD_ORACLE_CHECKS=",checks," FAILURES=",failures," ROWS=",rows.size()," RECORD=",record_mode)
	quit(1 if failures else 0)
