## 个人战早门与围城/会面共存的窄补充门；其他kind即使伪造注册元数据也不得跳过公共守卫。
extends "res://tests/test_registered_personal.gd"

## 用独立旧Kernel比较只读错误优先级；合成状态仅供守卫测试，不宣称可保存或可玩。
func run_checks() -> void:
	if not check(not legacy_path.is_empty(),"早门补充须有真实旧Kernel"): finish(); return
	legacy=load(legacy_path).new()
	if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"旧Kernel读取正式目录"): finish(); return
	var k=fresh()
	if not check(F.send(k,"move_local",{"place_id":"west.training"}).ok,"到真实当面切磋位置"): finish(); return
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	for variant in ["siege","encounter"]:
		var state: Dictionary=before.duplicate(true)
		if variant=="siege":
			state.armies["guard-army"]={"status":"active","member_actor_ids":[actor]}
			state.sieges["guard-siege"]={"phase":"active","armies":{"guard-army":{}},"withdrawn_army_ids":[]}
		else: state.encounters["guard-encounter"]={"id":"guard-encounter","status":"pending","participants":[actor]}
		for kind in Personal.KINDS:
			var params: Dictionary={"context":F.context(k,"role.wei")} if kind=="duel_start" else {}
			var unchanged: Dictionary=state.duplicate(true)
			var old: Dictionary=legacy._preview(state,kind,params,actor)
			check(k._preview(state,kind,params,actor)==old and state==unchanged,"个人战早门保持原首错且只读 "+variant+" "+kind)
		k._registry.commands["invented_personal"]={"owner":"personal","mode":"instant","gate_id":Commands.GATE_ID,"preview":k.probe.preview,"apply":k.probe.apply}
		var calls: Dictionary=k.probe.calls.duplicate(true)
		var blocked: Dictionary=k._preview(state,"invented_personal",{},actor)
		check(blocked.get("code")==("SiegePending" if variant=="siege" else "EncounterPending") and k.probe.calls==calls,"非原KINDS即使同owner/gate也必须留在公共门 "+variant)
	for row in [["dead","ActorUnavailable"],["busy","ActorBusy"],["following","NotPartyLeader"],["army","NotArmyCommander"]]:
		var state: Dictionary=before.duplicate(true)
		if row[0]=="dead": state.actors[actor].alive=false
		if row[0]=="busy": state.actors[actor].action_id="guard-action"
		if row[0]=="following": state.actors[actor].location={"kind":"Following"}
		if row[0]=="army": state.actors[actor].location={"kind":"InArmy"}
		var calls: Dictionary=k.probe.calls.duplicate(true)
		check(k._preview(state,"invented_personal",{},actor).get("code")==row[1] and k.probe.calls==calls,"未知kind不能借早门绕过 "+row[0])
	finish()

## 补充证据单独计数，不把已通过的267项重复跑作新增成绩。
func finish() -> void:
	print("REGISTERED_PERSONAL_EARLY_GUARD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
