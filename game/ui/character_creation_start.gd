## 标题自建流程只返回冻结选择，由宿主的新局事务一次创建世界。
extends VBoxContainer
signal confirmed(selection: Dictionary)
signal cancelled
const DraftPicker=preload("res://ui/character_creation_picker.gd")
const AppearancePicker=preload("res://ui/character_creation_appearance.gd")
const POLICY="created_character_v1"
var _epoch: int=0
var _closed: bool=true
var _page: Control
var _catalog: Dictionary={}
var _scenario_id: String=""
var _config: Dictionary={}
var _draft: Dictionary={}
var _selection: Dictionary={}
var _registry: RefCounted
var _status: Label

func configure(catalog: Dictionary, scenario_id: String, registry: RefCounted) -> Dictionary:
	close()
	if not catalog.get("ok",false) or not catalog.get("data") is Dictionary or not catalog.get("pack_set_digest") is String or catalog.pack_set_digest.is_empty(): return {"ok":false,"code":"InvalidCharacterCreationCatalog"}
	var scenario: Variant=catalog.data.get("scenarios",{}).get(scenario_id)
	if not scenario is Dictionary or not scenario.get("dynamic_world") is Dictionary or scenario.dynamic_world.get("founder_policy")!=POLICY or not scenario.get("character_creation") is Dictionary: return {"ok":false,"code":"CharacterCreationProfileRequired"}
	var config: Dictionary=scenario.character_creation
	if config.get("profile")!="point_buy_character_v1" or not config.get("allowed_modes") is Array or not config.get("appearance_choices") is Array: return {"ok":false,"code":"InvalidCharacterCreationCatalog"}
	var prototype: Variant=catalog.data.get("actors",{}).get(config.get("prototype_actor_id"))
	if not prototype is Dictionary or prototype.get("origin")!="original" or scenario.get("player_actor")!=config.get("prototype_actor_id") or config.prototype_actor_id in scenario.get("enabled",{}).get("actors",[]): return {"ok":false,"code":"CharacterCreationOriginalPrototypeRequired"}
	if scenario.has("skill_profile") and scenario.skill_profile!="full_sixteen_v1": return {"ok":false,"code":"UnsupportedSkillProfile"}
	for mode in config.allowed_modes:
		if not mode is String: return {"ok":false,"code":"InvalidCharacterCreationModes"}
	_catalog=catalog.duplicate(true); _scenario_id=scenario_id; _config=config.duplicate(true); _registry=registry; _draft={}; _selection={}; _closed=false
	return _show_draft()

func selected() -> Dictionary:
	return _selection.duplicate(true)

func close() -> void:
	_closed=true; _epoch+=1
	if is_instance_valid(_page) and _page.has_method("close"): _page.close()

static func _live(reference: WeakRef, epoch: int):
	var owner=reference.get_ref()
	if not is_instance_valid(owner) or owner.is_queued_for_deletion() or owner._closed or owner._epoch!=epoch or not owner.is_inside_tree(): return null
	return owner

static func _draft_confirmed(draft: Dictionary, reference: WeakRef, epoch: int) -> void:
	var owner=_live(reference,epoch)
	if owner==null: return
	owner._draft=draft.duplicate(true); owner._show_appearance()

static func _cancelled(reference: WeakRef, epoch: int, back: bool) -> void:
	var owner=_live(reference,epoch)
	if owner==null: return
	if back: owner._show_draft(); return
	owner.close(); owner.cancelled.emit()

static func _appearance_confirmed(choice_id: String, reference: WeakRef, epoch: int) -> void:
	var owner=_live(reference,epoch)
	if owner==null: return
	var result={"scenario_id":owner._scenario_id,"pack_set_digest":owner._catalog.pack_set_digest,"character_creation_request":{"draft":owner._draft.duplicate(true),"appearance_choice_id":choice_id}}
	owner._selection=result.duplicate(true); owner.close(); owner.confirmed.emit(result)

func _clear_page() -> void:
	_epoch+=1
	if is_instance_valid(_page) and _page.has_method("close"): _page.close()
	for child in get_children(): remove_child(child); child.queue_free()
	_page=null
	var place: Dictionary=_catalog.data.get("places",{}).get(_config.starting_place_id,{})
	var place_name: String=str(_catalog.data.get("texts",{}).get(place.get("name_key",""),_config.starting_place_id))
	var note=Label.new(); note.text="启程地点："+place_name+" · 盘缠："+str(_config.initial_money); add_child(note)
	var goods: PackedStringArray=[]
	for entry in _config.initial_goods:
		var item: Dictionary=_catalog.data.get("items",{}).get(entry.item_id,{})
		goods.append(str(_catalog.data.get("texts",{}).get(item.get("name_key",""),entry.item_id))+" ×"+str(entry.quantity))
	var initial_goods=Label.new(); initial_goods.text="行囊："+("、".join(goods) if not goods.is_empty() else "无"); initial_goods.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(initial_goods)
	_status=Label.new(); _status.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(_status)

func _show_draft() -> Dictionary:
	_clear_page(); var picker=DraftPicker.new(); _page=picker; add_child(picker)
	var modes: Array[String]=[]
	for mode in _config.allowed_modes: modes.append(mode)
	var result: Dictionary=picker.configure(_catalog.data.scenarios[_scenario_id].get("skill_profile")=="full_sixteen_v1",modes,_draft)
	if not result.get("ok",false): _status.text="人物创建不可用："+str(result.get("code","")); return result
	picker.confirmed.connect(_draft_confirmed.bind(weakref(self),_epoch)); picker.cancelled.connect(_cancelled.bind(weakref(self),_epoch,false)); return result

func _show_appearance() -> void:
	_clear_page(); var picker=AppearancePicker.new(); _page=picker; add_child(picker)
	var result: Dictionary=picker.configure(_catalog.data.actors[_config.prototype_actor_id],_draft,_config.appearance_choices,_registry)
	if not result.get("ok",false):
		_status.text="外貌选择不可用："+str(result.get("code",""))
		var back=Button.new(); back.text="返回修改人物"; add_child(back); back.pressed.connect(_cancelled.bind(weakref(self),_epoch,true)); return
	picker.confirmed.connect(_appearance_confirmed.bind(weakref(self),_epoch)); picker.cancelled.connect(_cancelled.bind(weakref(self),_epoch,true))
