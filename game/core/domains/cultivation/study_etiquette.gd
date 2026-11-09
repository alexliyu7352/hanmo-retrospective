## 历史礼议文件承载同一个课程小游戏实例适配器；新题面只按礼议/议价两个显式kind分派。
## 原terms/fact.etiquette继续原样存读，新challenge字段不造第二时钟、随机流或奖励账。
extends RefCounted
const Etiquette=preload("res://core/domains/cultivation/etiquette_challenge.gd")
const Bargain=preload("res://core/domains/cultivation/bargain_challenge.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const AUXILIARY={"rites":"administration","debate":"intellect"}
const BARGAIN_AUXILIARY={"commerce":"administration","negotiation":"charisma"}
const CHOOSE_KIND="study_etiquette_choose"
const EXIT_KIND="study_etiquette_exit"
const GENERIC_CHOOSE_KIND="study_challenge_choose"
const GENERIC_EXIT_KIND="study_challenge_exit"

## 课程只允许一种题面绑定；练习议价上下文由公开作者数据提供，绝不接受真实quote或隐含交易副作用。
static func course_valid(course: Dictionary) -> bool:
	if course.has("settlement_policy") and course.settlement_policy!="per_lesson_v1": return false
	if course.has("etiquette_challenge_id"):
		return not course.has("challenge") and course.etiquette_challenge_id is String and course.get("skill_id") in AUXILIARY
	if not course.has("challenge"): return true
	var value: Variant=course.challenge
	if not value is Dictionary or not value.get("kind") is String or not value.get("id") is String: return false
	if value.kind=="etiquette": return _fields(value,{"kind":TYPE_STRING,"id":TYPE_STRING}) and course.get("skill_id") in AUXILIARY
	return _fields(value,{"kind":TYPE_STRING,"id":TYPE_STRING,"context":TYPE_DICTIONARY}) and value.kind=="bargain" and course.get("skill_id") in BARGAIN_AUXILIARY and _practice_context(value.context)

## 报价沿旧礼议返回原四字段；新题面冻结显式kind及作者练习上下文，普通无题面课保持原自动基线。
static func quote(data: Dictionary, course: Dictionary, actor: Dictionary, mode: String="automatic") -> Dictionary:
	if not course_valid(course): return _fail("InvalidStudyChallengeCourse")
	var source: Dictionary=_course_source(course)
	if source.is_empty(): return {"ok":true} if mode=="automatic" else _fail("StudyChallengeNotOffered")
	var definition: Dictionary=_definition(data,source.kind,source.id)
	if definition.is_empty(): return _fail("MissingStudyChallenge")
	var xp: Variant=actor.get("skills",{}).get(course.skill_id)
	var auxiliary_name: String=AUXILIARY[course.skill_id] if source.kind=="etiquette" else BARGAIN_AUXILIARY[course.skill_id]
	var auxiliary: Variant=actor.get("attributes",{}).get(auxiliary_name)
	if not xp is int or xp<0 or xp>1200 or not auxiliary is int or auxiliary<1 or auxiliary>110: return _fail("InvalidStudyChallengeActor")
	var snapshot: Dictionary={"skill_level":Levels.level(xp),"auxiliary_attribute":auxiliary}
	var frozen: Dictionary={"challenge_id":source.id,"mode":mode,"actor_snapshot":snapshot,"source_sha256":""}
	if source.field=="challenge":
		frozen.kind=source.kind
		frozen.context=source.context.duplicate(true) if source.kind=="bargain" else null
	var checked: Dictionary=_create(data,definition,_normalized(frozen),{"actor_id":actor.id,"source_id":"preview"},"0")
	if not checked.ok: return checked
	frozen.source_sha256=checked.state.source_sha256
	return {"ok":true,source.field:frozen}

## 开课只推进现minigame流一次；原行动与同一事实持有种子来源，重复start不能覆盖已有实例。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, fact: Dictionary) -> Dictionary:
	var field: String=_field(fact)
	if field.is_empty(): return {"ok":true}
	if fact.has(field): return _fail("StudyChallengeAlreadyStarted")
	var origin: Variant=state.get("rng",{}).get("minigame")
	if not _stream(origin): return _fail("InvalidMinigameRandomState")
	var draw: Dictionary=_draw(origin)
	var terms: Dictionary=_normalized(fact.terms[field])
	var definition: Dictionary=_definition(data,terms.kind,terms.challenge_id)
	if definition.is_empty(): return _fail("MissingStudyChallenge")
	var created: Dictionary=_create(data,definition,terms,{"actor_id":fact.actor_id,"source_id":action.id},draw.seed)
	if not created.ok: return created
	if created.state.source_sha256!=terms.source_sha256: return _fail("StudyChallengeSourceChanged")
	fact[field]={"seed_origin":origin.duplicate(true),"challenge":created.state,"consumed":false}
	state.rng.minigame.state=draw.state
	return {"ok":true}

## 原课程声明与持久字段必须一一对应，禁止删题面重开、同时夹带两实例或把礼议改成议价。
static func matches_course(course: Dictionary, fact: Dictionary) -> bool:
	if not course_valid(course): return false
	var source: Dictionary=_course_source(course)
	var field: String=_field(fact)
	if source.is_empty(): return field.is_empty() and not fact.has("etiquette") and not fact.has("challenge")
	if field!=source.field or not fact.terms.get(field) is Dictionary: return false
	var frozen: Dictionary=_normalized(fact.terms[field])
	return frozen.get("kind")==source.kind and frozen.get("challenge_id")==source.id and frozen.get("context")==source.context

## 存档只接受两个已知类型，并与原行动/人物/作者/课程快照及一次抽种证据全部核对。
static func validate(data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	var terms: Dictionary=fact.terms
	var field: String=_field(fact)
	if field.is_empty(): return _fail("UnexpectedStudyChallenge") if fact.has("etiquette") or fact.has("challenge") else {"ok":true}
	if terms.has("etiquette") and terms.has("challenge") or fact.has("etiquette") and fact.has("challenge"): return _fail("InvalidStudyChallengeTerms")
	var frozen: Variant=terms[field]
	var shape: Dictionary={"challenge_id":TYPE_STRING,"mode":TYPE_STRING,"actor_snapshot":TYPE_DICTIONARY,"source_sha256":TYPE_STRING}
	if field=="challenge":
		shape.kind=TYPE_STRING
		shape.context=TYPE_DICTIONARY if frozen is Dictionary and frozen.get("kind")=="bargain" else TYPE_NIL
	if not _fields(frozen,shape): return _fail("InvalidStudyChallengeTerms")
	frozen=_normalized(frozen)
	var skills: Dictionary=AUXILIARY if frozen.kind=="etiquette" else BARGAIN_AUXILIARY if frozen.kind=="bargain" else {}
	if not terms.skill_id in skills or frozen.actor_snapshot.get("skill_level")!=Levels.level(terms.xp_before): return _fail("StudyChallengeSnapshotMismatch")
	var instance: Variant=fact.get(field)
	if not _fields(instance,{"seed_origin":TYPE_DICTIONARY,"challenge":TYPE_DICTIONARY,"consumed":TYPE_BOOL}) or not _stream(instance.seed_origin): return _fail("InvalidStudyChallengeInstance")
	var definition: Dictionary=_definition(data,frozen.kind,frozen.challenge_id)
	if definition.is_empty(): return _fail("MissingStudyChallenge")
	var challenge: Dictionary=instance.challenge
	var checked: Dictionary=_rules(frozen.kind).validate_state(definition,data.get("texts",{}),challenge)
	if not checked.ok: return checked
	if challenge.definition_id!=frozen.challenge_id or challenge.mode!=frozen.mode or challenge.source_sha256!=frozen.source_sha256 or challenge.actor_snapshot!=frozen.actor_snapshot or challenge.binding!={"actor_id":fact.actor_id,"source_id":action_id} or challenge.seed!=_draw(instance.seed_origin).seed: return _fail("StudyChallengeBindingMismatch")
	if frozen.kind=="bargain" and (not _practice_full_context(frozen.context) or challenge.context!=frozen.context): return _fail("StudyChallengeContextMismatch")
	if instance.consumed!=(fact.phase=="completed") or (instance.consumed and challenge.phase!="completed"): return _fail("StudyChallengeConsumptionMismatch")
	return {"ok":true}

## 两个通用入口使用step_id；旧礼议命令和node_id照旧，只能操作礼议实例，不成为议价隐含入口。
static func transition(data: Dictionary, fact: Dictionary, action_id: String, kind: String, params: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	var field: String=_field(fact)
	if field.is_empty(): return _fail("StudyChallengeNotOffered")
	var instance: Dictionary=fact[field]
	if instance.consumed: return _fail("StudyChallengeAlreadyConsumed")
	var frozen: Dictionary=_normalized(fact.terms[field])
	if kind in [CHOOSE_KIND,EXIT_KIND] and frozen.kind!="etiquette": return _fail("UnsupportedAction")
	var definition: Dictionary=_definition(data,frozen.kind,frozen.challenge_id)
	if kind in [CHOOSE_KIND,GENERIC_CHOOSE_KIND]:
		var step_key: String="node_id" if kind==CHOOSE_KIND else "step_id"
		if not _fields(params,{"action_id":TYPE_STRING,step_key:TYPE_STRING,"option_id":TYPE_STRING}) or params.action_id!=action_id: return _fail("InvalidStudyChallengeRequest")
		return _rules(frozen.kind).choose(definition,data.get("texts",{}),instance.challenge,params[step_key],params.option_id)
	if kind in [EXIT_KIND,GENERIC_EXIT_KIND]:
		if not _fields(params,{"action_id":TYPE_STRING}) or params.action_id!=action_id: return _fail("InvalidStudyChallengeRequest")
		return _rules(frozen.kind).exit_challenge(definition,data.get("texts",{}),instance.challenge)
	return _fail("UnsupportedAction")

## 结课、中断或取消关闭同一实例并标消费；是否授奖仍由原Study生命周期决定。
static func close(data: Dictionary, fact: Dictionary) -> Dictionary:
	var field: String=_field(fact)
	if field.is_empty(): return {"ok":true}
	var frozen: Dictionary=_normalized(fact.terms[field])
	var definition: Dictionary=_definition(data,frozen.kind,frozen.challenge_id)
	var closed: Dictionary=_rules(frozen.kind).exit_challenge(definition,data.get("texts",{}),fact[field].challenge)
	if not closed.ok: return closed
	fact[field].challenge=closed.state; fact[field].consumed=true
	return {"ok":true}

## 未答完的手动题面继续使用原自动基线，不阻断授课或重抽；奖励仍由课程唯一账处理。
static func pending(fact: Dictionary) -> bool:
	var field: String=_field(fact)
	return not field.is_empty() and fact[field].challenge.mode=="manual" and fact[field].challenge.phase=="active"

## 原公式逐课取整；已知成绩用于当前课或晚答补差，不在此改写人物经验。
static func lesson_xp(fact: Dictionary, base_xp: int) -> int:
	var multiplier: int=10000
	var field: String=_field(fact)
	if not field.is_empty() and fact[field].challenge.phase=="completed": multiplier=fact[field].challenge.result.performance_bp
	@warning_ignore("integer_division")
	return base_xp*multiplier/10000

## 旧课程终结汇总与新课程分段目标共用原倍率，不能把全课取整替代逐课取整。
static func earned(fact: Dictionary) -> int:
	var total: int=0
	for lesson in fact.lessons:
		total+=lesson_xp(fact,lesson.xp)
	return total

## 当前公开话题按已冻结类型分派，旧礼议返回形状保持；通用owner可另附明确kind。
static func view(data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	var field: String=_field(fact)
	if field.is_empty(): return _fail("StudyChallengeNotOffered")
	var frozen: Dictionary=_normalized(fact.terms[field])
	return _rules(frozen.kind).view(_definition(data,frozen.kind,frozen.challenge_id),data.get("texts",{}),fact[field].challenge)

## 返回已验证课程使用的真实实例引用，只供Study同事务写入；不作为可绕过规则的公开游戏接口。
static func instance(fact: Dictionary) -> Dictionary:
	var field: String=_field(fact)
	return fact[field] if not field.is_empty() else {}

## 课程回执明确题面种类，议价分数不代表已改价格；旧礼议回执由原owner保持旧键。
static func kind(fact: Dictionary) -> String:
	var field: String=_field(fact)
	return _normalized(fact.terms[field]).kind if not field.is_empty() else ""

## 只有固定两个Script类可分派，没有eval、动态路径、任意getattr或第三方脚本。
static func _rules(challenge_kind: String) -> GDScript:
	return Etiquette if challenge_kind=="etiquette" else Bargain if challenge_kind=="bargain" else null

## 两个作者表固定映射，定义ID须和真实索引键相同，不从调用方指定任意数据表。
static func _definition(data: Dictionary, challenge_kind: String, id: String) -> Dictionary:
	var table: String="etiquette_challenges" if challenge_kind=="etiquette" else "bargain_challenges" if challenge_kind=="bargain" else ""
	var value: Variant=data.get(table,{}).get(id)
	return value if value is Dictionary and value.get("id")==id else {}

## 两纯叶只在创建时有明确上下文差异；礼议不被硬塞议价预算，课程议价始终quote=null。
static func _create(data: Dictionary, definition: Dictionary, frozen: Dictionary, binding: Dictionary, seed: String) -> Dictionary:
	if frozen.kind=="etiquette": return Etiquette.create(definition,data.get("texts",{}),binding,frozen.actor_snapshot,seed,frozen.mode)
	if frozen.kind=="bargain": return Bargain.create(definition,data.get("texts",{}),binding,frozen.actor_snapshot,frozen.context,seed,frozen.mode)
	return _fail("UnknownStudyChallengeKind")

## 旧声明保留旧持久字段；新显式声明只冻结作者练习参数，不接受作者给出正式quote身份。
static func _course_source(course: Dictionary) -> Dictionary:
	if course.has("etiquette_challenge_id"): return {"field":"etiquette","kind":"etiquette","id":course.etiquette_challenge_id,"context":null}
	if not course.has("challenge"): return {}
	var source: Dictionary={"field":"challenge","kind":course.challenge.kind,"id":course.challenge.id,"context":null}
	if source.kind=="bargain":
		source.context=course.challenge.context.duplicate(true); source.context.quote=null
	return source

## 只对读取值补历史已知类型，不改存档原字段、题面或选择序列。
static func _normalized(frozen: Dictionary) -> Dictionary:
	if frozen.has("kind"): return frozen
	var result: Dictionary=frozen.duplicate(true)
	result.kind="etiquette"; result.context=null
	return result

## 新旧字段互斥由validate处理，此处只识别课程明确持有的字段名。
static func _field(fact: Dictionary) -> String:
	return "challenge" if fact.terms.has("challenge") else "etiquette" if fact.terms.has("etiquette") else ""

## 公开练习数值只表示题面，没有虚构当前市场关系或借款；作者不能夹带quote字段。
static func _practice_context(value: Dictionary) -> bool:
	return _fields(value,{"budget":TYPE_INT,"quantity":TYPE_INT,"relation_discount_bp":TYPE_INT}) and value.budget>=0 and value.budget<=2147483647 and value.quantity>=1 and value.quantity<=500 and value.relation_discount_bp>=0 and value.relation_discount_bp<=800

## 持久练习上下文只比作者形状多一个固定null；它不授予交易资格或消费价格优惠。
static func _practice_full_context(value: Variant) -> bool:
	if not value is Dictionary or value.size()!=4 or not value.has("quote") or value.quote!=null: return false
	var author: Dictionary=value.duplicate(true); author.erase("quote")
	return _practice_context(author)

## 恢复指定现有流后只抽一次，存读校验重算临时RNG但从不推进世界流。
static func _draw(origin: Dictionary) -> Dictionary:
	var random=RandomNumberGenerator.new(); random.seed=origin.seed.to_int(); random.state=origin.state.to_int()
	var seed: String=str(random.randi())
	return {"seed":seed,"state":str(random.state)}

## 世界随机状态保持原十进制int64合同，不接受浮点和前导零。
static func _stream(value: Variant) -> bool:
	if not _fields(value,{"seed":TYPE_STRING,"state":TYPE_STRING}): return false
	for key in ["seed","state"]:
		if not value[key].is_valid_int() or str(value[key].to_int())!=value[key]: return false
	return true

## 固定持久字段和真实类型精确校验，不把额外可执行字段解释为扩展。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary or value.size()!=fields.size(): return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 保留具体失败原因，候选事务不会在失败后伪装已消费奖励或新报价。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
