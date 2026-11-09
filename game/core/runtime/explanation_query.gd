## 基于已存见闻与本人命令的纯解释投影；不补知识、不执行命令、不遍历未知世界。
extends RefCounted
const Conditions=preload("res://core/condition_rules.gd")
const QUERY_FIELDS={"fact":"fact_id","command":"request_seq","result":"result_id","offer":"offer_id"}

## 只接受定向fact/command/result/offer查询；额外字段不能扩大观察者、范围或权限。
static func explain(state: Dictionary, data: Dictionary, query: Dictionary, observer_id: String="") -> Dictionary:
	var answer: Dictionary={"revision":state.get("revision",0),"known_causes":[],"unknown_parts":[],"next_options":[]}
	if state.is_empty(): return _reject(answer,"NoWorld")
	if observer_id.is_empty(): observer_id=state.get("player_actor_id","")
	if not state.get("actors",{}).has(observer_id): return _reject(answer,"UnknownObserver")
	if not query.get("kind") is String: return _reject(answer,"InvalidExplanationQuery")
	if not QUERY_FIELDS.has(query.kind): return _reject(answer,"UnsupportedExplanationQuery")
	var field: String=QUERY_FIELDS[query.kind]
	if query.size()!=2 or not query.has(field): return _reject(answer,"InvalidExplanationQuery")
	if query.kind=="command":
		if not query[field] is int or query[field]<1: return _reject(answer,"InvalidExplanationQuery")
	elif not query[field] is String or query[field].is_empty(): return _reject(answer,"InvalidExplanationQuery")
	var evidence: Dictionary={}
	match query.kind:
		"fact": evidence=_fact_evidence(state,query.fact_id,observer_id)
		"command": evidence=_command_evidence(state,observer_id,query.request_seq)
		"result": evidence=_result_evidence(state,query.result_id,observer_id)
		"offer": evidence=_offer_evidence(state,data,query.offer_id,observer_id)
	if evidence.is_empty():
		answer.unknown_parts.append({"code":"NotKnown"})
	else:
		answer.known_causes.append(evidence)
		answer.unknown_parts.append({"code":"FurtherCausesUnknown"})
		if evidence.kind=="fact" and evidence.knowledge=="reported": answer.unknown_parts.append({"code":"NotFirstHand"})
		if evidence.kind=="command": answer.unknown_parts.append({"code":"CommandDetailsNotExposed"})
		if query.kind=="command" and evidence.get("command",{}).has("result_id"):
			answer.next_options.append({"kind":"query","query":{"kind":"result","result_id":evidence.command.result_id}})
		elif query.kind=="result" and evidence.kind=="fact":
			answer.next_options.append({"kind":"query","query":{"kind":"fact","fact_id":evidence.fact_id}})
	return answer.duplicate(true)

## 错误同样保留四个查询字段；不在诊断中回显未知军令、道路或观察者内容。
static func _reject(answer: Dictionary, code: String) -> Dictionary:
	answer["ok"]=false
	answer["code"]=code
	return answer

## 从原社会事件或领域事实取一条已知记录；任何未存记忆、坏来源或未知类型均封闭返回。
static func _fact_evidence(state: Dictionary, fact_id: String, observer_id: String) -> Dictionary:
	var social: Dictionary=state.get("social",{})
	var fact_value: Variant=social.get("events",{}).get(fact_id,state.get("facts",{}).get(fact_id,{}))
	if not fact_value is Dictionary: return {}
	var fact: Dictionary=fact_value
	var participants: Array=_participants(fact)
	if participants.is_empty(): return {}
	var memory: Dictionary=_visible_memory(state,fact_id,observer_id,fact,participants)
	if memory.is_empty(): return {}
	var witnessed: bool=memory.source_actor_id==observer_id
	var projection: Dictionary=_fact_projection(fact,witnessed)
	if projection.is_empty(): return {}
	return {"kind":"fact","fact_id":fact_id,"knowledge":"witnessed" if witnessed else "reported","learned_tick":memory.learned_tick,"source_actor_id":memory.source_actor_id,"fact":projection}

## 沿现存首次来源逐段核对转述与较早记忆；只验证已有权限，不补写亲历或传播记录。
static func _visible_memory(state: Dictionary, fact_id: String, observer_id: String, fact: Dictionary, participants: Array) -> Dictionary:
	var social: Dictionary=state.get("social",{})
	var memories: Dictionary=social.get("memories",{})
	var events: Dictionary=social.get("events",{})
	var fact_tick: Variant=fact.get("completed_tick",fact.get("tick"))
	if not fact_tick is int or fact_tick<0 or fact_tick>state.get("tick",0): return {}
	var current: String=observer_id
	var visited: Dictionary={}
	var original: Dictionary={}
	var later_tick: int=state.get("tick",0)+1
	while not visited.has(current):
		visited[current]=true
		var actor_memories: Variant=memories.get(current,{})
		if not actor_memories is Dictionary: return {}
		var memory: Variant=actor_memories.get(fact_id)
		if not memory is Dictionary or not memory.get("learned_tick") is int or not memory.get("source_actor_id") is String: return {}
		if memory.learned_tick<fact_tick or memory.learned_tick>=later_tick or not state.actors.has(memory.source_actor_id): return {}
		if original.is_empty(): original=memory
		if memory.source_actor_id==current: return original if current in participants else {}
		var transmitted: bool=false
		for event in events.values():
			if event is Dictionary and event.get("kind")=="news" and event.get("fact_id")==fact_id and event.get("actor_id")==memory.source_actor_id and event.get("target_actor_id")==current and event.get("tick")==memory.learned_tick:
				transmitted=true
				break
		if not transmitted: return {}
		later_tick=memory.learned_tick
		current=memory.source_actor_id
	return {}

## 只识别现有social允许传播的三类事实；军政指令和未完成行动不获得隐含公开性。
static func _participants(fact: Dictionary) -> Array:
	match fact.get("kind",""):
		"aid","theft","refusal":
			if fact.get("actor_id") is String and fact.get("target_actor_id") is String: return [fact.actor_id,fact.target_actor_id]
		"training":
			if fact.get("phase")!="completed" or not fact.get("training") is Dictionary: return []
			var training: Dictionary=fact.training
			if not training.get("student_id") is String or not training.get("teacher_id") is String: return []
			var people: Array=[training.student_id]
			if not training.teacher_id.is_empty(): people.append(training.teacher_id)
			return people
		"personal_battle":
			if fact.get("participants") is Array:
				for participant in fact.participants:
					if not participant is String: return []
				return fact.participants
	return []

## 每类事实只输出可知摘要；context/source、财务条款、计划、路径与未知新增字段永不透传。
static func _fact_projection(fact: Dictionary, witnessed: bool) -> Dictionary:
	var projected: Dictionary=_select(fact,{"kind":TYPE_STRING,"tick":TYPE_INT})
	match fact.kind:
		"aid","theft","refusal":
			projected.merge(_select(fact,{"place_id":TYPE_STRING,"actor_id":TYPE_STRING,"target_actor_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT}))
			if witnessed: projected.merge(_select(fact,{"reason":TYPE_STRING}))
		"training":
			projected.merge(_select(fact,{"phase":TYPE_STRING,"completed_tick":TYPE_INT}))
			projected["training"]=_select(fact.training,{"student_id":TYPE_STRING,"teacher_id":TYPE_STRING,"skill_id":TYPE_STRING})
			if witnessed: projected.merge(_select(fact,{"success":TYPE_BOOL}))
		"personal_battle":
			if witnessed:
				projected.merge(_select(fact,{"battle_id":TYPE_STRING,"result_id":TYPE_STRING,"outcome":TYPE_STRING,"participants":TYPE_ARRAY,"withdrawn":TYPE_ARRAY}))
	return projected

## 精确键限定本人请求；回执details可能包含全世界阶段输出，因此从不整包返回。
static func _command_evidence(state: Dictionary, observer_id: String, request_seq: int) -> Dictionary:
	var receipt: Variant=state.get("results",{}).get(observer_id+":"+str(request_seq))
	if not receipt is Dictionary or not receipt.get("kind") is String or not receipt.get("result_id") is String: return {}
	var command: Dictionary=_select(receipt,{"kind":TYPE_STRING,"tick":TYPE_INT,"revision":TYPE_INT,"result_id":TYPE_STRING})
	command["actor_id"]=observer_id
	command["request_seq"]=request_seq
	return {"kind":"command","command":command}

## 结果只沿本人缓存、本人历史或已知事实中的可见result_id查找；不遍历全局战斗和军令。
static func _result_evidence(state: Dictionary, result_id: String, observer_id: String) -> Dictionary:
	var prefix: String=observer_id+":"
	for key in state.get("results",{}):
		if not key is String or not key.begins_with(prefix): continue
		var suffix: String=key.substr(prefix.length())
		if not suffix.is_valid_int() or suffix.to_int()<1 or str(suffix.to_int())!=suffix: continue
		var evidence: Dictionary=_command_evidence(state,observer_id,suffix.to_int())
		if evidence.get("command",{}).get("result_id")==result_id: return evidence
	for row in state.get("history",[]):
		if row is Dictionary and row.get("actor_id")==observer_id and row.get("result_id")==result_id:
			return {"kind":"command","command":_select(row,{"kind":TYPE_STRING,"tick":TYPE_INT,"actor_id":TYPE_STRING,"result_id":TYPE_STRING})}
	var ids: Array=state.get("social",{}).get("memories",{}).get(observer_id,{}).keys()
	ids.sort()
	for fact_id in ids:
		var evidence: Dictionary=_fact_evidence(state,fact_id,observer_id)
		if evidence.get("fact",{}).get("result_id")==result_id: return evidence
	return {}

## 只读取本人已存委托知识，使用原条件规则解释当前状态；不从作者目录发现新任务或道路。
static func _offer_evidence(state: Dictionary, data: Dictionary, offer_id: String, observer_id: String) -> Dictionary:
	if not data.get("offers",{}).has(offer_id): return {}
	var knowledge: Variant=state.get("knowledge",{}).get(observer_id,{}).get(offer_id)
	if not knowledge is Dictionary or not knowledge.get("tick") is int or not knowledge.get("source") is Dictionary: return {}
	if knowledge.tick<0 or knowledge.tick>state.get("tick",0): return {}
	var source: Dictionary=_select(knowledge.source,{"kind":TYPE_STRING,"id":TYPE_STRING})
	if source.size()!=2: return {}
	return {"kind":"offer","offer_id":offer_id,"knowledge":{"tick":knowledge.tick,"source":source},"status":Conditions.task_status(state,observer_id,offer_id)}

## 按固定字段及原始类型选取值；最外层统一深复制，未知附加字段不会成为隐式API。
static func _select(record: Dictionary, fields: Dictionary) -> Dictionary:
	var selected: Dictionary={}
	for field in fields:
		if record.has(field) and typeof(record[field])==fields[field]: selected[field]=record[field]
	return selected
