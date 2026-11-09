## 路遇注册用真实旧Kernel、完整状态及Codec对照，并覆盖原守卫次序、重放和候选失败回滚。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const LegacyKernel=preload("res://tests/fixtures/encounter_e6ba398/world_kernel.gd")
const LegacyBuiltin=preload("res://tests/fixtures/encounter_e6ba398/builtin_modules.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Module=preload("res://core/domains/interaction/encounter_module.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0

## 测试探针只记录并委托真实领域；失败在原apply真正修改候选之后注入。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	var changed: bool=false
	## 保持固定preview ABI和唯一真实报价，不重实现会面前提。
	func preview(state: Dictionary, data: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return Module.preview(state,data,actor,kind,params)
	## 真实规则写入后再制造失败，以证明外层丢弃的是已经发生变化的候选。
	func apply(state: Dictionary, data: Dictionary, actor: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=Module.apply(state,data,actor,kind,params,offer)
		changed=before!=state
		if failure=="tail": state.actors[actor].fatigue=-1
		return {"ok":false,"code":"InjectedEncounterApply"} if failure=="apply" else result

## 只替换路遇两项Callable；保留正式组合的observer、validator和所有其他命令。
class Observed extends Kernel:
	var probe=Probe.new()
	var mutation: String=""
	var target: String="encounter_talk"
	var dialogue_calls: int=0
	var dialogue_changed: bool=false
	## 从真实描述注入可证伪缺陷，正式new_game必须拒绝坏合同且保留旧世界。
	func _module_descriptors() -> Array:
		var rows: Array=Builtin.descriptors()
		var borrowed: Dictionary={}
		for row in rows:
			if row.id!="interaction": continue
			for index in range(row.commands.size()-1,-1,-1):
				var command: Dictionary=row.commands[index]
				if command.kind in Encounters.KINDS: command.preview=probe.preview; command.apply=probe.apply
				if command.kind!=target: continue
				match mutation:
					"missing": row.commands.remove_at(index)
					"duplicate": row.commands.append(command.duplicate(true))
					"gate": command.gate_id="legacy_kernel"
					"mode": command.mode="action"
					"owner": borrowed=row.commands.pop_at(index)
					"borrowed_kind","borrowed_owner":
						borrowed=command.duplicate(true); borrowed.kind="borrowed_encounter_gate"
						if mutation=="borrowed_kind": row.commands.append(borrowed); borrowed={}
		if not borrowed.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(borrowed)
		return rows
	## 调用原解释器而非复制节点逻辑；成功推进后的失败同样不得泄漏会话或ID。
	func _drive_dialogue(state: Dictionary, actor: String) -> Dictionary:
		dialogue_calls+=1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=super._drive_dialogue(state,actor)
		dialogue_changed=state!=before
		return {"ok":false,"code":"InjectedEncounterDialogue"} if probe.failure=="dialogue" else result

## 等资源初始化结束后执行真实目录与存档测试。
func _init() -> void:
	call_deferred("run_checks")

## 每个断言失败都会计入退出状态，不能以打印成功掩盖故障。
func check(ok: bool, label: String) -> bool:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)
	return ok

## 使用既有道路旅人夹具生成同一个内容目录，独立旧Kernel不读取新路遇注册器。
func fresh() -> Dictionary:
	var source=F.fresh(true)
	if not check(not source.snapshot().is_empty(),"既有道路内容正式加载"): return {}
	var current=Observed.new(); var legacy=LegacyKernel.new()
	var path: String="user://test-companions-true-role.carter.json"
	if not check(current.new_game("scenario.lived_city",path).ok and legacy.new_game("scenario.lived_city",path).ok,"新旧Kernel从同一目录真实开局"): return {}
	check(current.snapshot()==legacy.snapshot(),"完整初始世界相等")
	return {"current":current,"legacy":legacy}

## 每步比较正式preview/apply/submit、完整状态、重复回执和真实Codec字节，不忽略RNG或ID。
func compare(pair: Dictionary, kind: String, params: Dictionary={}, actor: String="") -> Dictionary:
	var current=pair.current; var legacy=pair.legacy
	var state: Dictionary=current.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	var quote: Dictionary=current.preview(kind,params,actor)
	var old_quote: Dictionary=legacy.preview(kind,params,actor)
	check(quote==old_quote and current.snapshot()==state and legacy.snapshot()==state,"纯预览旧新一致 "+kind)
	if quote.ok:
		var candidate: Dictionary=state.duplicate(true); var old_candidate: Dictionary=state.duplicate(true)
		var applied: Dictionary=current._apply(candidate,kind,params,actor,quote)
		var old_applied: Dictionary=legacy._apply(old_candidate,kind,params,actor,old_quote)
		check(applied==old_applied and candidate==old_candidate,"真实旧Kernel完整候选相等 "+kind)
	var command: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}
	var result: Dictionary=current.submit(command); var old_result: Dictionary=legacy.submit(command)
	var after: Dictionary=current.snapshot()
	check(result==old_result and after==legacy.snapshot(),"正式提交返回与完整世界相等 "+kind+" "+str(result))
	if not result.ok:
		check(after==state,"拒绝不改变整个世界 "+kind)
		return result
	var apply_calls: int=current.probe.calls.get(kind+":apply",0)
	check(current.submit(command)==result and legacy.submit(command)==old_result and current.snapshot()==after and legacy.snapshot()==after and current.probe.calls.get(kind+":apply",0)==apply_calls,"重复请求不再次执行或推进对白 "+kind)
	var encoded: String=Codec.encode(after,2,"registered-encounter",current.content())
	var old_encoded: String=Codec.encode(legacy.snapshot(),2,"registered-encounter",legacy.content())
	var decoded: Dictionary=Codec.decode(encoded,current.content())
	check(not encoded.is_empty() and encoded==old_encoded and decoded.ok and decoded.state==after,"完整Codec字节及解码状态相等 "+kind)
	if decoded.ok: check(current.restore(decoded.state).ok and legacy.restore(decoded.state).ok and current.snapshot()==after and legacy.snapshot()==after,"保存恢复不重演结算或对白 "+kind)
	return result

## 真实出发并推进到道路接触点，而非手工安装已成功的会面状态。
func arrive(pair: Dictionary) -> Dictionary:
	if pair.is_empty(): return {}
	if not check(compare(pair,"travel",{"destination":"village.east"}).ok,"正式出发"): return {}
	for _step in range(2):
		if not check(compare(pair,"advance").ok,"正式半日推进"): return {}
	var state: Dictionary=pair.current.snapshot()
	var encounter: Dictionary=Encounters.pending_for(state,state.player_actor_id)
	check(not encounter.is_empty(),"真实双方旅行产生会面")
	return encounter

## 冻结源码摘要和唯一重定向一并验证，旧期望不得悄悄跟随本轮实现。
func oracle_integrity() -> void:
	var root: String="res://tests/fixtures/encounter_e6ba398/"
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(root+"provenance.json"))
	if not check(parsed.ok,"冻结来源清单有效"): return
	check(parsed.value.commit=="e6ba39894c6fd24f76bf23c8b3a23dc50dbd6009","原基线提交明确")
	for filename in parsed.value.sources:
		var entry: Dictionary=parsed.value.sources[filename]
		var source: String=FileAccess.get_file_as_string(root+filename)
		check(source.sha256_text()==entry.fixture_sha256,"冻结文件摘要 "+filename)
		if filename=="world_kernel.gd": source=source.replace(root+"builtin_modules.gd","res://core/runtime/builtin_modules.gd")
		check(source.sha256_text()==entry.original_sha256,"唯一重定向还原后等于原文件 "+filename)

## 合同缺失、重复、错owner/模式/门位与陌生kind借门必须阻断真实启动。
func contracts() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	var old_registry: Dictionary=Registry.build(LegacyBuiltin.descriptors())
	if not check(registry.ok and old_registry.ok,"新旧真实组合均可构建"): return
	check(Builtin.command_contract_status(registry).ok and Builtin.observer_contract_status(registry).ok,"完整命令和事件观察合同保留")
	check(registry.validators==old_registry.validators and registry.observers==old_registry.observers and registry.modules.interaction.owns==old_registry.modules.interaction.owns,"interaction原验证器、观察器与拥有权未被替换")
	var kinds: Array=[]
	for command in Module.descriptor().commands: kinds.append(command.kind)
	check(kinds==Encounters.KINDS and Module.descriptor().actions.is_empty(),"原KINDS逐项即时注册无新增行动")
	for kind in Encounters.KINDS:
		check(registry.commands.has(kind) and registry.commands[kind].owner=="interaction" and registry.commands[kind].mode=="instant" and registry.commands[kind].gate_id=="encounter_before_pending","生产Registry具备唯一真实路遇命令 "+kind)
	check(Module.preview({}, {}, "", "unknown",{}).get("code")=="UnsupportedAction" and Module.apply({}, {}, "", "unknown",{},{}).get("code")=="UnsupportedAction","未知kind在领域解引用前拒绝")
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	var k=pair.current; var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var original_registry: Dictionary=k._registry
	for kind in Encounters.KINDS:
		k.target=kind
		for mutation in ["missing","duplicate","gate","mode","owner","borrowed_kind","borrowed_owner"]:
			k.mutation=mutation
			var result: Dictionary=k.new_game()
			var expected: String="MissingBuiltinCommand" if mutation=="missing" else ("DuplicateCommand" if mutation=="duplicate" else ("UnknownCommandAction" if mutation=="mode" else "InvalidBuiltinCommand"))
			check(result.get("code")==expected,"启动拒绝坏合同 "+kind+" "+mutation+" "+str(result))
			check(k.snapshot()==before and k.content()==catalog and k._registry==original_registry,"失败保留旧世界、目录和注册表 "+kind+" "+mutation)

## 相同被扰动快照交给真实旧新Kernel，显式冻结同时成立条件的第一错误。
func guard(pair: Dictionary, label: String, state: Dictionary, kind: String, params: Dictionary, expected: String, actor: String="") -> void:
	if actor.is_empty(): actor=state.player_actor_id
	var before: Dictionary=state.duplicate(true)
	var old_input: Dictionary=state.duplicate(true)
	var result: Dictionary=pair.current._preview(state,kind,params,actor)
	var old_result: Dictionary=pair.legacy._preview(old_input,kind,params,actor)
	check(result==old_result and result.get("code")==expected,"旧新首错相等 "+kind+" "+label+" "+str(result))
	check(state==before and old_input==before,"守卫纯读 "+kind+" "+label)

## 路遇门保留Battle/Siege/军团/同行优先；领域首错又先于EncounterPending和人物忙碌。
func guard_order(pair: Dictionary, encounter: Dictionary) -> void:
	var base: Dictionary=pair.current.snapshot(); var actor: String=base.player_actor_id
	var params: Dictionary={"encounter_id":encounter.id}
	for kind in Encounters.KINDS:
		guard(pair,"未知人物",base.duplicate(true),kind,{},"UnknownActor","missing")
		guard(pair,"会面不存在先于忙碌",base.duplicate(true),kind,{},"UnknownEncounter")
		var state: Dictionary=base.duplicate(true); state.actors[actor].available=false
		guard(pair,"领域参与者先于ActorUnavailable",state,kind,params,"NotEncounterParticipant")
		state=base.duplicate(true); state.actors[actor].alive=false
		guard(pair,"死亡参与者",state,kind,params,"NotEncounterParticipant")
		guard(pair,"非参与者",base.duplicate(true),kind,params,"NotEncounterParticipant","actor.carter")
		state=base.duplicate(true); state.encounters[encounter.id].status="resolved"; state.actors[actor].available=false
		guard(pair,"已结束先于参与者",state,kind,params,"EncounterResolved")
		state=base.duplicate(true); state.actors[actor].action_id="different_action"
		guard(pair,"锚点行动失配",state,kind,params,"EncounterMoved")
		state=base.duplicate(true); state.actors[actor].location={"kind":"InArmy","army_id":"missing"}; state.actors[actor].available=false
		guard(pair,"军团门先于会面",state,kind,{},"NotArmyCommander")
		state=base.duplicate(true); state.actors[actor].location={"kind":"Following","party_id":"missing"}; state.actors[actor].available=false
		guard(pair,"同行门先于会面",state,kind,{},"NotPartyLeader")
		state=base.duplicate(true); state.battles.probe={"kind":"personal","phase":"planning","units":{actor:{}}}
		guard(pair,"个人战先于会面",state,kind,{},"BattlePending")
		state=base.duplicate(true); state.battles.probe={"kind":"military","phase":"result","participant_actor_ids":[actor]}
		guard(pair,"军事结果占用先于会面",state,kind,{},"BattlePending")
		state=base.duplicate(true); state.armies.probe={"status":"active","member_actor_ids":[actor]}; state.sieges.probe={"phase":"planning","armies":{"probe":0},"withdrawn_army_ids":[]}
		guard(pair,"围城先于会面",state,kind,{},"SiegePending")
	guard(pair,"待会面阻断世界",base.duplicate(true),"advance",{},"EncounterPending")
	guard(pair,"待会面阻断普通交谈",base.duplicate(true),"talk",{},"EncounterPending")
	guard(pair,"待会面阻断同行",base.duplicate(true),"join_party",{},"EncounterPending")

## 真实交谈、继续、结束、再谈、道别及返程与旧源码完整相等，知识资产和占用无扩权。
func successful_paths() -> void:
	var pair: Dictionary=fresh(); var encounter: Dictionary=arrive(pair)
	if encounter.is_empty(): return
	guard_order(pair,encounter)
	var k=pair.current; var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	var params: Dictionary={"encounter_id":encounter.id}
	var hits: int=k.probe.calls.get("encounter_talk:preview",0)
	for _repeat in range(10): check(k.preview("encounter_talk",params).ok and k.snapshot()==state,"注册预览纯读且不消费RNG")
	check(k.probe.calls.get("encounter_talk:preview",0)==hits+10,"early preview确实调用注册Callable")
	var offer: Dictionary=Encounters.preview(state,k.content().data,actor,"encounter_talk",params)
	var candidate: Dictionary=state.duplicate(true); var old_candidate: Dictionary=state.duplicate(true)
	var original: Dictionary=Encounters.apply(old_candidate,k.content().data,actor,"encounter_talk",offer)
	check(Module.apply(candidate,k.content().data,actor,"encounter_talk",{"encounter_id":"wrong"},offer)==original and candidate==old_candidate,"适配器只用正式私有报价而非替换参数")
	check(compare(pair,"encounter_talk",params).ok,"交谈成功")
	var after: Dictionary=k.snapshot()
	check(k.interaction_view().phase=="say" and k.dialogue_calls>0,"注册apply后仍调用原对白解释器到首句")
	check(after.actions==state.actions and after.wallets==state.wallets and after.inventories==state.inventories and after.knowledge==state.knowledge and after.tick==state.tick,"交谈不解锁旅行、不更改知识物资或时钟")
	check(compare(pair,"encounter_talk",params).get("code")=="AlreadyTalking","同一会面不能叠加对白")
	check(compare(pair,"open_trade",{"shop_id":"shop.grain","context":k.interaction_view().context}).get("code")=="EncounterPending","道路会面不解锁机构服务")
	check(compare(pair,"dialogue_next").ok and k.interaction_view().phase=="scene","原对白结束不自动释放行程")
	check(not Encounters.pending_for(k.snapshot(),actor).is_empty(),"对白结束后仍需正式道别")
	check(compare(pair,"encounter_talk",params).ok,"已结束会话可重新开始原对白")
	check(compare(pair,"end_interaction").ok,"已有结束交互仍保留")
	check(compare(pair,"encounter_talk",params).ok,"再次交谈后可直接道别")
	var dialogue_hits: int=k.dialogue_calls
	check(compare(pair,"encounter_leave",params).ok and k.dialogue_calls==dialogue_hits,"道别不驱动对白并恢复原旅行")
	check(k.probe.calls.get("encounter_talk:apply",0)>0 and k.probe.calls.get("encounter_leave:apply",0)>0,"两命令实际命中注册apply")
	check(compare(pair,"encounter_leave",params).get("code")=="EncounterResolved","新序号重复道别拒绝")
	check(compare(pair,"advance").ok and k.snapshot().actors[actor].location.get("settlement_id")=="village.east","保存恢复后完成原行程且不重复收费")

## 玩家先合法停步再由真实旅人抵近；道别必须恢复停步而非擅自恢复前进。
func paused_path() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	if not check(compare(pair,"travel",{"destination":"village.east"}).ok and compare(pair,"advance").ok,"停步夹具先真实进入道路"): return
	if not check(compare(pair,"pause_travel").ok and compare(pair,"travel_wait").ok,"停步等待真实旅人抵近"): return
	var state: Dictionary=pair.current.snapshot(); var actor: String=state.player_actor_id
	var encounter: Dictionary=Encounters.pending_for(state,actor)
	if not check(not encounter.is_empty() and encounter.resume_paused[actor],"真实会面记住原暂停状态"): return
	var params: Dictionary={"encounter_id":encounter.id}
	check(compare(pair,"encounter_talk",params).ok and compare(pair,"encounter_leave",params).ok,"已保存暂停来源的会面可交谈道别")
	var after: Dictionary=pair.current.snapshot()
	check(after.actions[after.actors[actor].action_id].params.paused and after.actors[actor].location==state.actors[actor].location,"道别维持原停步及道路位置")

## 篡改运行测试注册表也不能让错owner、错gate或陌生kind从前置边界绕过人物守卫。
func gate_isolation() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	var k=pair.current; var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	state.actors[actor].available=false
	check(k._preview(state,"encounter_talk",{},actor).get("code")=="UnknownEncounter","正确门保留领域首错")
	var original: Dictionary=k._registry.commands.encounter_talk.duplicate(true)
	var hits: int=k.probe.calls.get("encounter_talk:preview",0)
	for field in ["owner","gate_id"]:
		k._registry.commands.encounter_talk=original.duplicate(true)
		k._registry.commands.encounter_talk[field]="tasks" if field=="owner" else "legacy_kernel"
		check(k._preview(state,"encounter_talk",{},actor).get("code")=="ActorUnavailable" and k.probe.calls.get("encounter_talk:preview",0)==hits,"篡改"+field+"不得借早门")
	var borrowed: Dictionary=original.duplicate(true); borrowed.kind="borrowed_encounter_gate"
	k._registry.commands.borrowed_encounter_gate=borrowed
	check(k._preview(state,"borrowed_encounter_gate",{},actor).get("code")=="ActorUnavailable","同owner陌生kind不得借早门")

## apply、真实对白推进和Schema尾门分别失败时，连同RNG、序号、会面和行程一起回滚。
func rollback() -> void:
	var pair: Dictionary=fresh(); var encounter: Dictionary=arrive(pair)
	if encounter.is_empty(): return
	var k=pair.current; var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	var params: Dictionary={"encounter_id":encounter.id}
	for kind in Encounters.KINDS:
		for failure in (["apply","dialogue","tail"] if kind=="encounter_talk" else ["apply","tail"]):
			k.probe.failure=failure
			var dialogue_hits: int=k.dialogue_calls
			var result: Dictionary=F.send(k,kind,params)
			check(not result.ok and k.probe.changed,"原领域实际变更后故障 "+kind+" "+failure+" "+str(result))
			check(k.snapshot()==before,"完整候选回滚 "+kind+" "+failure)
			if failure=="apply" or kind=="encounter_leave": check(k.dialogue_calls==dialogue_hits,"失败apply或道别不额外推进对白")
			if failure=="dialogue": check(k.dialogue_changed and result.get("code")=="InjectedEncounterDialogue","真实对白已推进后仍回滚")
	k.probe.failure=""
	var saved_catalog: Dictionary=k._catalog.duplicate(true)
	k._catalog.data.dialogues["dialog.walker"].entry="missing_node"
	var failed_dialogue: Dictionary=F.send(k,"encounter_talk",params)
	check(failed_dialogue.get("code")=="DialogueNodeMissing" and k.probe.changed and k.snapshot()==before,"真实解释器失败同样不泄漏交谈标记和会话")
	k._catalog=saved_catalog
	check(F.send(k,"encounter_talk",params).ok and F.send(k,"encounter_leave",params).ok,"失败后仍可从原世界完成会面")

## 输出限定路遇注册和相邻边界的证据；不冒充全量或新增内容验收。
func run_checks() -> void:
	oracle_integrity(); contracts(); successful_paths(); paused_path(); gate_isolation(); rollback()
	print("REGISTERED_ENCOUNTER_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
