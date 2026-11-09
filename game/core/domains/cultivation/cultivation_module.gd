## 修业命令注册连接既有武学与普通技艺；旧教学保持兼容，新课程使用完整长行动。
extends RefCounted
const Cultivation=preload("res://core/cultivation_rules.gd")
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")
const StudyAction=preload("res://core/domains/cultivation/skill_study_action.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const STUDY_CANCEL_GATE="study_before_actor"
const ETIQUETTE_GATE="study_etiquette_before_actor"
const CHALLENGE_GATE="study_challenge_before_actor"
const CHALLENGE_KINDS=["study_challenge_choose","study_challenge_exit"]
const ETIQUETTE_KINDS=["study_etiquette_choose","study_etiquette_exit"]

## 原五项命令沿用唯一规则集合，普通技艺增加完整生命周期且不冒充旧武学兼容载荷。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Cultivation.KINDS:
		var training: bool=kind in Cultivation.TRAINING_KINDS
		var command: Dictionary={"kind":kind,"mode":"action" if training else "instant","preview":preview,"apply":null if training else apply,"gate_id":"legacy_kernel"}
		if training: command.compat_action="legacy_begin_action"
		commands.append(command)
	commands.append({"kind":Study.KIND,"mode":"action","preview":preview,"apply":null,"gate_id":"legacy_kernel"})
	for kind in [Study.CANCEL_KIND]+Study.CONTROL_KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":STUDY_CANCEL_GATE})
	for kind in ETIQUETTE_KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":ETIQUETTE_GATE})
	for kind in CHALLENGE_KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":CHALLENGE_GATE})
	var action: Dictionary={"kind":Study.KIND,"prepare":StudyAction.prepare,"start":StudyAction.start,"step":StudyAction.step,"finish":StudyAction.finish,"validate":StudyAction.validate,"cancel":StudyAction.cancel,"resume":StudyAction.resume,"interrupt_policy":{"cancel":"supported","resume":"supported"}}
	return {"id":"cultivation","commands":commands,"actions":[action]}

## 能力守卫留在公共人物可用/忙碌检查之后，原领域继续决定报价与参数错误优先级。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Cultivation.KINDS and not kind in [Study.KIND,Study.CANCEL_KIND]+Study.CONTROL_KINDS+ETIQUETTE_KINDS+CHALLENGE_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not "cultivation" in data.required_capabilities: return {"ok":false,"code":"CultivationUnavailable"}
	if kind in CHALLENGE_KINDS: return Study.preview_challenge(state,data,actor_id,kind,params)
	if kind in ETIQUETTE_KINDS: return Study.preview_etiquette(state,data,actor_id,kind,params)
	if kind in Study.CONTROL_KINDS: return Study.preview_control(state,data,actor_id,kind,params)
	if kind==Study.KIND: return Study.preview(state,data,actor_id,params)
	if kind==Study.CANCEL_KIND: return Study.preview_cancel(state,data,actor_id,params)
	return Cultivation.preview(state,data,actor_id,kind,params)

## 即时装备与心法只使用私有报价重验权属；训练必须走明确的旧开工桥而非假即时结算。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind in CHALLENGE_KINDS: return Study.apply_challenge(state,data,actor_id,kind,params,offer)
	if kind in ETIQUETTE_KINDS: return Study.apply_etiquette(state,data,actor_id,kind,params,offer)
	if kind in Study.CONTROL_KINDS: return Study.apply_control(state,data,actor_id,kind,params,offer)
	if kind==Study.CANCEL_KIND:
		var checked: Dictionary=Study.preview_cancel(state,data,actor_id,params)
		if not checked.ok: return checked
		if checked!=offer: return {"ok":false,"code":"StaleStudyQuote"}
		return StudyAction.cancel(state,data,state.actions[checked.action_id],"requested")
	if not kind in Cultivation.KINDS or kind in Cultivation.TRAINING_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Cultivation.apply_instant(state,data,actor_id,kind,offer)

## 完整世界独立锚定源集合、拥有者、模式与守卫，旧训练载荷不能冒充完整action注册。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	var legacy: Dictionary=registry.get("legacy_action_validators",{})
	var actions: Dictionary=registry.get("actions",{})
	for kind in Cultivation.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		var training: bool=kind in Cultivation.TRAINING_KINDS
		if command.get("owner")!="cultivation" or command.get("mode")!=("action" if training else "instant") or command.get("gate_id")!="legacy_kernel": return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if (training and command.get("compat_action")!="legacy_begin_action") or (not training and command.has("compat_action")): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if training and (not legacy.has(kind) or legacy[kind].get("owner")!="cultivation" or actions.has(kind)): return {"ok":false,"code":"InvalidBuiltinAction","detail":kind}
	for kind in commands:
		if commands[kind].get("owner")=="cultivation" and not kind in Cultivation.KINDS and not kind in [Study.KIND,Study.CANCEL_KIND]+Study.CONTROL_KINDS+ETIQUETTE_KINDS+CHALLENGE_KINDS: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in ETIQUETTE_KINDS:
		var entry: Dictionary=commands.get(kind,{})
		if entry.get("owner")!="cultivation" or entry.get("mode")!="instant" or entry.get("gate_id")!=ETIQUETTE_GATE or entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		if commands[kind].get("gate_id")==ETIQUETTE_GATE and (not kind in ETIQUETTE_KINDS or commands[kind].get("owner")!="cultivation"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in CHALLENGE_KINDS:
		var entry: Dictionary=commands.get(kind,{})
		if entry.get("owner")!="cultivation" or entry.get("mode")!="instant" or entry.get("gate_id")!=CHALLENGE_GATE or entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		if commands[kind].get("gate_id")==CHALLENGE_GATE and (not kind in CHALLENGE_KINDS or commands[kind].get("owner")!="cultivation"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in actions:
		if actions[kind].get("owner")=="cultivation" and kind!=Study.KIND: return {"ok":false,"code":"InvalidBuiltinAction","detail":kind}
	if not commands.has(Study.KIND): return {"ok":false,"code":"MissingBuiltinCommand","detail":Study.KIND}
	var study: Dictionary=commands[Study.KIND]
	if study.get("owner")!="cultivation" or study.get("mode")!="action" or study.get("gate_id")!="legacy_kernel" or study.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":Study.KIND}
	if not actions.has(Study.KIND) or actions[Study.KIND].get("owner")!="cultivation" or legacy.has(Study.KIND): return {"ok":false,"code":"InvalidBuiltinAction","detail":Study.KIND}
	if not commands.has(Study.CANCEL_KIND): return {"ok":false,"code":"MissingBuiltinCommand","detail":Study.CANCEL_KIND}
	var cancellation: Dictionary=commands[Study.CANCEL_KIND]
	if cancellation.get("owner")!="cultivation" or cancellation.get("mode")!="instant" or cancellation.get("gate_id")!=STUDY_CANCEL_GATE or cancellation.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":Study.CANCEL_KIND}
	for kind in Study.CONTROL_KINDS:
		var control: Dictionary=commands.get(kind,{})
		if control.get("owner")!="cultivation" or control.get("mode")!="instant" or control.get("gate_id")!=STUDY_CANCEL_GATE or control.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		if commands[kind].get("gate_id")==STUDY_CANCEL_GATE and (not kind in [Study.CANCEL_KIND]+Study.CONTROL_KINDS or commands[kind].get("owner")!="cultivation"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
