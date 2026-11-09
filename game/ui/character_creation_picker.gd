## 标准与自由自建编辑器；确认仅返回草案，人物与初货由唯一新局事务建立。
extends VBoxContainer
const Draft=preload("res://ui/character_creation_draft.gd")
signal confirmed(draft: Dictionary)
signal cancelled
var _draft: Dictionary={}
var _extended: bool=false
var _allowed_modes: Array[String]=[]
var _epoch: int=0
var _closed: bool=true
var _summary: Label
var _confirm: Button

func configure(extended: bool, allowed_modes: Array[String], initial_draft: Dictionary={}) -> Dictionary:
	if allowed_modes.is_empty() or allowed_modes.size()>2 or allowed_modes.count("standard")!=1 or allowed_modes.count("free")>1: return {"ok":false,"code":"InvalidCharacterCreationModes"}
	for mode in allowed_modes:
		if not mode in ["standard","free"]: return {"ok":false,"code":"InvalidCharacterCreationMode"}
	if not initial_draft.is_empty() and (not Draft.inspect(initial_draft,extended).ok or not initial_draft.creation_mode in allowed_modes): return {"ok":false,"code":"InvalidCharacterCreationDraft"}
	_extended=extended; _allowed_modes=allowed_modes.duplicate(); _draft=Draft.create(extended) if initial_draft.is_empty() else initial_draft.duplicate(true); _closed=false
	_build(); return {"ok":true}

func current_draft() -> Dictionary:
	return _draft.duplicate(true)

func close() -> void:
	_closed=true; _epoch+=1

func cancel_selection() -> void:
	if _closed: return
	close(); cancelled.emit()

static func _live(owner_ref: WeakRef, epoch: int):
	var owner=owner_ref.get_ref()
	if not is_instance_valid(owner) or owner.is_queued_for_deletion() or owner._closed or owner._epoch!=epoch or not owner.is_inside_tree(): return null
	return owner

static func _changed(value: Variant, owner_ref: WeakRef, epoch: int, scope: String, key: String) -> void:
	var owner=_live(owner_ref,epoch)
	if owner==null: return
	if scope=="name": owner._draft.display_name=str(value)
	elif scope=="age": owner._draft.age_years=int(value)
	elif scope=="attribute": owner._draft.attributes[key]=int(value)
	elif scope=="skill": owner._draft.skills[key]=[0,100,300][int(value)] if owner._draft.creation_mode=="standard" else int(value)
	elif scope=="mode":
		var mode: String="standard" if int(value)==0 else "free"
		if not mode in owner._allowed_modes: return
		owner._draft=Draft.change_mode(owner._draft,mode,owner._extended); owner._build(); return
	owner._refresh()

static func _pressed(owner_ref: WeakRef, epoch: int, accept: bool) -> void:
	var owner=_live(owner_ref,epoch)
	if owner==null: return
	if not accept: owner.cancel_selection(); return
	if not Draft.inspect(owner._draft,owner._extended).ok: return
	var result: Dictionary=owner.current_draft(); owner.close(); owner.confirmed.emit(result)

func _spin(parent: Node, title: String, low: int, high: int, value: int, scope: String, key: String="") -> void:
	var row=HBoxContainer.new(); parent.add_child(row)
	var label=Label.new(); label.text=title; label.size_flags_horizontal=Control.SIZE_EXPAND_FILL; row.add_child(label)
	var editor=SpinBox.new(); editor.min_value=low; editor.max_value=high; editor.step=1; editor.value=value; editor.custom_minimum_size.x=112; row.add_child(editor)
	editor.value_changed.connect(_changed.bind(weakref(self),_epoch,scope,key))

func _build() -> void:
	_epoch+=1
	for child in get_children(): remove_child(child); child.queue_free()
	var title=Label.new(); title.text="自建人物"; title.add_theme_font_size_override("font_size",24); add_child(title)
	var mode=OptionButton.new(); mode.add_item("标准创建"); mode.add_item("自由创建"); mode.set_item_disabled(1,not "free" in _allowed_modes); mode.select(0 if _draft.creation_mode=="standard" else 1); add_child(mode)
	mode.item_selected.connect(_changed.bind(weakref(self),_epoch,"mode",""))
	var name_edit=LineEdit.new(); name_edit.placeholder_text="人物姓名"; name_edit.max_length=80; name_edit.text=_draft.display_name; add_child(name_edit)
	name_edit.text_changed.connect(_changed.bind(weakref(self),_epoch,"name",""))
	var standard: bool=_draft.creation_mode=="standard"
	_spin(self,"年龄",18,60 if standard else 120,_draft.age_years,"age")
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.custom_minimum_size.y=280; add_child(scroll)
	var fields=VBoxContainer.new(); fields.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(fields)
	for id in Draft.ATTRIBUTES: _spin(fields,Draft.ATTRIBUTES[id],20 if standard else 1,85 if standard else 100,_draft.attributes[id],"attribute",id)
	var skill_header=Label.new(); skill_header.text="技艺初始等级 · 共4级，每项最多2级" if standard else "技艺初始经验 · 每项0至1200"; fields.add_child(skill_header)
	for id in Draft.skill_names(_extended): _spin(fields,Draft.skill_names(_extended)[id],0,2 if standard else 1200,Draft.level(_draft.skills[id]) if standard else _draft.skills[id],"skill",id)
	_summary=Label.new(); _summary.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(_summary)
	var buttons=HBoxContainer.new(); add_child(buttons)
	var cancel=Button.new(); cancel.text="取消"; buttons.add_child(cancel); cancel.pressed.connect(_pressed.bind(weakref(self),_epoch,false))
	_confirm=Button.new(); _confirm.text="继续选择外貌"; buttons.add_child(_confirm); _confirm.pressed.connect(_pressed.bind(weakref(self),_epoch,true))
	_refresh()

func _refresh() -> void:
	var result: Dictionary=Draft.inspect(_draft,_extended); _confirm.disabled=not result.ok
	if _draft.creation_mode=="standard":
		_summary.text="五属性 %d/300，剩余 %d；技艺 %d/4，剩余 %d。"%[result.get("attribute_total",0),result.get("attribute_remaining",0),result.get("skill_level_total",0),result.get("skill_remaining",0)]
	else: _summary.text="可自由调整属性和技艺。跨局图鉴会记录自由创建来源。"
	if _draft.display_name.is_empty(): _summary.text+=" 请填写姓名。"
