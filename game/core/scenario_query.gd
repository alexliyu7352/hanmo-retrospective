## 从受信安装结果投影可启动剧本；只读查询，不读取路径、不初始化世界或消费RNG。
## 暂复用ContentSetResolver._installed内部纯辅助；统一公开安装校验入口时由中央迁移。
extends RefCounted
const ContentSetResolver=preload("res://core/content_set_resolver.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")

## installed必须来自已验证的ContentCatalog及安装sidecar；无效项不列出，诊断留给安装层。
static func list_playable(installed: Array) -> Array:
	var result: Array=[]
	for catalog in installed:
		if not ContentSetResolver._installed(catalog): continue
		if not catalog.get("catalog_path") is String or catalog.catalog_path.is_empty(): continue
		var data: Dictionary=catalog.data
		if not data.get("scenarios") is Dictionary or not data.get("actors") is Dictionary or not data.get("texts") is Dictionary: continue
		for id in data.scenarios:
			if not id is String: continue
			var entry: Dictionary=_entry(catalog,id)
			if not entry.is_empty(): result.append(entry)
	result.sort_custom(_before)
	return result.duplicate(true)

## 名称来自当前目录文本；新模式只借已登记Schema作快照检查，legacy保留原单一人物。
static func _entry(catalog: Dictionary, scenario_id: String) -> Dictionary:
	var data: Dictionary=catalog.data
	var scenario: Variant=data.scenarios[scenario_id]
	if not scenario is Dictionary or scenario.get("id")!=scenario_id: return {}
	for field in ["name_key","player_actor"]:
		if not scenario.get(field) is String or scenario[field].is_empty(): return {}
	var name: Variant=data.texts.get(scenario.name_key)
	if not name is String or name.is_empty(): return {}
	if not data.actors.get(scenario.player_actor) is Dictionary: return {}
	var playable: Array=[scenario.player_actor]
	var snapshot_mode: bool=false
	for field in Snapshot.SNAPSHOT_FIELDS:
		if scenario.has(field): snapshot_mode=true
	if snapshot_mode:
		var resolved: Dictionary=Snapshot.resolve(catalog,scenario_id)
		if not resolved.ok: return {}
		playable=resolved.catalog.data.scenarios[scenario_id].playable_actor_ids.duplicate(true)
	playable.sort()
	return {"pack_set_digest":catalog.pack_set_digest,"scenario_id":scenario_id,"name":name,"playable_actor_ids":playable,"catalog_path":catalog.catalog_path}

## 锁身份优先，随后剧本ID与受信路径；不以翻译名称或输入枚举决定顺序。
static func _before(left: Dictionary, right: Dictionary) -> bool:
	for key in ["pack_set_digest","scenario_id","catalog_path"]:
		if left[key]!=right[key]: return left[key]<right[key]
	return false
