## 正式章节按政策选择完整成人代表或骨相五官组合；本地草案不创建人物或修改存档。
extends VBoxContainer
signal confirmed(draft: Dictionary)
signal cancelled
const Accepted=preload("res://ui/character_appearance_adapter.gd")
const Composition=preload("res://ui/adult_appearance_composition.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Character=preload("res://ui/character_presentation.gd")
const Portrait=preload("res://ui/layered_portrait.gd")
var _actor_id: String=""
var _definition: Dictionary={}
var _age: int=-1
var _registry: RefCounted
var _choices: Array[Dictionary]=[]
var _draft: Dictionary={}
var _closed: bool=true
var _epoch: int=0
var _half: Control
var _avatar: Control
var _status: Label
var _confirm: Button
var _composition: bool=false
var _slot_controls: Dictionary={}
var _face_buttons: Dictionary={}
var _face_rows: Array[Dictionary]=[]
var _feature_options: Dictionary={}

## 全表与默认值一次核准；未知组合、重复元组或未制作年龄不交出部分选择。
func configure(actor_id: String, definition: Dictionary, age_years: int, registry: RefCounted, choices: Array, founder_policy: String="fixed_actor_appearance_v1") -> Dictionary:
	_epoch+=1
	_clear_configuration()
	if actor_id.is_empty() or age_years<18 or age_years>59: return {"ok":false,"code":"UnsupportedAppearanceAge"}
	var composition: bool=founder_policy=="fixed_actor_appearance_composition_v1"
	if founder_policy!="fixed_actor_appearance_v1" and not composition: return {"ok":false,"code":"UnsupportedFounderPolicy"}
	if definition.get("origin")=="historical" or choices.size()!=(192 if composition else 12): return {"ok":false,"code":"InvalidAppearanceChoices"}
	var authored: Variant=definition.get("appearance_definition",{})
	if not authored is Dictionary: return {"ok":false,"code":"InvalidAppearanceChoiceDefault"}
	if authored.get("kind")!="fixed" or authored.get("generator_version")!="appearance-v1": return {"ok":false,"code":"InvalidAppearanceChoiceDefault"}
	var known: Dictionary={}
	for row in (Composition.records() if composition else Accepted.records()): known[row.id]=row
	var seen: Dictionary={}; var candidate_choices: Array[Dictionary]=[]; var candidate_draft: Dictionary={}
	for row in choices:
		if not row is Dictionary or row.size()!=3 or not row.has_all(["id","base_features","attire_style"]) or not row.id is String or seen.has(row.id) or not known.has(row.id): return {"ok":false,"code":"InvalidAppearanceChoices"}
		var original: Dictionary=known[row.id]
		if row.base_features!=original.base_features or row.attire_style!=original.attire_style: return {"ok":false,"code":"InvalidAppearanceChoices"}
		seen[row.id]=true; var selected: Dictionary=row.duplicate(true); selected["label"]=original.label; candidate_choices.append(selected)
		if row.base_features==authored.get("base_features") and row.attire_style==authored.get("attire_style"):
			candidate_draft={"base_features":row.base_features.duplicate(true),"attire_style":row.attire_style}
	if candidate_draft.is_empty(): return {"ok":false,"code":"InvalidAppearanceChoiceDefault"}
	var candidate_faces: Array[Dictionary]=[]; var candidate_options: Dictionary={}
	if composition:
		candidate_faces=Accepted.records()
		if candidate_faces.size()!=12: return {"ok":false,"code":"InvalidAppearanceChoices"}
		var options: Dictionary=Composition.options_for_face(candidate_draft.base_features.face_shape)
		if not options.ok: return options
		candidate_options=options.options
	_actor_id=actor_id; _definition=definition.duplicate(true); _age=age_years; _registry=registry
	_choices=candidate_choices; _draft=candidate_draft; _composition=composition
	_face_rows=candidate_faces; _feature_options=candidate_options
	_build_controls()
	var rendered: Dictionary=_render(_draft)
	if not rendered.ok:
		_clear_configuration()
		return rendered
	_closed=false
	return {"ok":true}

## 配置失败或重新配置均清除全部候选、控件和旧身份引用，避免暴露半份配置。
func _clear_configuration() -> void:
	_closed=true; _actor_id=""; _definition={}; _age=-1; _registry=null
	_choices.clear(); _draft.clear(); _slot_controls.clear(); _face_buttons.clear(); _composition=false
	_face_rows.clear(); _feature_options.clear()
	_half=null; _avatar=null; _status=null; _confirm=null
	for child in get_children(): remove_child(child); child.queue_free()

## 草案只返回深副本；关闭窗口后原宿主的旧回调不能重新确认。
func draft() -> Dictionary:
	return _draft.duplicate(true)

## 骨相与五官选择共享同一草案；旧政策保留完整代表按钮，不获得新的混搭入口。
func _build_controls() -> void:
	name="RegionalAppearancePicker"
	var title=Label.new(); title.text="选择启程外貌"; title.add_theme_font_size_override("font_size",24); add_child(title)
	var note=Label.new(); note.text=("选择三成人族的十二种骨相，并分别调整眉、眼、鼻、口。" if _composition else "本章开放三成人族的十二个完整代表。")+"只改变外貌，不改变主人公的年龄、能力、财产或身份。"; note.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(note)
	var preview=HBoxContainer.new(); preview.size_flags_vertical=Control.SIZE_EXPAND_FILL; add_child(preview)
	_half=Portrait.new(); _half.name="RegionalAppearanceHalf"; _half.custom_minimum_size=Vector2(220,330); _half.size_flags_horizontal=Control.SIZE_EXPAND_FILL; preview.add_child(_half)
	_avatar=Portrait.new(); _avatar.name="RegionalAppearanceAvatar"; _avatar.custom_minimum_size=Vector2(96,96); preview.add_child(_avatar)
	var scroll=ScrollContainer.new(); scroll.custom_minimum_size.y=132; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; add_child(scroll)
	var list=VBoxContainer.new(); list.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(list)
	if _composition:
		_build_feature_controls(list)
	else:
		for row in _choices:
			var button=Button.new(); button.text=row.label; button.custom_minimum_size.y=36; button.set_meta("appearance_choice_id",row.id)
			button.pressed.connect(_select.bind(row.id,_epoch)); list.add_child(button)
	_status=Label.new(); _status.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; add_child(_status)
	var actions=HFlowContainer.new(); add_child(actions)
	_confirm=Button.new(); _confirm.text="以此外貌启程"; _confirm.set_meta("appearance_action","confirm"); _confirm.pressed.connect(confirm_selection.bind(_epoch)); actions.add_child(_confirm)
	var cancel=Button.new(); cancel.text="返回章节"; cancel.set_meta("appearance_action","cancel"); cancel.pressed.connect(cancel_selection.bind(_epoch)); actions.add_child(cancel)
	_sync_controls()

## 标签与候选来自已验目录；界面只保存语义token，不维护另一份美术资源白名单。
func _build_feature_controls(list: VBoxContainer) -> void:
	var faces=HFlowContainer.new(); list.add_child(faces)
	for row in _face_rows:
		var button=Button.new(); button.text=row.label; button.toggle_mode=true; button.custom_minimum_size.y=36
		button.set_meta("appearance_feature_slot","face_shape"); button.set_meta("appearance_feature_token",row.base_features.face_shape)
		button.pressed.connect(_select_feature.bind("face_shape",row.base_features.face_shape,_epoch)); faces.add_child(button)
		_face_buttons[row.base_features.face_shape]=button
	var features=HFlowContainer.new(); add_child(features)
	for entry in [{"slot":"brows","label":"眉"},{"slot":"eyes","label":"眼"},{"slot":"nose","label":"鼻"},{"slot":"mouth","label":"口"}]:
		var group=HBoxContainer.new(); features.add_child(group)
		var label=Label.new(); label.text=entry.label; group.add_child(label)
		var choice=OptionButton.new(); choice.set_meta("appearance_feature_slot",entry.slot)
		for option in _feature_options[entry.slot]:
			choice.add_item(option.label); choice.set_item_metadata(choice.item_count-1,option.id)
		choice.item_selected.connect(_feature_selected.bind(entry.slot,choice,_epoch)); group.add_child(choice)
		_slot_controls[entry.slot]=choice

## 只以最后成功的草案刷新控件；失败读取不留下与头像不一致的已选状态。
func _sync_controls() -> void:
	if not _composition: return
	for face in _face_buttons: _face_buttons[face].set_pressed_no_signal(face==_draft.base_features.face_shape)
	for slot in _slot_controls:
		var choice: OptionButton=_slot_controls[slot]
		for index in range(choice.item_count):
			if choice.get_item_metadata(index)==_draft.base_features[slot]: choice.select(index); break

## 旧代次下拉回调先失效，再读取当前控件；伪造索引不能改变草案。
func _feature_selected(index: int, slot: String, control: OptionButton, epoch: int) -> void:
	if _closed or epoch!=_epoch or _slot_controls.get(slot)!=control or index<0 or index>=control.item_count: return
	_select_feature(slot,str(control.get_item_metadata(index)),epoch)

## 独立修改单槽后仍必须精确落入整表；双图完成前不采纳任何草案字段。
func _select_feature(slot: String, token: String, epoch: int) -> void:
	if _closed or epoch!=_epoch or not _composition or (slot!="face_shape" and not _slot_controls.has(slot)): return
	var candidate: Dictionary=_draft.duplicate(true)
	candidate.base_features[slot]=token
	var covered: bool=false
	for row in _choices:
		if row.base_features==candidate.base_features and row.attire_style==candidate.attire_style: covered=true; break
	if not covered: _sync_controls(); return
	var result: Dictionary=_render(candidate)
	if result.ok: _draft=candidate
	else: _status.text="无法显示此外貌："+str(result.get("code","Unknown"))
	_sync_controls()

## 预览的custom档案采用原九槽格式，原验证器不接收额外资源或GUI字段。
func _profile(selection: Dictionary) -> Dictionary:
	return {"format_version":1,"generator_version":"appearance-v1","generation_seed":"not-sampled","base_features":selection.base_features.duplicate(true),"attire_style":selection.attire_style,"source":"custom","parent_sources":[]}

## 两种视图在离树新控件中完整合成后才同时采纳；每次确认重读纹理，任一失败保留原整对。
func _render(selection: Dictionary) -> Dictionary:
	var profile: Dictionary=_profile(selection)
	var valid: Dictionary=Appearance.validate(profile)
	if not valid.ok: return valid
	var actor: Dictionary={"actor_id":_actor_id,"display_name":_definition.get("display_name",_actor_id),"age_years":_age,"presentation_profile_id":_definition.get("presentation_profile_id",""),"portrait_kind":_definition.get("portrait_kind","")}
	var half: Dictionary=Character.resolve(_actor_id,actor,profile,"half_body",{"resolution":[320,480]},_registry)
	var avatar: Dictionary=Character.resolve(_actor_id,actor,profile,"avatar",{"resolution":[96,96]},_registry)
	if not half.ok: return half
	if not avatar.ok: return avatar
	if not half.degradation.is_empty() or not avatar.degradation.is_empty(): return {"ok":false,"code":"AppearancePresentationUnavailable"}
	if (half.layers.is_empty() and not half.has("adult_composition")) or (avatar.layers.is_empty() and not avatar.has("adult_composition")): return {"ok":false,"code":"AppearancePresentationUnavailable"}
	var prepared_half: Control=Portrait.new()
	var prepared_avatar: Control=Portrait.new()
	var rendered: Dictionary=prepared_half.configure(half,_registry)
	if rendered.ok: rendered=prepared_avatar.configure(avatar,_registry)
	if not rendered.ok:
		prepared_half.free(); prepared_avatar.free()
		return rendered
	_half=_adopt_portrait(_half,prepared_half)
	_avatar=_adopt_portrait(_avatar,prepared_avatar)
	_status.text="半身与头像采用同一完整外貌。确认后随人物保存。"
	return {"ok":true}

## 仅安装已完成真实合成的候选控件；沿用原布局与名字，不再次触发可失败的资源读取。
func _adopt_portrait(previous: Control, prepared: Control) -> Control:
	var parent: Node=previous.get_parent()
	var index: int=previous.get_index()
	prepared.name=previous.name
	prepared.custom_minimum_size=previous.custom_minimum_size
	prepared.size_flags_horizontal=previous.size_flags_horizontal
	prepared.size_flags_vertical=previous.size_flags_vertical
	parent.remove_child(previous); parent.add_child(prepared); parent.move_child(prepared,index)
	previous.queue_free()
	return prepared

## 选件按钮只认本代完整作者项；迟到或伪造ID不重开草案。
func _select(id: String, epoch: int) -> void:
	if _closed or epoch!=_epoch: return
	for row in _choices:
		if row.id!=id: continue
		var candidate: Dictionary={"base_features":row.base_features.duplicate(true),"attire_style":row.attire_style}
		var result: Dictionary=_render(candidate)
		if not result.ok: _status.text="无法显示此代表："+str(result.get("code","Unknown")); return
		_draft=candidate; return

## 最终确认重验像素来源且只发一次；原生仍独立校验作者许可后才创建世界。
func confirm_selection(epoch: int=-1) -> void:
	if _closed or (epoch!=-1 and epoch!=_epoch): return
	var result: Dictionary=_render(_draft)
	if not result.ok: _status.text="无法启程："+str(result.get("code","Unknown")); return
	_closed=true; _confirm.disabled=true; confirmed.emit(_draft.duplicate(true))

## 取消丢弃草案，不提交空命令或默认人物。
func cancel_selection(epoch: int=-1) -> void:
	if _closed or (epoch!=-1 and epoch!=_epoch): return
	_closed=true; _draft.clear(); cancelled.emit()

## 宿主正常关闭采用同一终结语义。
func close() -> void:
	cancel_selection()

## 离树立即失效旧按钮，即使窗口对象稍后才销毁。
func _exit_tree() -> void:
	_closed=true
