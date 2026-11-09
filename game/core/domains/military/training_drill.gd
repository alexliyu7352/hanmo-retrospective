## 军阵题面绑定一条真实训练行动；只保存实例和首课前固定系数，训练、费用及占用由Training唯一结算。
extends RefCounted
const Drill=preload("res://core/domains/cultivation/drill_challenge.gd")
const Common=preload("res://core/domains/cultivation/challenge_basis.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const DRAFT_KIND="military_drill_draft"
const CONFIRM_KIND="military_drill_confirm"
const CANCEL_KIND="military_drill_cancel_pending"
const EXIT_KIND="military_drill_exit"

## 只对营地明确提供的题面冻结本人兵法/智谋；无题面自动课不增加任何持久键或随机消费。
static func quote(data: Dictionary, terms: Dictionary, actor: Dictionary, mode: String="automatic") -> Dictionary:
	var source: Dictionary=_source(data,terms)
	if not source.ok: return source
	if source.definition.is_empty(): return {"ok":true} if mode=="automatic" else _fail("TrainingDrillNotOffered")
	var xp: Variant=actor.get("skills",{}).get("tactics")
	var auxiliary: Variant=actor.get("attributes",{}).get("intellect")
	if actor.get("id")!=terms.get("actor_id") or Levels.level(xp)<0 or not Common.whole(auxiliary,1,110): return _fail("InvalidTrainingDrillActor")
	var snapshot: Dictionary={"skill_level":Levels.level(xp),"auxiliary_attribute":auxiliary}
	var created: Dictionary=Drill.create(source.definition,data.get("texts",{}),{"actor_id":actor.id,"source_id":"preview"},snapshot,"0",mode)
	if not created.ok: return created
	var group_source: Dictionary=_group_source(terms,source.definition)
	if group_source.is_empty(): return _fail("InvalidTrainingDrillRoster")
	return {"ok":true,"drill":{"kind":"drill","challenge_id":source.definition.id,"mode":mode,"tactics_xp":xp,"actor_snapshot":snapshot,"source_sha256":created.state.source_sha256,"group_source":group_source}}

## 真实begin候选固定action后抽一次既有minigame流；任何失败都发生在改事实和随机流之前。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, fact: Dictionary) -> Dictionary:
	if not _envelope(fact,action.get("id","")) or action.get("actor_id")!=fact.actor_id or action.get("start_tick")!=fact.start_tick or fact.phase!="started" or not fact.lessons.is_empty(): return _fail("InvalidTrainingDrillEnvelope")
	if fact.has("drill"): return _fail("TrainingDrillAlreadyStarted")
	var frozen: Dictionary=_frozen(data,fact)
	if not frozen.ok: return frozen
	if not fact.terms.has("drill"): return {"ok":true}
	var current: Dictionary=quote(data,fact.terms,state.get("actors",{}).get(fact.actor_id,{}),fact.terms.drill.mode)
	if not current.ok: return current
	if current.drill!=fact.terms.drill: return _fail("TrainingDrillSnapshotChanged")
	var origin: Variant=state.get("rng",{}).get("minigame")
	if not _stream(origin): return _fail("InvalidMinigameRandomState")
	var draw: Dictionary=_draw(origin)
	var created: Dictionary=Drill.create(frozen.definition,data.get("texts",{}),{"actor_id":fact.actor_id,"source_id":action.id},fact.terms.drill.actor_snapshot,draw.seed,fact.terms.drill.mode)
	if not created.ok: return created
	if created.state.source_sha256!=fact.terms.drill.source_sha256: return _fail("TrainingDrillSourceChanged")
	fact.drill={"seed_origin":origin.duplicate(true),"challenge":created.state,"phase":"pending","frozen_tick":-1,"performance_bp":null}
	state.rng.minigame.state=draw.state
	return {"ok":true}

## 稳定存档核同一来源、行动、抽种和真实课时；首课后禁止删除固定系数或恢复可编辑实例。
static func validate(data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	if not _envelope(fact,action_id): return _fail("InvalidTrainingDrillEnvelope")
	var frozen: Dictionary=_frozen(data,fact)
	if not frozen.ok: return frozen
	if not fact.terms.has("drill"): return _fail("UnexpectedTrainingDrill") if fact.has("drill") else {"ok":true}
	var instance: Variant=fact.get("drill")
	if not instance is Dictionary or not instance.get("phase") in ["pending","frozen","closed"]: return _fail("InvalidTrainingDrillInstance")
	if not instance.has("performance_bp") or not Common.fields(instance,{"seed_origin":TYPE_DICTIONARY,"challenge":TYPE_DICTIONARY,"phase":TYPE_STRING,"frozen_tick":TYPE_INT,"performance_bp":TYPE_INT if instance.phase=="frozen" else TYPE_NIL}) or not _stream(instance.seed_origin): return _fail("InvalidTrainingDrillInstance")
	var challenge: Dictionary=instance.challenge; var t: Dictionary=fact.terms.drill
	var checked: Dictionary=Drill.validate_state(frozen.definition,data.get("texts",{}),challenge)
	if not checked.ok: return checked
	if challenge.definition_id!=t.challenge_id or challenge.source_sha256!=t.source_sha256 or challenge.mode!=t.mode or challenge.actor_snapshot!=t.actor_snapshot or challenge.binding!={"actor_id":fact.actor_id,"source_id":action_id} or challenge.seed!=_draw(instance.seed_origin).seed: return _fail("TrainingDrillBindingMismatch")
	if instance.phase=="pending":
		if fact.phase!="started" or not fact.lessons.is_empty() or instance.frozen_tick!=-1: return _fail("TrainingDrillConsumptionMismatch")
	elif instance.phase=="closed":
		if fact.phase!="completed" or not fact.lessons.is_empty() or instance.frozen_tick!=-1 or challenge.phase!="completed": return _fail("TrainingDrillConsumptionMismatch")
	else:
		if fact.lessons.is_empty() or instance.frozen_tick!=fact.start_tick+1 or challenge.phase!="completed" or instance.performance_bp!=challenge.result.performance_bp: return _fail("TrainingDrillConsumptionMismatch")
	return {"ok":true}

## 只给本人首课前的实例返回下一状态；调用方在原候选事务内写回challenge，不新增时间或释放占用。
static func transition(data: Dictionary, fact: Dictionary, action_id: String, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	if fact.actor_id!=actor_id: return _fail("TrainingDrillActorMismatch")
	if not fact.has("drill"): return _fail("TrainingDrillNotOffered")
	if fact.drill.phase!="pending": return _fail("TrainingDrillAlreadyFrozen")
	if params.get("action_id")!=action_id: return _fail("StaleTrainingDrillAction")
	var definition: Dictionary=_source(data,fact.terms).definition
	var challenge: Dictionary=fact.drill.challenge; var texts: Dictionary=data.get("texts",{})
	if kind==DRAFT_KIND:
		if not Common.fields(params,{"action_id":TYPE_STRING,"expected_step":TYPE_INT,"group_id":TYPE_STRING,"target":TYPE_ARRAY}): return _fail("InvalidTrainingDrillRequest")
		return Drill.draft_move(definition,texts,challenge,params.expected_step,params.group_id,params.target)
	if kind in [CONFIRM_KIND,CANCEL_KIND]:
		if not Common.fields(params,{"action_id":TYPE_STRING,"expected_step":TYPE_INT,"expected_pending":TYPE_DICTIONARY}): return _fail("InvalidTrainingDrillRequest")
		return Drill.confirm_move(definition,texts,challenge,params.expected_step,params.expected_pending) if kind==CONFIRM_KIND else Drill.cancel_pending(definition,texts,challenge,params.expected_step,params.expected_pending)
	if kind==EXIT_KIND:
		if not Common.fields(params,{"action_id":TYPE_STRING}): return _fail("InvalidTrainingDrillRequest")
		return Drill.exit_challenge(definition,texts,challenge)
	return _fail("UnsupportedAction")

## 首个真实课时落账前固定一次结果；后续只返回原系数，失败回滚和本期训练/付款仍在owner同一候选事务。
static func freeze_before_lesson(data: Dictionary, fact: Dictionary, action_id: String, finish_tick: int) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	if fact.phase!="started" or fact.lessons.size()>=4 or finish_tick!=fact.start_tick+fact.lessons.size()+1: return _fail("TrainingDrillClockMismatch")
	if not fact.has("drill"): return {"ok":true,"performance_bp":10000}
	var instance: Dictionary=fact.drill
	if instance.phase=="frozen": return {"ok":true,"performance_bp":instance.performance_bp}
	if instance.phase!="pending" or not fact.lessons.is_empty(): return _fail("TrainingDrillConsumptionMismatch")
	var closed: Dictionary=Drill.exit_challenge(_source(data,fact.terms).definition,data.get("texts",{}),instance.challenge)
	if not closed.ok: return closed
	instance.challenge=closed.state; instance.phase="frozen"; instance.frozen_tick=finish_tick; instance.performance_bp=closed.state.result.performance_bp
	return {"ok":true,"performance_bp":instance.performance_bp}

## 零课取消或供给中断只封题，不消费训练系数；owner须在同一候选内把原fact封为completed。
static func close_unprovided(data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	if not fact.has("drill") or not fact.lessons.is_empty() or fact.drill.phase=="closed": return {"ok":true}
	if fact.drill.phase!="pending": return _fail("TrainingDrillConsumptionMismatch")
	var closed: Dictionary=Drill.exit_challenge(_source(data,fact.terms).definition,data.get("texts",{}),fact.drill.challenge)
	if not closed.ok: return closed
	fact.drill.challenge=closed.state; fact.drill.phase="closed"
	return {"ok":true}

## 公共题面仍由原纯叶投影，附来源只解释三个抽象组；不会暴露解证据或声称创建真实士卒。
static func view(data: Dictionary, fact: Dictionary, action_id: String, actor_id: String) -> Dictionary:
	var checked: Dictionary=validate(data,fact,action_id)
	if not checked.ok: return checked
	if fact.actor_id!=actor_id: return _fail("TrainingDrillActorMismatch")
	if not fact.has("drill"): return _fail("TrainingDrillNotOffered")
	var shown: Dictionary=Drill.view(_source(data,fact.terms).definition,data.get("texts",{}),fact.drill.challenge)
	if shown.ok:
		shown.instance_phase=fact.drill.phase; shown.performance_bp=fact.drill.performance_bp
		shown.group_source=fact.terms.drill.group_source.duplicate(true)
	return shown

## 作者绑定和已冻结三抽象组必须始终一致；不把题面组解释为三支真实部队或兵员配额。
static func _frozen(data: Dictionary, fact: Dictionary) -> Dictionary:
	var source: Dictionary=_source(data,fact.terms)
	if not source.ok: return source
	if source.definition.is_empty(): return _fail("UnexpectedTrainingDrill") if fact.terms.has("drill") else source
	var frozen: Variant=fact.terms.get("drill")
	if not Common.fields(frozen,{"kind":TYPE_STRING,"challenge_id":TYPE_STRING,"mode":TYPE_STRING,"tactics_xp":TYPE_INT,"actor_snapshot":TYPE_DICTIONARY,"source_sha256":TYPE_STRING,"group_source":TYPE_DICTIONARY}) or frozen.kind!="drill" or frozen.challenge_id!=source.definition.id or not frozen.mode in ["manual","automatic"] or Levels.level(frozen.tactics_xp)<0 or not Common.snapshot(frozen.actor_snapshot) or frozen.actor_snapshot.skill_level!=Levels.level(frozen.tactics_xp): return _fail("InvalidTrainingDrillTerms")
	var groups: Dictionary=_group_source(fact.terms,source.definition)
	if groups.is_empty() or not Common.fields(frozen.group_source,{"kind":TYPE_STRING,"unit_id":TYPE_STRING,"troop_type_id":TYPE_STRING,"roster_sha256":TYPE_STRING,"group_ids":TYPE_ARRAY}) or frozen.group_source!=groups: return _fail("TrainingDrillRosterMismatch")
	var checked: Dictionary=Drill.create(source.definition,data.get("texts",{}),{"actor_id":fact.actor_id,"source_id":"verify"},frozen.actor_snapshot,"0",frozen.mode)
	if not checked.ok: return checked
	if checked.state.source_sha256!=frozen.source_sha256: return _fail("TrainingDrillSourceChanged")
	return source

## 数据表和营地字段是唯一题面来源；缺题面关闭失败，不接受调用者路径、脚本或可变最新版本。
static func _source(data: Dictionary, terms: Dictionary) -> Dictionary:
	var camp: Variant=data.get("garrisons",{}).get(terms.get("garrison_id"))
	if not camp is Dictionary: return _fail("MissingTrainingDrillGarrison")
	if not camp.has("training_drill_id"): return {"ok":true,"definition":{}}
	if not camp.training_drill_id is String: return _fail("InvalidTrainingDrillSource")
	var definition: Variant=data.get("drill_challenges",{}).get(camp.training_drill_id)
	if not definition is Dictionary or definition.get("id")!=camp.training_drill_id: return _fail("MissingTrainingDrill")
	var checked: Dictionary=Drill.validate_definition(definition,data.get("texts",{}))
	return {"ok":true,"definition":definition} if checked.ok else checked

## 冻结真实唯一有序名册的摘要和本题三组ID；不把参训者分配给虚构步骑弓单位。
static func _group_source(terms: Dictionary, definition: Dictionary) -> Dictionary:
	if not terms.get("unit_id") is String or not terms.get("troop_type_id") is String or not terms.get("fit_ids") is Array or terms.fit_ids.is_empty(): return {}
	var previous: String=""
	for id in terms.fit_ids:
		if not id is String or id.is_empty() or (not previous.is_empty() and id<=previous): return {}
		previous=id
	var ids: Array=[]
	for group in definition.groups: ids.append(group.id)
	return {"kind":"abstract_training","unit_id":terms.unit_id,"troop_type_id":terms.troop_type_id,"roster_sha256":JSON.stringify(terms.fit_ids).sha256_text(),"group_ids":ids}

## 本叶只核与实例相关外壳，完整军务授权、费用和课时历史仍交给Training原validator。
static func _envelope(fact: Dictionary, action_id: String) -> bool:
	return not action_id.is_empty() and action_id.length()<=160 and not "\n" in action_id and not "\r" in action_id and fact.get("kind")=="military_training" and fact.get("actor_id") is String and fact.get("terms") is Dictionary and fact.terms.get("actor_id")==fact.actor_id and fact.get("phase") in ["started","completed"] and Common.whole(fact.get("start_tick"),0,2147483647) and fact.get("lessons") is Array and fact.lessons.size()<=4

## 临时RNG重放仅核一次派生值，不推进世界流或生成额外模板。
static func _draw(origin: Dictionary) -> Dictionary:
	var random=RandomNumberGenerator.new(); random.seed=origin.seed.to_int(); random.state=origin.state.to_int()
	var seed: String=str(random.randi())
	return {"seed":seed,"state":str(random.state)}

## 保持已有十进制int64随机流合同，拒绝非规范文本与浮点。
static func _stream(value: Variant) -> bool:
	return Common.fields(value,{"seed":TYPE_STRING,"state":TYPE_STRING}) and Common.seed(value.seed) and Common.seed(value.state)

## 具体错误交原Kernel候选事务处理，不能在失败后伪装已提供训练。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
