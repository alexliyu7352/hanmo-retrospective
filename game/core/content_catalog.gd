## 装载并校验准备后的作者内容；不建立游戏实例或接受旧硬编码夹具。
extends RefCounted
const CapabilityContract = preload("res://core/runtime/capability_contract.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Installed=preload("res://core/installed_content.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict = preload("res://core/strict_json.gd")
const Shape = preload("res://core/content_shape.gd")
const Personal = preload("res://core/personal_content.gd")
const BargainContent=preload("res://core/domains/economy/bargain_content.gd")
const StudyContent=preload("res://core/domains/cultivation/study_content.gd")
const Military=preload("res://core/military_content.gd")
const World=preload("res://core/world_content.gd")
const TABLES: Array[String] = ["settlements","routes","places","actors","organizations","roles","wallets","items","inventories","shops","props","offers","dialogues","events","relations","scenarios","personal_skills","personal_battlefields","personal_objectives","world_maps","estates","etiquette_challenges","bargain_challenges","drill_challenges","cards","actor_cards","legacy_control_consent_requests","succession_consent_requests","adult_adoption_requests","teaching_requests","study_teaching_requests","insight_cards"]+Military.TABLES
const OPTIONAL_TABLES: Array[String] = ["personal_skills","personal_battlefields","personal_objectives","world_maps","estates","etiquette_challenges","bargain_challenges","drill_challenges","cards","actor_cards","legacy_control_consent_requests","succession_consent_requests","adult_adoption_requests","teaching_requests","study_teaching_requests","insight_cards"]+Military.TABLES

## 使用匹配Schema校验运行目录；AUTHORING_ONLY标记不等于已经验证可玩。
static func load_checked(index_text: String) -> Dictionary:
	return _load_checked(index_text)

## 仅安装信任链可以传入已核原字节的Schema；公开文本入口仍使用发行受信索引。
static func _load_checked(index_text: String, installed_schema: String="") -> Dictionary:
	if index_text.to_utf8_buffer().size() > 4*1024*1024:
		return {"ok":false,"code":"ContentSizeLimit"}
	var decoded: Dictionary = Strict.parse_text(index_text)
	if not decoded.ok or not decoded.value is Dictionary:
		return {"ok":false,"code":"InvalidContentIndex"}
	var header: Dictionary = decoded.value
	if header.get("format") != "HM-CONTENT-INDEX" or header.get("format_version") != 1 or header.get("content_abi") != 1:
		return {"ok":false,"code":"UnsupportedContentVersion"}
	for key in ["catalog_sha256","schema_sha256","rule_capabilities_sha256","content_id","content_version"]:
		if not header.get(key) is String:
			return {"ok":false,"code":"InvalidContentHeader","field":key}
	if not header.get("catalog") is Dictionary:
		return {"ok":false,"code":"InvalidContentCatalog"}
	if header.has("pack_set_digest") and installed_schema.is_empty(): return {"ok":false,"code":"ContentSetNotInstalled"}
	var data: Dictionary = header.catalog
	for derived in ["definitions","active_entity_ids","active_static_actor_ids"]:
		if data.has(derived): return {"ok":false,"code":"InvalidContentCatalog"}
	# Python准备器生成排序紧凑UTF-8 JSON及一个换行；ABI1数值必须为整数。
	var canonical: String = JSON.stringify(data,"",true,true)+"\n"
	if canonical.sha256_text() != header.catalog_sha256:
		return {"ok":false,"code":"ContentDigestMismatch"}
	if CapabilityContract.digest() != header.rule_capabilities_sha256:
		return {"ok":false,"code":"ContentCapabilityContractMismatch"}
	var schema_result: Dictionary = _schema_checked(header.schema_sha256) if installed_schema.is_empty() else _schema_text(installed_schema,header.schema_sha256)
	if not schema_result.ok: return schema_result
	var schema: Dictionary = schema_result.schema
	var authored: Dictionary = {"format":"HM-CONTENT","schema_version":1,"id":header.content_id,"version":header.content_version}
	for table in TABLES:
		if not data.has(table) and table in OPTIONAL_TABLES: continue
		if not data.get(table) is Dictionary:
			return {"ok":false,"code":"MissingContentTable","field":table}
		authored[table] = []
		for id in data[table]:
			if not data[table][id] is Dictionary or data[table][id].get("id") != id:
				return {"ok":false,"code":"ContentIndexIdentity","field":table}
			authored[table].append(data[table][id])
	for key in ["texts","flags","required_capabilities"]:
		if not data.has(key):
			return {"ok":false,"code":"MissingContentField","field":key}
		authored[key] = data[key]
	var shape: Dictionary = Shape.validate(authored,schema,schema,data)
	if not shape.ok:
		return shape
	for table in OPTIONAL_TABLES:
		# 新卡片表保持显式存在语义，旧目录不增加空表或改变其身份摘要。
		if table in ["cards","actor_cards","legacy_control_consent_requests","succession_consent_requests","adult_adoption_requests","teaching_requests","study_teaching_requests","insight_cards"]: continue
		if not data.has(table): data[table]={}
	var supported: Dictionary = CapabilityContract.validate(data)
	if not supported.ok:
		return supported
	var catalog: Dictionary={"ok":true,"data":data.duplicate(true),"content_id":header.content_id,"content_version":header.content_version,"content_digest":header.catalog_sha256,"schema_sha256":header.schema_sha256}
	var has_snapshot: bool=false
	for scenario in data.scenarios.values():
		for field in Snapshot.SNAPSHOT_FIELDS:
			if scenario.has(field): has_snapshot=true
	if has_snapshot:
		for id in data.scenarios:
			var resolved: Dictionary=Snapshot.resolve(catalog,id)
			if not resolved.ok: return resolved
			var semantics: Dictionary=_links(resolved.catalog.data)
			if not semantics.ok: return semantics
	else:
		var semantics: Dictionary=_links(data)
		if not semantics.ok: return semantics
	return catalog

## 只从受信生成索引选取确切Schema；无索引的旧入口仅兼容当前alias的原字节摘要。
static func _schema_checked(sha: String) -> Dictionary:
	var base: String = "res://data/generated/"
	var index_path: String = base+"schema-index.json"
	var path: String = base+"content.schema.json"
	if FileAccess.file_exists(index_path):
		var decoded: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(index_path))
		if not decoded.ok or not decoded.value is Dictionary: return {"ok":false,"code":"InvalidContentSchemaIndex"}
		var index: Dictionary = decoded.value
		if index.size()!=2 or index.get("format")!="HM-CONTENT-SCHEMA-INDEX/1" or not index.get("schemas") is Dictionary or index.schemas.is_empty():
			return {"ok":false,"code":"InvalidContentSchemaIndex"}
		var digest_pattern := RegEx.new()
		digest_pattern.compile("^[0-9a-f]{64}$")
		for digest in index.schemas:
			if digest.length()!=64 or digest_pattern.search(digest)==null or index.schemas[digest]!="schemas/"+digest+".json":
				return {"ok":false,"code":"InvalidContentSchemaIndex"}
		if not index.schemas.has(sha): return {"ok":false,"code":"ContentSchemaMismatch"}
		path=base+index.schemas[sha]
	if not FileAccess.file_exists(path): return {"ok":false,"code":"ContentSchemaMissing"}
	return _schema_text(FileAccess.get_file_as_string(path),sha)

## 原字节摘要核对后才解析和登记轻量引用缓存，不把任意包Schema当运行合同。
static func _schema_text(text: String, sha: String) -> Dictionary:
	if text.sha256_text()!=sha: return {"ok":false,"code":"ContentSchemaMismatch"}
	var parsed: Dictionary = Strict.parse_text(text)
	if not parsed.ok or not parsed.value is Dictionary or parsed.value.is_empty():
		return {"ok":false,"code":"InvalidContentSchema"}
	var registered: Dictionary=Snapshot.register_schema(text)
	if not registered.ok: return registered
	return {"ok":true,"schema":parsed.value}

## 读取已准备目录；开发阶段仍由唯一作者源文件生成。
static func read_prepared(path: String = "res://data/generated/two_stories.hmcatalog.json") -> Dictionary:
	if not FileAccess.file_exists(path):
		return {"ok":false,"code":"MissingContent","path":path}
	var text: String=FileAccess.get_file_as_string(path)
	var decoded: Dictionary=Strict.parse_text(text)
	var set_root: String=ProjectSettings.globalize_path(Installed.BASE+"content-sets/")
	if ProjectSettings.globalize_path(path).simplify_path().begins_with(set_root) or (decoded.ok and decoded.value is Dictionary and decoded.value.has("pack_set_digest")):
		var artifacts: Dictionary=Installed.read(path)
		if not artifacts.ok: return artifacts
		var loaded: Dictionary=_load_checked(artifacts.index_text,artifacts.schema_text)
		if not loaded.ok: return loaded
		for field in ["pack_set","pack_set_digest","catalog_path","presentation_path"]: loaded[field]=artifacts[field]
		return loaded
	return load_checked(text)

## 枚举受信索引内全部已核目录；任何坏安装项显式失败，不暗选另一个内容集。
static func installed_catalogs() -> Dictionary:
	if not FileAccess.file_exists(Installed.INDEX): return {"ok":true,"catalogs":[]}
	var listed: Dictionary=Installed.index()
	if not listed.ok: return listed
	var catalogs: Array=[]
	for entry in listed.sets:
		var loaded: Dictionary=read_prepared(Installed.BASE+entry.directory+"/catalog.hmcatalog.json")
		if not loaded.ok: return loaded
		catalogs.append(loaded)
	return {"ok":true,"catalogs":catalogs}

## 复核运行必需的跨记录授权；作者校验另外提供更丰富的定位信息。
static func _links(data: Dictionary) -> Dictionary:
	var military: Dictionary=Military.validate(data)
	if not military.ok: return military
	var world: Dictionary=World.validate(data)
	if not world.ok: return world
	for actor in data.actors.values():
		if actor.has("appearance_definition"):
			var appearance: Dictionary=Appearance.validate_definition(actor.appearance_definition)
			if not appearance.ok: return appearance
			if actor.origin=="historical" and actor.appearance_definition.kind!="fixed": return {"ok":false,"code":"HistoricalAppearanceMustBeFixed"}
		if actor.has("social") and (actor.social.target_actor_id==actor.id or data.items[actor.social.food_item_id].kind!="commodity"): return {"ok":false,"code":"InvalidSocialPolicy"}
	for place in data.places.values():
		for passage in place.passages:
			if data.places[passage].settlement_id != place.settlement_id:
				return {"ok":false,"code":"CrossSettlementPassage"}
		var chain: Array = [place.id]
		var parent: Variant = place.parent_place
		while parent != null:
			if parent in chain:
				return {"ok":false,"code":"PlaceParentCycle"}
			chain.append(parent)
			parent = data.places[parent].parent_place
	for settlement in data.settlements.values():
		if data.places[settlement.entry_place].settlement_id != settlement.id:
			return {"ok":false,"code":"WrongSettlementEntry"}
	for role in data.roles.values():
		if role.has("dialogue_id") and data.dialogues[role.dialogue_id].context_role!=role.id:
			return {"ok":false,"code":"RoleDialogueContextMismatch"}
		if data.actors[role.initial_actor].initial_place != role.place_id:
			return {"ok":false,"code":"InitialRoleAbsent"}
	for role in data.roles.values():
		for kind in role.get("activities",{}):
			var terms: Dictionary=role.activities[kind]
			if not kind in role.services or not _represents(role,data.wallets[terms.wallet_id].owner):
				return {"ok":false,"code":"ServiceAuthorityMismatch"}
	for shop in data.shops.values():
		var owner: Dictionary = data.wallets[shop.wallet_id].owner
		if owner != data.inventories[shop.inventory_id].owner:
			return {"ok":false,"code":"ShopOwnershipMismatch"}
		for role_id in shop.representative_roles:
			var role: Dictionary = data.roles[role_id]
			if role.place_id != shop.place_id or not "trade" in role.services or not _represents(role,owner):
				return {"ok":false,"code":"ShopAuthorityMismatch"}
	for offer in data.offers.values():
		if data.wallets[offer.funding_wallet].owner != offer.issuer:
			return {"ok":false,"code":"FundingAuthorityMismatch"}
		for kind in ["accept","report"]:
			var role: Dictionary = data.roles[offer[kind+"_role"]]
			if not kind+"_offer" in role.services or not _represents(role,offer.issuer):
				return {"ok":false,"code":"OfferAuthorityMismatch"}
		if offer.template == "procurement" and (offer.goal.kind != "deliver_item" or not _represents(data.roles[offer.goal.receiver_role],offer.issuer)):
			return {"ok":false,"code":"ReceiverAuthorityMismatch"}
		if offer.template == "message" and offer.goal.kind != "deliver_message":
			return {"ok":false,"code":"GoalTemplateMismatch"}
		if offer.template == "escort":
			if offer.goal.kind!="escort" or data.personal_objectives[offer.goal.objective_id].kind!="escort": return {"ok":false,"code":"EscortObjectiveMismatch"}
			if data.actors[offer.goal.actor_id].initial_place!=data.roles[offer.accept_role].place_id: return {"ok":false,"code":"EscortPersonAbsent"}
			var invited: bool=false
			for role in data.roles.values():
				if role.initial_actor==offer.goal.actor_id and "join_party" in role.services: invited=true
			if not invited: return {"ok":false,"code":"EscortCompanionMissing"}
	for dialogue in data.dialogues.values():
		var nodes: Dictionary = {}
		for node in dialogue.nodes:
			if nodes.has(node.id):
				return {"ok":false,"code":"DuplicateDialogueNode"}
			nodes[node.id] = node
		if not nodes.has(dialogue.entry):
			return {"ok":false,"code":"DialogueEntryMissing"}
		for node in nodes.values():
			var targets: Array = []
			for key in ["next","if_true","if_false","on_success","on_failure"]:
				if node.has(key):
					targets.append(node[key])
			for option in node.get("options",[]):
				targets.append(option.next)
			for target in targets:
				if not nodes.has(target):
					return {"ok":false,"code":"DialogueTargetMissing"}
	var challenge: Dictionary=StudyContent.validate(data)
	if not challenge.ok: return challenge
	challenge=BargainContent.validate(data)
	if not challenge.ok: return challenge
	var personal: Dictionary = Personal.validate(data)
	return _mobility_links(data) if personal.ok else personal

## 区分公共岗位代理与私人自身授权，不能混用资产归属。
static func _represents(role: Dictionary, owner: Dictionary) -> bool:
	return (role.organization_id == owner.id) if owner.kind == "organization" else (role.organization_id == null and role.initial_actor == owner.id)

## 运行装载时复核路上对白边界；不能只依赖编辑器曾经检查过。
static func _mobility_links(data: Dictionary) -> Dictionary:
	for role in data.roles.values():
		if role.has("companion_cost_per_tick") and not "join_party" in role.services: return {"ok":false,"code":"CompanionAuthority"}
	for actor in data.actors.values():
		if data.has("active_static_actor_ids") and not actor.id in data.active_static_actor_ids: continue
		if actor.has("travel_plan"):
			for scenario in data.scenarios.values():
				if scenario.player_actor==actor.id: return {"ok":false,"code":"PlayerAutoPlan"}
		if not actor.has("road_dialogue_id"): continue
		var dialogue: Dictionary=data.dialogues[actor.road_dialogue_id]
		if data.roles[dialogue.context_role].initial_actor!=actor.id: return {"ok":false,"code":"RoadDialogueOwner"}
		for node in dialogue.nodes:
			if node.kind=="action": return {"ok":false,"code":"RoadDialogueAction"}
			if node.kind=="say" and not node.speaker in ["$provider","$player"]: return {"ok":false,"code":"RoadDialogueSpeaker"}
	return {"ok":true}
