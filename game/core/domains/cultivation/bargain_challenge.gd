## 议价筹算的独立三轮规则：公开作者条件只决定分数与折扣候选，不读取全局供需或执行钱货。
## 真报价ID、完整报价摘要和一次消费由经济owner负责；课程owner仍是唯一经验结算者。
extends RefCounted
const SharedChallenge=preload("res://core/domains/cultivation/challenge_basis.gd")
const FORMAT="HM-BARGAIN-CHALLENGE/1"
const METHODS={"quantity":"small_quantity","batch":"combined_purchase","payment":"cash"}
const ROUND_COUNT=3

## 题面只允许三轮三提议；供需和已知条件均引用实际公开文字及显式布尔，不能附加脚本或信用交易。
static func validate_definition(definition: Variant, texts: Dictionary) -> Dictionary:
	if not SharedChallenge.fields(definition,{"id":TYPE_STRING,"difficulty":TYPE_INT,"conditions":TYPE_ARRAY,"rounds":TYPE_ARRAY}) or not SharedChallenge.symbol(definition.id,true) or not SharedChallenge.whole(definition.difficulty,0,3) or definition.rounds.size()!=ROUND_COUNT or definition.conditions.size()>18: return _fail("InvalidBargainDefinition")
	var conditions: Dictionary={}
	for condition in definition.conditions:
		if not SharedChallenge.fields(condition,{"id":TYPE_STRING,"kind":TYPE_STRING,"text_key":TYPE_STRING,"matched":TYPE_BOOL}) or not SharedChallenge.symbol(condition.id) or conditions.has(condition.id) or not condition.kind in ["supply","terms"] or not SharedChallenge.text(texts,condition.text_key): return _fail("InvalidBargainCondition")
		conditions[condition.id]=condition
	var seen: Array=[]
	var maximum: int=20
	for round_row in definition.rounds:
		if not SharedChallenge.fields(round_row,{"id":TYPE_STRING,"prompt_key":TYPE_STRING,"options":TYPE_ARRAY}) or not SharedChallenge.symbol(round_row.id) or round_row.id in seen or not SharedChallenge.text(texts,round_row.prompt_key) or round_row.options.size()!=3: return _fail("InvalidBargainRound")
		seen.append(round_row.id)
		var ids: Array=[]; var kinds: Array=[]; var best: int=0
		for option in round_row.options:
			if not option is Dictionary or option.size()!=6 or not SharedChallenge.fields(_option_fields(option),{"id":TYPE_STRING,"kind":TYPE_STRING,"method":TYPE_STRING,"text_key":TYPE_STRING}) or not SharedChallenge.symbol(option.id) or option.id in ids or not option.kind in METHODS or option.kind in kinds or option.method!=METHODS[option.kind] or not SharedChallenge.text(texts,option.text_key): return _fail("InvalidBargainOption")
			if not option.has("supply_condition_id") or not option.has("terms_condition_id") or not _condition_reference(conditions,option.supply_condition_id,"supply") or not _condition_reference(conditions,option.terms_condition_id,"terms"): return _fail("InvalidBargainConditionReference")
			ids.append(option.id); kinds.append(option.kind)
			best=maxi(best,_points(option,conditions))
		maximum+=best
	return {"ok":true,"maximum_raw_score":mini(100,maximum)}

## 创建固定题面与owner输入的值；本叶不生成seed或quote_id，自动模式立即保正常基线。
static func create(definition: Dictionary, texts: Dictionary, binding: Dictionary, actor_snapshot: Dictionary, context: Dictionary, seed: String, mode: String) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if not _binding(binding) or not SharedChallenge.snapshot(actor_snapshot) or not _context(context) or not SharedChallenge.seed(seed) or not mode in ["manual","automatic"]: return _fail("InvalidBargainStart")
	if context.quote!=null and context.quote.quote_id!=binding.source_id: return _fail("BargainQuoteBindingMismatch")
	var state: Dictionary={"format":FORMAT,"definition_id":definition.id,"source_sha256":_source_digest(definition,texts),"binding":binding.duplicate(true),"actor_snapshot":actor_snapshot.duplicate(true),"context":context.duplicate(true),"seed":seed,"mode":mode,"phase":"active","choices":[],"result":{}}
	if mode=="automatic":
		state.phase="completed"; state.result=_result(definition,state,"automatic")
	return {"ok":true,"state":state}

## 只接受当前轮的一个公开提议；不会暗改数量、拆分订单或把现款改成债务。
static func choose(definition: Dictionary, texts: Dictionary, state: Dictionary, round_id: String, option_id: String) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="active": return _fail("BargainAlreadyCompleted")
	var round_row: Dictionary=definition.rounds[state.choices.size()]
	if round_id!=round_row.id: return _fail("StaleBargainRound")
	var option: Dictionary=_option(round_row,option_id)
	if option.is_empty(): return _fail("UnknownBargainOption")
	var next: Dictionary=state.duplicate(true)
	next.choices.append({"round_id":round_id,"option_id":option_id,"points":_points(option,_conditions(definition))})
	if next.choices.size()==ROUND_COUNT:
		next.phase="completed"; next.result=_result(definition,next,"manual")
	return {"ok":true,"state":next}

## 退出回到同一实例自动成绩；终态保留原结果，不能重新累计关系优惠或再领一份折扣。
static func exit_challenge(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	var next: Dictionary=state.duplicate(true)
	if next.phase=="active":
		next.phase="completed"; next.result=_result(definition,next,"exited")
	return {"ok":true,"state":next}

## 返回当前公开提议、预算、数量及已披露条件；分数不能用来推断未实现的世界市场需求。
static func view(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase=="completed": return {"ok":true,"phase":"completed","result":state.result.duplicate(true)}
	var round_row: Dictionary=definition.rounds[state.choices.size()]
	var options: Array=[]; var conditions: Array=[]
	for option in round_row.options: options.append({"id":option.id,"kind":option.kind,"method":option.method,"text":texts[option.text_key],"supply_condition_id":option.supply_condition_id,"terms_condition_id":option.terms_condition_id})
	for condition in definition.conditions: conditions.append({"id":condition.id,"kind":condition.kind,"text":texts[condition.text_key],"matched":condition.matched})
	return {"ok":true,"phase":"active","round":state.choices.size()+1,"round_id":round_row.id,"prompt":texts[round_row.prompt_key],"budget":state.context.budget,"quantity":state.context.quantity,"conditions":conditions,"options":options}

## 存读按原题面重算每个选项和终态，owner还须把绑定及context与自己的原行动/真实报价快照核对。
static func validate_state(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if not SharedChallenge.fields(state,{"format":TYPE_STRING,"definition_id":TYPE_STRING,"source_sha256":TYPE_STRING,"binding":TYPE_DICTIONARY,"actor_snapshot":TYPE_DICTIONARY,"context":TYPE_DICTIONARY,"seed":TYPE_STRING,"mode":TYPE_STRING,"phase":TYPE_STRING,"choices":TYPE_ARRAY,"result":TYPE_DICTIONARY}) or state.format!=FORMAT or state.definition_id!=definition.id: return _fail("InvalidBargainState")
	if state.source_sha256!=_source_digest(definition,texts): return _fail("BargainSourceChanged")
	if not _binding(state.binding) or not SharedChallenge.snapshot(state.actor_snapshot) or not _context(state.context) or not SharedChallenge.seed(state.seed) or not state.mode in ["manual","automatic"] or not state.phase in ["active","completed"] or state.choices.size()>ROUND_COUNT: return _fail("InvalidBargainState")
	if state.context.quote!=null and state.context.quote.quote_id!=state.binding.source_id: return _fail("BargainQuoteBindingMismatch")
	var conditions: Dictionary=_conditions(definition)
	for index in range(state.choices.size()):
		var choice: Variant=state.choices[index]
		if not SharedChallenge.fields(choice,{"round_id":TYPE_STRING,"option_id":TYPE_STRING,"points":TYPE_INT}): return _fail("InvalidBargainChoice")
		var round_row: Dictionary=definition.rounds[index]
		var option: Dictionary=_option(round_row,choice.option_id)
		if choice.round_id!=round_row.id or option.is_empty() or choice.points!=_points(option,conditions): return _fail("InvalidBargainChoice")
	if state.phase=="active":
		return {"ok":true} if state.mode=="manual" and state.choices.size()<ROUND_COUNT and state.result.is_empty() else _fail("InvalidBargainProgress")
	var completion: Variant=state.result.get("completion")
	if not completion is String or not completion in ["manual","automatic","exited"]: return _fail("InvalidBargainResult")
	if completion=="automatic" and (state.mode!="automatic" or not state.choices.is_empty()): return _fail("InvalidBargainResult")
	if completion=="manual" and (state.mode!="manual" or state.choices.size()!=ROUND_COUNT): return _fail("InvalidBargainResult")
	if completion=="exited" and (state.mode!="manual" or state.choices.size()>=ROUND_COUNT): return _fail("InvalidBargainResult")
	if not SharedChallenge.fields(state.result,{"completion":TYPE_STRING,"score":TYPE_INT,"raw_score":TYPE_INT if completion=="manual" else TYPE_NIL,"auto_score":TYPE_INT,"performance_bp":TYPE_INT,"discount_bp":TYPE_INT,"relation_discount_bp":TYPE_INT,"challenge_discount_bp":TYPE_INT,"choices":TYPE_ARRAY}) or state.result!=_result(definition,state,completion): return _fail("InvalidBargainResult")
	return {"ok":true}

## owner提交前用自己保存的真实上下文核对整个实例；返回总折扣候选而非可继续叠加的增量。
## 这里不能替代owner的quote_id消费位、世界revision或原报价时效检查，也不自行发放任何结果。
static func result_for_context(definition: Dictionary, texts: Dictionary, state: Dictionary, expected_binding: Dictionary, expected_context: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="completed": return _fail("BargainNotCompleted")
	if not _binding(expected_binding) or not _context(expected_context) or expected_binding!=state.binding or expected_context!=state.context: return _fail("BargainContextChanged")
	return {"ok":true,"binding":state.binding.duplicate(true),"context":state.context.duplicate(true),"result":state.result.duplicate(true)}

## 课程可复用已核表现系数的整数计算；这里只返回值，不能作为独立领奖或交易提交入口。
static func scaled_experience(definition: Dictionary, texts: Dictionary, state: Dictionary, normal_xp: int) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="completed" or not SharedChallenge.whole(normal_xp,0,1200): return _fail("InvalidBargainExperience")
	@warning_ignore("integer_division")
	var amount: int=normal_xp*state.result.performance_bp/10000
	return {"ok":true,"normal_xp":normal_xp,"xp":amount,"performance_bp":state.result.performance_bp}

## 作者公开供需匹配给20分、已知条件匹配给10分；没有引用或不匹配都为零。
static func _points(option: Dictionary, conditions: Dictionary) -> int:
	var points: int=0
	if option.supply_condition_id!=null and conditions[option.supply_condition_id].matched: points+=20
	if option.terms_condition_id!=null and conditions[option.terms_condition_id].matched: points+=10
	return points

## 基础20加三轮并封顶100，自动兜底与礼议共用；关系和本次成绩只合并一次再封顶800。
static func _result(definition: Dictionary, state: Dictionary, completion: String) -> Dictionary:
	var automatic: int=SharedChallenge.automatic_score(state.actor_snapshot,definition.difficulty)
	var raw: Variant=null; var score: int=automatic
	if completion=="manual":
		raw=20
		for choice in state.choices: raw+=choice.points
		raw=mini(100,raw); score=maxi(raw,automatic-10)
	var challenge_discount: int=score*5
	return {"completion":completion,"score":score,"raw_score":raw,"auto_score":automatic,"performance_bp":SharedChallenge.performance_bp(score,automatic),"discount_bp":mini(800,state.context.relation_discount_bp+challenge_discount),"relation_discount_bp":state.context.relation_discount_bp,"challenge_discount_bp":challenge_discount,"choices":state.choices.duplicate(true)}

## 公开条件和题面文字一同绑定摘要，显示条件变动不能继续消费旧选择。
static func _source_digest(definition: Dictionary, texts: Dictionary) -> String:
	var selected: Dictionary={}
	for condition in definition.conditions: selected[condition.text_key]=texts[condition.text_key]
	for round_row in definition.rounds:
		selected[round_row.prompt_key]=texts[round_row.prompt_key]
		for option in round_row.options: selected[option.text_key]=texts[option.text_key]
	return JSON.stringify({"definition":definition,"texts":selected},"",true).sha256_text()

## 预算、数量、优惠与报价绑定由可信owner冻结，不接受付款路径、赊欠或调用方自报新价。
static func _context(value: Dictionary) -> bool:
	if value.size()!=4 or not value.has("quote") or not value.get("budget") is int or not value.get("quantity") is int or not value.get("relation_discount_bp") is int or not SharedChallenge.whole(value.budget,0,2147483647) or not SharedChallenge.whole(value.quantity,1,500) or not SharedChallenge.whole(value.relation_discount_bp,0,800): return false
	if value.quote==null: return true
	if not SharedChallenge.fields(value.quote,{"quote_id":TYPE_STRING,"quote_sha256":TYPE_STRING,"market_revision":TYPE_INT,"valid_tick":TYPE_INT}) or not _source_id(value.quote.quote_id) or not SharedChallenge.whole(value.quote.market_revision,0,2147483647) or not SharedChallenge.whole(value.quote.valid_tick,0,2147483647): return false
	var digest: String=value.quote.quote_sha256
	if digest.length()!=64: return false
	for character in digest:
		if not character in "0123456789abcdef": return false
	return true

## 来源可为课程action或正式quote；ID只作身份比较，不解释成可读取的路径。
static func _binding(value: Dictionary) -> bool:
	return SharedChallenge.fields(value,{"actor_id":TYPE_STRING,"source_id":TYPE_STRING}) and SharedChallenge.symbol(value.actor_id,true) and _source_id(value.source_id)

## 现有运行实例ID包含冒号，保留其精确有界文本；拒绝换行而不偷偷标准化身份。
static func _source_id(value: String) -> bool:
	return not value.is_empty() and value.length()<=160 and not "\n" in value and not "\r" in value

## 有限条件只能引用正确类别的当前公开作者记录，不执行自由表达式或隐含世界查询。
static func _condition_reference(conditions: Dictionary, id: Variant, kind: String) -> bool:
	return id==null or (id is String and conditions.has(id) and conditions[id].kind==kind)

## 只提取四个必需标量的类型；完整字段数和两个显式可空引用另行共同校验。
static func _option_fields(option: Dictionary) -> Dictionary:
	return {"id":option.get("id"),"kind":option.get("kind"),"method":option.get("method"),"text_key":option.get("text_key")}

## 定义已验证后构造局部条件索引，返回引用只用于本次只读评分。
static func _conditions(definition: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for condition in definition.conditions: result[condition.id]=condition
	return result

## 只接受本轮明确存在的选项，任意字符串不能被解释成交易动作。
static func _option(round_row: Dictionary, id: String) -> Dictionary:
	for option in round_row.options:
		if option.id==id: return option
	return {}

## 稳定错误只返回诊断，不把未完成题面或无效报价变成成功折扣。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
