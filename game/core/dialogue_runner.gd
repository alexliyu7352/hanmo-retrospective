## 解释已保存会话中的五类作者节点；资源变化仍由领域规则执行。
extends RefCounted
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Conditions=preload("res://core/condition_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Identity=preload("res://core/actor_identity.gd")

## 开启会话前解析当前岗位人员及其实际对白定义。
static func can_begin(state: Dictionary, data: Dictionary, actor_id: String, role_id: Variant) -> Dictionary:
	if not role_id is String or not data.roles.has(role_id): return {"ok":false,"code":"UnknownRole"}
	var ctx: Dictionary=Interaction.for_role(state,data,role_id)
	var valid: Dictionary=Interaction.validate(state,data,actor_id,ctx)
	if not valid.ok: return valid
	var dialogue_id: Variant=data.roles[role_id].get("dialogue_id",Identity.definition(state,data,ctx.provider_actor_id).get("dialogue_id"))
	if dialogue_id==null or not data.dialogues.has(dialogue_id) or data.dialogues[dialogue_id].context_role!=role_id: return {"ok":false,"code":"NoDialogueForRole"}
	return {"ok":true,"context":ctx,"dialogue_id":dialogue_id,"duration":0}

## 建立真实交互；见到人物本身不发放任务或金钱。
static func begin(state: Dictionary, data: Dictionary, actor_id: String, definition: Dictionary) -> void:
	var dialogue: Dictionary=data.dialogues[definition.dialogue_id]
	state.interactions[actor_id]={"id":Factory.next_id(state,"interaction"),"dialogue_id":dialogue.id,"context":definition.context.duplicate(true),"node_id":dialogue.entry,"phase":"stepping","pending_command":{},"pending_action_id":"","last_outcome":{},"trade_completed":false}

## 按稳定ID读取节点，不另外维护一份可修改对白图。
static func node(data: Dictionary, session: Dictionary) -> Dictionary:
	for record in data.dialogues[session.dialogue_id].nodes:
		if record.id==session.node_id: return record
	return {}

## 推进自动分支到真正输入边界，并限制无输出的循环步数。
static func pump(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var session: Dictionary=state.interactions[actor_id]
	if session.phase in ["waiting","trade"]: return {"ok":true}
	var present: Dictionary=Interaction.validate(state,data,actor_id,session.context)
	if not present.ok: return present
	for _step in range(64):
		var current: Dictionary=node(data,session)
		if current.is_empty(): return {"ok":false,"code":"DialogueNodeMissing"}
		match current.kind:
			"branch":
				session.node_id=current.if_true if Conditions.evaluate(state,actor_id,current.condition) else current.if_false
			"say":
				var speaker: String=current.speaker
				if speaker=="$provider": speaker=session.context.provider_actor_id
				elif speaker=="$player": speaker=actor_id
				if not Interaction.speaker_present(state,session.context,speaker): return {"ok":false,"code":"SpeakerAbsent"}
				session.phase="say"
				return {"ok":true}
			"choice":
				session.phase="choice"
				return {"ok":true}
			"action":
				session.phase="action"
				session.pending_command=current.command.duplicate(true)
				return {"ok":true}
			"end":
				session.phase="end"
				return {"ok":true}
			_: return {"ok":false,"code":"UnsupportedDialogueNode"}
	return {"ok":false,"code":"DialogueStepLimit"}

## 根据当前条件校验继续或选择，不相信界面缓存的旧选项。
static func validate_input(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not state.interactions.has(actor_id): return {"ok":false,"code":"NoInteraction"}
	var session: Dictionary=state.interactions[actor_id]
	var presence: Dictionary=Interaction.validate(state,data,actor_id,session.context)
	if not presence.ok: return presence
	var current: Dictionary=node(data,session)
	if kind=="dialogue_next" and session.phase=="say": return {"ok":true,"next":current.next,"duration":0}
	if kind=="dialogue_choose" and session.phase=="choice" and params.get("option_id") is String:
		for option in current.options:
			if option.id==params.option_id and Conditions.all_true(state,actor_id,option.conditions): return {"ok":true,"next":option.next,"duration":0}
	return {"ok":false,"code":"InvalidDialogueInput"}

## 核心校验通过后才切换到明确的下一节点。
static func advance(state: Dictionary, actor_id: String, target: String) -> void:
	var session: Dictionary=state.interactions[actor_id]
	session.node_id=target
	session.phase="stepping"
	session.pending_command={}

## 从已保存动作节点继续，将真实结果送入成功或失败分支。
static func resolve(state: Dictionary, data: Dictionary, actor_id: String, outcome: Dictionary) -> void:
	var session: Dictionary=state.interactions[actor_id]
	var current: Dictionary=node(data,session)
	session.node_id=current.on_success if outcome.get("success",outcome.get("ok",false)) else current.on_failure
	session.phase="stepping"
	session.pending_action_id=""
	session.pending_command={}
	session.last_outcome=outcome.duplicate(true)

## 仅提供当前表现所需的隔离副本，查询不改变会话进度。
static func view(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	if not state.interactions.has(actor_id): return {"phase":"scene"}
	var session: Dictionary=state.interactions[actor_id]
	var current: Dictionary=node(data,session)
	var output: Dictionary={"phase":session.phase,"context":session.context.duplicate(true),"node_id":session.node_id,"last_outcome":session.last_outcome.duplicate(true)}
	if session.phase=="say":
		var speaker: String=current.speaker
		output.speaker=session.context.provider_actor_id if speaker=="$provider" else (actor_id if speaker=="$player" else speaker)
		output.text=data.texts[current.text_key]
	elif session.phase=="choice":
		output.text=data.texts[current.text_key]
		output.options=[]
		for option in current.options:
			if Conditions.all_true(state,actor_id,option.conditions): output.options.append({"id":option.id,"text":data.texts[option.text_key]})
	elif session.phase in ["action","trade","waiting"]:
		output.command=session.pending_command.duplicate(true)
		output.pending_action_id=session.pending_action_id
	return output
