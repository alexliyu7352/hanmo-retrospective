## 复用原异营返程领域准备，固定返回/报告/围城的旧查询；不跑原40半日和Codec矩阵。
extends "res://tests/test_campaign_return.gd"
const OLD_SOURCE="a4fa824f308b5b33bd602a7286dae19219dde50f"
var oracle_path: String=""
var record_mode: bool=false
var rows: Array=[]
var native_probe=preload("res://tests/native_oracle_probe.gd").new()

## 原生产查询的完整批次与领域helper输出一起冻结，所有候选仅在副本上查询。
func capture(label: String, f: Dictionary) -> void:
	var before: Dictionary=f.state.duplicate(true);var original: Dictionary=f.data.duplicate(true)
	var proposals: Array=Schedule.proposals(f.state,f.data)
	var destination: Dictionary=Campaign.return_destination(f.state,f.data,f.campaign_id)
	var roles: Array=Campaign.report_roles(f.state,f.data,f.campaign_id)
	check(f.state==before and f.data==original,label+"旧数组及领域读口只读")
	var compared: Dictionary=native_probe.compare(label,f.state,f.data,proposals)
	check(compared.ok,label+"已选provider完整数组："+str(compared if not compared.ok else "ok"))
	rows.append({"label":label,"proposals":proposals,"return_destination":destination,"report_roles":roles})

## 只调用原prepared真实任官、编组、破门守点和易帜来源，然后运行有限纯查询。
func run_checks() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--oracle="):oracle_path=arg.trim_prefix("--oracle=")
		if arg=="--record":record_mode=true
	if not check(not oracle_path.is_empty() and ProjectSettings.get_setting("application/config/native_military_schedule_isolated",false),"明确原oracle与隔离项目"):finish();return
	var initialized: Dictionary=native_probe.initialize(OS.get_cmdline_user_args())
	if not check(initialized.ok,"provider模式前置："+str(initialized)):finish();return
	var f: Dictionary=prepared()
	if not check(not f.is_empty(),"原领域准备确实形成有来源的世界"):finish();return
	capture("domain_actual_occupation",f)
	var result: Dictionary=command(f,"campaign_return",{"campaign_id":f.campaign_id,"garrison_id":f.friend_camp})
	if not check(result.ok,"原正式领域返营命令受理"):finish();return
	capture("domain_actual_return_requested",f)
	for mode in ["no_friend","no_route","pending_active","pending_result_player","pending_result_npc","report_far","report_same_city","report_absent"]:
		var candidate: Dictionary=f.duplicate(true)
		match mode:
			"no_friend":candidate.data.garrisons.erase(f.friend_camp)
			"no_route":
				for y in range(5):candidate.data.world_maps["map.return"].cells[y*10+5].terrain="water"
			"pending_active","pending_result_player","pending_result_npc":
				candidate.state.sieges[f.siege_id].withdrawn_army_ids=[f.army_a] if mode=="pending_result_npc" else []
				candidate.state.sieges[f.siege_id].phase="active" if mode=="pending_active" else "result"
			"report_far","report_same_city","report_absent":
				candidate.state=f.before_occupation.duplicate(true)
				candidate.state.campaigns[f.campaign_id].status="awaiting_report"
				candidate.state.campaigns[f.campaign_id].return_garrison_id=f.friend_camp
				if mode=="report_same_city":candidate.state.actors[f.enemy].location=Factory.settlement_location(candidate.data,candidate.data.roles[f.report_role].place_id)
				if mode=="report_absent":
					for role in candidate.state.roles.values():role.enabled=false
		capture("pure_return/"+mode,candidate)
	# 原领域已有真实result及完整部队来源；仅采用原测试同样的控制者切换边界核NPC关闭。
	var closing: Dictionary=f.duplicate(true);closing.state=f.before_occupation.duplicate(true)
	closing.state.player_actor_id=f.provider
	closing.data.actors[f.actor].military_plan={"office_id":f.office,"order_id":f.office,"unit_ids":[f.unit],"food_quantity":100,"money":40}
	capture("domain_npc_before_close",closing)
	var request: Dictionary={}
	for proposal in Schedule.proposals(closing.state,closing.data):
		if proposal.actor_id==f.actor:request=proposal
	if check(request.get("kind")=="siege_close","真实领域result产生NPC正式关闭提案"):
		result=command(closing,request.kind,request.params,f.actor)
		check(result.ok and closing.state.sieges[f.siege_id].phase=="closed","NPC关闭经过原登记preview/apply")
		rows.append({"label":"domain_npc_close_result","request":request,"result":result,"state":closing.state})
	if record_mode and failures==0:
		if check(not FileAccess.file_exists(oracle_path),"领域旧oracle仅首次捕获"):
			var file=FileAccess.open(oracle_path,FileAccess.WRITE)
			file.store_string(JSON.stringify({"format":"HM-MILITARY-RETURN-ORACLE/1","source_commit":OLD_SOURCE,"rows":rows},"\t",true,true)+"\n");file.close()
	elif not record_mode:
		var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(oracle_path),16777216)
		check(parsed.get("ok",false) and parsed.value.get("source_commit")==OLD_SOURCE and parsed.value.get("rows")==rows,"原异营来源完整数组及实际关闭结果一致")
	finish()

## 本门通过只代表原领域来源对照，不代表完整Kernel/GUI或native实现通过。
func finish() -> void:
	var ended: Dictionary=native_probe.finish()
	check(ended.ok,"独立provider证据："+str(ended))
	print("NATIVE_MILITARY_RETURN_ORACLE_CHECKS=",checks," FAILURES=",failures," ROWS=",rows.size())
	quit(1 if failures else 0)
