## 三域中央组合仅核对注册交集与早门顺序；不重复完整旧源差分和长期世界成长。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Personal=preload("res://core/personal_combat.gd")
const Journey=preload("res://core/journey_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Parties=preload("res://core/party_rules.gd")
const Movement=preload("res://core/domains/travel/movement_commands.gd")
var checks: int=0
var failures: int=0

## 仅统计真实预览回调，仍调用组合根原规则；不得用假成功掩盖早门漏接。
class ProbeKernel extends "res://core/world_kernel.gd":
	var calls: Dictionary={}
	## 包装三域11种命令，其余军事、同行和其他领域保持正式绑定。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for row in rows:
			for command in row.commands:
				if command.kind in Personal.KINDS+Journey.KINDS+Rest.KINDS:
					command.preview=observed_preview.bind(command.preview)
		return rows
	## 每次调用仍执行原预览，便于独立比较同一状态下的领域首错。
	func observed_preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,run: Callable) -> Dictionary:
		calls[kind]=calls.get(kind,0)+1
		return run.call(state,data,actor,kind,params)

## 完整中央合同与短守卫用例都须到达汇总，失败进程不能静默成功。
func _init() -> void:
	call_deferred("run_checks")

## 记录每项实际断言并保留非零退出码。
func check(ok: bool,label: String) -> bool:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)
	return ok

## 证明两个travel命令片段共存，三域任一命令合同缺损都不能启动。
func registration(registry: Dictionary) -> void:
	check(registry.ok and Builtin.command_contract_status(registry).ok,"中央真实组合合同有效")
	var travel: Array=[]
	for kind in registry.commands:
		if registry.commands[kind].owner=="travel": travel.append(kind)
	travel.sort(); var expected: Array=Parties.KINDS+Journey.KINDS+Movement.KINDS; expected.sort()
	check(travel==expected,"travel精确合并Party、Journey与Movement，不覆盖旧片段")
	for kind in Personal.KINDS+Journey.KINDS+Rest.KINDS:
		for mutation in ["missing","owner","gate_id"]:
			var altered: Dictionary=registry.duplicate(true)
			if mutation=="missing": altered.commands.erase(kind)
			else: altered.commands[kind][mutation]="invalid_central_contract"
			check(not Builtin.command_contract_status(altered).ok,"三域组合保持启动拒绝 "+kind+"/"+mutation)

## 人物不可用时原11种kind先走自身早门；其他命令不可借相同owner或gate绕过人物守卫。
func early_guards(k: ProbeKernel) -> void:
	var initial: Dictionary=k.snapshot(); var actor: String=initial.player_actor_id
	var state: Dictionary=initial.duplicate(true); state.actors[actor].available=false
	var unchanged: Dictionary=state.duplicate(true)
	for kind in Personal.KINDS+Journey.KINDS+Rest.KINDS:
		var before: int=k.calls.get(kind,0)
		var outcome: Dictionary=k._preview(state,kind,{},actor)
		check(k.calls.get(kind,0)==before+1,"已知kind真实走原早门Callable "+kind)
		var command: Dictionary=Registry.build(Builtin.descriptors()).commands[kind]
		check(outcome==command.preview.call(state,k.content().data,actor,kind,{}),"保留原领域首错 "+kind)
		check(state==unchanged,"早门预览只读 "+kind)
	for owner_gate in [["personal","personal_before_world"],["travel","journey_before_actor"],["services","rest_before_actor"]]:
		k._registry.commands.central_foreign={"owner":owner_gate[0],"mode":"instant","gate_id":owner_gate[1],"preview":Callable(),"apply":Callable()}
		check(k._preview(state,"central_foreign",{},actor).get("code")=="ActorUnavailable","同owner/gate未知kind不借早门 "+owner_gate[0])
	k._registry.commands.erase("central_foreign")
	for location in ["InArmy","Following"]:
		state=initial.duplicate(true); state.actors[actor].location={"kind":location}
		for kind in Journey.KINDS+Rest.KINDS:
			var before: int=k.calls.get(kind,0)
			var outcome: Dictionary=k._preview(state,kind,{},actor)
			check(outcome.get("code")==("NotArmyCommander" if location=="InArmy" else "NotPartyLeader") and k.calls.get(kind,0)==before,"共同占用守卫仍先于Journey/Rest "+location+"/"+kind)
	state=initial.duplicate(true)
	state.encounters["central-meeting"]={"id":"central-meeting","status":"pending","participants":[actor]}
	for kind in Journey.KINDS+Rest.KINDS:
		var before: int=k.calls.get(kind,0)
		check(k._preview(state,kind,{},actor).get("code")=="EncounterPending" and k.calls.get(kind,0)==before,"会面早于途中和休整门 "+kind)

## 只用正式alpha_life开局；缺内容或未开局须明确失败而非减少断言冒充通过。
func run_checks() -> void:
	registration(Registry.build(Builtin.descriptors()))
	var k=ProbeKernel.new()
	if check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"中央组合正式开局"):
		early_guards(k)
	print("EARLY_GATE_INTEGRATION_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
