## 部队条件的纯投影与历史折叠；不写世界、事实、角色疲劳或随机。活动行只是待接线投影，成功不证明动作已真实执行。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const ACTIVITIES={"march":3,"qualified_garrison_rest":-10}
const SOURCE_KINDS={"march":"executed_movement","qualified_garrison_rest":"qualified_garrison_rest"}

## 仅投影已明定的两种增减并夹到0至100；本叶不判断驻地资格，也不接受调用者自报数值或未知活动。
static func project_profile(profile: Variant, activity: Variant) -> Dictionary:
	if not _profile(profile): return _fail("InvalidMilitaryConditionProfile")
	if not activity is String or not ACTIVITIES.has(activity): return _fail("UnknownMilitaryConditionActivity")
	return {"ok":true,"profile":{"training":profile.training,"fatigue":clampi(profile.fatigue+ACTIVITIES[activity],0,100)}}

## 引用原训练事实的课时并合并有限活动投影，核唯一来源、同半日互斥与前后连续；真实付款/授权仍由Training先验。
static func fold(unit_id: Variant, initial: Variant, training_sources: Variant, activity_projections: Variant, through_tick: Variant) -> Dictionary:
	if not unit_id is String or unit_id.is_empty() or not _profile(initial) or not Values.whole(through_tick): return _fail("InvalidMilitaryConditionHistory")
	if not training_sources is Array or not activity_projections is Array: return _fail("InvalidMilitaryConditionHistory")
	var rows: Array=[]; var source_ids: Dictionary={}
	for source in training_sources:
		if not Values.fields(source,{"action_id":TYPE_STRING,"fact":TYPE_DICTIONARY}) or source.size()!=2 or source.action_id.is_empty() or source_ids.has("training:"+source.action_id): return _fail("InvalidMilitaryConditionTrainingSource")
		var fact: Dictionary=source.fact
		if not Values.fields(fact,{"kind":TYPE_STRING,"terms":TYPE_DICTIONARY,"lessons":TYPE_ARRAY}) or fact.kind!="military_training" or not fact.terms.get("unit_id") is String or fact.terms.unit_id!=unit_id: return _fail("InvalidMilitaryConditionTrainingSource")
		source_ids["training:"+source.action_id]=true
		for lesson in fact.lessons:
			if not Values.fields(lesson,{"tick":TYPE_INT,"supply_fact_id":TYPE_STRING,"fee_paid":TYPE_INT,"training_before":TYPE_INT,"training_after":TYPE_INT,"fatigue_before":TYPE_INT,"fatigue_after":TYPE_INT}) or lesson.size()!=7 or not _row_profiles(lesson): return _fail("InvalidMilitaryConditionTrainingSource")
			rows.append(lesson)
	for row in activity_projections:
		var checked: Dictionary=_projection(row,unit_id)
		if not checked.ok: return checked
		var source_key: String="activity:"+row.source.kind+":"+row.source.id
		if source_ids.has(source_key): return _fail("DuplicateMilitaryConditionSource")
		source_ids[source_key]=true; rows.append(row)
	# 只排序本次引用数组，不排序或复制回原事实的lessons，不生成第二份持久日记。
	rows.sort_custom(func(a,b): return a.tick<b.tick)
	var profile: Dictionary=initial.duplicate(true); var previous_tick: int=-1
	for row in rows:
		if row.tick<=previous_tick or row.tick>through_tick: return _fail("MilitaryConditionHistoryTimeMismatch")
		if row.training_before!=profile.training or row.fatigue_before!=profile.fatigue: return _fail("MilitaryConditionHistoryMismatch")
		profile={"training":row.training_after,"fatigue":row.fatigue_after}; previous_tick=row.tick
	return {"ok":true,"profile":profile}

## 活动投影须携带精确类型的来源引用；此处只核引用形状与算术，未来执行适配器须另验来源事实，不能传validated布尔冒充证据。
static func _projection(row: Variant, unit_id: String) -> Dictionary:
	if not Values.fields(row,{"unit_id":TYPE_STRING,"tick":TYPE_INT,"kind":TYPE_STRING,"source":TYPE_DICTIONARY,"training_before":TYPE_INT,"training_after":TYPE_INT,"fatigue_before":TYPE_INT,"fatigue_after":TYPE_INT}) or row.size()!=8 or row.unit_id!=unit_id or not Values.whole(row.tick,1) or not _row_profiles(row): return _fail("InvalidMilitaryConditionProjection")
	if not ACTIVITIES.has(row.kind): return _fail("UnknownMilitaryConditionActivity")
	if not Values.fields(row.source,{"kind":TYPE_STRING,"id":TYPE_STRING}) or row.source.size()!=2 or row.source.kind!=SOURCE_KINDS[row.kind] or row.source.id.is_empty(): return _fail("InvalidMilitaryConditionSource")
	var projected: Dictionary=project_profile({"training":row.training_before,"fatigue":row.fatigue_before},row.kind)
	if row.training_after!=projected.profile.training or row.fatigue_after!=projected.profile.fatigue: return _fail("MilitaryConditionProjectionMismatch")
	return {"ok":true}

## 纯输入只允许两个有界整数，不从演员、作者默认或其他字段补条件值。
static func _profile(profile: Variant) -> bool:
	return profile is Dictionary and profile.size()==2 and Values.whole(profile.get("training"),0,100) and Values.whole(profile.get("fatigue"),0,100)

## 训练与活动历史均须给出完整有界前后值，具体训练增量仍由原事实校验器负责。
static func _row_profiles(row: Dictionary) -> bool:
	for key in ["training_before","training_after","fatigue_before","fatigue_after"]:
		if not Values.whole(row.get(key),0,100): return false
	return true

## 返回明确失败而不抛出类型错误；调用方按原领域错误合同映射历史失败。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
