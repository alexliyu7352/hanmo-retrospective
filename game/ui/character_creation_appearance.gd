## 自建外貌只选作者允许的完整组合；复用现有表现解析与肖像渲染。
extends VBoxContainer
signal confirmed(choice_id: String)
signal cancelled
const Character=preload("res://ui/character_presentation.gd")
const Portrait=preload("res://ui/layered_portrait.gd")
const LABELS={"face_shape":"脸型","eyes":"眼睛","brows":"眉形","nose":"鼻形","mouth":"口形","hair":"发式","beard":"胡须","skin_palette":"肤色","build":"体型"}
var _epoch: int=0
var _closed: bool=true
var _choices: Dictionary={}
var _choice_id: String=""
var _draft: Dictionary={}
var _definition: Dictionary={}
var _registry: RefCounted
var _portrait: Control
var _status: Label
var _confirm: Button
var _selectors: Dictionary={}

func configure(definition: Dictionary, draft: Dictionary, choices: Array, registry: RefCounted) -> Dictionary:
	close()
	if choices.is_empty() or choices.size()>4096 or definition.get("origin")!="original" or not draft.get("display_name") is String or typeof(draft.get("age_years"))!=TYPE_INT: return {"ok":false,"code":"InvalidCharacterCreationAppearance"}
	var prepared: Dictionary={}
	for choice in choices:
		if not choice is Dictionary or choice.size()!=3 or not choice.get("id") is String or choice.id.is_empty() or prepared.has(choice.id) or not choice.get("base_features") is Dictionary or choice.base_features.size()!=LABELS.size() or not choice.get("attire_style") is String: return {"ok":false,"code":"InvalidCharacterCreationAppearance"}
		for feature in LABELS:
			if not choice.base_features.get(feature) is String or choice.base_features[feature].is_empty(): return {"ok":false,"code":"InvalidCharacterCreationAppearance"}
		prepared[choice.id]=choice.duplicate(true)
	_choices=prepared; _draft=draft.duplicate(true); _definition=definition.duplicate(true); _registry=registry
	var ids: Array=_choices.keys(); ids.sort(); _choice_id=ids[0]; _closed=false
	_build(); return {"ok":true}

func close() -> void:
	_closed=true; _epoch+=1

func selection() -> Dictionary:
	return _choices.get(_choice_id,{}).duplicate(true)

static func _live(reference: WeakRef, epoch: int):
	var owner=reference.get_ref()
	if not is_instance_valid(owner) or owner.is_queued_for_deletion() or owner._closed or owner._epoch!=epoch or not owner.is_inside_tree(): return null
	return owner

static func _select(index: int, reference: WeakRef, epoch: int, feature: String) -> void:
	var owner=_live(reference,epoch)
	if owner==null: return
	var control: OptionButton=owner._selectors.get(feature)
	if not is_instance_valid(control) or index<0 or index>=control.item_count or control.is_item_disabled(index): return
	var selected: String=str(control.get_item_metadata(index))
	if not owner._choices.has(selected): return
	owner._choice_id=selected; owner._refresh()

static func _press(reference: WeakRef, epoch: int, accept: bool) -> void:
	var owner=_live(reference,epoch)
	if owner==null: return
	if accept and (not is_instance_valid(owner._confirm) or owner._confirm.disabled): return
	var choice: String=owner._choice_id; owner.close()
	if accept: owner.confirmed.emit(choice)
	else: owner.cancelled.emit()

func _build() -> void:
	_epoch+=1; _selectors={}
	for child in get_children(): remove_child(child); child.queue_free()
	var title=Label.new(); title.text=_draft.display_name+" · "+str(_draft.age_years)+"岁"; add_child(title)
	_portrait=Portrait.new(); _portrait.custom_minimum_size=Vector2(160,220); add_child(_portrait)
	var presets=OptionButton.new(); _selectors["preset"]=presets; add_child(presets)
	var ids: Array=_choices.keys(); ids.sort()
	for id in ids:
		presets.add_item(id); presets.set_item_metadata(presets.item_count-1,id)
	presets.item_selected.connect(_select.bind(weakref(self),_epoch,"preset"))
	var scroll=ScrollContainer.new(); scroll.custom_minimum_size.y=210; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; add_child(scroll)
	var rows=VBoxContainer.new(); rows.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(rows)
	for feature in LABELS:
		var row=HBoxContainer.new(); rows.add_child(row)
		var label=Label.new(); label.text=LABELS[feature]; label.custom_minimum_size.x=60; row.add_child(label)
		var select=OptionButton.new(); select.size_flags_horizontal=Control.SIZE_EXPAND_FILL; row.add_child(select); _selectors[feature]=select
		select.item_selected.connect(_select.bind(weakref(self),_epoch,feature))
	_status=Label.new(); _status.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(_status)
	var actions=HBoxContainer.new(); add_child(actions)
	var cancel=Button.new(); cancel.text="返回修改人物"; actions.add_child(cancel); cancel.pressed.connect(_press.bind(weakref(self),_epoch,false))
	_confirm=Button.new(); _confirm.text="确认人物并启程"; actions.add_child(_confirm); _confirm.pressed.connect(_press.bind(weakref(self),_epoch,true))
	_refresh()

func _refresh() -> void:
	var current: Dictionary=_choices[_choice_id]
	var presets: OptionButton=_selectors.preset
	for index in presets.item_count:
		if presets.get_item_metadata(index)==_choice_id: presets.select(index); break
	for feature in LABELS:
		var control: OptionButton=_selectors[feature]; control.clear()
		var variants: Dictionary={}
		for id in _choices:
			var candidate: Dictionary=_choices[id]
			if candidate.attire_style!=current.attire_style: continue
			var compatible: bool=true
			for other in LABELS:
				if other!=feature and candidate.base_features[other]!=current.base_features[other]: compatible=false; break
			if compatible: variants[candidate.base_features[feature]]=id
		var values: Array=variants.keys(); values.sort()
		for value in values:
			control.add_item(value); var index: int=control.item_count-1; control.set_item_metadata(index,variants[value])
			if value==current.base_features[feature]: control.select(index)
		control.disabled=control.item_count<2
	var profile={"format_version":1,"generator_version":"appearance-v1","generation_seed":"not-sampled","source":"custom","base_features":current.base_features.duplicate(true),"attire_style":current.attire_style,"parent_sources":[]}
	var actor_view={"actor_id":"character_creation_preview","display_name":_draft.display_name,"presentation_profile_id":_definition.get("presentation_profile_id",""),"portrait_kind":_definition.get("portrait_kind","")}
	var descriptor: Dictionary=Character.preview({"actor_id":actor_view.actor_id,"actor_view":actor_view,"appearance":profile},_draft.age_years,{},_registry)
	_confirm.disabled=true; _portrait.visible=false
	if not descriptor.get("ok",false):
		if descriptor.get("code")=="UnsupportedAppearanceAge":
			_confirm.disabled=false; _status.text="此年龄的肖像待补。姓名、年龄和本次选择的实际外貌会原样保存。"; return
		_status.text="当前年龄或组合的肖像尚不可用："+str(descriptor.get("code",""))+"。人物草案与实际外貌选择已保留。"; return
	var rendered: Dictionary=_portrait.configure(descriptor,_registry)
	_portrait.visible=rendered.get("ok",false)
	if not rendered.get("ok",false) or not descriptor.get("degradation",[]).is_empty() or descriptor.get("layers",[]).is_empty():
		_status.text="此组合尚没有完整可见资源，请选择其他已制作组合。"; return
	_confirm.disabled=false; _status.text="衣着："+current.attire_style+"。确认后保存本次选择；预览不消耗世界随机或财物。"
