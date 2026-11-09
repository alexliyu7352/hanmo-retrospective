## 从已核剧本初态选择已有成年人；仅冻结人物ID与作者政策，不创建世界、消耗随机或写存档。
extends VBoxContainer
signal confirmed(selection: Dictionary)
signal cancelled
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Character=preload("res://ui/character_presentation.gd")
const Portrait=preload("res://ui/layered_portrait.gd")
const UnlockStart=preload("res://ui/actor_unlock_start.gd")
const POLICY="selected_existing_actor_v1"
const ROSTER_POLICY="selected_existing_actor_roster_v1"
const COMPOSITION="fixed_actor_appearance_composition_v1"
const PRESERVE="preserve_existing_v1"
const ATTRIBUTES={"leadership":"统率","martial":"武艺","intellect":"智识","administration":"政务","charisma":"风仪"}
const SKILLS={"infantry":"步战","cavalry":"骑战","archery":"弓术","martial":"武学","tactics":"谋略","debate":"辩论","negotiation":"交涉","rites":"礼法","commerce":"商事","transport":"转运","naval":"水战","espionage":"谍报","fortification":"筑城","agriculture":"农政","medicine":"医术","forging":"锻造"}
var _epoch: int=0
var _closed: bool=true
var _selection: Dictionary={}
var _actors: Dictionary={}
var _buttons: Dictionary={}
var _scenario_id: String=""
var _pack_set_digest: String=""
var _confirm: Button

## 所有候选按各自ScenarioSnapshot和原表现绑定预备成功后才显示，坏末项不会留下半份人物列表。
func configure(catalog: Dictionary, scenario_id: String, registry: RefCounted, unlock_profile_path: String="user://profiles/actor_unlock_v1.json") -> Dictionary:
	_epoch+=1; _clear()
	if not catalog.get("ok",false) or not catalog.get("data") is Dictionary or not catalog.get("pack_set_digest") is String or catalog.pack_set_digest.is_empty(): return _fail("InvalidExistingActorCatalog")
	if not catalog.data.get("scenarios") is Dictionary: return _fail("InvalidExistingActorCatalog")
	var scenario: Variant=catalog.data.scenarios.get(scenario_id)
	if not scenario is Dictionary or not scenario.get("dynamic_world") is Dictionary or not scenario.dynamic_world.get("founder_policy") in [POLICY,ROSTER_POLICY]: return _fail("UnsupportedFounderPolicy")
	var roster: bool=scenario.dynamic_world.founder_policy==ROSTER_POLICY
	var eligibility: Dictionary=UnlockStart.resolve(catalog.data,scenario,unlock_profile_path) if scenario.has("actor_card_unlocks") else {"ok":true,"actors":{}}
	if not eligibility.get("ok",false): return eligibility
	var ids: Variant=scenario.get("playable_actor_ids")
	var authored_policies: Variant=scenario.get("playable_actor_appearance_policies")
	if not ids is Array or ids.is_empty() or not authored_policies is Array or authored_policies.size()!=ids.size(): return _fail("InvalidPlayableActorPolicies")
	var policies: Dictionary={}
	for entry in authored_policies:
		if not entry is Dictionary or entry.size()!=2 or not entry.get("actor_id") is String or not entry.get("actor_id") in ids or policies.has(entry.actor_id) or not entry.get("policy") in [COMPOSITION,PRESERVE]: return _fail("InvalidPlayableActorPolicies")
		policies[entry.actor_id]=entry.policy
	var rows: Dictionary={}; var portraits: Dictionary={}
	for actor_id in ids:
		if not actor_id is String or actor_id.is_empty() or rows.has(actor_id) or not policies.get(actor_id) in [COMPOSITION,PRESERVE]:
			_free_portraits(portraits); return _fail("InvalidPlayableActorPolicies")
		var resolved: Dictionary=Snapshot.resolve(catalog,scenario_id,actor_id)
		if not resolved.get("ok",false): _free_portraits(portraits); return resolved
		var definition: Dictionary=resolved.catalog.data.actors[actor_id]
		if typeof(definition.get("initial_age_years"))!=TYPE_INT or definition.initial_age_years<18 or definition.initial_age_years>(120 if roster else 59):
			_free_portraits(portraits); return _fail("UnsupportedAppearanceAge")
		var authored: Variant=definition.get("appearance_definition")
		if not authored is Dictionary or authored.get("kind")!="fixed":
			_free_portraits(portraits); return _fail("ExistingActorAppearanceMustBeFixed")
		var made: Dictionary=Appearance.create(actor_id,Appearance.NO_SAMPLE,authored)
		if not made.ok: _free_portraits(portraits); return made
		var name: String=_name(resolved.catalog.data,definition,actor_id)
		var actor_view: Dictionary={"actor_id":actor_id,"display_name":name,"age_years":definition.initial_age_years,"portrait_kind":definition.get("portrait_kind",""),"presentation_profile_id":definition.get("presentation_profile_id","")}
		var descriptor: Dictionary=Character.resolve(actor_id,actor_view,made.profile,"avatar",{"resolution":[96,96]},registry)
		# 旧非regional投影可能以空图层成功降级；新名单不能把透明空图冒称本人肖像。
		if roster and descriptor.get("ok",false) and descriptor.get("layers",[]).is_empty() and not descriptor.has("adult_composition"):
			descriptor={"ok":false,"code":"MissingRenderTexture"}
		var portrait: Control
		if not descriptor.get("ok",false):
			if not roster or not _portrait_unavailable(descriptor): _free_portraits(portraits); return descriptor
			portrait=_text_portrait(actor_id,name)
			descriptor={"degradation":[{"code":"IdentityTextOnly","reason":descriptor.get("code","")}]}
		else:
			portrait=Portrait.new()
			var rendered: Dictionary=portrait.configure(descriptor,registry)
			if not rendered.ok:
				portrait.free()
				if not roster or not _portrait_unavailable(rendered): _free_portraits(portraits); return rendered
				portrait=_text_portrait(actor_id,name)
				descriptor={"degradation":[{"code":"IdentityTextOnly","reason":rendered.get("code","")}]}
		portraits[actor_id]=portrait
		var place_id: String=definition.initial_place
		var roles: Array=[]
		for role_row in resolved.initial.get("roles",[]):
			if role_row.actor_id==actor_id and role_row.enabled:
				var role: Dictionary=resolved.catalog.data.roles[role_row.role_id]
				roles.append(_name(resolved.catalog.data,role,role.id))
		rows[actor_id]={"actor_id":actor_id,"display_name":name,"age_years":definition.initial_age_years,"origin":definition.get("origin",""),"place_id":place_id,"place_name":_name(resolved.catalog.data,resolved.catalog.data.places[place_id],place_id),"roles":roles,"attributes":definition.attributes.duplicate(true),"skills":definition.skills.duplicate(true),"appearance_policy":policies[actor_id],"presentation_profile_id":definition.get("presentation_profile_id",""),"portrait_kind":definition.get("portrait_kind",""),"appearance":made.profile.duplicate(true),"portrait_degradation":descriptor.get("degradation",[]).duplicate(true),"start_description":_start_description(resolved.catalog.data,scenario,actor_id),"initial_goods":definition.get("initial_goods",[]).duplicate(true),"goods_text":_goods_text(resolved.catalog.data,definition.get("initial_goods",[])),"personal_balance":_personal_balance(resolved.catalog.data,actor_id)}
	for actor_id in rows:
		if eligibility.actors.has(actor_id): rows[actor_id]["collection"]=eligibility.actors[actor_id].duplicate(true)
	_actors=rows; _scenario_id=scenario_id; _pack_set_digest=catalog.pack_set_digest
	_build(ids,portraits); _closed=false
	return {"ok":true,"count":ids.size()}

## 新名单缺少对应美术时只降为真实姓名文本；结构损坏、陈旧描述和非法资源仍明确失败。
func _portrait_unavailable(result: Dictionary) -> bool:
	return result.get("code","") in ["RegistryEmpty","MissingCharacterProfile","MissingRenderTexture","MissingRegionalAppearanceResource","UnsupportedAppearanceAge","MissingAppearanceAttire","MissingCharacterOverride","MissingAdultAppearanceCatalog","MissingAdultCompositionPreset","MissingAdultCompositionRegistry","MissingAdultCompositionResource","UnmappedAdultAppearance"]

## 文本肖像保留本人稳定ID和姓名，既不使用他人头像，也不重新生成出生或外貌档案。
func _text_portrait(actor_id: String, display_name: String) -> Control:
	var label=Label.new(); label.text=display_name+"\n肖像待补"; label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
	label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; label.set_meta("identity_text_actor_id",actor_id)
	return label

## 名称优先本章真实文本；缺少文案时显示稳定ID，不编造职业或人物身份。
func _name(data: Dictionary, definition: Dictionary, fallback: String) -> String:
	return str(data.get("texts",{}).get(definition.get("name_key",""),definition.get("display_name",fallback)))

## 作者启程文案按稳定人物ID查本章文本，不从姓名或头像推断身份经历。
func _start_description(data: Dictionary, scenario: Dictionary, actor_id: String) -> String:
	for entry in scenario.get("player_start_descriptions",[]):
		if entry is Dictionary and entry.get("actor_id")==actor_id: return str(data.get("texts",{}).get(entry.get("text_key",""),""))
	return ""

## 私钱沿原生固定人物选钱包顺序读取；庄园钱包不冒充随身盘缠，不生成新钱包。
func _personal_balance(data: Dictionary, actor_id: String) -> int:
	var estate_wallets: Array=[]
	for estate in data.get("estates",{}).values(): estate_wallets.append(estate.get("wallet_id",""))
	var ids: Array=data.get("wallets",{}).keys(); ids.sort()
	for id in ids:
		var wallet: Dictionary=data.wallets[id]
		if not id in estate_wallets and wallet.get("owner",{}).get("kind")=="actor" and wallet.owner.get("id")==actor_id: return int(wallet.initial_balance)
	return 0

## 随身物品只来自本章投影后的initial_goods，不能取人物基表另一年代的行囊。
func _goods_text(data: Dictionary, goods: Array) -> String:
	var words: PackedStringArray=[]
	for row in goods: words.append(_name(data,data.get("items",{}).get(row.item_id,{}),row.item_id)+" ×"+str(row.quantity))
	return "、".join(words) if not words.is_empty() else "无"

## 预备失败只回收本控件尚未入树的头像，不改注册表或源素材。
func _free_portraits(portraits: Dictionary) -> void:
	for portrait in portraits.values(): portrait.free()

## 配置失效时清除整代候选，原节点虽等待帧末销毁也不能提交新一代人物。
func _clear() -> void:
	_closed=true; _selection={}; _actors={}; _buttons={}; _scenario_id=""; _pack_set_digest=""; _confirm=null
	for child in get_children(): remove_child(child); child.queue_free()

## 只提供实际快照字段的深副本；调用者不能借详情修改作者初态或所选人物。
func actors() -> Array:
	return _actors.values().duplicate(true)

## 未明确点击人物时始终为空；宿主只接收冻结的稳定ID与政策，不接收新的世界初态。
func selected() -> Dictionary:
	return _selection.duplicate(true)

## 实际头像、年龄、初始地点与能力来自同一人物投影；保留每人的原表现绑定。
func _build(ids: Array, portraits: Dictionary) -> void:
	name="ExistingActorStartPicker"
	var title=Label.new(); title.text="选择启程人物"; title.add_theme_font_size_override("font_size",24); add_child(title)
	var note=Label.new(); note.text="沿用本章已有人物的年龄、身份和能力。请先选择一人，再确认启程。"; note.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(note)
	for actor_id in ids:
		var actor: Dictionary=_actors[actor_id]
		var card=HBoxContainer.new(); card.set_meta("existing_actor_id",actor_id); add_child(card)
		var portrait: Control=portraits[actor_id]; portrait.name="ExistingActorAvatar"; portrait.custom_minimum_size=Vector2(96,96); card.add_child(portrait)
		var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; card.add_child(body)
		var button=Button.new(); button.text=actor.display_name+" · "+str(actor.age_years)+"岁"; button.toggle_mode=true; button.set_meta("existing_actor_action","select"); button.set_meta("player_actor_id",actor_id)
		button.pressed.connect(_select.bind(actor_id,_epoch)); body.add_child(button); _buttons[actor_id]=button
		if actor.has("collection"):
			button.disabled=not actor.collection.available
			var qualification=Label.new(); qualification.name="ActorCollectionStatus"; qualification.text=actor.collection.reason; qualification.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(qualification)
		var identity=Label.new(); identity.text="本章初始地点："+actor.place_name
		if not actor.roles.is_empty(): identity.text+="\n职事："+"、".join(actor.roles)
		identity.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(identity)
		var supplies=Label.new(); supplies.text="随身钱："+str(actor.personal_balance)+"钱 · 行囊："+actor.goods_text; supplies.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(supplies)
		if not actor.start_description.is_empty():
			var description=Label.new(); description.text=actor.start_description; description.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(description)
		var attributes=Label.new(); attributes.text=_fields(actor.attributes,ATTRIBUTES); attributes.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(attributes)
		var skills=Label.new(); skills.text="技能经验："+_fields(actor.skills,SKILLS); skills.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(skills)
		var appearance=Label.new(); appearance.text="确认人物后可调整外貌。" if actor.appearance_policy==COMPOSITION else "沿用此人的原有外貌。"
		if not actor.portrait_degradation.is_empty():
			appearance.text+=" 当前以本人姓名代替肖像。" if actor.portrait_degradation[0].get("code")=="IdentityTextOnly" else " 当前显示既有简化肖像。"
		appearance.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; body.add_child(appearance)
	var actions=HFlowContainer.new(); add_child(actions)
	_confirm=Button.new(); _confirm.text="确认人物"; _confirm.disabled=true; _confirm.set_meta("existing_actor_action","confirm"); _confirm.pressed.connect(confirm_selection.bind(_epoch)); actions.add_child(_confirm)
	var cancel=Button.new(); cancel.text="返回章节"; cancel.set_meta("existing_actor_action","cancel"); cancel.pressed.connect(cancel_selection.bind(_epoch)); actions.add_child(cancel)

## 数字按原快照展示，不把技能经验推算成未定义等级或战力。
func _fields(values: Dictionary, labels: Dictionary) -> String:
	var words: PackedStringArray=[]
	for key in labels:
		if values.has(key): words.append(labels[key]+" "+str(values[key]))
	return " · ".join(words)

## 每次明确点击只改变本地选中项；未知ID、已关闭窗口和过期按钮完全无效。
func _select(actor_id: String, epoch: int) -> void:
	if _closed or epoch!=_epoch or not _actors.has(actor_id) or not _actors[actor_id].get("collection",{}).get("available",true): return
	_selection={"scenario_id":_scenario_id,"pack_set_digest":_pack_set_digest,"actor_id":actor_id,"appearance_policy":_actors[actor_id].appearance_policy}
	for id in _buttons: _buttons[id].set_pressed_no_signal(id==actor_id)
	_confirm.disabled=false

## 一次性发出冻结选择深副本；不默认选择、修改外貌或替宿主调用原生创建。
func confirm_selection(epoch: int=-1) -> void:
	if _closed or (epoch!=-1 and epoch!=_epoch) or _selection.is_empty(): return
	if not _actors.get(_selection.actor_id,{}).get("collection",{}).get("available",true): return
	_closed=true; _confirm.disabled=true; confirmed.emit(_selection.duplicate(true))

## 取消只丢弃本地人物选择，不发空人物请求，不触碰任何银行。
func cancel_selection(epoch: int=-1) -> void:
	if _closed or (epoch!=-1 and epoch!=_epoch): return
	_closed=true; _selection.clear(); cancelled.emit()

## 宿主关闭沿用本代取消语义，先撤销宿主引用即可防止取消回调重入。
func close() -> void:
	cancel_selection()

## 离树立刻终止本代确认资格，节点销毁时序不改变稳定人物ID。
func _exit_tree() -> void:
	_closed=true

## 结构化拒绝保留稳定诊断，不返回半份人物清单或替代人物。
func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
