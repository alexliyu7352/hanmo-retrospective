## 稳定外貌语义的唯一纯实现；不读取世界、资源或随机流，不在读档时重新生成人物。
extends RefCounted
const VERSION="appearance-v1"
const LEGACY_VERSION="legacy-portrait-v1"
const NO_SAMPLE="not-sampled"
const FEATURES=["face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"]
const PROFILE_FIELDS=["format_version","generator_version","generation_seed","base_features","attire_style","source","parent_sources"]
const VISIBLE_FIELDS=["format_version","generator_version","base_features","attire_style","legacy"]

## 从固定配置或稳定排序候选选择实际特征；任何失败只返回诊断，输入保持原样。
static func create(actor_id: String, generation_seed: String, definition: Dictionary) -> Dictionary:
	if not _actor_id(actor_id): return _fail("InvalidAppearanceActor")
	var checked: Dictionary=validate_definition(definition)
	if not checked.ok: return checked
	if definition.kind=="fixed":
		return {"ok":true,"profile":_profile(definition.base_features,NO_SAMPLE,definition.attire_style,"fixed")}
	if not _seed(generation_seed): return _fail("InvalidAppearanceSeed")
	var features: Dictionary={}
	for feature in FEATURES:
		var choices: Array=definition.candidates[feature].duplicate()
		choices.sort()
		features[feature]=choices[_index([VERSION,generation_seed,actor_id,feature],choices.size())]
	return {"ok":true,"profile":_profile(features,generation_seed,definition.attire_style,"generated")}

## 在人物创建候选内准备一次档案；无新定义只留legacy，草案只能选择作者允许值且不修改任何输入。
static func prepare(actor_id: String, generation_seed: String, actor_definition: Dictionary, draft: Dictionary={}) -> Dictionary:
	if not _actor_id(actor_id): return _fail("InvalidAppearanceActor")
	if not actor_definition.has("appearance_definition"):
		return {"ok":true,"profile":legacy()} if draft.is_empty() else _fail("MissingAppearanceDefinition")
	var definition: Variant=actor_definition.appearance_definition
	if not definition is Dictionary: return _fail("InvalidAppearanceDefinition")
	var checked: Dictionary=validate_definition(definition)
	if not checked.ok: return checked
	if actor_definition.get("origin")=="historical" and definition.kind!="fixed": return _fail("HistoricalAppearanceMustBeFixed")
	if draft.is_empty(): return create(actor_id,generation_seed,definition)
	if not _actor_id(actor_id) or not _exact(draft,["base_features","attire_style"]) or not _features(draft.base_features) or draft.attire_style!=definition.attire_style: return _fail("InvalidAppearanceSelection")
	for feature in FEATURES:
		if definition.kind=="fixed" and draft.base_features[feature]!=definition.base_features[feature]: return _fail("InvalidAppearanceSelection")
		if definition.kind=="generated" and not draft.base_features[feature] in definition.candidates[feature]: return _fail("InvalidAppearanceSelection")
	return {"ok":true,"profile":_profile(draft.base_features,NO_SAMPLE,draft.attire_style,"custom")}

## 旧人物显式保留未记录特征；原portrait_kind继续由既有原型解析，不能从图片或新作者定义补脸。
static func legacy() -> Dictionary:
	return {"format_version":1,"generator_version":LEGACY_VERSION,"generation_seed":NO_SAMPLE,"base_features":{},"attire_style":"legacy","source":"legacy","parent_sources":[]}

## 校验已保存实际结果而不重跑候选或生成器；未知版本、混合来源和资源路径明确拒绝。
static func validate(profile: Dictionary) -> Dictionary:
	if not _exact(profile,PROFILE_FIELDS) or typeof(profile.format_version)!=TYPE_INT or profile.format_version!=1: return _fail("InvalidAppearanceProfile")
	if profile.source=="legacy": return {"ok":true} if profile==legacy() else _fail("InvalidLegacyAppearance")
	if profile.generator_version!=VERSION: return _fail("UnsupportedAppearanceGenerator")
	if not profile.source in ["fixed","custom","generated","inherited"] or not _features(profile.base_features) or not _token(profile.attire_style): return _fail("InvalidAppearanceProfile")
	if profile.source in ["fixed","custom"]:
		if profile.generation_seed!=NO_SAMPLE: return _fail("InvalidAppearanceSeed")
	elif not _seed(profile.generation_seed): return _fail("InvalidAppearanceSeed")
	if not profile.parent_sources is Array: return _fail("InvalidAppearanceParents")
	if profile.source!="inherited": return {"ok":true} if profile.parent_sources.is_empty() else _fail("InvalidAppearanceParents")
	if profile.parent_sources.is_empty() or profile.parent_sources.size()>FEATURES.size(): return _fail("InvalidAppearanceParents")
	var seen: Array=[]
	var parent_ids: Array=[]
	for entry in profile.parent_sources:
		if not entry is Dictionary or not entry.get("feature") in FEATURES or entry.feature in seen: return _fail("InvalidAppearanceParents")
		seen.append(entry.feature)
		if entry.get("source")=="fallback":
			if not _exact(entry,["feature","source"]): return _fail("InvalidAppearanceParents")
		elif entry.get("source")=="parent":
			if not _exact(entry,["feature","source","actor_id","value","generator_version","generation_seed"]): return _fail("InvalidAppearanceParents")
			if not _actor_id(entry.actor_id) or entry.value!=profile.base_features[entry.feature] or entry.generator_version!=VERSION or not _seed(entry.generation_seed): return _fail("InvalidAppearanceParents")
			if not entry.actor_id in parent_ids: parent_ids.append(entry.actor_id)
		else: return _fail("InvalidAppearanceParents")
	if parent_ids.size()>2: return _fail("InvalidAppearanceParents")
	return {"ok":true}

## 完整档案先沿原验证，再只复制五个公开外观字段；不输出或伪造seed、亲代与来源类别。
static func project_visible(profile: Variant) -> Dictionary:
	if not profile is Dictionary: return _fail("InvalidAppearanceProfile")
	# 仅挡住原比较会抛异常的两类来源类型；先保原结构首错，legacy仍由原canonical分支拒绝。
	if _exact(profile,PROFILE_FIELDS) and typeof(profile.format_version)==TYPE_INT and profile.format_version==1:
		if not profile.source is String: return _fail("InvalidAppearanceProfile")
		if profile.source!="legacy" and not profile.generator_version is String: return _fail("UnsupportedAppearanceGenerator")
	var checked: Dictionary=validate(profile)
	if not checked.ok: return checked
	return {"ok":true,"appearance_view":{"format_version":profile.format_version,"generator_version":profile.generator_version,"base_features":profile.base_features.duplicate(true),"attire_style":profile.attire_style,"legacy":profile.source=="legacy"}}

## 公开输入严格限于五字段与原语义类型；legacy形状不可和普通外貌混合，不接受私密附加字段。
static func validate_visible(view: Variant) -> Dictionary:
	if not view is Dictionary or not _exact(view,VISIBLE_FIELDS) or typeof(view.format_version)!=TYPE_INT or view.format_version!=1 or typeof(view.legacy)!=TYPE_BOOL: return _fail("InvalidAppearanceProfile")
	if view.legacy:
		return {"ok":true} if view=={"format_version":1,"generator_version":LEGACY_VERSION,"base_features":{},"attire_style":"legacy","legacy":true} else _fail("InvalidLegacyAppearance")
	if not view.generator_version is String or view.generator_version!=VERSION: return _fail("UnsupportedAppearanceGenerator")
	if not _features(view.base_features) or not _token(view.attire_style): return _fail("InvalidAppearanceProfile")
	return {"ok":true}

## 只接收至多两名已确认生物亲代的档案快照；未知/legacy按显式fallback记录，真实出生事务留给家族域。
static func inherit_features(parent_profiles: Array, generation_seed: String, definition: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definition(definition)
	if not checked.ok: return checked
	if definition.kind!="generated" or not definition.has("inheritance_features"): return _fail("MissingAppearanceInheritance")
	if parent_profiles.size()>2: return _fail("InvalidAppearanceParents")
	var parents: Dictionary={}
	for entry in parent_profiles:
		if not entry is Dictionary or not _exact(entry,["actor_id","appearance"]) or not _actor_id(entry.actor_id) or parents.has(entry.actor_id): return _fail("InvalidAppearanceParents")
		if entry.appearance!=null and (not entry.appearance is Dictionary or not validate(entry.appearance).ok): return _fail("InvalidAppearanceParents")
		parents[entry.actor_id]=entry.appearance
	# child seed由出生事务带永久人物/来源派生；此接口不分配ID，也不创建血缘事实。
	var made: Dictionary=create("inheritance",generation_seed,definition)
	if not made.ok: return made
	var profile: Dictionary=made.profile
	profile.source="inherited"
	var features: Array=definition.inheritance_features.duplicate(); features.sort()
	var ids: Array=parents.keys(); ids.sort()
	for feature in features:
		var choices: Array=[]
		for id in ids:
			var parent: Variant=parents[id]
			if parent is Dictionary and parent.source!="legacy" and parent.base_features[feature] in definition.candidates[feature]: choices.append(id)
		if choices.is_empty():
			profile.parent_sources.append({"feature":feature,"source":"fallback"})
		else:
			var id: String=choices[_index([VERSION,generation_seed,"inherit",feature],choices.size())]
			var parent: Dictionary=parents[id]
			profile.base_features[feature]=parent.base_features[feature]
			profile.parent_sources.append({"feature":feature,"source":"parent","actor_id":id,"value":parent.base_features[feature],"generator_version":parent.generator_version,"generation_seed":parent.generation_seed})
	return made

## 按已登记闭区间选择年龄层；未知年龄不猜生日，缺口/重叠拒绝，衣服和基础特征完全不参与计算。
static func age_band(age_years: int, variants: Array) -> Dictionary:
	if age_years < -1 or variants.size()>32: return _fail("InvalidAppearanceAge")
	var seen: Array=[]
	var ranges: Array=[]
	var selected: String=""
	for variant in variants:
		if not variant is Dictionary or not _age_variant_id(variant.get("id")) or variant.id in seen: return _fail("InvalidAppearanceAgeVariants")
		var ages: Variant=variant.get("ages")
		if not ages is Array or ages.size()!=2 or typeof(ages[0])!=TYPE_INT or typeof(ages[1])!=TYPE_INT or ages[0]<0 or ages[1]>1000 or ages[0]>ages[1]: return _fail("InvalidAppearanceAgeVariants")
		for previous in ranges:
			if ages[0]<=previous[1] and previous[0]<=ages[1]: return _fail("AmbiguousAppearanceAgeVariant")
		seen.append(variant.id); ranges.append(ages)
		if age_years>=ages[0] and age_years<=ages[1]: selected=variant.id
	if age_years==-1: return {"ok":true,"age_band":"unknown"}
	return {"ok":true,"age_band":selected} if not selected.is_empty() else _fail("MissingAppearanceAgeVariant")

## 与作者Schema保持同一受限合同；候选仅语义token，不允许路径、脚本、未知特征或隐式默认抽样。
static func validate_definition(definition: Dictionary) -> Dictionary:
	if definition.get("generator_version")!=VERSION: return _fail("UnsupportedAppearanceGenerator")
	if not _token(definition.get("attire_style")): return _fail("InvalidAppearanceDefinition")
	if definition.get("kind")=="fixed":
		return {"ok":true} if _exact(definition,["kind","generator_version","base_features","attire_style"]) and _features(definition.base_features) else _fail("InvalidAppearanceDefinition")
	if definition.get("kind")!="generated": return _fail("InvalidAppearanceDefinition")
	var fields: Array=["kind","generator_version","candidates","attire_style"]
	if definition.has("inheritance_features"): fields.append("inheritance_features")
	if not _exact(definition,fields) or not definition.candidates is Dictionary or not _exact(definition.candidates,FEATURES): return _fail("InvalidAppearanceDefinition")
	for feature in FEATURES:
		var choices: Variant=definition.candidates[feature]
		if not choices is Array or choices.is_empty() or choices.size()>64: return _fail("InvalidAppearanceDefinition")
		var seen: Array=[]
		for value in choices:
			if not _token(value) or value in seen: return _fail("InvalidAppearanceDefinition")
			seen.append(value)
	if definition.has("inheritance_features"):
		var inherited: Variant=definition.inheritance_features
		if not inherited is Array or inherited.is_empty() or inherited.size()>FEATURES.size(): return _fail("InvalidAppearanceDefinition")
		var seen: Array=[]
		for feature in inherited:
			if not feature in FEATURES or feature in seen: return _fail("InvalidAppearanceDefinition")
			seen.append(feature)
	return {"ok":true}

## 已选特征深复制为档案；调用方后来编辑草案不能改写已创建结果。
static func _profile(features: Dictionary, seed: String, attire: String, source: String) -> Dictionary:
	return {"format_version":1,"generator_version":VERSION,"generation_seed":seed,"base_features":features.duplicate(true),"attire_style":attire,"source":source,"parent_sources":[]}

## SHA256输入为规范紧凑JSON字符串数组；取前四字节无符号大端整数模候选数，禁用语言hash与玩法RNG。
static func _index(parts: Array, count: int) -> int:
	return JSON.stringify(parts,"",true,true).sha256_text().substr(0,8).hex_to_int()%count

## 实际特征必须恰好覆盖九个语义槽，拒绝数值捏脸或额外资源数据。
static func _features(value: Variant) -> bool:
	if not value is Dictionary or not _exact(value,FEATURES): return false
	for feature in FEATURES:
		if not _token(value[feature]): return false
	return true

## 严格限定字段集合；缺失与多余字段都不自动补修。
static func _exact(value: Dictionary, fields: Array) -> bool:
	if value.size()!=fields.size(): return false
	for field in fields:
		if not value.has(field): return false
	return true

## 语义名采用可移植小写ASCII，不能包含文件路径、冒号或控制字符。
static func _token(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>64: return false
	for index in range(value.length()):
		var c: int=value.unicode_at(index)
		if not (c>=97 and c<=122) and (index==0 or not (c>=48 and c<=57 or c in [45,46,95])): return false
	return true

## 年龄变体沿用V0.1表现ID的80字ASCII合同；独立于九槽语义的64字上限，拒绝路径与控制字符。
static func _age_variant_id(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>80: return false
	for index in range(value.length()):
		var c: int=value.unicode_at(index)
		if not (c>=97 and c<=122) and (index==0 or not (c>=48 and c<=57 or c in [45,46,95])): return false
	return true

## 接受既有作者ID和运行实例ID；永久ID从不使用显示姓名。
static func _actor_id(value: Variant) -> bool:
	if not value is String or value.is_empty(): return false
	if "::" in value:
		var qualified := RegEx.new()
		qualified.compile("^[a-z][a-z0-9_-]{0,31}::[a-z][a-z0-9_.-]{0,79}(?![\\s\\S])")
		return value.length()<=114 and qualified.search(value)!=null
	if value.length()>80: return false
	for index in range(value.length()):
		var c: int=value.unicode_at(index)
		if not (c>=97 and c<=122) and (index==0 or not (c>=48 and c<=57 or c in [45,46,58,95])): return false
	return true

## seed原样保存为有界UTF-8文本；拒绝空白/控制字符，不把坏类型强转成另一种种子。
static func _seed(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>256 or value!=value.strip_edges(): return false
	for index in range(value.length()):
		if value.unicode_at(index)<32 or value.unicode_at(index)==127: return false
	return true

## 返回可传播的诊断；调用者不能把失败字典当作实际档案写入人物。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
