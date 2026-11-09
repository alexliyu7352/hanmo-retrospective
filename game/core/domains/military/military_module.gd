## 军事人产、战略、会战和围城统一登记；适配层只委托各自唯一真实规则。
extends RefCounted
const Training=preload("res://core/domains/military/training_rules.gd")
const TrainingAction=preload("res://core/domains/military/training_action.gd")
const DRILL_GATE="military_drill_before_actor"
const DRILL_KINDS=["military_drill_draft","military_drill_confirm","military_drill_cancel_pending","military_drill_exit"]
const TRAINING_CANCEL="military_train_cancel"
const TRAINING_CANCEL_GATE="military_training_before_actor"
const Assets=preload("res://core/military_assets.gd")
const SiegeCommands=preload("res://core/domains/military/siege_commands.gd")
const CampaignModule=preload("res://core/domains/military/campaign_module.gd")
const CombatCommands=preload("res://core/domains/military/military_combat_commands.gd")
const Action=preload("res://core/domains/military/military_action.gd")
const Registry=preload("res://core/runtime/module_registry.gd")

## 直接使用领域公开集合生成代码绑定；合入唯一military拥有权描述后才能构建注册表。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	var actions: Array=[]
	for kind in Assets.KINDS:
		var timed: bool=kind in Assets.ACTION_KINDS
		commands.append({"kind":kind,"mode":"action" if timed else "instant","preview":preview,"apply":null if timed else apply,"gate_id":"legacy_kernel"})
	commands.append_array(CampaignModule.descriptor().commands)
	commands.append_array(CombatCommands.descriptor().commands)
	commands.append_array(SiegeCommands.descriptor().commands)
	for kind in Assets.ACTION_KINDS:
		actions.append({"kind":kind,"prepare":Action.prepare,"start":Action.start,"step":Action.step,"finish":Action.finish,"validate":Assets.validate_action,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}})
	commands.append({"kind":Training.KIND,"mode":"action","preview":preview,"apply":null,"gate_id":"legacy_kernel"})
	commands.append({"kind":TRAINING_CANCEL,"mode":"instant","preview":preview,"apply":apply,"gate_id":TRAINING_CANCEL_GATE})
	for kind in DRILL_KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":DRILL_GATE})
	actions.append({"kind":Training.KIND,"prepare":TrainingAction.prepare,"start":TrainingAction.start,"step":TrainingAction.step,"finish":TrainingAction.finish,"validate":TrainingAction.validate,"cancel":TrainingAction.cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"supported","resume":"unsupported"}})
	return {"id":"military","commands":commands,"actions":actions}

## 保留原Kernel的军事能力检查顺序；领域未知命令不转入其他兼容路径。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Assets.KINDS+[Training.KIND,TRAINING_CANCEL]+DRILL_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not "military" in data.required_capabilities: return {"ok":false,"code":"MilitaryUnavailable"}
	if kind in DRILL_KINDS: return Training.preview_drill(state,data,actor_id,kind,params)
	if kind==Training.KIND: return Training.preview(state,data,actor_id,params)
	if kind==TRAINING_CANCEL: return Training.preview_cancel(state,data,actor_id,params)
	return Assets.preview(state,data,actor_id,kind,params)

## 即时命令只接受原私有报价；受任、编组、补给和归营必须走完整耗时生命周期。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind in DRILL_KINDS: return Training.apply_drill(state,data,actor_id,kind,params,offer)
	if kind==TRAINING_CANCEL:
		var checked: Dictionary=Training.preview_cancel(state,data,actor_id,params)
		if not checked.ok: return checked
		if checked!=offer: return {"ok":false,"code":"StaleMilitaryTrainingCancellation"}
		return Training.cancel(state,data,actor_id,checked.action_id)
	if not kind in Assets.KINDS or kind in Assets.ACTION_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Assets.apply(state,data,actor_id,kind,offer)

## 真实人产与战略军令集合锚定完整世界；禁止漏命令、借owner、改模式或改特殊守卫。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	var actions: Dictionary=registry.get("actions",{})
	var expected_kinds: Array=Assets.KINDS+[Training.KIND,TRAINING_CANCEL]+DRILL_KINDS+CampaignModule.command_kinds()+CombatCommands.command_kinds()+SiegeCommands.command_kinds()
	for kind in expected_kinds:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		var expected: String="action" if kind in Assets.ACTION_KINDS+[Training.KIND] else "instant"
		if command.get("owner")!="military" or command.get("mode")!=expected: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		var expected_gate: String=CampaignModule.gate_id(kind) if kind in CampaignModule.command_kinds() else "legacy_kernel"
		if kind in CombatCommands.command_kinds(): expected_gate=CombatCommands.gate_id(kind)
		if kind in SiegeCommands.command_kinds(): expected_gate=SiegeCommands.gate_id(kind)
		if kind==TRAINING_CANCEL: expected_gate=TRAINING_CANCEL_GATE
		if kind in DRILL_KINDS: expected_gate=DRILL_GATE
		if command.get("gate_id")!=expected_gate: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in Assets.ACTION_KINDS+[Training.KIND]:
		if not actions.has(kind) or actions[kind].get("owner")!="military": return {"ok":false,"code":"MissingBuiltinAction","detail":kind}
	for kind in commands:
		if commands[kind].get("owner")=="military" and not kind in expected_kinds: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		if commands[kind].get("gate_id")==DRILL_GATE and (not kind in DRILL_KINDS or commands[kind].get("owner")!="military"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in actions:
		if actions[kind].get("owner")=="military" and not kind in Assets.ACTION_KINDS+[Training.KIND]: return {"ok":false,"code":"InvalidBuiltinAction","detail":kind}
	return {"ok":true}
