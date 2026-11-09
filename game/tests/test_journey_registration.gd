## 五项途中命令以真实旧Kernel作全状态对照，并验证注册回调、首错和候选回滚。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Legacy=preload("res://tests/fixtures/journey_94e1477/world_kernel.gd")
const Module=preload("res://core/domains/travel/journey_module.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Journey=preload("res://core/journey_commands.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
var compared: int=0
var military_path: String=""
var trace: Array=[]

## 探针始终先执行原Callable或真实半日，只在写入后注错以证明完整候选丢弃。
class ProbeKernel extends "res://core/world_kernel.gd":
	var calls: Dictionary={}
	var failure: String=""
	var changed: bool=false
	var advanced: bool=false
	var held: Array=[]
	## 只包装五个本片kind，其他注册或原内联命令不改。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for row in rows:
			if row.id!="travel": continue
			for command in row.commands:
				if command.kind in Journey.KINDS:
					command.preview=observed_preview.bind(command.preview)
					command.apply=observed_apply.bind(command.apply)
		return rows
	## 真实报价保持只读，计数证明实际命中注册入口而非仅描述存在。
	func observed_preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,run: Callable) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return run.call(state,data,actor,kind,params)
	## 暂停或路线已真实改写后才注入失败；tail模式交正式Schema拒绝。
	func observed_apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary,run: Callable) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=run.call(state,data,actor,kind,params,offer)
		changed=state!=before
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedJourneyApply"}
		if result.ok and failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result
	## 记录传入hold并执行整个原半日，返回错误时不得提交tick/RNG/ID与旁人进度。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		held=hold.duplicate()
		var tick: int=state.tick
		var result: Dictionary=super._advance(state,hold)
		advanced=result.ok and state.tick==tick+1
		return {"ok":false,"code":"InjectedJourneyAdvance"} if result.ok and failure=="advance" else result

## 启动故障探针只改变发行描述，不在生产模块加入测试开关。
class MutantKernel extends "res://core/world_kernel.gd":
	var mutation: String=""
	var selected: String=""
	## 通过真实new_game路径损坏一项注册，保留旧世界的拒绝必须发生在正式门中。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for row in rows:
			if row.id!="travel": continue
			if mutation=="foreign_gate": row.commands.append({"kind":"foreign_journey","mode":"instant","gate_id":"journey_before_actor","preview":Module.preview,"apply":Module.apply})
			for command in row.commands.duplicate():
				if command.kind!=selected: continue
				match mutation:
					"missing": row.commands.erase(command)
					"duplicate": row.commands.append(command.duplicate(true))
					"owner": moved=command; row.commands.erase(command)
					"mode": command.mode="action"
					"gate_id": command.gate_id="legacy_kernel"
					"compat_action": command.compat_action="legacy_begin_action"
		if not moved.is_empty():
			for row in rows:
				if row.id=="interaction": row.commands.append(moved)
		return rows

## 等引擎初始化后核对正式注册与原行旅边界。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): military_path=arg.trim_prefix("--military-content=")
	call_deferred("run_checks")

## 任一断言失败保留明确标签和非零退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 同一目录分别创建当前与冻结旧Kernel，领域算法和初始状态保持一致。
func pair(scenario: String="scenario.two_stories",path: String="res://data/generated/two_stories.hmcatalog.json") -> Array:
	var a=ProbeKernel.new(); var b=Legacy.new()
	if not check(a.new_game(scenario,path).ok and b.new_game(scenario,path).ok,"旧新实际开局 "+scenario): return []
	check(a.snapshot()==b.snapshot(),"初始全状态相同")
	return [a,b]

## 两端提交同一公开请求，逐步比较原返回和含RNG/ID/历史/缓存的全快照。
func send(p: Array,kind: String,params: Dictionary={},actor: String="",expected: bool=true) -> Dictionary:
	var state: Dictionary=p[0].snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	var input: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}
	var old: Dictionary=p[1].submit(input); var current: Dictionary=p[0].submit(input)
	compared+=1
	check(current==old and p[0].snapshot()==p[1].snapshot(),"逐命令旧新完全相同："+kind)
	check(current.ok==expected,"命令预期："+kind+" "+str(current))
	trace.append({"input":input,"result":old,"state_sha256":JSON.stringify(p[1].snapshot(),"",true,true).sha256_text()})
	if current.ok and kind in Journey.KINDS:
		var committed: Dictionary=p[0].snapshot(); var calls: Dictionary=p[0].calls.duplicate(true)
		check(p[0].submit(input)==current and p[1].submit(input)==old and p[0].snapshot()==committed and p[1].snapshot()==committed and p[0].calls==calls,"原请求重复不再次入注册/推进/消费："+kind)
	return current

## 直接冻结旧Schema兼容字节，再分别恢复；不另造假的预期字典。
func codec(p: Array) -> void:
	var old: String=Codec.encode(p[1].snapshot(),2,"journey-registration",p[1].content())
	var current: String=Codec.encode(p[0].snapshot(),2,"journey-registration",p[0].content())
	var decoded: Dictionary=Codec.decode(old,p[0].content())
	check(not old.is_empty() and current==old and decoded.ok,"旧新Codec字节一致且旧字节可读")
	if decoded.ok:
		for k in p: check(k.restore(decoded.state).ok and k.snapshot()==decoded.state,"实际旧档恢复全状态")

## 真实两路行者、暂停等待、改道与端点即时释放；不把长世界作为本片门。
func graph_trace() -> void:
	var p: Array=pair()
	if p.is_empty(): return
	var initial: Dictionary=p[0].snapshot(); var hero: String=initial.player_actor_id
	initial.wallets[initial.actors["actor.he"].wallet_id].balance=80
	for k in p: check(k.restore(initial).ok,"独立NPC持有真实可用路费")
	send(p,"move_local",{"place_id":"east.entrance"},"actor.he")
	send(p,"travel",{"destination":"city.west"},"actor.he")
	send(p,"travel",{"destination":"village.east"})
	send(p,"advance")
	var moving: Dictionary=p[0].snapshot()
	send(p,"pause_travel"); codec(p)
	var before: Dictionary=p[0].snapshot()
	send(p,"travel_wait")
	var after: Dictionary=p[0].snapshot()
	check(after.tick==before.tick+1 and after.actors[hero].location==before.actors[hero].location and after.wallets[after.actors[hero].wallet_id]==before.wallets[before.actors[hero].wallet_id],"等待本人不移动不重收费，只推进一个半日")
	check(p[0].held==[hero] and after.actors["actor.he"].location.get("settlement_id")=="city.west","等待精确hold本人，NPC完成另一条行程")
	send(p,"resume_travel")
	send(p,"reroute",{"destination":"city.west"}); codec(p)
	send(p,"reroute",{"destination":"missing"},"",false)
	send(p,"return"); send(p,"advance")
	check(p[0].snapshot().actors[hero].location.get("settlement_id")=="city.west","实走折返抵达原城")
	send(p,"travel",{"destination":"village.east"})
	before=p[0].snapshot(); var action: String=before.actors[hero].action_id
	send(p,"return")
	after=p[0].snapshot()
	check(after.tick==before.tick and after.actors[hero].action_id.is_empty() and not after.actions.has(action) and after.wallets==before.wallets,"零距离折返释放原行动与占用，不退费或推进")
	for kind in Journey.KINDS: check(p[0].calls.get(kind+":preview",0)>0 and p[0].calls.get(kind+":apply",0)>0,"五种真实Callable都已命中："+kind)
	guard_vectors(p,moving)
	gate_isolation(moving)
	rollback(moving)

## 同行的行动参与者、真实费用和原地折返释放仍保留原规则。
func party_trace() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/lived_city.hmcatalog.json")).value
	header.catalog.roles["role.carter"].services.append("join_party")
	header.catalog.roles["role.carter"].companion_cost_per_tick=6
	if not "companions" in header.catalog.required_capabilities: header.catalog.required_capabilities.append("companions")
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://journey-registration-party.json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var p: Array=pair("scenario.lived_city",path)
	if p.is_empty(): return
	send(p,"move_local",{"place_id":"west.tavern"}); send(p,"join_party",{"context":F.context(p[0])}); send(p,"move_local",{"place_id":"west.street"})
	send(p,"travel",{"destination":"village.east"})
	var before: Dictionary=p[0].snapshot(); var actor: String=before.player_actor_id; var id: String=before.actors[actor].action_id
	check("actor.carter" in before.actions[id].participants,"真实同行被原行旅占用")
	send(p,"return")
	var after: Dictionary=p[0].snapshot()
	check(after.actors[actor].action_id.is_empty() and after.actors["actor.carter"].action_id.is_empty() and not after.actions.has(id) and after.parties==before.parties,"端点释放所有同行占用但保留队伍")
	send(p,"travel",{"destination":"village.east"}); send(p,"advance"); send(p,"pause_travel")
	before=p[0].snapshot(); send(p,"travel_wait"); after=p[0].snapshot()
	check(after.wallets[after.actors[actor].wallet_id]==before.wallets[before.actors[actor].wallet_id] and after.actors["actor.carter"].fatigue==before.actors["actor.carter"].fatigue,"同行等待不再收取脚夫费或旅程疲劳")
	send(p,"resume_travel"); send(p,"reroute",{"destination":"city.west"}); codec(p); send(p,"advance")

## 正式原野曲折路线保留剩余point计划、会面守卫与可恢复的控制状态。
func terrain_trace() -> void:
	var p: Array=pair("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if p.is_empty(): return
	var quote: Dictionary=p[0].preview("travel",{"destination":"village.east"})
	check(quote.ok and quote.points.size()>2,"作者地形旅程实际包含多段路径")
	send(p,"travel",{"destination":"village.east"}); send(p,"advance"); clear_meeting(p)
	send(p,"pause_travel"); send(p,"travel_wait"); clear_meeting(p)
	send(p,"resume_travel"); send(p,"reroute",{"destination":"city.west"}); codec(p)
	send(p,"return"); send(p,"advance")

## 会面由真实世界生成，明确道别后才继续控制，不越过EncounterPending。
func clear_meeting(p: Array) -> void:
	var state: Dictionary=p[0].snapshot(); var meeting: Dictionary=Encounters.pending_for(state,state.player_actor_id)
	if not meeting.is_empty(): send(p,"encounter_leave",{"encounter_id":meeting.id})

## 原军政手续形成军团后对照控制短窗，军令来源和编组不能被改道替换。
func military_trace() -> void:
	if not check(not military_path.is_empty(),"必须提供经正式作者管线生成的军事目录"): return
	var p: Array=pair("scenario.alpha_life",military_path)
	if p.is_empty(): return
	send(p,"office_accept",{"office_id":"office.military.a","context":F.context(p[0],"role.military.a")}); send(p,"advance")
	send(p,"campaign_accept",{"template_id":"order.military.a","context":F.context(p[0],"role.military.a")})
	var order: String=p[0].snapshot().campaigns.keys()[0]
	send(p,"army_form",{"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":160,"money":100}); send(p,"advance")
	send(p,"campaign_depart",{"campaign_id":order}); send(p,"advance"); clear_meeting(p)
	var moving: Dictionary=p[0].snapshot(); var actor: String=moving.player_actor_id; var action: String=moving.actors[actor].action_id
	send(p,"pause_travel"); send(p,"travel_wait"); clear_meeting(p); send(p,"resume_travel")
	var before_reroute: Dictionary=p[0].snapshot()
	send(p,"reroute",{"destination":"city.west"}); codec(p)
	var after: Dictionary=p[0].snapshot()
	check(after.actors[actor].action_id==action and after.actions[action].participants==moving.actions[action].participants and after.campaigns==before_reroute.campaigns,"军旅改道保留原行动、参与者、军令")
	for id in moving.armies:
		check(after.armies[id].order_id==moving.armies[id].order_id and after.armies[id].formation_source==moving.armies[id].formation_source,"军令与编组权源未被改道重建")
	send(p,"return"); send(p,"advance")

## 构造相互冲突的前提只调用纯预览，以真实旧Kernel冻结首错次序。
func guard_vectors(p: Array,moving: Dictionary) -> void:
	var actor: String=moving.player_actor_id
	for kind in Journey.KINDS:
		for mode in ["unknown","settled_unavailable","travel_unavailable","busy_unavailable","personal_battle","military_battle","siege","in_army","following","encounter","player_encounter"]:
			var state: Dictionary=moving.duplicate(true); var who: String=actor; var expected: String=""
			match mode:
				"unknown": who="missing"; expected="UnknownActor"
				"settled_unavailable": state.actors[actor].action_id=""; state.actors[actor].available=false; expected="NotTravelling"
				"travel_unavailable": state.actors[actor].available=false; expected="ActorUnavailable"
				"busy_unavailable": state.actions[state.actors[actor].action_id].kind="wait"; state.actors[actor].available=false; expected="NotTravelling"
				"personal_battle": state.battles.probe={"kind":"personal","phase":"planning","units":{actor:{}}}; expected="BattlePending"
				"military_battle": state.battles.probe={"kind":"military","phase":"planning","participant_actor_ids":[actor]}; expected="" if kind=="travel_wait" else "BattlePending"
				"siege": state.armies.probe={"status":"active","member_actor_ids":[actor]}; state.sieges.probe={"phase":"planning","armies":{"probe":0},"withdrawn_army_ids":[]}; expected="" if kind=="travel_wait" else "SiegePending"
				"in_army": state.actors[actor].location={"kind":"InArmy","army_id":"missing"}; state.actors[actor].available=false; expected="NotArmyCommander"
				"following": state.actors[actor].location={"kind":"Following","party_id":"missing"}; state.actors[actor].available=false; expected="NotPartyLeader"
				"encounter": state.encounters.probe={"status":"pending","participants":[actor],"id":"probe"}; state.actors[actor].available=false; expected="EncounterPending"
				"player_encounter": who="actor.he"; state.encounters.probe={"status":"pending","participants":[actor],"id":"probe"}; expected="EncounterPending" if kind=="travel_wait" else ""
			var before: Dictionary=state.duplicate(true); var params: Dictionary={"destination":"city.west"}
			var old: Dictionary=p[1]._preview(state,kind,params,who); var current: Dictionary=p[0]._preview(state,kind,params,who)
			check(old==current and state==before,"首错旧新与只读："+kind+"/"+mode)
			if not expected.is_empty(): check(current.get("code")==expected,"固定首错："+kind+"/"+mode+" "+str(current))

## 即便进程内描述被试验探针损坏，三重白名单也不能退回旧直分派或让外域借门。
func gate_isolation(moving: Dictionary) -> void:
	var k=ProbeKernel.new(); check(k.new_game().ok and k.restore(moving).ok,"隔离门恢复真实途中状态")
	var accepted: Dictionary=k._registry.duplicate(true)
	for kind in Journey.KINDS:
		for mutation in ["missing","owner","gate_id"]:
			k._registry=accepted.duplicate(true)
			if mutation=="missing": k._registry.commands.erase(kind)
			else: k._registry.commands[kind][mutation]="foreign"
			check(k.preview(kind,{"destination":"city.west"}).get("code")=="ActorBusy","错误注册不走旧Journey兜底："+kind+"/"+mutation)
	for owner in ["travel","interaction"]:
		k._registry=accepted.duplicate(true)
		k._registry.commands.foreign={"owner":owner,"gate_id":"journey_before_actor","preview":Module.preview}
		check(k.preview("foreign").get("code")=="ActorBusy","同名门不能使外kind提前通过："+owner)
	k._registry=accepted

## 三类写后失败分别丢弃完整候选，重试同序号只提交一次。
func rollback(moving: Dictionary) -> void:
	for kind in Journey.KINDS:
		for mode in ["apply","tail","advance"]:
			if mode=="advance" and kind!="travel_wait": continue
			var k=ProbeKernel.new(); check(k.new_game().ok and k.restore(moving).ok,"写后回滚恢复真实旅程")
			if kind=="resume_travel": check(F.send(k,"pause_travel").ok,"继续前先真实暂停")
			var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
			var input: Dictionary={"kind":kind,"params":{"destination":"city.west"},"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
			k.failure=mode; var result: Dictionary=k.submit(input)
			check(not result.ok and k.changed and k.snapshot()==before,"真实领域写入后整候选回滚："+kind+"/"+mode)
			if mode=="advance": check(k.advanced and k.held==[actor],"真正推进半日并hold后才注错")
			k.failure=""; var retried: Dictionary=k.submit(input); var after: Dictionary=k.snapshot(); var calls: Dictionary=k.calls.duplicate(true)
			check(retried.ok and k.submit(input)==retried and k.snapshot()==after and k.calls==calls,"同序号重试及成功去重："+kind+"/"+mode)

## 描述损坏必须在真正启动时拒绝并保留旧世界；纯合同不以普通Registry替代完整门。
func registration_contracts() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.ok,"发行注册表可建立")
	for kind in Journey.KINDS:
		var row: Dictionary=registry.commands.get(kind,{})
		check(row.get("owner")=="travel" and row.get("mode")=="instant" and row.get("gate_id")=="journey_before_actor","正式Journey注册："+kind)
		for key in ["missing","owner","mode","gate_id","compat_action"]:
			var altered: Dictionary=registry.duplicate(true)
			if key=="missing": altered.commands.erase(kind)
			else: altered.commands[kind][key]="wrong"
			check(not Builtin.command_contract_status(altered).ok,"缺项或合同变质拒绝："+kind+"/"+key)
	for owner in ["travel","interaction"]:
		var altered: Dictionary=registry.duplicate(true)
		altered.commands.foreign={"owner":owner,"mode":"instant","gate_id":"journey_before_actor"}
		check(not Builtin.command_contract_status(altered).ok,"相同或不同owner借专用门都拒绝："+owner)
	var k=MutantKernel.new(); check(k.new_game().ok,"启动拒绝测试保留一个真实旧世界")
	var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var accepted: Dictionary=k._registry.duplicate(true)
	for kind in Journey.KINDS:
		for mutation in ["missing","duplicate","owner","mode","gate_id","compat_action"]:
			k.selected=kind; k.mutation=mutation
			var result: Dictionary=k.new_game()
			check(not result.ok and k.snapshot()==before and k.content()==catalog and k._registry==accepted,"真实启动失败原子保留："+kind+"/"+mutation)
	k.mutation="foreign_gate"
	check(not k.new_game().ok and k.snapshot()==before and k._registry==accepted,"真实启动拒绝其他kind借Journey门")
	check(Module.preview({}, {},"missing","other",{}).get("code")=="UnsupportedAction" and Module.apply({}, {},"missing","other",{},{}).get("code")=="UnsupportedAction" and Module.after_command({},"missing","other",{},Callable()).get("code")=="UnsupportedAction","适配器未知kind不转入其他领域")

## 短多段轨迹与聚焦合同共同验收，汇总不声称完整世界或发行通过。
func run_checks() -> void:
	registration_contracts(); graph_trace(); party_trace(); terrain_trace(); military_trace()
	var file=FileAccess.open("user://journey-registration-trace.json",FileAccess.WRITE)
	if file: file.store_string(JSON.stringify(trace,"",true,true)+"\n"); file.close()
	print("JOURNEY_REGISTRATION_CHECKS=",checks," COMPARED_COMMANDS=",compared," FAILURES=",failures)
	quit(0 if failures==0 else 1)
