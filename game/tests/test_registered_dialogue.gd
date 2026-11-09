## 九项旧交互协议的短全状态对照；只使用原作者内容和既有路遇夹具，不生成新对白。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Commands=preload("res://core/domains/interaction/dialogue_commands.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var legacy_path: String=""
var legacy=null
var kernels: Array=[]

## 探针仅计数并委托唯一适配器；故障发生在真实候选改变后，不替代规则。
class Probe extends RefCounted:
	var hits: Dictionary={}
	var fail_kind: String=""
	var tail_kind: String=""
	var changed: bool=false
	## 保留Kernel注入的唯一递归预览，证明confirm没有绕过公共守卫。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,recursive: Callable=Callable()) -> Dictionary:
		hits[kind+":preview"]=hits.get(kind+":preview",0)+1
		return Commands.preview(state,data,actor,kind,params,recursive)
	## 原适配器写入之后才注入失败或尾部不变量破坏。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary,drive: Callable=Callable(),recursive: Callable=Callable()) -> Dictionary:
		hits[kind+":apply"]=hits.get(kind+":apply",0)+1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer,drive,recursive)
		changed=state!=before
		if result.ok and kind==fail_kind: return {"ok":false,"code":"InjectedDialogueApply"}
		if result.ok and kind==tail_kind: state.wallets["wallet.player"].balance=-1
		return result

## 覆盖生产描述并保留真实事务、世界推进与对白驱动的窄故障入口。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var missing_kind: String=""
	var fail_drive: bool=false
	var fail_nested: String=""
	var drive_changed: bool=false
	## 不增删命令，仅替换受信函数计数；所有元数据及observer仍来自生产组合根。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for row in rows:
			if row.id!="interaction": continue
			for entry in row.commands.duplicate():
				if entry.kind==missing_kind: row.commands.erase(entry); continue
				if entry.kind in Commands.KINDS: entry.preview=probe.preview; entry.apply=probe.apply
		return rows
	## 原对白泵完成后才拒绝，检验自动发现、继续点和事件shown一起回滚。
	func _drive_dialogue(state: Dictionary,actor: String) -> Dictionary:
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=super._drive_dialogue(state,actor)
		drive_changed=state!=before
		return {"ok":false,"code":"InjectedDialogueDrive"} if fail_drive and result.ok else result
	## 递归内层真实开工后失败，外层confirm不得保存占用、预留或半写会话。
	func _apply(state: Dictionary,kind: String,params: Dictionary,actor: String,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._apply(state,kind,params,actor,offer)
		return {"ok":false,"code":"InjectedDialogueNested"} if kind==fail_nested and result.ok else result

## 精确旧源码由运行命令从已核提交导出，缺失时明确不计旧源对照。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每条断言都有可定位标签，脚本失败不能伪称完整通过。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 基础场景使用原two_stories完整世界，不预填钱物或任务来源。
func fresh():
	var k=TestKernel.new(); kernels.append(k)
	check(k.new_game().ok,"原作者内容开局")
	return k

## 所有提交使用当前版本和真实下一序号。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 旧核仅移除本次新增的九项注册，以恢复基线实际内联分派；脚本字节原样保留。
func sync_legacy(k) -> void:
	legacy._catalog=k.content()
	for kind in Commands.KINDS: legacy._registry.commands.erase(kind)
	check(legacy.restore(k.snapshot()).ok,"旧源码恢复完整当前状态")

## 逐条比较完整preview/result/state与Codec字节，重放请求不会重复命中注册回调。
func send(k,kind: String,params: Dictionary={},expected: String="") -> bool:
	var before: Dictionary=k.snapshot(); var command: Dictionary=request(k,kind,params)
	if legacy!=null:
		sync_legacy(k)
		check(legacy.preview(kind,params)==k.preview(kind,params),"旧preview逐字段相同 "+kind)
	var old_result: Dictionary=legacy.submit(command) if legacy!=null else {}
	var prior: Dictionary=k.probe.hits.duplicate(true); var result: Dictionary=k.submit(command)
	check(k.snapshot()==before if not result.ok else true,"拒绝提交完整回滚 "+kind)
	if not check(result.ok if expected.is_empty() else result.get("code")==expected,"命令结果 "+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot(); var hits: Dictionary=k.probe.hits.duplicate(true)
	if kind in Commands.KINDS and result.ok:
		check(hits.get(kind+":preview",0)>prior.get(kind+":preview",0) and hits.get(kind+":apply",0)>prior.get(kind+":apply",0),"命中注册preview/apply "+kind)
	if result.ok: check(k.submit(command)==result and k.snapshot()==after and k.probe.hits==hits,"重放完整幂等 "+kind)
	if legacy!=null:
		check(old_result==result and legacy.snapshot()==after,"旧源码完整state/result相同 "+kind)
		check(Codec.encode(legacy.snapshot(),0,"dialogue-parity",legacy.content())==Codec.encode(after,0,"dialogue-parity",k.content()),"旧源码完整Codec字节相同 "+kind)
		comparisons+=1
	return true

## 每个稳定输入边界保存/恢复不重演发现、收款或会话转移。
func roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot(); var encoded: String=Codec.encode(before,0,"dialogue-roundtrip",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(not encoded.is_empty() and decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"完整Codec恢复 "+label)

## 每项缺失、错owner/gate/mode都拒绝；未知适配器kind不能访问空世界或调用回调。
func contracts() -> void:
	var built: Dictionary=Registry.build(Builtin.descriptors())
	if not check(built.ok and Builtin.command_contract_status(built).ok,"正式组合根九项注册通过"): return
	for kind in Commands.KINDS:
		check(built.commands.has(kind) and built.commands[kind].owner=="interaction","拥有权与九项覆盖 "+kind)
		for mutation in ["missing","owner","gate","mode","compat"]:
			var changed: Dictionary=built.duplicate(true)
			match mutation:
				"missing": changed.commands.erase(kind)
				"owner": changed.commands[kind].owner="tasks"
				"gate": changed.commands[kind].gate_id="encounter_before_pending"
				"mode": changed.commands[kind].mode="action"
				"compat": changed.commands[kind].compat_action="legacy_begin_action"
			check(not Builtin.command_contract_status(changed).ok,"完整合同拒绝 "+kind+" "+mutation)
	check(built.observers.has("interaction.events_reconcile"),"保留唯一Events observer")
	check(Commands.preview({}, {}, "missing", "*", {}).get("code")=="UnsupportedAction" and Commands.apply({}, {}, "missing", "*", {}, {}).get("code")=="UnsupportedAction","未知kind无catchall及世界访问")
	var k=fresh(); var before: Dictionary=k.snapshot()
	check(Commands.apply(before,k.content().data,before.player_actor_id,"talk",{},{}).get("code")=="DialogueCoordinationUnavailable" and before==k.snapshot(),"缺窄回调在修改之前拒绝")
	var content: Dictionary=k.content(); k.missing_kind="dialogue_confirm"
	check(k.new_game().get("code")=="MissingBuiltinCommand" and k.snapshot()==before and k.content()==content,"重开局缺必要注册时保留原世界和内容")
	k.missing_kind=""

## 失败注入包围真实适配器、泵和Schema；每项失败都保留所有世界字段。
func rollback(k,kind: String,params: Dictionary={},drive: bool=true) -> void:
	var before: Dictionary=k.snapshot()
	for fault in ["apply","tail"]:
		if fault=="apply": k.probe.fail_kind=kind
		else: k.probe.tail_kind=kind
		var result: Dictionary=k.submit(request(k,kind,params))
		check(not result.ok and k.snapshot()==before,"真实变更后整候选回滚 "+kind+" "+fault)
		k.probe.fail_kind=""; k.probe.tail_kind=""
	if drive:
		k.fail_drive=true
		check(k.submit(request(k,kind,params)).get("code")=="InjectedDialogueDrive" and k.snapshot()==before,"原对白泵后失败整候选回滚 "+kind)
		k.fail_drive=false

## 公共守卫和缺目标首错保留，状态变体只用于原preview比较，不冒充合法存档。
func guards(k) -> void:
	if legacy==null: return
	sync_legacy(k)
	for kind in Commands.KINDS:
		for mode in ["base","dead","busy","following","army"]:
			var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
			if mode=="dead": state.actors[actor].alive=false
			if mode=="busy": state.actors[actor].action_id="missing"
			if mode=="following": state.actors[actor].location={"kind":"Following","leader_actor_id":"actor.chen"}
			if mode=="army": state.actors[actor].location={"kind":"InArmy","army_id":"missing"}
			var before: Dictionary=state.duplicate(true)
			check(k._preview(state,kind,{},actor)==legacy._preview(state,kind,{},actor) and state==before,"公共守卫首错与纯读 "+kind+" "+mode)

## 真实官署自动发现、选择取消、确认开工、半日完成和事件shown均复用原规则。
func conversations() -> void:
	var k=fresh(); guards(k)
	for row in [["talk",{"role_id":"missing"},"UnknownRole"],["open_event",{"event_id":"missing"},"UnknownEvent"],["open_trade",{"shop_id":"missing"},"UnknownShop"],["dialogue_next",{},"NoInteraction"],["dialogue_choose",{"option_id":"missing"},"NoInteraction"],["dialogue_confirm",{},"NoPendingDialogueAction"],["dialogue_cancel",{},"NoPendingDialogueAction"],["close_trade",{},"NoPendingDialogueAction"],["unknown_dialogue",{},"UnsupportedAction"]]: send(k,row[0],row[1],row[2])
	send(k,"end_interaction"); send(k,"move_local",{"place_id":"west.office"})
	rollback(k,"talk",{"role_id":"role.office"}); send(k,"talk",{"role_id":"role.office"}); roundtrip(k,"say")
	rollback(k,"dialogue_next"); send(k,"dialogue_next"); roundtrip(k,"choice")
	send(k,"dialogue_choose",{"option_id":"missing"},"InvalidDialogueInput")
	rollback(k,"dialogue_choose",{"option_id":"ask"}); send(k,"dialogue_choose",{"option_id":"ask"})
	check(k.snapshot().knowledge[k.snapshot().player_actor_id].has("offer.grain"),"原_drive_dialogue自动发现成功")
	send(k,"dialogue_next"); send(k,"dialogue_choose",{"option_id":"accept"}); roundtrip(k,"action")
	rollback(k,"dialogue_cancel"); send(k,"dialogue_cancel"); check(k.interaction_view().last_outcome=={"ok":false,"code":"Cancelled"},"action取消保留原结果")
	send(k,"dialogue_next"); send(k,"talk",{"role_id":"role.office"}); send(k,"dialogue_next"); send(k,"dialogue_choose",{"option_id":"accept"})
	var before: Dictionary=k.snapshot(); k.fail_nested="accept_offer"
	check(k.submit(request(k,"dialogue_confirm")).get("code")=="InjectedDialogueNested" and k.snapshot()==before,"confirm递归实际开工后失败整体回滚")
	k.fail_nested=""; rollback(k,"dialogue_confirm",{},false); send(k,"dialogue_confirm")
	check(k.interaction_view().phase=="waiting" and k.snapshot().actions.size()==1,"confirm保留旧waiting/行动转移")
	roundtrip(k,"waiting"); send(k,"advance"); check(k.interaction_view().phase=="say","完工按原_drive_dialogue恢复")
	rollback(k,"end_interaction",{},false); send(k,"end_interaction"); send(k,"move_local",{"place_id":"west.street"}); send(k,"move_local",{"place_id":"west.tavern"})
	rollback(k,"open_event",{"event_id":"event.letter"}); send(k,"open_event",{"event_id":"event.letter"}); roundtrip(k,"event-shown")
	check(k.snapshot().events["actor.player|event.letter"].status=="shown","事件展示后才标shown")
	send(k,"end_interaction"); send(k,"open_event",{"event_id":"event.letter"},"EventNotHere")

## 原店主与当前context实际开商店；取消、未成交关闭、已成交关闭各走原分支。
func open_shop(k) -> void:
	send(k,"talk",{"role_id":"role.merchant"}); send(k,"dialogue_next"); send(k,"dialogue_choose",{"option_id":"goods"})

## 同一原作者发现节点也可合法恢复为待确认点；递归instant结果仍沿原resolve分支继续。
func instant_confirm() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.office"}); send(k,"talk",{"role_id":"role.office"})
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id; var session: Dictionary=state.interactions[actor]
	for node in k.content().data.dialogues[session.dialogue_id].nodes:
		if node.id=="discover": session.node_id=node.id; session.phase="action"; session.pending_command=node.command.duplicate(true)
	check(k.restore(state).ok,"原作者instant行动节点通过正式Schema恢复")
	var unavailable: Dictionary=state.duplicate(true); unavailable.actors[session.context.provider_actor_id].available=false
	check(k.restore(unavailable).ok,"人物离场前提仍是合法会话存档")
	send(k,"dialogue_confirm",{},"PersonAbsent"); check(k.restore(state).ok,"恢复原真实在场人")
	rollback(k,"dialogue_confirm"); send(k,"dialogue_confirm",{"kind":"unknown","context":{"provider_actor_id":"forged"}})
	check(k.interaction_view().node_id=="terms" and k.snapshot().actions.is_empty(),"confirm只取保存请求/context并恢复instant成功分支")
	roundtrip(k,"instant-confirmed")

## 商店阶段和实际交易仅用原作者报价；确认本身不交易、不改钱物。
func trades() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.grain"})
	var context: Dictionary=Interaction.for_role(k.snapshot(),k.content().data,"role.merchant")
	var params: Dictionary={"shop_id":"shop.grain","context":context}
	var data: Dictionary=k.content().data; var state: Dictionary=k.snapshot(); var before: Dictionary=state.duplicate(true)
	data.shops["shop.grain"].representative_roles=["role.clerk"]
	check(Commands.preview(state,data,state.player_actor_id,"open_trade",params).get("code")=="WrongShopRepresentative" and state==before,"在场且trade授权仍必须是本店代表")
	data=k.content().data; data.shops["shop.grain"].place_id="west.office"
	check(Commands.preview(state,data,state.player_actor_id,"open_trade",params).get("code")=="WrongShopRepresentative" and state==before,"岗位与商店地点同时匹配才准入")
	rollback(k,"open_trade",params,false); send(k,"open_trade",params)
	check(k.interaction_view().phase=="scene","直接open_trade仍不创建会话")
	var invalid: Dictionary=params.duplicate(true); invalid.context.provider_actor_id="missing"; send(k,"open_trade",invalid,"UnknownInteractionTarget")
	open_shop(k); rollback(k,"close_trade"); send(k,"close_trade"); check(k.interaction_view().node_id=="unavailable","action阶段close_trade沿未成交取消")
	send(k,"dialogue_next")
	for ending in ["dialogue_cancel","close_trade","completed"]:
		open_shop(k); rollback(k,"dialogue_confirm",{},false); send(k,"dialogue_confirm"); roundtrip(k,"trade")
		check(k.interaction_view().phase=="trade","确认只进入trade阶段")
		if ending=="completed":
			send(k,"trade",{"shop_id":"shop.grain","item_id":"item.grain","quantity":1,"side":"buy","payer_wallet_id":"wallet.player","context":context})
			send(k,"advance"); check(k.snapshot().interactions[k.snapshot().player_actor_id].trade_completed,"原交易完成标记由旧领域设置")
		var command: String="close_trade" if ending=="completed" else ending
		rollback(k,command); send(k,command)
		check(k.interaction_view().node_id==("done" if ending=="completed" else "unavailable"),"关闭/取消保留成功失败分支 "+ending)
		roundtrip(k,ending); send(k,"dialogue_next")

## 道路会面保留真实旅行占用；三项例外仅匹配同一pending encounter会话。
func encounters() -> void:
	var road=F.fresh(true); var k=fresh(); k._catalog=road.content(); check(k.restore(road.snapshot()).ok,"既有真实路遇夹具恢复")
	send(k,"travel",{"destination":"village.east"}); send(k,"advance"); send(k,"advance")
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id; var encounter: Dictionary=Encounters.pending_for(state,actor)
	if not check(not encounter.is_empty(),"真实行旅接触生成pending会面"): return
	send(k,"encounter_talk",{"encounter_id":encounter.id}); roundtrip(k,"road-say")
	for kind in ["talk","open_event","dialogue_confirm","dialogue_cancel","close_trade","open_trade"]: send(k,kind,{},"EncounterPending")
	send(k,"dialogue_choose",{"option_id":"missing"},"InvalidDialogueInput")
	if legacy!=null:
		sync_legacy(k); state=k.snapshot(); state.interactions[actor].context.encounter_id="other"
		for kind in ["dialogue_next","dialogue_choose","end_interaction"]: check(k._preview(state,kind,{},actor)==legacy._preview(state,kind,{},actor) and k._preview(state,kind,{},actor).get("code")=="ActorBusy","错误会面不能借ActorBusy例外 "+kind)
	rollback(k,"end_interaction",{},false); send(k,"end_interaction"); check(not k.snapshot().actors[actor].action_id.is_empty(),"结束交谈不释放真实旅行")
	send(k,"encounter_talk",{"encounter_id":encounter.id}); rollback(k,"dialogue_next"); send(k,"dialogue_next"); send(k,"encounter_leave",{"encounter_id":encounter.id}); send(k,"advance")

## 只执行短命令链与故障边界，不启动长世界或Linux全量。
func run_checks() -> void:
	if not legacy_path.is_empty():
		var script=load(legacy_path); legacy=script.new()
		if not check(legacy.new_game().ok,"精确旧Kernel开局"): finish(); return
	contracts(); conversations(); instant_confirm(); trades(); encounters(); finish()

## 明确报告旧源码对照条数；释放测试闭包避免把测试引用循环当作运行泄漏。
func finish() -> void:
	for k in kernels: k._registry={}
	kernels.clear(); legacy=null
	print("REGISTERED_DIALOGUE_LEGACY_COMPARISONS=",comparisons)
	print("REGISTERED_DIALOGUE_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
