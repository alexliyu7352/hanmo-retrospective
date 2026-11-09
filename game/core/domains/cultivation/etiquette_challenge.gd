## 礼议应答的纯规则叶：作者只配置公开选项与有限前置选择，不执行脚本，也不直接改写世界奖励。
## 外层owner负责真实人物快照、来源授权、唯一实例/seed及一次结算；本叶不冒充跨实例防重放系统。
extends RefCounted
const SharedChallenge=preload("res://core/domains/cultivation/challenge_basis.gd")
const FORMAT="HM-ETIQUETTE-CHALLENGE/1"
const BASE_POINTS=25
const MAX_NODE_POINTS=25
const NODE_COUNT=3

## 严格校验三节点作者定义及真实显示文本，历史条件只能指向前面的已定义选择。
static func validate_definition(definition: Variant, texts: Dictionary) -> Dictionary:
	if not _fields(definition,{"id":TYPE_STRING,"difficulty":TYPE_INT,"nodes":TYPE_ARRAY}) or not _symbol(definition.id,true) or not _whole(definition.difficulty,0,3) or definition.nodes.size()!=NODE_COUNT: return _fail("InvalidEtiquetteDefinition")
	var earlier: Dictionary={}
	for node in definition.nodes:
		if not _fields(node,{"id":TYPE_STRING,"topic_key":TYPE_STRING,"stance_key":TYPE_STRING,"options":TYPE_ARRAY}) or not _symbol(node.id) or earlier.has(node.id) or node.options.size()!=3: return _fail("InvalidEtiquetteNode")
		for key in [node.topic_key,node.stance_key]:
			if not _text(texts,key): return _fail("MissingEtiquetteText")
		var options: Dictionary={}
		for option in node.options:
			if not _fields(option,{"id":TYPE_STRING,"text_key":TYPE_STRING,"points":TYPE_INT,"requires_choices":TYPE_ARRAY}) or not _symbol(option.id) or options.has(option.id) or not _whole(option.points,0,MAX_NODE_POINTS) or not _text(texts,option.text_key): return _fail("InvalidEtiquetteOption")
			var conditions: Dictionary={}
			for condition in option.requires_choices:
				if not _fields(condition,{"node_id":TYPE_STRING,"option_id":TYPE_STRING}) or not earlier.has(condition.node_id) or not earlier[condition.node_id].has(condition.option_id) or conditions.has(condition.node_id): return _fail("InvalidEtiquetteCondition")
				conditions[condition.node_id]=condition.option_id
			options[option.id]=true
		earlier[node.id]=options
	return {"ok":true,"maximum_raw_score":BASE_POINTS+_maximum(definition.nodes,0,{})}

## 自动成绩只依调用方已核的固定人物快照与作者难度；不抽新随机或读取隐藏NPC状态。
static func automatic_score(snapshot: Dictionary, difficulty: int) -> int:
	return SharedChallenge.automatic_score(snapshot,difficulty)

## 创建一个明确的新实例值；唯一创建资格由owner核验，自动模式当场得到正常基线结果。
static func create(definition: Dictionary, texts: Dictionary, binding: Dictionary, actor_snapshot: Dictionary, seed: String, mode: String) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if not _binding(binding) or not _snapshot(actor_snapshot) or not _seed(seed) or not mode in ["manual","automatic"]: return _fail("InvalidEtiquetteStart")
	var state: Dictionary={"format":FORMAT,"definition_id":definition.id,"source_sha256":_source_digest(definition,texts),"binding":binding.duplicate(true),"actor_snapshot":actor_snapshot.duplicate(true),"seed":seed,"mode":mode,"phase":"active","choices":[],"result":{}}
	if mode=="automatic":
		state.phase="completed"
		state.result=_result(definition,state,"automatic")
	return {"ok":true,"state":state}

## 每次只接受当前节点的一条显式选项，节点身份防止重复回复被误当下一轮；原状态保持不变。
static func choose(definition: Dictionary, texts: Dictionary, state: Dictionary, node_id: String, option_id: String) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="active": return _fail("EtiquetteAlreadyCompleted")
	var node: Dictionary=definition.nodes[state.choices.size()]
	if node_id!=node.id: return _fail("StaleEtiquetteNode")
	var option: Dictionary=_option(node,option_id)
	if option.is_empty(): return _fail("UnknownEtiquetteOption")
	var next: Dictionary=state.duplicate(true)
	next.choices.append({"node_id":node_id,"option_id":option_id,"points":_points(option,_history(state.choices))})
	if next.choices.size()==NODE_COUNT:
		next.phase="completed"
		next.result=_result(definition,next,"manual")
	return {"ok":true,"state":next}

## 中途退出统一返回该实例自动成绩；已经完成则仅返回原结果，不能换seed或重算成更有利模式。
static func exit_challenge(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	var next: Dictionary=state.duplicate(true)
	if next.phase=="active":
		next.phase="completed"
		next.result=_result(definition,next,"exited")
	return {"ok":true,"state":next}

## 显示当前公开话题、已知立场和论据，或者已结束结果；不泄露未到节点也不提供奖励入口。
static func view(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase=="completed": return {"ok":true,"phase":"completed","result":state.result.duplicate(true)}
	var node: Dictionary=definition.nodes[state.choices.size()]
	var options: Array=[]
	for option in node.options:
		options.append({"id":option.id,"text":texts[option.text_key]})
	return {"ok":true,"phase":"active","round":state.choices.size()+1,"node_id":node.id,"topic":texts[node.topic_key],"stance":texts[node.stance_key],"options":options}

## 保存值必须能由原作者题面及实际选择重算；更换题面/公开文字、伪造积分或畸形终态均拒绝。
## 外部来源/人物快照/seed真实性仍由owner对自己的冻结值复核，本函数不授予世界调用权限。
static func validate_state(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if not _fields(state,{"format":TYPE_STRING,"definition_id":TYPE_STRING,"source_sha256":TYPE_STRING,"binding":TYPE_DICTIONARY,"actor_snapshot":TYPE_DICTIONARY,"seed":TYPE_STRING,"mode":TYPE_STRING,"phase":TYPE_STRING,"choices":TYPE_ARRAY,"result":TYPE_DICTIONARY}) or state.format!=FORMAT or state.definition_id!=definition.id: return _fail("InvalidEtiquetteState")
	if state.source_sha256!=_source_digest(definition,texts): return _fail("EtiquetteSourceChanged")
	if not _binding(state.binding) or not _snapshot(state.actor_snapshot) or not _seed(state.seed) or not state.mode in ["manual","automatic"] or not state.phase in ["active","completed"] or state.choices.size()>NODE_COUNT: return _fail("InvalidEtiquetteState")
	var history: Dictionary={}
	for index in range(state.choices.size()):
		var choice: Variant=state.choices[index]
		if not _fields(choice,{"node_id":TYPE_STRING,"option_id":TYPE_STRING,"points":TYPE_INT}): return _fail("InvalidEtiquetteChoice")
		var node: Dictionary=definition.nodes[index]
		var option: Dictionary=_option(node,choice.option_id)
		if choice.node_id!=node.id or option.is_empty() or choice.points!=_points(option,history): return _fail("InvalidEtiquetteChoice")
		history[node.id]=choice.option_id
	if state.phase=="active":
		if state.mode!="manual" or state.choices.size()>=NODE_COUNT or not state.result.is_empty(): return _fail("InvalidEtiquetteProgress")
		return {"ok":true}
	var completion: Variant=state.result.get("completion")
	if not completion is String or not completion in ["manual","automatic","exited"]: return _fail("InvalidEtiquetteResult")
	if completion=="automatic" and (state.mode!="automatic" or not state.choices.is_empty()): return _fail("InvalidEtiquetteResult")
	if completion=="manual" and (state.mode!="manual" or state.choices.size()!=NODE_COUNT): return _fail("InvalidEtiquetteResult")
	if completion=="exited" and (state.mode!="manual" or state.choices.size()>=NODE_COUNT): return _fail("InvalidEtiquetteResult")
	if not _fields(state.result,{"completion":TYPE_STRING,"score":TYPE_INT,"raw_score":TYPE_INT if completion=="manual" else TYPE_NIL,"auto_score":TYPE_INT,"performance_bp":TYPE_INT,"request_delta":TYPE_INT,"choices":TYPE_ARRAY}) or state.result!=_result(definition,state,completion): return _fail("InvalidEtiquetteResult")
	return {"ok":true}

## 在已有正常经验上做一次有界整数比例计算，不写人物；调用者不能传入未由本题面验证的倍率。
static func scaled_experience(definition: Dictionary, texts: Dictionary, state: Dictionary, normal_xp: int) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="completed" or not _whole(normal_xp,0,1200): return _fail("InvalidEtiquetteExperience")
	@warning_ignore("integer_division")
	var amount: int=normal_xp*state.result.performance_bp/10000
	return {"ok":true,"normal_xp":normal_xp,"xp":amount,"performance_bp":state.result.performance_bp}

## 一条条件不满足只让此选项得零分，仍允许选择；不会凭文本或隐藏立场臆测价值。
static func _points(option: Dictionary, history: Dictionary) -> int:
	for condition in option.requires_choices:
		if history.get(condition.node_id)!=condition.option_id: return 0
	return option.points

## 最多枚举27条有限路径，仅返回作者诊断，不强迫题面必须达到100分。
static func _maximum(nodes: Array, index: int, history: Dictionary) -> int:
	if index==NODE_COUNT: return 0
	var best: int=0
	var node: Dictionary=nodes[index]
	for option in node.options:
		var next: Dictionary=history.duplicate()
		next[node.id]=option.id
		best=maxi(best,_points(option,history)+_maximum(nodes,index+1,next))
	return best

## 终态分数遵循自动兜底和手动不低于自动减10；礼议影响严格使用原文负数向下取整映射。
static func _result(definition: Dictionary, state: Dictionary, completion: String) -> Dictionary:
	var automatic: int=automatic_score(state.actor_snapshot,definition.difficulty)
	var raw: Variant=null
	var score: int=automatic
	if completion=="manual":
		raw=BASE_POINTS
		for choice in state.choices: raw+=choice.points
		score=maxi(raw,automatic-10)
	var multiplier: int=SharedChallenge.performance_bp(score,automatic)
	return {"completion":completion,"score":score,"raw_score":raw,"auto_score":automatic,"performance_bp":multiplier,"request_delta":clampi(_floor_div(score-60,8),-5,5),"choices":state.choices.duplicate(true)}

## 负数遵循数学floor，避免Godot整数截断把59分的负面调整错误抹成零。
static func _floor_div(value: int, divisor: int) -> int:
	@warning_ignore("integer_division")
	var result: int=value/divisor
	return result-1 if value<0 and value%divisor!=0 else result

## 来源摘要包括实际公开题目文字，防止显示文本变化却沿用旧题面选择记录。
static func _source_digest(definition: Dictionary, texts: Dictionary) -> String:
	var selected: Dictionary={}
	for node in definition.nodes:
		selected[node.topic_key]=texts[node.topic_key]
		selected[node.stance_key]=texts[node.stance_key]
		for option in node.options: selected[option.text_key]=texts[option.text_key]
	return JSON.stringify({"definition":definition,"texts":selected},"",true).sha256_text()

## 原选择映射只来自已校验的逐轮记录，供下一节点的显式一致性条件使用。
static func _history(choices: Array) -> Dictionary:
	var result: Dictionary={}
	for choice in choices: result[choice.node_id]=choice.option_id
	return result

## 仅查当前节点的已声明选项，任意字符串不能转成分数或可执行效果。
static func _option(node: Dictionary, id: String) -> Dictionary:
	for option in node.options:
		if option.id==id: return option
	return {}

## 来源绑定只记录已核actor与owner发出的实例来源，路径字符串不会在此被读取或执行。
static func _binding(value: Dictionary) -> bool:
	return _fields(value,{"actor_id":TYPE_STRING,"source_id":TYPE_STRING}) and _symbol(value.actor_id,true) and not value.source_id.is_empty() and value.source_id.length()<=160 and not "\n" in value.source_id and not "\r" in value.source_id

## 自动成绩输入是调用方冻结的已派生等级和辅助属性，范围之外不做隐式截断或随机修正。
static func _snapshot(value: Dictionary) -> bool:
	return SharedChallenge.snapshot(value)

## 种子保存为规范十进制int64文本，禁止浮点转换丢失身份；本叶从不推进世界随机流。
static func _seed(value: String) -> bool:
	return SharedChallenge.seed(value)

## 显示键必须引用调用方提供的现有文本，不回退拼接字符串或读取外部文件。
static func _text(texts: Dictionary, key: String) -> bool:
	return SharedChallenge.text(texts,key)

## 实体/文本可有一个受限namespace，节点及选项始终局部；尾换行等非ID字符拒绝。
static func _symbol(value: String, qualified: bool=false) -> bool:
	return SharedChallenge.symbol(value,qualified)

## 类型及字段集合必须精确匹配，不接受附加脚本、回调、隐藏条件或自报奖励。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	return SharedChallenge.fields(value,fields)

## 只接受真实整数，拒绝布尔、浮点和越界分数。
static func _whole(value: Variant, minimum: int, maximum: int) -> bool:
	return SharedChallenge.whole(value,minimum,maximum)

## 作者和运行输入错误以稳定代码返回，不把未评分或无效题面当作完成。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
