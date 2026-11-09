## 内容驱动的场景控制器：显示地点和人物，游戏结果仅由规则核心计算。
extends Control
const ActivityPolicy=preload("res://ui/activity_policy.gd")
const ActionContext=preload("res://core/views/action_context.gd")
const BargainScreen=preload("res://ui/bargain_screen.gd")
const LocalizedFileDialog=preload("res://ui/localized_file_dialog.gd")
const Ambience=preload("res://ui/audio_ambience.gd")
const WorldMapScreen = preload("res://ui/world_map_screen.gd")
const JourneyView = preload("res://ui/journey_view.gd")
const DynamicWorldPanel=preload("res://ui/shared_world_observation_panel.gd")
const ResidentPageRequests=preload("res://ui/resident_page_requests.gd")
const GrowthCardsController=preload("res://ui/growth_cards_controller.gd")
const StudyTeachingScreen=preload("res://ui/study_teaching_screen.gd")
const ActorUnlockController=preload("res://ui/actor_unlock_controller.gd")
const ActorUnlockStart=preload("res://ui/actor_unlock_start.gd")
const SelfProfileAdapter=preload("res://ui/self_profile_adapter.gd")
const ResidentJourneyAdapter=preload("res://ui/resident_journey_adapter.gd")
const ActivityContinuationControls=preload("res://ui/activity_continuation_controls.gd")
const Screen = preload("res://ui/screen_composer.gd")
const Art = preload("res://ui/art_catalog.gd")
const Kernel=preload("res://core/world_kernel.gd")
const ContentResolver=preload("res://core/content_set_resolver.gd")
const ScenarioQuery=preload("res://core/scenario_query.gd")
const ScenarioSnapshot=preload("res://core/scenario_snapshot.gd")
const Codec=preload("res://core/save_codec.gd")
const ResidentSlotCodec=preload("res://storage/resident_slot_codec.gd")
const Storage=preload("res://storage/storage_service.gd")
const PersonToken=preload("res://ui/person_token.gd")
const Landscape=preload("res://ui/scene_stage.gd")
const SaveFiles=preload("res://storage/save_files.gd")
const Conditions=preload("res://core/condition_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const CombatSession=preload("res://core/combat_session.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Identity=preload("res://core/actor_identity.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Factory=preload("res://core/world_factory.gd")
const AppearancePicker=preload("res://ui/appearance_picker.gd")
const RegionalAppearancePicker=preload("res://ui/regional_appearance_picker.gd")
const ExistingActorStartPicker=preload("res://ui/existing_actor_start_picker.gd")
const CharacterCreationStart=preload("res://ui/character_creation_start.gd")
const UiFontScale=preload("res://ui/ui_font_scale.gd")
var _ui_font_scale=UiFontScale.new()
var journey_view=JourneyView.new()
var _ambience=Ambience.new()
var kernel=Kernel.new()
var storage=Storage.new()
var page: VBoxContainer
var main_column: VBoxContainer
var status_label: Label
var last_storage_result: Dictionary={}
var _data: Dictionary={}
var _started: bool=false
var _dynamic_open: bool=false
var _courses_open: bool=false
var _study_teaching_open: bool=false
var _study_teaching_page: int=0
var _player_plan_open: bool=false
var _player_plan_pending_start: Dictionary={}
var _player_plan_start_epoch: int=0
var _self_profile_open: bool=false
var _self_profile_view: Dictionary={}
var _cards_controller=GrowthCardsController.new()
var _cards_open: bool=false
var _cards_offset: int=0
var _cards_last_observed: Dictionary={}
var _actor_cards_controller=ActorUnlockController.new()
var _insight_cards_offset: int=0
var _insight_cards_last_observed: Dictionary={}
var _actor_cards_offset: int=0
var _actor_cards_last_observed: Dictionary={}
var _ordinary_skills_open: bool=false
var _ordinary_skill_id: String=""
var _growth_open: bool=false
var _estate_id: String=""
var _military_open: bool=false
var _military_context: Dictionary={}
var _family_open: bool=false
var _family_context: Dictionary={}
var _rest_auto: bool=false
var _rest_refresh_elapsed: float=0.0
var _advancing: bool=false
var _message: String=""
var _selected_slot: String=""
var _slot_epoch: int=0
var _slot_dialog: Window
var _slot_choice: Dictionary={}
var _slot_context: Dictionary={}
var _pending_slot: Dictionary={}
var _save_generation: int=-1
var _storage_intent: String=""
var _scene_counter: int=0
var _continuing: bool=false
var _quantity: SpinBox
var _item: OptionButton
var _payer: OptionButton
var _side: OptionButton
var _quote_label: Label
var _bargain_box: VBoxContainer
var _trade_button: Button
var _trade_request: Dictionary={}
var _trade_actor_id: String=""
var _trade_receipt: Dictionary={}
var _scenario_epoch: int=0
var _scenario_dialog: Window
var _existing_actor_picker: Control
var _appearance_epoch: int=0
var _appearance_dialog: Window
var _appearance_picker: Control
var _resident_title_resume: bool=false
var _resident_mode: bool=false
var _resident_adapter: Node
var _resident_frame_pending: bool=false
var _resident_rendering: bool=false
var _resident_hold: Dictionary={}
var _resident_command: Dictionary={}
var _resident_result: Dictionary={}
var _resident_quote: Dictionary={}
var _resident_slot: Dictionary={}
var _resident_cold_job: Dictionary={}
var _resident_handling_failure: bool=false
var _resident_bootstrap_loading: bool=false
var _resident_pending_art: Dictionary={}
var _resident_load_candidate: Node
var _resident_page_error: Dictionary={}
var _resident_provider_pending: String=""
var _resident_completion: Callable
var _resident_short_action: String=""
var _resident_read_cycle: bool=false
var _resident_trade_receipt_pending: String=""
var _resident_step_after_plan: bool=false
var _resident_journal_pending: bool=false
var _activity_draft: Dictionary={}
var _resident_pending_slot_mode: String=""

## 建立自适应界面、字体和存储回调，不在界面中建立第二套模拟。
func _ready() -> void:
	_configure_native_growth_read()
	_configure_native_travel_schedule()
	_configure_native_military_schedule()
	var font: Font=SystemFont.new()
	font.font_names=PackedStringArray(["Noto Sans CJK SC","WenQuanYi Zen Hei","DejaVu Sans"])
	for path in ["res://assets/fonts/cjk.tres","res://assets/fonts/cjk.otf","res://assets/fonts/cjk.ttf"]:
		if ResourceLoader.exists(path):
			font=load(path)
			break
	add_theme_font_override("font",font)
	add_theme_font_size_override("font_size",20)
	add_child(journey_view)
	journey_view.step_requested.connect(_auto_step)
	journey_view.suspended.connect(_render)
	add_child(_ambience)
	add_child(storage)
	storage.completed.connect(_stored)
	var margin=MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left","right","top","bottom"]: margin.add_theme_constant_override("margin_"+edge,0)
	add_child(margin)
	page=VBoxContainer.new(); page.add_theme_constant_override("separation",0); margin.add_child(page)
	theme=Art.theme(font)
	add_child(_ui_font_scale)
	_ui_font_scale.configure(self)

	_render()

## 每个实际安装的Kernel都接同一显式开发配置；缺省关闭且不在此处创建原生对象。
func _configure_native_growth_read() -> void:
	var enabled: Variant=ProjectSettings.get_setting("development/native_growth_action_enabled",false)
	var source_commit: Variant=ProjectSettings.get_setting("development/native_projection_source_commit","")
	kernel.configure_native_growth_read(enabled is bool and enabled,source_commit if source_commit is String else "")

## 单独开发开关绑定当前库身份，未配置保持legacy；新Kernel仍安装相同显式模式。
func _configure_native_travel_schedule() -> void:
	var enabled: Variant=ProjectSettings.get_setting("development/native_travel_schedule_enabled",false)
	var expected: Variant=ProjectSettings.get_setting("development/native_travel_schedule_expected_build",{})
	kernel.configure_native_travel_schedule(enabled is bool and enabled,expected if expected is Dictionary else {})

## 每次安装真实Kernel时恢复显式军政开发配置；默认项目仍使用原GD调度。
func _configure_native_military_schedule() -> void:
	var enabled: Variant=ProjectSettings.get_setting("development/native_military_schedule_enabled",false)
	var expected: Variant=ProjectSettings.get_setting("development/native_military_schedule_expected_build",{})
	kernel.configure_native_military_schedule(enabled is bool and enabled,expected if expected is Dictionary else {})

## 创建自动换行文本，使较长对白与窄窗口仍能正常显示。
func _text(parent: Node, text: String, size: int=20) -> Label:
	var label=Label.new(); label.text=text; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	label.add_theme_font_size_override("font_size",size)
	parent.add_child(label)
	if parent!=page: label.add_theme_color_override("font_color",Color("e9dec5"))
	var scope: Node=parent
	while scope!=null:
		if scope.get_meta("paper",false): label.add_theme_color_override("font_color",Color("29352c"));break
		scope=scope.get_parent()
	return label

## 创建带语义标记的控件，供玩家输入、可访问性和场景测试共用。
func _button(parent: Node, label: String, kind: String, target: String, callback: Callable, disabled: bool=false, show_portrait: bool=true) -> Button:
	var button: Button=PersonToken.new() if kind=="talk" else Button.new()
	if kind=="talk":
		button.show_portrait=show_portrait
		button.caption=label; button.appearance_key=target; button.custom_minimum_size=Vector2(174,224)
		if show_portrait and _data.has("roles") and _data.roles.has(target):
			if _resident_mode:
				var sidebar: Dictionary=kernel.place_sidebar()
				for person in sidebar.get("people",[]):
					for topic in person.get("topics",[]):
						if topic.role_id==target: button.render_descriptor=Art.character_input(kernel.character_input(person.actor_id),"half_body")
			else:
				var state: Dictionary=kernel.snapshot()
				var actor_id: String=state.roles[target].actor_id
				button.render_descriptor=Art.character(actor_id,state,_data,"half_body")
	else:
		button.text=label; button.custom_minimum_size=Vector2(90,40)
	button.set_meta("command_kind",kind); button.set_meta("target_id",target)
	button.disabled=disabled
	if _resident_mode and kind in ["save","load","system_menu","activity_stop"]: button.disabled=false
	if _resident_mode and kind=="system_menu": button.pressed.connect(_stop_journey_auto)
	if _resident_mode and kind in ["save","load","journal","map","self_profile_open","ordinary_skills_open","growth_cards_open","study_teaching_open","courses_open","family_open","military_open","player_plan_open","title"]:
		button.pressed.connect(func(): set_meta("system_menu_open",false))
	if _resident_mode and kind in ["export","import"]:
		button.pressed.connect(_resident_unavailable)
	else: button.pressed.connect(callback)
	parent.add_child(button)
	return button

## 从作者文本表读取显示名称；名称不能代替稳定实体ID。
func _name(record: Dictionary) -> String:
	return record.get("display_name",_data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))

## 通过运行人物解析作者原型与实际姓名；界面不假定所有后来加入的人都在开局目录中。
func _actor(actor_id: String) -> Dictionary:
	if _resident_mode:
		var value: Dictionary=kernel.character_input(actor_id)
		return value.get("actor_view",{}).duplicate(true)
	return Identity.definition(kernel.snapshot(),_data,actor_id)

## 只重建界面控件，世界状态和未结束对白留在规则核心。
func _render() -> void:
	if _resident_mode and _started and not _resident_rendering:
		_resident_frame_pending=true; return
	if _resident_mode and _started:
		var discovery_ready: bool=_prepare_card_discovery()
		var actor_unlock_ready: bool=_prepare_actor_unlocks()
		var insight_ready: bool=_prepare_insight_discovery()
		var page_ready: bool=ResidentPageRequests.prepare(self)
		if not discovery_ready or not actor_unlock_ready or not insight_ready or not page_ready:
			_advancing=true
			_resident_read_cycle=kernel.has_pending_reads()
			if not _resident_page_error.is_empty(): _resident_failed(_resident_page_error)
			elif is_instance_valid(status_label): status_label.text="正在读取当前页面…"
			return
	if _started and not _resident_mode:
		var cache: Dictionary=kernel.trade_cache_context()
		if not cache.is_trade or cache.actor_id!=_trade_actor_id: _reset_trade_view()
	Screen.render(self)
	_render_player_plan_menu()
	_ui_font_scale.apply_subtree(self)
	if _resident_mode: _resident_read_cycle=kernel.has_pending_reads()

## 货单选择与最近收据只属于当前交谈；换世界或退出货单不保留上一笔成功提示。
func _reset_trade_view() -> void:
	_trade_request={}; _trade_actor_id=""; _trade_receipt={}

## 正常启程枚举已安装剧本；显式命令行夹具仍沿指定目录，不按显示名称猜内容。
func _start() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--content=") or arg.begins_with("--scenario="):
			var selection: Dictionary=_default_content_selection()
			_begin_appearance(selection.scenario,selection.path)
			return
	_choose_scenario()

## 只列受信安装结果中的合法剧本与可控人物；打开和取消不实例化世界。
func _choose_scenario() -> Dictionary:
	if _started or _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_invalidate_scenario(); _invalidate_appearance()
	var installed: Dictionary=Catalog.installed_catalogs()
	if not installed.ok: return _appearance_error(installed)
	var entries: Array=ScenarioQuery.list_playable(installed.catalogs)
	if entries.is_empty(): return _appearance_error({"ok":false,"code":"NoPlayableScenario"})
	var dialog=Window.new(); dialog.name="ScenarioSelectionWindow"; dialog.title="启程 · 剧本与人物"
	dialog.transient=true; dialog.exclusive=true; dialog.min_size=Vector2i(360,460)
	add_child(dialog)
	var scroll=ScrollContainer.new(); dialog.add_child(scroll)
	scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var options=VBoxContainer.new(); options.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(options)
	_scenario_dialog=dialog
	var epoch: int=_scenario_epoch
	for entry in entries:
		var data: Dictionary={}
		for content in installed.catalogs:
			if content.catalog_path==entry.catalog_path: data=content.data; break
		if data.scenarios[entry.scenario_id].get("dynamic_world",{}).get("founder_policy","") in [ExistingActorStartPicker.POLICY,ExistingActorStartPicker.ROSTER_POLICY,CharacterCreationStart.POLICY]:
			var button: Button=_button(options,entry.name+(" · 自建人物" if data.scenarios[entry.scenario_id].dynamic_world.founder_policy==CharacterCreationStart.POLICY else " · 选择人物"),"scenario_select",entry.scenario_id,_scenario_selected.bind(entry.duplicate(true),"",dialog,epoch))
			button.set_meta("pack_set_digest",entry.pack_set_digest); button.set_meta("founder_policy",data.scenarios[entry.scenario_id].dynamic_world.founder_policy)
			button.tooltip_text=entry.scenario_id+" · "+entry.pack_set_digest.substr(0,12)
			continue
		for actor_id in entry.playable_actor_ids:
			var definition: Dictionary=data.actors[actor_id]
			var actor_name: String=data.texts.get(definition.name_key,actor_id)
			var button: Button=_button(options,entry.name+" · "+actor_name,"scenario_select",entry.scenario_id,_scenario_selected.bind(entry.duplicate(true),actor_id,dialog,epoch))
			button.set_meta("player_actor_id",actor_id); button.set_meta("pack_set_digest",entry.pack_set_digest)
			button.tooltip_text=entry.scenario_id+" · "+entry.pack_set_digest.substr(0,12)
	_button(options,"取消","scenario_cancel","",_scenario_cancelled.bind(dialog,epoch))
	dialog.close_requested.connect(_scenario_cancelled.bind(dialog,epoch))
	dialog.popup_centered(Vector2i(620,650)); _ui_font_scale.apply_subtree(dialog)
	return {"ok":true,"count":entries.size()}

## 候选按钮只对当前窗口生效；再次读取固定目录确认锁未变，不能点击后换成另一安装代。
func _scenario_selected(entry: Dictionary, actor_id: String, dialog: Window, epoch: int) -> void:
	if epoch!=_scenario_epoch or dialog!=_scenario_dialog or not is_instance_valid(dialog) or not dialog.is_inside_tree() or dialog.is_queued_for_deletion() or not dialog.visible or _started or _advancing or not _storage_intent.is_empty(): return
	var loaded: Dictionary=Catalog.read_prepared(entry.catalog_path)
	if not loaded.ok:
		_invalidate_scenario(); _appearance_error(loaded); return
	if loaded.get("pack_set_digest","")!=entry.pack_set_digest:
		_invalidate_scenario(); _appearance_error({"ok":false,"code":"PackDigestMismatch"}); return
	_invalidate_scenario()
	_begin_appearance(entry.scenario_id,entry.catalog_path,actor_id)

## 关闭当前选择窗口即作废其所有旧信号，不影响现有世界或安装目录。
func _scenario_cancelled(dialog: Window, epoch: int) -> void:
	if epoch==_scenario_epoch and dialog==_scenario_dialog: _invalidate_scenario()

## 先撤销身份再隐藏并排队释放，避免窗口回调重入或释放中的按钮新建世界。
func _invalidate_scenario() -> void:
	_scenario_epoch+=1
	var dialog: Window=_scenario_dialog
	var picker: Control=_existing_actor_picker
	_scenario_dialog=null; _existing_actor_picker=null
	if is_instance_valid(picker): picker.close()
	if is_instance_valid(dialog): dialog.hide(); dialog.queue_free()

## 只解析发行配置与命令行覆盖，不实例化世界或修改存档。
func _default_content_selection() -> Dictionary:
	var scenario: String=ProjectSettings.get_setting("application/config/default_scenario","scenario.lived_city")
	var path: String=ProjectSettings.get_setting("application/config/default_catalog","res://data/generated/lived_city.hmcatalog.json")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--scenario="): scenario=arg.trim_prefix("--scenario=")
		if arg.begins_with("--content="): path=arg.trim_prefix("--content=")
	return {"scenario":scenario,"path":path}

## 显式旧入口仍可按发行默认创建；继续和导入不得调用这个新局便捷方法。
func _start_default_content() -> Dictionary:
	var selection: Dictionary=_default_content_selection()
	var loaded: Dictionary=Catalog.read_prepared(selection.path)
	if loaded.get("ok",false) and loaded.data.scenarios.get(selection.scenario,{}).get("dynamic_world",{}).get("founder_policy","") in [ExistingActorStartPicker.POLICY,ExistingActorStartPicker.ROSTER_POLICY,CharacterCreationStart.POLICY]:
		return _begin_appearance(selection.scenario,selection.path)
	return start_content(selection.scenario,selection.path)

## 新政策先显示真实人物初态；不把默认player_actor视为玩家确认，也不准备任何原生安装。
func _choose_existing_actor(catalog: Dictionary, scenario: String, catalog_path: String) -> Dictionary:
	if _started or _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_invalidate_scenario(); _invalidate_appearance()
	var ready: Dictionary=Art.prepare_content(catalog)
	if not ready.ok: return _appearance_error(ready)
	var picker: Control=ExistingActorStartPicker.new()
	var configured: Dictionary=picker.configure(catalog,scenario,ready.registry)
	if not configured.ok: picker.free(); return _appearance_error(configured)
	var dialog=Window.new(); dialog.name="ExistingActorSelectionWindow"; dialog.title="启程 · 选择人物"
	dialog.transient=true; dialog.exclusive=true; dialog.min_size=Vector2i(380,480); add_child(dialog)
	var scroll=ScrollContainer.new(); dialog.add_child(scroll); scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED
	picker.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(picker)
	_scenario_dialog=dialog; _existing_actor_picker=picker
	var epoch: int=_scenario_epoch
	picker.confirmed.connect(_existing_actor_confirmed.bind(picker,dialog,epoch,scenario,catalog_path,catalog.pack_set_digest))
	picker.cancelled.connect(_scenario_cancelled.bind(dialog,epoch))
	dialog.close_requested.connect(picker.cancel_selection)
	dialog.popup_centered(Vector2i(700,760)); _ui_font_scale.apply_subtree(dialog)
	return {"ok":true,"mode":"existing_actor_selection"}

## 只读当前受信剧本的人物政策；没有明确成员ID时绝不使用默认人物。
func _existing_actor_policy(catalog: Dictionary, scenario: String, actor_id: String) -> Dictionary:
	var definition: Dictionary=catalog.data.scenarios.get(scenario,{})
	if actor_id.is_empty() or not actor_id in definition.get("playable_actor_ids",[]): return {"ok":false,"code":"InvalidPlayableActor"}
	for row in definition.get("playable_actor_appearance_policies",[]):
		if row.get("actor_id")==actor_id and row.get("policy") in [ExistingActorStartPicker.COMPOSITION,ExistingActorStartPicker.PRESERVE]: return {"ok":true,"policy":row.policy}
	return {"ok":false,"code":"InvalidPlayableActorAppearancePolicies"}

## 人物确认先重核窗口、冻结选择和目录锁，再撤销本代；后续只进入该人的原外貌分支。
func _existing_actor_confirmed(selection: Dictionary, picker: Control, dialog: Window, epoch: int, scenario: String, catalog_path: String, digest: String) -> void:
	if epoch!=_scenario_epoch or picker!=_existing_actor_picker or dialog!=_scenario_dialog or not is_instance_valid(picker) or not is_instance_valid(dialog) or not picker.is_inside_tree() or not dialog.is_inside_tree() or picker.is_queued_for_deletion() or dialog.is_queued_for_deletion() or not dialog.visible or _started or _advancing or not _storage_intent.is_empty(): return
	if selection.size()!=4 or selection!=picker.selected() or selection.get("scenario_id")!=scenario or selection.get("pack_set_digest")!=digest: return
	var loaded: Dictionary=Catalog.read_prepared(catalog_path)
	if not loaded.get("ok",false): _invalidate_scenario(); _appearance_error(loaded); return
	if loaded.get("pack_set_digest","")!=digest: _invalidate_scenario(); _appearance_error({"ok":false,"code":"PackDigestMismatch"}); return
	var policy: Dictionary=_existing_actor_policy(loaded,scenario,selection.get("actor_id",""))
	if not policy.ok or policy.policy!=selection.get("appearance_policy"):
		_invalidate_scenario(); _appearance_error({"ok":false,"code":"InvalidPlayableActorAppearancePolicies"}); return
	var actor_id: String=selection.actor_id
	_invalidate_scenario()
	_begin_appearance(scenario,catalog_path,actor_id)

## 读取作者候选供控件预览；确认前不调用Kernel.new_game，取消保留当前完整世界。
func _begin_appearance(scenario: String, catalog_path: String, player_actor_id: String="") -> Dictionary:
	if _started or _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_invalidate_appearance()
	var loaded: Dictionary=Catalog.read_prepared(catalog_path)
	if not loaded.ok: return _appearance_error(loaded)
	if not loaded.data.scenarios.has(scenario): return _appearance_error({"ok":false,"code":"UnknownScenario"})
	var founder_policy: String=loaded.data.scenarios[scenario].get("dynamic_world",{}).get("founder_policy","")
	if founder_policy==CharacterCreationStart.POLICY: return _begin_character_creation(loaded,scenario,catalog_path)
	var effective: Dictionary={}
	if founder_policy in [ExistingActorStartPicker.POLICY,ExistingActorStartPicker.ROSTER_POLICY]:
		if player_actor_id.is_empty(): return _choose_existing_actor(loaded,scenario,catalog_path)
		var policy: Dictionary=_existing_actor_policy(loaded,scenario,player_actor_id)
		if not policy.ok: return _appearance_error(policy)
		effective=ScenarioSnapshot.resolve(loaded,scenario,player_actor_id)
		if not effective.ok: return _appearance_error(effective)
		founder_policy=policy.policy
		if founder_policy==ExistingActorStartPicker.PRESERVE: return start_content(scenario,catalog_path,{},player_actor_id)
	var regional_appearance: bool=founder_policy in ["fixed_actor_appearance_v1","fixed_actor_appearance_composition_v1"]
	if loaded.data.scenarios[scenario].has("dynamic_world") and not regional_appearance: return start_content(scenario,catalog_path,{},player_actor_id)
	if effective.is_empty(): effective=ScenarioSnapshot.resolve(loaded,scenario,player_actor_id)
	if not effective.ok: return _appearance_error(effective)
	var scenario_definition: Dictionary=effective.catalog.data.scenarios[scenario]
	var actor_id: String=scenario_definition.player_actor
	var definition: Dictionary=effective.catalog.data.actors[actor_id]
	if not definition.has("appearance_definition"): return start_content(scenario,catalog_path,{},actor_id)
	var ready: Dictionary=Art.prepare_content(effective.catalog)
	if not ready.ok: return _appearance_error(ready)
	var seed: String=Factory._random_streams(scenario_definition.seed_text).world.seed
	var picker: Control=RegionalAppearancePicker.new() if regional_appearance else AppearancePicker.new()
	var configured: Dictionary=picker.configure(actor_id,definition,definition.get("initial_age_years",-1),ready.registry,scenario_definition.get("player_appearance_choices",[]),founder_policy) if regional_appearance else picker.configure(actor_id,definition,seed,definition.get("initial_age_years",-1),ready.registry)
	if not configured.ok:
		picker.free(); return _appearance_error(configured)
	var dialog=Window.new(); dialog.name="AppearanceDraftWindow"; dialog.title="启程 · 人物外貌"
	dialog.transient=true; dialog.exclusive=true; dialog.min_size=Vector2i(360,460)
	add_child(dialog)
	var margin=MarginContainer.new(); dialog.add_child(margin)
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left","right","top","bottom"]: margin.add_theme_constant_override("margin_"+edge,12)
	margin.add_child(picker); picker.name="AppearanceDraftPicker"
	_appearance_dialog=dialog; _appearance_picker=picker
	var epoch: int=_appearance_epoch
	picker.confirmed.connect(_appearance_confirmed.bind(picker,dialog,epoch,scenario,catalog_path,actor_id))
	picker.cancelled.connect(_appearance_cancelled.bind(picker,dialog,epoch))
	dialog.close_requested.connect(picker.cancel_selection)
	dialog.popup_centered(Vector2i(520,700))
	return {"ok":true,"mode":"appearance_draft"}

## 自建只持草案，确认前没有原生世界、财物或随机实例。
func _begin_character_creation(catalog: Dictionary, scenario: String, catalog_path: String) -> Dictionary:
	var ready: Dictionary=Art.prepare_content(catalog)
	if not ready.get("ok",false): return _appearance_error(ready)
	var picker=CharacterCreationStart.new()
	var configured: Dictionary=picker.configure(catalog,scenario,ready.registry)
	if not configured.get("ok",false): picker.free(); return _appearance_error(configured)
	var dialog=Window.new(); dialog.name="CharacterCreationWindow"; dialog.title="启程 · 自建人物"
	dialog.transient=true; dialog.exclusive=true; dialog.min_size=Vector2i(380,520); add_child(dialog)
	var margin=MarginContainer.new(); dialog.add_child(margin); margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.add_child(picker); picker.size_flags_horizontal=Control.SIZE_EXPAND_FILL; picker.size_flags_vertical=Control.SIZE_EXPAND_FILL
	_appearance_dialog=dialog; _appearance_picker=picker
	var epoch: int=_appearance_epoch
	picker.confirmed.connect(_character_creation_confirmed.bind(weakref(self),weakref(picker),weakref(dialog),epoch,scenario,catalog_path,catalog.pack_set_digest))
	picker.cancelled.connect(_character_creation_cancelled.bind(weakref(self),weakref(picker),weakref(dialog),epoch))
	dialog.close_requested.connect(_character_creation_cancelled.bind(weakref(self),weakref(picker),weakref(dialog),epoch))
	dialog.popup_centered(Vector2i(600,780)); _ui_font_scale.apply_subtree(dialog)
	return {"ok":true,"mode":"character_creation"}

static func _character_creation_confirmed(selection: Dictionary, host_ref: WeakRef, picker_ref: WeakRef, dialog_ref: WeakRef, epoch: int, scenario: String, catalog_path: String, digest: String) -> void:
	var host=host_ref.get_ref(); var picker=picker_ref.get_ref(); var dialog=dialog_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(picker) or not is_instance_valid(dialog) or not host._appearance_current(picker,dialog,epoch): return
	if selection!=picker.selected() or selection.get("scenario_id")!=scenario or selection.get("pack_set_digest")!=digest or not selection.get("character_creation_request") is Dictionary: return
	var loaded: Dictionary=Catalog.read_prepared(catalog_path)
	if not loaded.get("ok",false) or loaded.get("pack_set_digest")!=digest:
		host._invalidate_appearance(); host._appearance_error(loaded if not loaded.get("ok",false) else {"ok":false,"code":"PackDigestMismatch"}); return
	var request: Dictionary=selection.character_creation_request.duplicate(true)
	host._invalidate_appearance(); host.start_content(scenario,catalog_path,{},"",request)

static func _character_creation_cancelled(host_ref: WeakRef, picker_ref: WeakRef, dialog_ref: WeakRef, epoch: int) -> void:
	var host=host_ref.get_ref(); var picker=picker_ref.get_ref(); var dialog=dialog_ref.get_ref()
	if is_instance_valid(host) and is_instance_valid(picker) and is_instance_valid(dialog) and host._appearance_current(picker,dialog,epoch): host._invalidate_appearance()

## 选件配置失败留在标题并显示诊断，不以默认人物代替失败的新定义。
func _appearance_error(result: Dictionary) -> Dictionary:
	_message="外貌选项尚不能使用："+str(result.get("code","Unknown")); _render()
	return result

## 宿主也核验当前节点与代次，防止关闭、换页或重开后旧信号创建新世界。
func _appearance_current(picker: Control, dialog: Window, epoch: int) -> bool:
	return epoch==_appearance_epoch and picker==_appearance_picker and dialog==_appearance_dialog and is_instance_valid(picker) and is_instance_valid(dialog) and picker.is_inside_tree() and dialog.is_inside_tree() and not picker.is_queued_for_deletion() and not dialog.is_queued_for_deletion() and dialog.visible and not _started and _storage_intent.is_empty()

## 草案确认只进入同一新局工厂，节点先失效以拒绝重入和重复确认。
func _appearance_confirmed(draft: Dictionary, picker: Control, dialog: Window, epoch: int, scenario: String, catalog_path: String, player_actor_id: String="") -> void:
	if not _appearance_current(picker,dialog,epoch): return
	_invalidate_appearance()
	start_content(scenario,catalog_path,draft,player_actor_id)

## 取消与窗口关闭只撤销本次草案，不替换Kernel、世界或作者定义。
func _appearance_cancelled(picker: Control, dialog: Window, epoch: int) -> void:
	if not _appearance_current(picker,dialog,epoch): return
	_invalidate_appearance()

## 先撤销宿主引用再关闭控件，关闭所发信号不能重入确认路径。
func _invalidate_appearance() -> void:
	_appearance_epoch+=1
	var picker: Control=_appearance_picker
	var dialog: Window=_appearance_dialog
	_appearance_picker=null; _appearance_dialog=null
	if is_instance_valid(picker): picker.close()
	if is_instance_valid(dialog):
		dialog.hide()
		# 输入仍可能沿Window传播；先隐藏及排队释放，避免同步离树引擎错误。
		dialog.queue_free()

## 编辑器、发行与场景验收共用的内容入口；通过目录和表现校验后才显示新世界。
func start_content(scenario: String, catalog_path: String, appearance_draft: Dictionary={}, player_actor_id: String="", character_creation_request: Dictionary={}) -> Dictionary:
	if _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_invalidate_scenario(); _invalidate_appearance(); _invalidate_slots()
	var loaded: Dictionary=Catalog.read_prepared(catalog_path)
	if not loaded.get("ok",false): return _appearance_error(loaded)
	var definition: Dictionary=loaded.get("data",{}).get("scenarios",{}).get(scenario,{})
	if definition.has("dynamic_world"):
		var art: Dictionary=Art.prepare_content(loaded)
		if not art.get("ok",false): return _appearance_error(art)
		var backend_script=load("res://ui/native_resident_backend.gd")
		if backend_script==null: return _appearance_error({"ok":false,"code":"ResidentBackendUnavailable"})
		var request: Dictionary=backend_script.native_request(loaded,scenario,player_actor_id,appearance_draft,null,character_creation_request)
		if definition.has("actor_card_unlocks"):
			# 仅新局读取独立收藏账；原生仍核原人物资格及政策，冷恢复不走此入口。
			var qualification: Dictionary=ActorUnlockStart.request_qualification(definition,player_actor_id)
			if not qualification.get("ok",false): return _appearance_error(qualification)
			request["unlocked_actor_ids"]=qualification.unlocked_actor_ids
		return _start_resident(null,art,true,{"catalog":loaded,"request":request})
	var candidate=Kernel.new()
	var result: Dictionary=candidate.new_game(scenario,catalog_path,appearance_draft,player_actor_id)
	if not result.ok:
		_message="内容尚不能运行："+result.get("code","Unknown")
		_render(); return result
	var art: Dictionary=Art.prepare_content(candidate.content())
	if not art.ok:
		_message="内容引用的美术资源缺失："+str(art.get("path",art.get("code","Unknown")))
		_render(); return art
	if _resident_requested(): return _start_resident(candidate,art)
	_reset_player_plan_entry()
	kernel=candidate
	_configure_native_growth_read()
	_configure_native_travel_schedule()
	_configure_native_military_schedule()
	Art.commit_content(art)
	journey_view.reset()
	_self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _family_open=false; _military_open=false; _rest_auto=false
	_data=kernel.content().data
	_reset_trade_view()
	var save_context: Dictionary=kernel.save_context()
	storage.configure(save_context.catalog,save_context.registry)
	_save_generation=-1; _selected_slot=""
	_started=true; _message=""
	_render()
	return {"ok":true}

## 使用当前序号与版本提交一次命令；正常短行动交给统一时间结算。
func _perform(kind: String, params: Dictionary={}) -> void:
	if _resident_mode:
		_resident_perform(kind,params); return
	if kind!="advance": _stop_journey_auto(); _rest_auto=false
	if _advancing or not _storage_intent.is_empty(): return
	var context: Dictionary=kernel.command_context()
	if not context.ok:
		_message="此时不能这样做："+_reason(context.get("code","Unknown"))
		_render(); return
	var command: Dictionary={"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}
	var result: Dictionary=kernel.submit(command)
	_apply_command_result(kind,result)

## 岗位按钮保留显示时的人物与版本；旧页面或换世界后不借最新revision提交原候选。
func _topic_request(role_id: String, actor_id: String, expected_revision: int, scene_epoch: int) -> Dictionary:
	if not _started or _advancing or not _storage_intent.is_empty() or scene_epoch!=_scene_counter: return {"ok":false,"code":"StaleSceneRequest"}
	var context: Dictionary=kernel.command_context()
	if not context.ok: return context
	if context.actor_id!=actor_id: return {"ok":false,"code":"StaleSceneRequest"}
	if context.revision!=expected_revision: return {"ok":false,"code":"StalePreview"}
	var params: Dictionary={"role_id":role_id}
	var quote: Dictionary=kernel.preview("talk",params,actor_id)
	if not quote.ok: return quote
	if quote.get("revision")!=expected_revision: return {"ok":false,"code":"StalePreview"}
	if _resident_mode: return _resident_submit_frozen({"kind":"talk","params":params,"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":expected_revision})
	_stop_journey_auto(); _rest_auto=false
	var result: Dictionary=kernel.submit({"kind":"talk","params":params,"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":expected_revision})
	_apply_command_result("talk",result)
	return result

## 仅接受当前活场景绑定的完整请求；原actor、版本、目标与代次不能被新快照替换。
func _scene_request(request: Dictionary, host: Control) -> Dictionary:
	if not _started or not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree() or not is_ancestor_of(host) or not host.is_visible_in_tree():
		return {"ok":false,"code":"StaleSceneRequest"}
	if _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	if not request.get("kind") is String or not request.get("params") is Dictionary or not request.get("actor_id") is String or typeof(request.get("expected_revision"))!=TYPE_INT or not request.get("target_ref") is Dictionary or typeof(request.get("scene_epoch"))!=TYPE_INT:
		return {"ok":false,"code":"InvalidSceneRequest"}
	var context: Dictionary=kernel.command_context()
	if not context.ok: return context
	if request.actor_id!=context.actor_id: return {"ok":false,"code":"StaleSceneRequest"}
	if request.expected_revision!=context.revision: return {"ok":false,"code":"StalePreview"}
	if not host.has_method("validate_request"): return {"ok":false,"code":"InvalidSceneHost"}
	var checked: Dictionary=host.validate_request(request)
	if not checked.get("ok",false): return checked
	var preview: Dictionary=kernel.preview(request.kind,request.params,request.actor_id)
	if not preview.ok: return preview
	if preview.get("revision")!=request.expected_revision: return {"ok":false,"code":"StalePreview"}
	var command: Dictionary=request.duplicate(true)
	command.request_seq=context.next_request_seq
	if _resident_mode: return _resident_submit_frozen(command)
	_stop_journey_auto(); _rest_auto=false
	var result: Dictionary=kernel.submit(command)
	_apply_command_result(request.kind,result)
	return result

## 共享提交后的文字、短行动与重绘收尾，不重建请求或改写提交前版本。
func _apply_command_result(kind: String, result: Dictionary) -> void:
	_message="" if result.ok else "此时不能这样做："+_reason(result.get("code","Unknown"))
	if kind=="inspect" and result.ok: _message="你把告示上的署名与办事去处记在了手记里。"
	if kind=="ask_news" and result.ok: _message="听来的近事已记入手记，注明了消息来源。" if not result.details.news_ids.is_empty() else "对方暂时没有新的近事相告。"
	var context: Dictionary=kernel.action_context()
	if result.ok and kind=="trade":
		var receipt: Dictionary=kernel.trade_action_receipt(result.details.get("action_id",""))
		if not receipt.is_empty(): _trade_receipt=receipt
	if not result.ok or context.action_kind!="travel" or kernel.action_needs_input(): _stop_journey_auto()
	var action_id: String=context.action_id
	if result.ok and kind in ["study_skill","military_train"]: _courses_open=false
	if result.ok and kind in ["military_plan","siege_plan"]:
		var advanced: Dictionary=kernel.advance_one_tick()
		if not advanced.ok: _message="军议尚待完成："+_reason(advanced.get("code","Unknown"))
	if result.ok and not kind.begins_with("player_plan_") and kind not in ["advance","family_adopt_existing","naval_battle_plan","naval_capture","naval_claim_decline","naval_battle_continue","naval_form","naval_disband","military_request","party_accept","party_leave","decline_offer","decline_runtime_offer","task_personal_advance","military_plan","siege_plan","study_pause","study_resume","study_cancel","study_challenge_choose","study_challenge_exit","study_etiquette_choose","study_etiquette_exit"] and not action_id.is_empty() and not context.action_kind in ["travel","rescue","long_rest"] and not kernel.action_manual_window():
		_advancing=true
		call_deferred("_finish_short")
	_render()

## 只推进已提交的短行动；菜单不另行发放奖励或启动私有计时器。
func _finish_short() -> void:
	if _resident_mode:
		var current: Dictionary=kernel.action_context()
		if current.get("action_id","")!=_resident_short_action or kernel.is_busy() or kernel.has_pending_reads() or kernel.action_manual_window(): return
		_advancing=false; _resident_perform("advance",{}); return
	var context: Dictionary=kernel.action_context()
	var action_id: String=context.action_id
	if not action_id.is_empty():
		var result: Dictionary=kernel.advance_one_tick()
		if not result.ok: _message="办理中断："+_reason(result.get("code","Unknown"))
	_advancing=false
	_render()

## 按对白运行器的当前阶段显示内容，不从台词或按钮名称推断任务成功。
func _conversation(view: Dictionary, speaker_name: Variant=null) -> void:
	Screen.conversation_body(self,view,speaker_name)

## 使用实际商户和在场代表显示货单，报价来自规则核心。
func _trade(box: VBoxContainer, view: Dictionary) -> void:
	var sheet: Dictionary=kernel.trade_view(view.command.shop_id,view.context.provider_actor_id,_trade_receipt.get("action_id",""))
	var selection: Dictionary=_trade_request.duplicate(true) if _trade_actor_id==sheet.actor_id and _trade_request.get("shop_id")==view.command.shop_id and _trade_request.get("context")==view.context else {}
	if selection.is_empty(): _trade_receipt={}
	_trade_actor_id=sheet.actor_id
	_trade_request={"shop_id":view.command.shop_id,"context":view.context.duplicate(true)}
	_text(box,"货单 · "+_name(_data.places[view.context.place_id])+" · 正与"+sheet.provider_display_name+"交易",18)
	if not _trade_receipt.is_empty():
		var receipt: Dictionary=_trade_receipt.quote
		var disposition: String="已入本人行囊" if receipt.owner=={"kind":"actor","id":sheet.actor_id} else "公物已入行囊"
		var words: String="已买入 %s ×%d · 支付 %d钱 · %s"%[_name(_data.items[receipt.item_id]),receipt.quantity,receipt.total,disposition]
		if receipt.side=="sell": words="已卖出 %s ×%d · 收到 %d钱"%[_name(_data.items[receipt.item_id]),receipt.quantity,receipt.total]
		words+="\n钱货已交割，装货尚未完成。" if sheet.receipt_action_pending else "\n交易完成，已用半日。"
		_text(box,"最近一笔："+words,17).name="TradeReceipt"
	var row=HFlowContainer.new(); box.add_child(row)
	_item=OptionButton.new(); row.add_child(_item)
	var first_stocked: int=-1; var initial_quantity: int=10
	for entry in sheet.items:
		var item: Dictionary=_data.items[entry.item_id]
		var stock: int=entry.stock
		_item.add_item("%s · 店存 %d"%[_name(item),stock]); _item.set_item_metadata(_item.item_count-1,item.id)
		if first_stocked<0 and stock>0:
			first_stocked=_item.item_count-1; initial_quantity=mini(10,stock)
	if first_stocked>=0: _item.select(first_stocked)
	for index in range(_item.item_count):
		if _item.get_item_metadata(index)==selection.get("item_id"): _item.select(index); break
	_side=OptionButton.new(); _side.add_item("买入"); _side.add_item("卖出"); row.add_child(_side)
	_side.select(1 if selection.get("side")=="sell" else 0)
	_quantity=SpinBox.new(); _quantity.min_value=1; _quantity.max_value=500; _quantity.step=1; _quantity.value=initial_quantity; row.add_child(_quantity)
	_quantity.value=selection.get("quantity",initial_quantity)
	_payer=OptionButton.new(); row.add_child(_payer)
	_payer.add_item("个人钱财"); _payer.set_item_metadata(0,sheet.personal_wallet_id)
	for payer in sheet.task_payers:
		_payer.add_item("委托经费 · "+_name(_data.offers.get(payer.offer_id,{"display_name":payer.get("display_name","已接委托 · "+payer.offer_id)}))); _payer.set_item_metadata(_payer.item_count-1,payer.wallet_id)
	for index in range(_payer.item_count):
		if _payer.get_item_metadata(index)==selection.get("payer_wallet_id"): _payer.select(index); break
	_quote_label=_text(box,"")
	_trade_button=_button(box,"确认交易","trade","",_confirm_trade,_advancing)
	_button(box,"收起货单","close_trade","",_perform.bind("close_trade",{}),_advancing)
	_item.item_selected.connect(_quote_changed); _side.item_selected.connect(_quote_changed); _payer.item_selected.connect(_quote_changed); _quantity.value_changed.connect(_quote_changed)
	_bargain_box=VBoxContainer.new(); _bargain_box.name="TradeBargainHost"; box.add_child(_bargain_box)
	_update_quote()

## 数量和支付来源改变时刷新只读报价，不预占资金或推进时间。
func _quote_changed(_value: Variant) -> void:
	_update_quote()

## 汇总货单输入的独立副本，向统一核心请求当前有效报价。
func _update_quote() -> void:
	if _item.item_count==0: _trade_button.disabled=true; return
	_trade_request.item_id=_item.get_item_metadata(_item.selected)
	_trade_request.side="buy" if _side.selected==0 else "sell"
	_trade_request.quantity=int(_quantity.value)
	_trade_request.payer_wallet_id=_payer.get_item_metadata(_payer.selected)
	_trade_request.erase("quoted_total"); _trade_request.erase("market_revision")
	var quote: Dictionary=kernel.preview("trade",_trade_request)
	_trade_button.disabled=not quote.ok or _advancing
	if quote.ok:
		_quote_label.text="合计 %d钱 · 交割与装货耗时半日"%quote.total
		_trade_request.quoted_total=quote.total; _trade_request.market_revision=quote.market_revision
	else: _quote_label.text=("下一笔暂不能成交：" if not _trade_receipt.is_empty() else "不能成交：")+_reason(quote.get("code","Unknown"))

	if is_instance_valid(_bargain_box) and _bargain_box.is_inside_tree():
		for child in _bargain_box.get_children():
			_bargain_box.remove_child(child); child.queue_free()
		if _data.shops.get(_trade_request.get("shop_id",""),{}).has("bargaining"):
			BargainScreen.render(self,_bargain_box,_trade_request.duplicate(true))

## 确认玩家看到的报价；过期报价和无效交易仍由核心拒绝。
func _confirm_trade() -> void:
	_perform("trade",_trade_request.duplicate(true))

## 显示实际路程及前进、折返、救助选项，不通过界面传送人物。
func _route(state: Dictionary, actor: Dictionary) -> void:
	var edge: Dictionary=_data.routes[actor.location.edge_id]
	_text(main_column,"旅途中 · %s → %s"%[_name(_data.settlements[edge.origin]),_name(_data.settlements[edge.destination])],30)
	_text(main_column,"已走进度 %d / %d；此前已花的时间与路费不会退还。"%[actor.location.progress,edge.duration_ticks*100])
	for kind in ["advance","return","rescue"]:
		var labels: Dictionary={"advance":"继续行进半日","return":"沿已走路段折返","rescue":"请求救助返回 · 两个半日"}
		_button(main_column,labels[kind],kind,"",_perform.bind(kind,{}),not kernel.preview(kind).ok or _advancing)

## 显示已知约定与随身物品；手记不提供远程受托或领奖入口。
func _journal() -> void:
	_resident_short_action=""; _resident_step_after_plan=false
	_rest_auto=false
	_stop_journey_auto()
	if _resident_mode:
		_resident_journal_pending=true
		_resident_finish_journal()
		return
	_show_journal(journal_text())

## 手记窗口只接收已经完成的本人只读文本，不运行任务规则或查询隐藏世界。
func _show_journal(text: String) -> void:
	var dialog=AcceptDialog.new(); dialog.title="行旅手记"; dialog.dialog_text=text
	if _player_plan_enabled():
		var source: Dictionary=kernel.source_identity()
		var plans: Button=dialog.add_button("安排已知生活",true,"player_plan")
		plans.set_meta("command_kind","player_plan_open"); plans.set_meta("target_id","")
		dialog.custom_action.connect(func(action: String):
			if action!="player_plan" or not is_instance_valid(dialog) or not dialog.visible or not _player_plan_enabled() or source!=kernel.source_identity(): return
			dialog.hide(); dialog.queue_free(); _open_player_plan())
	add_child(dialog); dialog.popup_centered(Vector2i(660,540)); dialog.confirmed.connect(dialog.queue_free)
	dialog.close_requested.connect(dialog.queue_free)

## 新情景的只读能力来自当前原生目录；不向旧世界添加计划状态或篡改能力清单。
func _player_plan_enabled() -> bool:
	return _resident_mode and _started and kernel.has_method("player_plan_enabled") and kernel.player_plan_enabled()

## 生活安排加入原系统菜单，现有场景和手记入口仍通向同一个页面。
func _render_player_plan_menu() -> void:
	if not _player_plan_enabled(): return
	var scroll: ScrollContainer=page.find_child("SystemMenuScroll",true,false) as ScrollContainer
	if scroll==null or scroll.get_child_count()==0: return
	var entries: Node=scroll.get_child(0)
	if entries.get_node_or_null("PlayerPlanMenuOpen")!=null: return
	_button(entries,"生活安排","player_plan_open","",_open_player_plan,_advancing).name="PlayerPlanMenuOpen"

## 打开手记中的有限安排只停止驱动，保留已经接受的课程、行路与托付。
func _open_player_plan() -> void:
	if not _player_plan_enabled() or _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty(): return
	if not kernel.personal_battle_view().get("battle_id","").is_empty() or kernel.military_battle_page().get("active",false) or not kernel.siege_page().get("siege_id","").is_empty(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false
	_dynamic_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _courses_open=false; _military_open=false; _family_open=false; _estate_id=""
	_player_plan_open=true; _render()

## 返回原场所也是本人阅读操作；不会取消已接受的行动或复活上次许可。
func _close_player_plan() -> void:
	_stop_journey_auto(); _player_plan_open=false; _render()

## 换世界或回标题清除全部界面启动意图；保存的计划真相只由世界checkpoint恢复。
func _reset_player_plan_entry() -> void:
	_player_plan_start_epoch+=1; _player_plan_pending_start={}; _player_plan_open=false
	for key in ["player_plan_source_consent","player_plan_step_consent"]:
		if has_meta(key): remove_meta(key)

## 打开本处明确岗位课程；只读列表由真实角色/上下文产生，报名仍走原Kernel。
func _open_courses() -> void:
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	if not _started or _advancing or not _storage_intent.is_empty(): return
	_stop_journey_auto(); _rest_auto=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _military_open=false; _family_open=false; _estate_id=""; _courses_open=true
	_render()

## 返回场所仅关闭输入块，不中断已经开始的课程或修改世界。
func _close_courses() -> void:
	_courses_open=false; _render()

## 查看本人已学卡片和实际行囊，打开界面不会授艺、换装或推进半日。
func _open_growth() -> void:
	_self_profile_open=false; _self_profile_view={}
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	_rest_auto=false
	if not _started: return
	for key in ["growth_ui_draft","growth_selected_skill"]:
		if has_meta(key): remove_meta(key)
	_stop_journey_auto(); journey_view.opened=false; _family_open=false; _military_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _growth_open=true; _render()

## 返回当前真实地点，保留已开始修炼与保存中的所有人物行动。
func _close_growth() -> void:
	for key in ["growth_ui_draft","growth_selected_skill"]:
		if has_meta(key): remove_meta(key)
	_self_profile_open=false; _self_profile_view={}; _growth_open=false; _render()

## 正常菜单打开当前本人只读页；保留真实动作和对白，仅停止界面自动推进。
func _open_self_profile() -> void:
	if not _resident_mode or not _started or _advancing or not _storage_intent.is_empty(): return
	if not kernel.personal_battle_view().get("battle_id","").is_empty() or kernel.military_battle_page().get("active",false) or not kernel.siege_page().get("siege_id","").is_empty(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false
	_player_plan_open=false; _dynamic_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _growth_open=false
	_courses_open=false; _military_open=false; _family_open=false; _estate_id=""
	_self_profile_open=true; _self_profile_view={}; _render()

## 关闭仅释放本页投影；场所、课时与存储保持原业务状态。
func _close_self_profile() -> void:
	_self_profile_open=false; _self_profile_view={}; _render()

## 导航只接当前本人和同源三元组，旧代或拒绝后重绘使有效控件可再次使用。
func _navigate_self_profile(target: String, selected_id: String, source: Dictionary) -> void:
	if not _self_profile_open or not _resident_mode or not _started or _advancing or not _storage_intent.is_empty(): return
	var current: Dictionary=kernel.source_identity()
	var context: Dictionary=kernel.command_context()
	var valid: bool=context.get("ok",false) and _self_profile_view.get("actor_id")==context.get("actor_id")
	for key in ["world_id","execution_epoch","revision"]:
		valid=valid and source.get(key)==current.get(key)
	if not valid:
		_self_profile_view={}; _render(); return
	if target=="ordinary_skills": _open_ordinary_skills(selected_id)
	elif target=="martial_skills":
		set_meta("growth_page","skills"); _open_growth()
	if _self_profile_open: _render()

## 从现有菜单读取本人普通技艺；常驻模式通过同源有限页获取，不退回整态。
func _open_ordinary_skills(skill_id: String="") -> void:
	_player_plan_open=false
	_resident_short_action=""
	if not _started or _advancing or not _storage_intent.is_empty(): return
	if not kernel.personal_battle_view().get("battle_id","").is_empty() or kernel.military_battle_page().get("active",false) or not kernel.siege_page().get("siege_id","").is_empty(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false
	_self_profile_open=false; _self_profile_view={}; _growth_open=false; _courses_open=false; _military_open=false; _family_open=false; _estate_id=""
	_cards_open=false; _study_teaching_open=false; _ordinary_skills_open=true; _ordinary_skill_id=skill_id; _render()

## 收藏菜单只开放当前显式原生profile，沿原页面优先级与有限读取计划进入。
func _cards_available() -> bool:
	return _resident_mode and ((kernel.has_method("growth_cards_enabled") and kernel.growth_cards_enabled()) or (kernel.has_method("actor_cards_enabled") and kernel.actor_cards_enabled()) or (kernel.has_method("insight_cards_enabled") and kernel.insight_cards_enabled()))

func _open_cards(offset: int=0) -> void:
	if not _cards_available() or not _started or _advancing or not _storage_intent.is_empty(): return
	if not kernel.personal_battle_view().get("battle_id","").is_empty() or kernel.military_battle_page().get("active",false) or not kernel.siege_page().get("siege_id","").is_empty(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false; _resident_short_action=""
	_player_plan_open=false; _dynamic_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _courses_open=false; _military_open=false; _family_open=false; _estate_id=""
	_study_teaching_open=false; _cards_open=true; _cards_offset=maxi(0,offset); _render()

## 返回不写世界，旧卡片控件即时脱离树。
func _close_cards() -> void:
	_cards_controller.close(); _cards_open=false; _study_teaching_open=false; _render()

## 统一有限query用于prepare、重绘和跨局同步，不取整态或构造另一份人物。
func _query_cards(args: Dictionary) -> Dictionary: return kernel.growth_cards(args)
func _query_actor_cards(args: Dictionary) -> Dictionary: return kernel.actor_cards(args)
func _query_insight_cards(args: Dictionary) -> Dictionary: return kernel.insight_cards(args)
## 页面prepare与重绘共用同一图鉴参数和隔离副本。
func _cards_view() -> Dictionary:
	var view: Dictionary={"ok":true,"actor_id":kernel.command_context().get("actor_id",""),"cards":[],"offset":0,"limit":32,"next_offset":null}
	if kernel.growth_cards_enabled():
		view=_cards_controller.page_view(_query_cards,_cards_offset)
		if not view.get("ok",true): return view
	if kernel.actor_cards_enabled():
		var actors: Dictionary=_actor_cards_controller.page_view(_query_actor_cards,_actor_cards_offset)
		if not actors.get("ok",true): return actors
		view["actor_cards"]=actors
	if kernel.insight_cards_enabled():
		var insights: Dictionary=_cards_controller.page_view(_query_insight_cards,_insight_cards_offset)
		if not insights.get("ok",true): return insights
		view["insight_cards"]=insights
	return view
## 收藏控制器只挂载到既有stage，分页返回原异步prepare。
func _render_cards(parent: Control) -> void: _cards_controller.open_view(self,parent,_cards_view(),_close_cards,_open_cards,_open_actor_cards_page,_open_insight_cards_page)

## 见闻在同一收藏页独立分页，继续经过原Main读屏障。
func _open_insight_cards_page(offset: int) -> void:
	if not _cards_open or _advancing or not _storage_intent.is_empty() or not kernel.insight_cards_enabled(): return
	_insight_cards_offset=maxi(0,offset); _render()

func _open_actor_cards_page(offset: int) -> void:
	if not _cards_open or _advancing or not _storage_intent.is_empty() or not kernel.actor_cards_enabled(): return
	_actor_cards_offset=maxi(0,offset); _render()

## 只有真实基础frame发布后调用；新世界/人物或取得数改变才读取各有限页，读失败不回滚提交。
func _prepare_card_discovery() -> bool:
	if not _resident_mode or not kernel.has_method("growth_cards_enabled") or not kernel.growth_cards_enabled(): return true
	var summary: Dictionary=kernel.growth_cards_summary(); var source: Dictionary=kernel.source_identity()
	if summary.get("enabled")!=true: return true
	var signature: Dictionary={"world_id":source.world_id,"execution_epoch":source.execution_epoch,"actor_id":summary.actor_id,"acquired_count":summary.acquired_count}
	if signature==_cards_last_observed and not _cards_controller.profile.needs_save(): return true
	# 当前准入只含已有角色；未来自由创建须以正式工厂标记传free，不能按属性猜模式。
	var result: Dictionary=_cards_controller.sync_after_commit(_query_cards,_card_source_context,"standard")
	if result.get("code")=="NativeAsyncRequired": return false
	if result.get("ok",false): _cards_last_observed=signature
	else: _message="跨局图鉴暂未保存，本局行旅仍已保留。"
	return true

## 人物资格只消费完整同提交来源；失败保留原文件，既有世界照常继续。
func _prepare_insight_discovery() -> bool:
	if not _resident_mode or not kernel.has_method("insight_cards_enabled") or not kernel.insight_cards_enabled(): return true
	var mode: String=kernel.insight_cards_creation_mode()
	if not mode in ["standard","free"]: _message="见闻来源暂不可识别，本局经历仍已保留。"; return true
	var summary: Dictionary=kernel.insight_cards_summary();var source: Dictionary=kernel.source_identity()
	if summary.get("enabled")!=true: return true
	var signature: Dictionary={"world_id":source.world_id,"execution_epoch":source.execution_epoch,"actor_id":summary.actor_id,"acquired_count":summary.acquired_count}
	if signature==_insight_cards_last_observed and not _cards_controller.profile.needs_save(): return true
	var result: Dictionary=_cards_controller.sync_insights_after_commit(_query_insight_cards,_card_source_context,mode)
	if result.get("code")=="NativeAsyncRequired": return false
	if result.get("ok",false): _insight_cards_last_observed=signature
	else: _message="跨局见闻暂未保存，本局经历仍已保留。"
	return true

## 人物卡资格继续使用其独立来源账，不借见闻发现授予可选人物。
func _prepare_actor_unlocks() -> bool:
	if not _resident_mode or not kernel.has_method("actor_cards_enabled") or not kernel.actor_cards_enabled(): return true
	var mode: String=kernel.actor_cards_creation_mode()
	if mode.is_empty():
		_message="当前人生来源尚不支持记录人物解锁资格。"; return true
	var summary: Dictionary=kernel.actor_cards_summary(); var source: Dictionary=kernel.source_identity()
	if summary.get("enabled")!=true: return true
	var signature: Dictionary={"world_id":source.world_id,"execution_epoch":source.execution_epoch,"actor_id":summary.actor_id,"acquired_count":summary.acquired_count}
	if signature==_actor_cards_last_observed and not _actor_cards_controller.profile.needs_save(): return true
	var result: Dictionary=_actor_cards_controller.sync_after_commit(_query_actor_cards,_card_source_context,mode)
	if result.get("code")=="NativeAsyncRequired": return false
	if result.get("ok",false): _actor_cards_last_observed=signature
	else: _message="人物解锁记录暂未保存，本局委托成果仍已保留。"
	return true

## 同人物/世界/执行代/revision绑定纯读；返回隔离字典，供跨页期间复核。
func _card_source_context() -> Dictionary:
	var context: Dictionary=kernel.command_context().duplicate(true)
	context.merge(kernel.source_identity(),true)
	return context

## 关闭只读总览恢复当前场所或课程；不取消动作、不重发课时或结算经验。
func _close_ordinary_skills() -> void:
	_ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _render()

## 生成已知事实的只读文本，不改变时间、合同或财物。
func journal_text() -> String:
	if _resident_mode:
		var view: Dictionary=kernel.demand_view("journal_text")
		return view.text if view.get("ok",false) else _reason(view.get("code","Unknown"))
	return kernel.journal_text()

## 选槽只锁定界面上下文，时间来自银行文件；列出和取消不创建世界或改写槽。
func _open_slots(mode: String) -> Dictionary:
	if _resident_mode and (kernel.is_busy() or _resident_adapter.is_busy()): return {"ok":false,"code":"ActorBusy"}
	if mode not in ["save","load"] or (mode=="save" and not _started): return {"ok":false,"code":"NoWorld"}
	if _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_stop_journey_auto(); _rest_auto=false
	_invalidate_scenario(); _invalidate_appearance(); _invalidate_slots()
	_continuing=mode=="load" and not _started
	if not _started: storage.configure({},null,_resident_storage_profile())
	_slot_context={"mode":mode,"identity":_slot_world_identity(),"epoch":_slot_epoch,"resident":_resident_storage_profile()}
	return _request_storage(mode+"_list","list",{})

## 标题明确选择地方旅途的实际银行格式，不依赖开发开关，也不创建默认候选世界。
func _continue_dynamic() -> void:
	if _started or _advancing or not _storage_intent.is_empty(): return
	_resident_title_resume=true
	var opened: Dictionary=_open_slots("load")
	if not opened.get("ok",false): _message="无法读取地方旅途："+_reason(opened.get("code","Unknown")); _render()

## 空标题只配置常驻物理槽格式，不安装默认世界；运行中保持现有后端的存储类型。
func _resident_storage_profile() -> bool:
	return _resident_mode or (not _started and (_resident_title_resume or _resident_requested()))

## 当前人物、内容和页面共同约束候选；同revision的另一页面也不能提交旧选择。
func _slot_world_identity() -> Dictionary:
	if _resident_mode:
		var identity: Dictionary=kernel.source_identity()
		identity.started=_started; identity.scene_epoch=_scene_counter
		return identity
	var state: Dictionary=kernel.snapshot()
	var content: Dictionary=kernel.content()
	return {"started":_started,"scene_epoch":_scene_counter,"revision":state.get("revision",-1),"actor_id":state.get("player_actor_id",""),"content_digest":content.get("content_digest",""),"pack_set_digest":content.get("pack_set_digest","")}

## 确定槽名沿原生固定命名范围，界面不接受自定义文件路径。
func _manual_slot_ids() -> Array:
	var result: Array=[]
	for index in range(1,11): result.append("manual-"+str(index))
	return result

## 兼容性来自同一精确解析器；正文与当前世界匹配还须确认后的完整decode，不能把旧v1信息不足标损坏。
func _slot_status(row: Dictionary, installed: Dictionary, registry: Dictionary, mode: String) -> String:
	if not row.get("ok",false): return str(row.get("code","InvalidSaveMetadata"))
	if _resident_storage_profile(): return "" # 常驻列表只报告物理代数，确认后由唯一worker做完整语义准入。
	if not installed.ok: return str(installed.get("code","MissingContent"))
	var resolved: Dictionary=ContentResolver.resolve(row.metadata,installed.catalogs,registry)
	if not resolved.ok: return str(resolved.get("code","IncompatibleContent"))
	if mode=="save":
		var current: Dictionary=kernel.content()
		for key in ["content_id","content_version","content_digest"]:
			if current.get(key)!=resolved.catalog.get(key): return "IncompatibleContent"
		if row.metadata.get("format_version")==2 and current.get("pack_set_digest","")!=resolved.catalog.pack_set_digest: return "PackDigestMismatch"
	return ""

## 用实际物理列表建立独立窗口；空槽仅建议，选择与确认以前绝不写入。
func _show_slots(listed: Dictionary) -> void:
	if _slot_context.get("identity")!=_slot_world_identity(): _slot_failed({"ok":false,"code":"StalePreview"}); return
	var mode: String=_slot_context.mode
	var installed: Dictionary={} if _resident_storage_profile() else Catalog.installed_catalogs()
	var prepared: Dictionary={"ok":true,"registry":{}} if _resident_storage_profile() else kernel.prepare_restore_registry()
	if not prepared.ok: _slot_failed(prepared); return
	var registry: Dictionary={} if _resident_storage_profile() else (kernel.save_context().registry if _started else prepared.registry)
	var dialog=Window.new(); dialog.name="StorageSlotWindow"; dialog.title="保存行旅 · 选择存档槽" if mode=="save" else "读取行旅 · 选择存档槽"
	dialog.transient=true; dialog.exclusive=true; dialog.min_size=Vector2i(420,420)
	add_child(dialog)
	var scroll=ScrollContainer.new(); dialog.add_child(scroll); scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var box=VBoxContainer.new(); box.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(box)
	_slot_dialog=dialog
	var occupied: Dictionary={}; var epoch: int=_slot_epoch
	for row in listed.slots:
		var slot: String=row.slot
		if not slot in _manual_slot_ids() and mode=="save": continue
		occupied[slot]=true
		var code: String=_slot_status(row,installed,registry,mode)
		var metadata: Dictionary=row.get("metadata",{})
		var identity: String="行旅存档" if _resident_storage_profile() else str(metadata.get("content_id","未知内容"))
		if metadata.get("pack_set") is Dictionary:
			var parts: PackedStringArray=[]
			for pack in metadata.pack_set.get("packs",[]): parts.append(str(pack.id)+"@"+str(pack.version))
			identity=" + ".join(parts)
		var scenario: String="内容与剧本待读取时核对" if _resident_storage_profile() else str(metadata.get("scenario_id","旧格式 · 剧本待读取"))
		var modified: int=int(row.get("modified_unix",0))
		var stamp: String=Time.get_datetime_string_from_unix_time(modified).replace("T"," ")+" UTC" if modified>0 else "未知"
		var caption: String=slot+" · "+identity.left(120)+"\n"+scenario+" · 文件保存时间："+stamp+"\n"+("确认时完整校验" if code.is_empty() else _reason(code))
		var candidate: Dictionary={"slot":slot,"generation":int(row.get("generation",0)),"ok":code.is_empty(),"new":false}
		var button: Button=_button(box,caption,"slot_select",slot,_select_slot.bind(candidate,dialog,epoch),not code.is_empty())
		button.tooltip_text=identity+" · "+code
	if mode=="save":
		var free_slot: String=""
		for slot in _manual_slot_ids():
			if not occupied.has(slot): free_slot=slot; break
		if free_slot.is_empty(): _text(box,"十个手动槽均已占用。不会自动覆盖或删除；可取消后导出文件。",16)
		else: _button(box,"新存档槽 · "+free_slot+"（建议，须选择确认）","slot_select",free_slot,_select_slot.bind({"slot":free_slot,"generation":0,"ok":true,"new":true},dialog,epoch))
	elif listed.slots.is_empty(): _text(box,"尚无可读取的存档。",18)
	var confirm: Button=_button(box,"保存到所选槽" if mode=="save" else "读取所选槽","slot_confirm","",_confirm_slot.bind(dialog,epoch),true)
	confirm.name="SlotConfirm"
	_button(box,"取消","slot_cancel","",_cancel_slots.bind(dialog,epoch))
	dialog.close_requested.connect(_cancel_slots.bind(dialog,epoch))
	dialog.popup_centered(Vector2i(760,660)); _ui_font_scale.apply_subtree(dialog)

## 候选必须来自仍活跃的当前窗口；按钮选择只存独立副本，不改变CAS版本。
func _select_slot(candidate: Dictionary, dialog: Window, epoch: int) -> void:
	if not _slot_window_current(dialog,epoch) or not candidate.get("ok",false): return
	_slot_choice=candidate.duplicate(true)
	var confirm: Button=dialog.find_child("SlotConfirm",true,false)
	if confirm!=null: confirm.disabled=false

## 关闭中、脱离树及过期窗口均不能发起读写，世界变化交确认守卫单独拒绝。
func _slot_window_current(dialog: Window, epoch: int) -> bool:
	return epoch==_slot_epoch and dialog==_slot_dialog and is_instance_valid(dialog) and dialog.is_inside_tree() and not dialog.is_queued_for_deletion() and dialog.visible

## 确认后仍先inspect同槽和已显示generation；请求上下文复制给唯一ticket，不能换包或槽。
func _confirm_slot(dialog: Window, epoch: int) -> void:
	if not _slot_window_current(dialog,epoch) or _slot_choice.is_empty() or not _storage_intent.is_empty(): return
	if _slot_context.identity!=_slot_world_identity():
		_invalidate_slots(); _slot_failed({"ok":false,"code":"StalePreview"}); return
	var pending: Dictionary=_slot_context.duplicate(true)
	pending.merge(_slot_choice,true)
	_invalidate_slots()
	pending.epoch=_slot_epoch
	_pending_slot=pending
	var accepted: Dictionary=_request_storage(pending.mode+"_check","inspect",{"slot":pending.slot})
	if not accepted.ok: _pending_slot={}

## 取消只撤销当前窗口；不清另一在途请求，不创建默认世界或删除任何槽。
func _cancel_slots(dialog: Window, epoch: int) -> void:
	if not _slot_window_current(dialog,epoch): return
	_invalidate_slots(); _continuing=false
	if is_instance_valid(_resident_load_candidate):
		_resident_discard_load_candidate(); _render()

## 作废旧选择并安全隐藏窗口；确认流程另持独立请求副本。
func _invalidate_slots() -> void:
	_slot_epoch+=1
	_slot_choice={}
	var dialog: Window=_slot_dialog
	_slot_dialog=null
	if is_instance_valid(dialog): dialog.hide(); dialog.queue_free()

## 所有确认失败只更新说明，保留当前世界、已选槽与磁盘银行。
func _slot_failed(result: Dictionary) -> void:
	last_storage_result=result.duplicate(true); _continuing=false
	_message="存档操作未完成："+_reason(result.get("code","Unknown"))
	if is_instance_valid(_resident_load_candidate): _resident_discard_load_candidate()
	_render()

## 复核列出时的代数与世界身份；新槽要求NotFound，旧槽写入先按当前受信上下文完整decode。
func _checked_slot(result: Dictionary) -> void:
	var pending: Dictionary=_pending_slot.duplicate(true); _pending_slot={}
	if pending.is_empty() or pending.get("epoch")!=_slot_epoch or pending.get("identity")!=_slot_world_identity(): _slot_failed({"ok":false,"code":"StalePreview"}); return
	var actual: int=int(result.get("generation",0)) if result.ok else 0
	if not result.ok and not (pending.mode=="save" and pending.new and result.get("code")=="NotFound"):
		_slot_failed(result); return
	if actual!=pending.generation or (pending.new and result.ok): _slot_failed({"ok":false,"code":"GenerationConflict"}); return
	if pending.get("resident",false):
		_resident_slot=pending.duplicate(true); _resident_slot.previous=result.duplicate(true)
		_storage_intent="resident_cold_wait"; _advancing=true; return
	if pending.mode=="save":
		if result.ok:
			var context: Dictionary=kernel.save_context()
			var checked: Dictionary=Codec.decode(result.text,context.catalog,context.registry)
			if not checked.ok: _slot_failed(checked); return
		_selected_slot=pending.slot; _save_generation=actual
		_write_snapshot()
	else:
		var decoded: Dictionary=_restore_text(result.text)
		if not decoded.ok: _slot_failed(decoded); return
		_selected_slot=pending.slot; _save_generation=actual; _started=true; _continuing=false
		_reset_player_plan_entry()
		_reset_trade_view(); journey_view.reset(); _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _family_open=false; _military_open=false
		last_storage_result=decoded; _message="已恢复此前的旅途。"; _render()

## 首次保存时读取槽位版本；后续写入使用本页面已确认的版本号。
func _save() -> void:
	if _resident_mode:
		_queue_resident_slots("save"); return
	_rest_auto=false
	_stop_journey_auto()
	if not _started or _advancing or not _storage_intent.is_empty(): return
	if not OS.has_feature("web"):
		_open_slots("save"); return
	if _save_generation>=0:
		_write_snapshot()
		return
	_message="正在核对存档槽……"; status_label.text=_message
	_request_storage("save_read","get",{"slot":"manual-1"})

## 编码稳定世界并按已知版本写入，不静默采纳其他窗口的更新。
func _write_snapshot() -> void:
	var context: Dictionary=kernel.save_context()
	var slot: String=_selected_slot if not _selected_slot.is_empty() else "manual-1"
	var text: String=Codec.encode(context.state,_save_generation+1,slot,context.catalog,context.registry)
	if text.is_empty():
		_message="状态未通过校验，未保存。"; _storage_intent=""; _render(); return
	_request_storage("save_write","put",{"slot":slot,"text":text,"expected_generation":_save_generation})

## 读取最近的物理存档，再经过内容校验和世界状态校验恢复。
func _load() -> void:
	if _resident_mode:
		_queue_resident_slots("load"); return
	_rest_auto=false
	_stop_journey_auto()
	if not _started or _advancing or not _storage_intent.is_empty(): return
	if not OS.has_feature("web"):
		_open_slots("load"); return
	_request_storage("load","get" if OS.has_feature("web") else "inspect",{"slot":"manual-1"})

## 发起成功后才等待完成；被拒时只撤销本次尚未提交的意图，既有请求不被清除。
func _request_storage(intent: String, operation: String, args: Dictionary) -> Dictionary:
	if not _storage_intent.is_empty(): return {"ok":false,"code":"StorageBusy"}
	_storage_intent=intent
	var accepted: Dictionary=storage.request(operation,args)
	if not accepted.ok and _storage_intent==intent:
		_storage_intent=""
		_continuing=false
		last_storage_result=accepted.duplicate(true)
		_message="存储请求尚未开始："+_reason(accepted.get("code","Unknown"))
		_render()
	return accepted

## 处理真实存储回调；只有写入并读回成功后才显示保存完成。
func _stored(operation: String, result: Dictionary) -> void:
	if _resident_mode and _storage_intent=="resident_write":
		if operation!="put_resident": return
		_resident_slot_finished(result); return
	var expected: Dictionary={"save_list":"list","load_list":"list","save_check":"inspect","load_check":"inspect","save_read":"get","save_write":"put","load":"get" if OS.has_feature("web") else "inspect","import":"import","export":"export"}
	if not expected.has(_storage_intent) or operation!=expected[_storage_intent]: return
	last_storage_result=result.duplicate(true)
	if _storage_intent in ["save_list","load_list"]:
		_storage_intent=""
		if result.ok: _show_slots(result)
		else: _slot_failed(result)
		return
	if _storage_intent in ["save_check","load_check"]:
		_storage_intent=""; _checked_slot(result); return
	if _storage_intent=="export" and operation=="export":
		_storage_intent=""
		_message="已将存档交给浏览器下载。" if result.ok else "导出失败："+_reason(result.get("code","Unknown"))
		_render(); return
	if _storage_intent=="import" and operation=="import":
		_storage_intent=""
		if result.get("code")=="Cancelled":
			_message="已取消导入。"; _render(); return
		var was_started: bool=_started
		var restored: Dictionary=_restore_text(result.get("text","")) if result.ok else result
		last_storage_result=_apply_import(restored,was_started)
		return
	if _storage_intent=="save_read" and operation=="get":
		if not result.ok and result.get("code")!="NotFound":
			_message="不能覆盖这个槽："+_reason(result.get("code","Unknown")); _storage_intent=""; _render(); return
		_save_generation=result.get("generation",0)
		_storage_intent=""
		_write_snapshot()
		return
	if _storage_intent=="load" and result.ok:
		var decoded: Dictionary=_restore_text(result.text)
		if decoded.ok:
			_reset_trade_view()
			_save_generation=result.generation
			_started=true
		elif _continuing: _started=false
		last_storage_result=decoded
		_message="已恢复此前的旅途。" if decoded.ok else "读档失败："+_reason(decoded.get("code","Unknown"))
	elif _storage_intent=="save_write":
		if result.ok: _save_generation=result.generation
		_message="行旅已记下。" if result.ok else "保存未成功："+_reason(result.get("code","Unknown"))
	else:
		_message="存档读取未成功："+_reason(result.get("code","Unknown"))
		if _continuing: _started=false
	_storage_intent=""
	_continuing=false
	_render()

## 将常见错误转换成场景说明，未知错误仍保留诊断码。
func _reason(code: String) -> String:
	var reasons: Dictionary={"NotAtPlace":"你不在办理此事的地方","PersonAbsent":"此人现在不在场","RoleNotAuthorized":"对方无权办理这件事","ActorBusy":"你还在办理上一件事","ParticipantBusy":"对方正在忙","SourceUnknown":"还未得知这份托付","UnknownOffer":"找不到这份托付","InsufficientFunds":"经费不足","InsufficientCapacity":"携带空间不足","InsufficientItems":"物品数量不足","StaleQuote":"报价已有变化，请重新确认","InsufficientMoney":"钱不够，先另作打算","EmployerCannotPay":"店家眼下付不起工钱","ServiceNotOffered":"对方不办理这种事情","UnknownService":"没有这项约定","IncompatibleContent":"这是另一个内容版本的存档，请用原版本打开","NotFound":"尚无存档","GenerationConflict":"该槽已被另一个页面更新，请保留当前旅途"}
	reasons.merge({"MissingContent":"未安装此存档所需的准确内容版本，请安装原内容后再读取", "CheckpointCatalogMismatch":"存档内容身份与已安装版本不一致，当前旅途和原存档均已保留", "InvalidCheckpointSelection":"无法识别此存档的内容或人物身份，未载入存档"})
	reasons.merge({"NotAtCityExit":"请先走到城门，再安排出行", "AlreadyThere":"已经在这处据点", "InvalidDestination":"请选择可通行的地点", "InvalidWaypoints":"途经点不能重复，也不能与起终点冲突", "NoRoute":"当前没有可通行的路线", "TerrainBlocked":"前路不通，可以停步、改道或等候", "RoadBlocked":"道路暂时封锁，可以等候或折返", "TravelPaused":"已经停步，请先继续行走", "Stranded":"下一段路费不足，可等候或请求救助", "NotTravelling":"目前没有可调整的旅行行动"})
	reasons.merge({"InsufficientStock":"店中现货不足", "InsufficientGoods":"自己的行囊中没有这么多货物", "ShopInsufficientFunds":"店家眼下没有足够现钱", "ShopCapacity":"店中仓储已经放不下", "PublicGoodsRestricted":"这笔公款或公物不能用于所选交易", "NotTradeable":"这件物品不能在货单中交易"})
	reasons.merge({"AlreadyInParty":"此人已经有同行约定", "NotPartyLeader":"同行期间由领队安排路线", "PartyNeedsSettlement":"请抵达安全据点后再商量分开", "NotCompanion":"此人不在本次同行名单里", "EncounterPending":"请先处理眼前的会面", "EncounterResolved":"这次会面已经结束", "NoRoadDialogue":"对方示意先各自赶路", "AlreadyTalking":"正在与对方交谈", "RoadServiceDenied":"路上无法办理机构事务"})
	reasons.merge({"CompanionTrustRequired":"对方尚不信任你，不愿同行", "BattlePending":"请先处理当前战斗与结果", "WeaponRequired":"请先携带并装备自己的武器", "WeaponNotOwned":"行囊中没有属于你的这件武器", "EscortPersonAbsent":"受保护的人尚未在此等候", "EscortAlreadyPromised":"此人已经约定了另一趟护送", "EscortNotArrived":"尚未与受保护者同行抵达约定地点", "OfferAlreadyAccepted":"这份托付已经应承过", "TaskNotActive":"这份托付当前没有待办交接", "TeacherNotQualified":"对方尚未精通这项本领", "TeacherTrustRequired":"授艺人尚未信任你", "SkillAlreadyLearned":"已经学过，可在武学页继续修炼", "SkillNotLearned":"尚未学会这项本领", "TrainingNeedsRest":"伤势或疲劳妨碍修炼，请先歇息", "TeacherNeedsRest":"对方需要休养后才能授艺", "TrainingInterrupted":"这次修习因人员或地点变化中断", "StudentAlreadyQualified":"对方的这项本领已不低于你"})
	reasons.merge({"StudyAlreadyPending":"先恢复或结束原课程，再报名新课", "StudyResumeLevelLimit":"当前经验已达到原课程上限，请结束本课并另作修业安排", "StudyAlreadyPaused":"这门课程已经暂停", "StudyNotPaused":"这门课程正在进行", "StudyPauseUnsupported":"这门手动练习课程暂不支持暂停，可继续或结束课程", "StaleStudyAction":"原课程已经结束或发生变化，请重新查看", "StudyTeacherChanged":"原教师的授课条件已变化", "StudyGrowthChanged":"原课程的待结经验与当前经验冲突", "StudyPaymentBlocked":"原收款钱包当前不能继续接收课时费"})
	reasons.merge({"StudyCourseLevelLimit":"这处设施最多支持学到二级，请向合格教师继续求学", "StudyTeacherNotQualified":"这位教师的本领等级不足，请另寻等级更高的教师"})
	reasons.merge({"MilitaryPlanRequired":"请先为参战各部确认本轮军议", "SiegePlanRequired":"请先确认本轮围城军议", "SiegePending":"请先处理当前围城军议与结果"})
	reasons.merge({"EstateAuthorityRequired":"须由庄主或管事办理", "EstateActorUnavailable":"请在庄内办妥当前事务后再安排", "EstateManagerUnavailable":"受任者须在庄内空闲，且未被借调", "EstateResidentUnavailable":"此人外出、忙碌或无法受命", "EstateManagerHasDuty":"管事需专心主持庄务", "EstateFacilityBusy":"此项营造尚未完成", "EstateFacilityMaxLevel":"此设施已达到当前可建规模", "InsufficientEstateMaterials":"仓中建材不足，可从市集购买后存入", "InsufficientOwnedEstateGoods":"没有足够可领取的私产", "EstateStorageFull":"接收仓储或行囊空间不足", "EstateFundingAuthority":"只有庄主能将本人私钱存入钱柜"})
	reasons.merge({"FamilyUnavailable":"本剧本尚未开放家事", "FamilyHeirRequired":"请先指定一位成年继任人", "FamilyHeirNotAdult":"子女尚未到约定的继任年岁", "FamilyNotOwnChild":"须指定自己正式收养的子女", "FamilyNotTogether":"请与继任人同在一处办理", "FamilyRetirementAgeRequired":"尚未到可退隐的年岁", "FamilyGuardianAgeRequired":"年龄尚不符合监护与辈分要求", "FamilyPartyPending":"请先与同行者商妥分开", "FamilyContractPending":"请先结清未完成的托付", "FamilyConversationPending":"请先结束其他谈话", "FamilyMilitaryPending":"请先办理部曲归庄", "FamilyMilitaryOrOfficePending":"退隐前请交接公职与军务", "FamilyAssetsReserved":"还有钱物正在办理交割", "FamilyHeirCapacityExceeded":"继任人的行囊装不下，请先整理私物", "FamilyServiceNotOffered":"对方不办理收养事务", "FamilyEstateRetainersAway":"尚有部曲外出未归", "FamilyEstateConstructionPending":"庄中营造尚未办妥", "FamilyForeignCargo":"请先交清携带的公物与他人物品", "FamilyPrivateAssetsElsewhere":"请先收回留在外面的私产", "FamilyAlreadyRetired":"已经完成退隐交接", "FamilyAdoptionInterrupted":"手续因人员或场所变化中断", "RestNeedsOwnEstate":"须在自己的庄园安排长期休整", "RestNotAtEstate":"请先实际回到庄园", "RestNeedsQuiet":"请先结束交谈或军团、同行安排", "RestFoodShortage":"庄中口粮不足，休整已停止", "RestInterrupted":"当前处境已不能继续休整", "NotResting":"目前没有长期休整安排", "InvalidRestDuration":"每次休整可安排一至七百二十个半日"})
	reasons.merge({"MilitaryRequestRoundLimit":"本轮请求次数已用完", "MilitaryRequestKeyConflict":"这次请求标识已用于另一意图，请刷新", "MilitaryRequestBoundaryClosed":"本轮军议已经提交", "MilitaryRequestRequiresSubordinate":"仅部将可向本军统帅请示", "MilitaryRequestPolicyRequired":"当前剧本未启用部将请求", "MilitaryRequestEnemyRequired":"请从本场敌军名册选择目标"})
	return reasons.get(code,code)


## 先准备正确的内容上下文再继续游戏；坏档或缺档保持在标题界面。
func _continue() -> void:
	_resident_title_resume=false
	if _advancing or not _storage_intent.is_empty(): return
	if not OS.has_feature("web"):
		_open_slots("load"); return
	_invalidate_scenario(); _invalidate_appearance()
	_continuing=true
	storage.configure({})
	_request_storage("load","get" if OS.has_feature("web") else "inspect",{"slot":"manual-1"})

## 提示未保存进展后才返回标题，不删除磁盘上的存档。
func _title_prompt() -> void:
	if not _storage_intent.is_empty(): return
	_rest_auto=false
	_stop_journey_auto()
	var dialog=ConfirmationDialog.new()
	dialog.title="返回标题"
	dialog.get_ok_button().text="返回标题"
	dialog.get_cancel_button().text="取消"
	dialog.dialog_text="尚未保存的进展不会保留。确认前请先保存或导出存档。"
	add_child(dialog)
	dialog.confirmed.connect(func():
		if _resident_mode:
			_resident_adapter.reset()
		_resident_pending_slot_mode=""
		_reset_player_plan_entry()
		_started=false; _message=""; _resident_rendering=true; _render(); _resident_rendering=false; dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	dialog.popup_centered(Vector2i(540,180))

## 通过文件选择器确定导出位置，玩家无需安装开发工具。
func _choose_export() -> void:
	_rest_auto=false
	_stop_journey_auto()
	if not _started or _advancing or not _storage_intent.is_empty(): return
	if OS.has_feature("web"):
		var context: Dictionary=kernel.save_context()
		var text: String=Codec.encode(context.state,0,"export",context.catalog,context.registry)
		if text.is_empty():
			_message="状态未通过校验，未导出。"; _render(); return
		_request_storage("export","export",{"text":text,"filename":"hanmo-journey.hmsave"})
		return
	var dialog=LocalizedFileDialog.new()
	dialog.access=FileDialog.ACCESS_FILESYSTEM
	dialog.file_mode=FileDialog.FILE_MODE_SAVE_FILE
	dialog.filters=PackedStringArray(["*.hmsave ; 汉末行记存档"])
	dialog.current_file="hanmo-journey.hmsave"
	add_child(dialog)
	dialog.file_selected.connect(func(path: String):
		export_snapshot(path); dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	dialog.popup_centered(Vector2i(880,580))

## 选择导入文件；完成校验前不替换现有世界或磁盘槽位。
func _choose_import() -> void:
	_rest_auto=false
	_stop_journey_auto()
	if _advancing or not _storage_intent.is_empty(): return
	if OS.has_feature("web"):
		_request_storage("import","import",{})
		return
	var dialog=LocalizedFileDialog.new()
	dialog.access=FileDialog.ACCESS_FILESYSTEM
	dialog.file_mode=FileDialog.FILE_MODE_OPEN_FILE
	dialog.filters=PackedStringArray(["*.hmsave ; 汉末行记存档"])
	add_child(dialog)
	dialog.file_selected.connect(func(path: String):
		import_snapshot(path); dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	dialog.popup_centered(Vector2i(880,580))

## 导出稳定快照，只有实际写入并校验成功才反馈完成。
func export_snapshot(path: String) -> Dictionary:
	_stop_journey_auto()
	if not _started or _advancing: return {"ok":false,"code":"ActorBusy"}
	var context: Dictionary=kernel.save_context()
	var result: Dictionary=SaveFiles.write_world(context.state,path,context.catalog,context.registry)
	_message="存档已导出："+path if result.ok else "导出失败："+_reason(result.get("code","Unknown"))
	_render()
	return result

## 在候选状态中解码导入内容；错误文件不覆盖原有旅途。
func import_snapshot(path: String) -> Dictionary:
	_stop_journey_auto()
	if _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	_invalidate_scenario(); _invalidate_appearance(); _invalidate_slots()
	var was_started: bool=_started
	var raw: Dictionary=SaveFiles.read_text(path)
	var result: Dictionary=_restore_text(raw.text) if raw.ok else raw
	return _apply_import(result,was_started)

## 同一封装按元信息选原目录，再按候选founder投影一次；全部校验与表现就绪前不替换世界。
func _restore_text(text: String) -> Dictionary:
	var metadata: Dictionary=Codec.inspect_metadata(text)
	if not metadata.ok: return metadata
	var prepared: Dictionary=kernel.prepare_restore_registry()
	if not prepared.ok: return prepared
	var installed: Dictionary=Catalog.installed_catalogs()
	if not installed.ok: return installed
	var matched: Dictionary=ContentResolver.resolve(metadata.metadata,installed.catalogs,prepared.registry)
	if not matched.ok: return matched
	var selection: Dictionary=Codec.inspect_restore_selection(text)
	if not selection.ok: return selection
	var effective: Dictionary=ScenarioSnapshot.resolve(matched.catalog,selection.scenario_id,selection.founder_actor_id)
	if not effective.ok: return effective
	var decoded: Dictionary=Codec.decode(text,effective.catalog,prepared.registry)
	if not decoded.ok: return decoded
	var art: Dictionary=Art.prepare_content(effective.catalog)
	if not art.ok: return art
	var restored: Dictionary=kernel.restore_with_catalog(decoded.state,effective.catalog,prepared.registry)
	if not restored.ok: return restored
	# 成功恢复才使个人战输入代次前进；相同Kernel与世界标识也不能沿用读档前草稿。
	set_meta("personal_battle_generation",int(get_meta("personal_battle_generation",0))+1)
	Art.commit_content(art)
	_data=kernel.content().data
	storage.configure(effective.catalog,prepared.registry)
	return {"ok":true,"generation":decoded.generation,"save_id":decoded.save_id}

## 原生文件与浏览器文本共用候选恢复；错误不覆盖旅途或槽位，成功后清除旧世界的界面选择。
func _apply_import(result: Dictionary, was_started: bool) -> Dictionary:
	if result.ok:
		_reset_player_plan_entry()
		_started=true; _save_generation=-1; _selected_slot=""
		_reset_trade_view()
		journey_view.reset(); _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _family_open=false; _military_open=false
		_message="已导入旅途。文件已读取；点击保存才会写入本机存档槽。"
	else:
		_started=was_started
		_message="导入失败，原有旅途未被覆盖："+_reason(result.get("code","Unknown"))
	_render()
	return result

## 只切换声音表现，不改变时间、财物、关系或待执行行动。
func _toggle_audio() -> void:
	_ambience.toggle()
	_render()

## 在当前真实庄园打开账务与分工；出门后旧页面不再提供远程经营操作。
func _open_estate(id: String) -> void:
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	_rest_auto=false
	if not _resident_mode and not kernel.estate_available(id): return
	_stop_journey_auto(); _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _family_open=false; _military_open=false; _estate_id=id; _render()

## 关闭庄务回到真实庄院，不推进半日或取消已经开始的建设。
func _close_estate() -> void:
	_estate_id=""; _render()

## 打开只读舆图；场所内可以看路线，但启程仍需通过城门规则。
func _open_map() -> void:
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	_rest_auto=false
	_stop_journey_auto(); _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _family_open=false; _military_open=false; _estate_id=""; journey_view.opened=true; _render()
## 关闭地图返回当前场景；道路上仍由地图显示实际行程。
func _close_map() -> void:
	_stop_journey_auto(); journey_view.opened=false; _render()
## 保存地图选择并停止自动请求，选点本身不改变位置、费用或随机状态。
func _map_select(id: String) -> void:
	if not _data.settlements.has(id): return
	_stop_journey_auto(); journey_view.selected=id; journey_view.selected_point=[]; _message=""; _render()
## 仅保存地图的整数候选落点，实际可达性仍由世界预览检查。
func _map_select_point(point: Array) -> void:
	_stop_journey_auto(); journey_view.selected=""; journey_view.selected_point=point.duplicate(); _message=""; _render()

## 以真实行动识别途中状态，自由落点停留不被误当作尚未完成的旅行。
func _travelling(state: Dictionary) -> bool:
	return ActionContext.travelling(state)

## 行程可保留在真实会面、个人战、军团战或围城下；此时含待确认结果都须先交还本人操作。
func _travel_needs_input(state: Dictionary) -> bool:
	return ActionContext.needs_input(state)

## 请求同一核心的完整路线预览，不在界面重复寻路算法。
func _map_quote() -> Dictionary:
	if journey_view.selected.is_empty() and journey_view.selected_point.is_empty(): return {"ok":false,"code":"InvalidDestination"}
	var context: Dictionary=kernel.action_context()
	var moving: bool=context.action_kind=="travel"
	var kind: String="reroute" if moving else "travel"
	var params: Dictionary={"destination":journey_view.selected if not journey_view.selected.is_empty() else journey_view.selected_point.duplicate(),"via":journey_view.via.duplicate()}
	var quote: Dictionary=kernel.preview(kind,params)
	return quote
## 确认所选路线后只提交标准命令，途中改道保留实际位置。
func _map_confirm() -> void:
	if not _map_quote().get("ok",false): return
	var context: Dictionary=kernel.action_context()
	var moving: bool=context.action_kind=="travel"
	var params: Dictionary={"destination":journey_view.selected if not journey_view.selected.is_empty() else journey_view.selected_point.duplicate(),"via":journey_view.via.duplicate()}
	journey_view.via=[]; journey_view.selected=""; journey_view.selected_point=[]; journey_view.opened=true
	_perform("reroute" if moving else "travel",params)
## 添加有序途经点；重复点和超过上限的选择不修改当前计划。
func _map_add_via() -> void:
	_stop_journey_auto()
	var id: String=journey_view.selected
	if id.is_empty() or id in journey_view.via or journey_view.via.size()>=8: return
	journey_view.via.append(id); journey_view.selected=""; _render()
## 清除未确认的途经点，不取消已经执行中的真实行程。
func _map_clear_via() -> void:
	_stop_journey_auto(); journey_view.via=[]; _render()
## 用户明确启用或停止自动赶路；暂停过的行动先按原规则恢复。
func _toggle_auto_travel() -> void:
	if _resident_mode:
		_resident_toggle_auto(); return
	var context: Dictionary=kernel.action_context()
	if kernel.action_needs_input(): _stop_journey_auto(); _render(); return
	if journey_view.auto_running:
		_stop_journey_auto(); _perform("pause_travel",{}); return
	if context.action_kind!="travel": return
	if context.action_kind!="travel": return
	if context.travel_paused: _perform("resume_travel",{})
	if not kernel.preview("advance").ok: return
	journey_view.start_auto(); _render()
## 每次调度只提交一次世界半日；决策边界、失效、保存和抵达后的迟来回调不能代替本人多走一步。
func _auto_step() -> void:
	if _resident_mode: return # 常驻许可仅来自带RunRef的新控制器，旧信号不得补发半日。
	if not journey_view.auto_running: return
	var context: Dictionary=kernel.action_context()
	if _advancing or not _storage_intent.is_empty() or context.action_kind!="travel" or kernel.action_needs_input(): _stop_journey_auto(); _render(); return
	_perform("advance",{})
	if journey_view.auto_running: journey_view.schedule_next()

## 只在当前实际场所打开家事；收养对象取此处岗位的现任人，是否能办理仍由规则检查。
func _open_family() -> void:
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	_military_open=false
	_rest_auto=false; _stop_journey_auto(); journey_view.opened=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""
	_family_context={} if _resident_mode else kernel.family_provider_context()
	if _resident_mode: _resident_provider_pending="family"
	_family_open=true; _render()

## 返回原场所仅关闭显示与自动推进，休整行动可以保留供随后保存或明确结束。
func _close_family() -> void:
	_stop_journey_auto()
	_rest_auto=false; _family_open=false; _military_open=false; _render()

## 自动休整只反复提交正常半日；启用前先核对真实行动，不能借开关跳过工期。
func _toggle_rest_auto() -> void:
	if _resident_mode:
		_resident_toggle_auto(); return
	if _rest_auto: _rest_auto=false; _render(); return
	var context: Dictionary=kernel.action_context()
	if context.action_kind!="long_rest" or _advancing or not _storage_intent.is_empty(): return
	_rest_auto=true; _rest_refresh_elapsed=0.0; _render()

## 每帧至多提交一个稳定半日，定期刷新可见进展；输入、存档或中断立即停止后续提交。
func _process(delta: float) -> void:
	if _resident_mode or _storage_intent in ["resident_cold_wait","resident_cold"]:
		_resident_process(); return
	if not _rest_auto: return
	if not _started or _advancing or not _storage_intent.is_empty(): _rest_auto=false; return
	var result: Dictionary=kernel.advance_one_tick()
	var context: Dictionary=kernel.action_context()
	if not result.ok:
		_rest_auto=false; _message="休整中断："+_reason(result.get("code","Unknown"))
	elif context.action_kind!="long_rest":
		_rest_auto=false; _message="这段休整已经结束。"
		for outcome in result.details.get("completed_actions",{}).values():
			if not outcome.get("success",true): _message=_reason(outcome.get("code","RestInterrupted"))
	_rest_refresh_elapsed+=delta
	if not _rest_auto or _rest_refresh_elapsed>=0.2:
		_rest_refresh_elapsed=0.0; _render()

## 营务入口保留实际地点与办理人；庄中借调和营中编组各自再由Kernel核验。
func _open_military_camp() -> void:
	_player_plan_open=false
	_dynamic_open=false
	_resident_short_action=""
	_rest_auto=false; _stop_journey_auto(); journey_view.opened=false
	_family_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _military_context={}
	_military_context={} if _resident_mode else kernel.military_camp_provider_context()
	if _resident_mode: _resident_provider_pending="military"
	_military_open=true; _render()

## 关闭营务仅返回真实场所，已提交的半日手续与军令继续留在同一世界。
func _close_military_camp() -> void:
	_military_open=false; _render()


## 常驻入口保持显式；首次沿原Factory/Schema创建一次seed，之后只持原生世界。
func _resident_requested() -> bool:
	return ProjectSettings.get_setting("development/native_resident_world_enabled",false)==true or "--native-resident" in OS.get_cmdline_user_args()

## 冷安装一次原世界并绑定同一worker；后续页面与命令不保GD世界副本。
func _start_resident(candidate: RefCounted, art: Dictionary, activate: bool=true, native_options: Dictionary={}) -> Dictionary:
	var backend_script=load("res://ui/native_resident_backend.gd")
	if backend_script==null: return {"ok":false,"code":"ResidentBackendUnavailable"}
	if _resident_mode and (kernel.is_busy() or _resident_adapter.is_busy()): return {"ok":false,"code":"ActorBusy"}
	var backend=backend_script.new(); add_child(backend)
	var installed: Dictionary=backend.install(candidate.content(),candidate.snapshot()) if native_options.is_empty() else backend.install_native(native_options["catalog"],native_options["request"])
	if not installed.get("ok",false):
		backend.queue_free(); _message="内容尚不能运行："+_reason(installed.get("code","Unknown")); _render(); return installed
	var next_adapter=ResidentJourneyAdapter.new(); add_child(next_adapter)
	var bound: Dictionary=next_adapter.bind_context(backend.transport(),backend.source_identity(),_scene_counter)
	if not bound.get("ok",false): next_adapter.queue_free(); backend.queue_free(); return bound
	if _resident_mode:
		_resident_adapter.reset(); _resident_adapter.queue_free(); kernel.queue_free()
	kernel=backend; _resident_mode=true
	_resident_adapter=next_adapter
	_resident_adapter.operation_completed.connect(_resident_run_completed)
	_resident_adapter.failed.connect(_resident_failed)
	_resident_adapter.stopped.connect(_resident_stopped)
	kernel.frame_ready.connect(_resident_frame_ready)
	kernel.command_completed.connect(_resident_command_completed)
	kernel.preview_ready.connect(_resident_preview_ready)
	kernel.page_reads_ready.connect(_resident_page_reads_ready)
	kernel.failed.connect(_resident_failed)
	_resident_bootstrap_loading=not activate; _resident_pending_art={} if activate else art
	if activate: Art.commit_content(art)
	_reset_player_plan_entry()
	journey_view.reset(); journey_view.opened=false
	_activity_draft={}; _resident_pending_slot_mode=""; set_meta("system_menu_open",false)
	_data=kernel.presentation_data(); _reset_trade_view()
	storage.configure(kernel.content(),null,true)
	_dynamic_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _family_open=false; _military_open=false; _courses_open=false; _rest_auto=false
	_resident_command={}; _resident_result={}; _resident_quote={}; _resident_hold={}; _resident_frame_pending=activate
	_resident_completion=Callable(); _resident_short_action=""; _resident_step_after_plan=false; _resident_journal_pending=false; _resident_provider_pending=""; _resident_trade_receipt_pending=""; _resident_page_error={}; _resident_read_cycle=false
	_save_generation=-1; _selected_slot=""; _started=activate; _advancing=activate; _message=""
	return {"ok":true,"pending":true}

## 场所近况入口只停止自动许可和打开有限页，不推进世界或取得后台活动控制。
func _open_dynamic_world() -> void:
	_player_plan_open=false
	if not _resident_mode or not kernel.dynamic_world_enabled(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false
	_self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _military_open=false; _family_open=false; _estate_id=""; _courses_open=false
	_dynamic_open=true; DynamicWorldPanel.prepare_draft(self); _render()

## 关闭近况只清显示选择；实际已接合同、在途货物与工资责任保持在core内。
func _close_dynamic_world() -> void:
	_dynamic_open=false; _render()

## 原读取/菜单入口统一撤销许可；旧JourneyView只保存地图选择，不再运行其Timer。
func _stop_journey_auto(reason: String="reading") -> void:
	_player_plan_start_epoch+=1; _player_plan_pending_start={}
	_resident_journal_pending=false
	_resident_short_action=""
	if _resident_mode: _rest_auto=false
	journey_view.stop_auto()
	if _resident_mode and is_instance_valid(_resident_adapter):
		var pending_read: bool=kernel.is_busy()
		kernel.invalidate() # 撤销链式读发布资格在优先cancel之前；已接收command仍由backend排空。
		if pending_read and _started: _resident_frame_pending=true
		_resident_adapter.suspend(reason)

## 仅为仍未接入的入口显示诊断并停止后续旅行许可。
func _resident_unavailable() -> void:
	_stop_journey_auto(); _message="此入口尚未接入当前行旅模式。"; _render()

## 已接受命令保留原页面请求序号与版本；取消完成后也不得用新版本重写它。
func _resident_perform(kind: String, params: Dictionary) -> void:
	if _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty() or not _resident_command.is_empty(): return
	# 已授半日会使用玩家下一个request_seq；忙时不可预占它给另一种命令。
	if _resident_adapter.is_busy() or kernel.is_busy():
		_message="当前操作尚未完成。"
		if is_instance_valid(status_label): status_label.text=_message
		return
	var context: Dictionary=kernel.command_context()
	if not context.get("ok",false): _resident_failed(context); return
	var accepted: Dictionary=_resident_submit_frozen({"kind":kind,"params":params.duplicate(true),"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
	if not accepted.get("ok",false): _resident_failed(accepted)

## 一次只消费/接纳一个正常操作；纯查询整组发布后才重新启动1.2秒许可Timer。
func _resident_process() -> void:
	if not _resident_cold_job.is_empty():
		_resident_poll_cold(); return
	if _storage_intent=="resident_cold_wait":
		if _resident_mode and (_resident_adapter.is_busy() or kernel.is_busy()): return
		_resident_begin_cold(); return
	if not _started or not _resident_mode or not is_instance_valid(_resident_adapter): return
	journey_view.auto_running=_resident_adapter.auto_running
	if _resident_adapter.is_busy() or kernel.is_busy(): return
	if not _storage_intent.is_empty() or not _started: return
	if not _resident_command.is_empty():
		var command: Dictionary=_resident_command; _resident_command={}
		var accepted: Dictionary=kernel.submit_async(command,_scene_counter)
		if not accepted.get("ok",false): _resident_failed(accepted)
		return
	if _resident_frame_pending:
		_resident_frame_pending=false
		var current: Dictionary=kernel.source_identity()
		_resident_adapter.update_context(current,_scene_counter)
		var held: Dictionary=_resident_adapter.hold_for_refresh(current)
		_resident_hold=held.get("token",{})
		var accepted: Dictionary=kernel.refresh(_scene_counter)
		if not accepted.get("ok",false): _resident_failed(accepted)
		return
	if _resident_journal_pending: _resident_finish_journal()
	if kernel.has_pending_reads():
		if _resident_hold.is_empty():
			var held: Dictionary=_resident_adapter.hold_for_refresh(kernel.source_identity())
			if not held.get("ok",false): _resident_failed(held); return
			_resident_hold=held.token
		var accepted: Dictionary=kernel.begin_pending_read(_scene_counter)
		if not accepted.get("ok",false): _resident_failed(accepted)
		return
	if not _resident_quote.is_empty():
		if _resident_hold.is_empty():
			var held: Dictionary=_resident_adapter.hold_for_refresh(kernel.source_identity())
			if not held.get("ok",false): _resident_failed(held); return
			_resident_hold=held.token
		var request: Dictionary=_resident_quote; _resident_quote={}
		var accepted: Dictionary=kernel.preview_async(request.kind,request.params,request.actor_id,_scene_counter)
		if not accepted.get("ok",false): _resident_failed(accepted)
		return
	if not _resident_pending_slot_mode.is_empty() and not _advancing:
		var mode: String=_resident_pending_slot_mode; _resident_pending_slot_mode=""
		var opened: Dictionary=_open_slots(mode)
		if not opened.get("ok",false): _resident_failed(opened)
		return
	if not _player_plan_pending_start.is_empty(): _resident_finish_player_plan_start()

## 接收唯一worker的真实提交回执，等同源frame到齐后交付UI回调。
func _resident_command_completed(kind: String, result: Dictionary, source: Dictionary, current: Dictionary) -> void:
	if not _resident_mode or current!=kernel.source_identity(): return
	if not _player_plan_pending_start.is_empty():
		if kind=="player_plan_budget" and _player_plan_pending_start.get("phase")=="budget_submitted" and _player_plan_pending_start.get("source")==source and result.get("ok",false):
			_player_plan_pending_start.phase="budget_committed"; _player_plan_pending_start.source=current.duplicate(true)
		else: _player_plan_pending_start={}
	_resident_result={"kind":kind,"result":result.duplicate(true),"completion":_resident_completion}
	_resident_completion=Callable()
	_resident_adapter.update_context(current,_scene_counter)
	_resident_frame_pending=true

## 保留许可控制器当前身份和刷新持有，不在结果信号里再授半日。
func _resident_run_completed(kind: String, result: Dictionary, source: Dictionary, current: Dictionary) -> void:
	if not _resident_mode or not _started: return
	var adopted: Dictionary=kernel.adopt_source(current)
	if not adopted.get("ok",false): _resident_failed(adopted); return
	journey_view.auto_running=_resident_adapter.auto_running
	if kernel.has_method("invalidate_continuation_state"): kernel.invalidate_continuation_state()
	if kind=="grant":
		var held: Dictionary=_resident_adapter.hold_for_refresh(current)
		_resident_hold=held.get("token",{}); _resident_frame_pending=true; _advancing=true
	elif kind=="begin_run":
		_resident_frame_pending=true
	elif kind=="cancel_run":
		_resident_frame_pending=true
	if not result.get("ok",false): _message="此时不能这样做："+_reason(result.get("code","Unknown"))

## 完整同源frame到达后刷新真实选中页，再按手动门决定是否续已提交行动。
func _resident_frame_ready(source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or not _started or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	_advancing=false
	var finished: Dictionary=_resident_result; _resident_result={}
	if not finished.is_empty():
		var result: Dictionary=finished.result
		_message="" if result.get("ok",false) else "此时不能这样做："+_reason(result.get("code","Unknown"))
		if result.get("ok",false):
			if finished.kind in ["study_skill","military_train"]: _courses_open=false
			if finished.kind in ["military_plan","siege_plan"]: _resident_step_after_plan=true
			if kernel.action_context().get("action_kind")!="long_rest": _rest_auto=false
			if finished.kind=="trade": _resident_trade_receipt_pending=result.get("details",{}).get("action_id","")
			var completion: Callable=finished.get("completion",Callable())
			if completion.is_valid(): completion.call(result.duplicate(true))
	journey_view.auto_running=_resident_adapter.auto_running
	_resident_rendering=true; _render(); _resident_rendering=false
	_resident_adapter.update_context(source,_scene_counter)
	if not _resident_hold.is_empty() and _resident_quote.is_empty() and not kernel.has_pending_reads():
		_resident_adapter.release_refresh(_resident_hold); _resident_hold={}
	if not finished.is_empty():
		var result: Dictionary=finished.result
		if result.get("ok",false):
			var context: Dictionary=kernel.action_context()
			if (finished.kind=="wait" and not result.get("details",{}).has("action_id")) or finished.kind.begins_with("player_plan_") or finished.kind in ["advance","family_adopt_existing","naval_battle_plan","naval_capture","naval_claim_decline","naval_battle_continue","naval_form","naval_disband","military_request","party_accept","party_leave","decline_offer","decline_runtime_offer","task_personal_advance","military_plan","siege_plan","study_pause","study_resume","study_cancel","study_challenge_choose","study_challenge_exit","study_etiquette_choose","study_etiquette_exit"]:
				# 本人新短行动仍自动走首半日；已完成半日的附属wait不得继承NPC新行动。
				_resident_short_action=""
			elif not context.get("action_id","").is_empty() and context.get("action_kind","") not in ["travel","rescue","long_rest"]:
				_resident_short_action=context.action_id
		else: _resident_short_action=""
	_resident_continue_short()

## 仅当前页面消费纯预览结果，过期source或scene不能刷新控件。
func _resident_preview_ready(kind: String, params: Dictionary, result: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	_resident_rendering=true; _render(); _resident_rendering=false
	_resident_adapter.update_context(source,_scene_counter)
	if not _resident_hold.is_empty() and _resident_quote.is_empty() and not kernel.has_pending_reads():
		_resident_adapter.release_refresh(_resident_hold); _resident_hold={}

## 兼容原页入口只启动明确预算；停止自动许可不会偷偷暂停原旅行或课程。
func _resident_toggle_auto() -> void:
	if _resident_adapter.auto_running:
		_stop_activity_continuation(); return
	var context: Dictionary=kernel.action_context()
	_begin_activity_continuation(context.get("actor_id",""),context.get("action_id",""),kernel.source_identity(),_scene_counter)

## 草案绑定当前人物和原行动；换行动或换世界不继承上一项选择。
func _activity_budget_draft(context: Dictionary) -> Dictionary:
	var source: Dictionary=kernel.source_identity()
	var key: Dictionary={"world_id":source.get("world_id"),"execution_epoch":source.get("execution_epoch"),"actor_id":context.get("actor_id"),"action_id":context.get("action_id")}
	if _activity_draft.get("key")!=key: _activity_draft={"key":key,"half_days":1,"money":0}
	return _activity_draft.duplicate(true)

## SpinBox只保存真整数的本页草案，编辑本身撤销旧许可，不提交半日。
func _set_activity_budget(value: float, field: String, actor_id: String, action_id: String, source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	var context: Dictionary=kernel.action_context()
	if context.get("actor_id")!=actor_id or context.get("action_id")!=action_id or field not in ["half_days","money"]: return
	_activity_budget_draft(context)
	_activity_draft[field]=int(value)
	if _resident_adapter.auto_running: _stop_journey_auto("selection_changed")

## 显式确认按钮绑定原行动，报价就绪且未决输入已由本人处理后才创建新epoch许可。
func _begin_activity_continuation(actor_id: String, action_id: String, source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	var context: Dictionary=kernel.action_context()
	if context.get("actor_id")!=actor_id or context.get("action_id")!=action_id: return
	var draft: Dictionary=_activity_budget_draft(context)
	_start_activity_continuation(int(draft.half_days),int(draft.money))

## 读取原生停止上下文只用于显示；不会把本地草案冒充已接受的钱包或剩余额度。
func _activity_continuation_state() -> Dictionary:
	if not _resident_mode: return {}
	var state: Dictionary=_resident_adapter.continuation_state()
	if state.is_empty() and kernel.has_method("continuation_state"):
		var restored: Dictionary=kernel.continuation_state()
		if restored.get("ok",false) and restored.get("available",false) and restored.get("continuation") is Dictionary: state=restored.continuation.duplicate(true)
	var context: Dictionary=kernel.action_context()
	if state.get("actor_id")!=context.get("actor_id") or state.get("action_id")!=context.get("action_id") or state.get("source_identity",{})!=kernel.source_identity(): return {}
	return state

## 恢复后必须再次按本人确认；剩余额度是原生快照的差值，旧run与permit从不复用。
func _resume_activity_continuation(expected: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	var state: Dictionary=_activity_continuation_state()
	if state.is_empty() or state!=expected: return
	_start_activity_continuation(maxi(0,state.get("max_half_days",0)-state.get("committed_half_days",0)),maxi(0,state.get("max_gross_outflow",0)-state.get("gross_outflow",0)),true)

## 自动许可只等实际未答题与待决会面；手动课原manual_window仍保留其逐半日语义。
func _activity_continuation_needs_input() -> bool:
	if kernel.action_needs_input(): return true
	var interaction: Dictionary=kernel.interaction_view()
	var context: Dictionary=kernel.action_context()
	var waiting_for_original: bool=interaction.get("phase")=="waiting" and not context.get("action_id","").is_empty() and interaction.get("pending_action_id")==context.action_id
	if interaction.get("phase","scene")!="scene" and not waiting_for_original: return true
	var activity: Dictionary=kernel.study_activity()
	if activity.get("active",false) and activity.get("kind")=="study_skill":
		if activity.has("challenge_pending"): return activity.challenge_pending
		if "study" in activity.get("challenge_scopes",[]):
			var challenge: Dictionary=kernel.study_challenge(activity.actor_id,activity.action_id)
			return not challenge.get("ok",false) or challenge.get("phase")!="completed"
	return false

## 三种行动共享唯一许可driver；底层恢复、答题与取消只能由各自现有按钮明确提交。
func _start_activity_continuation(half_days: int, money: int, resume_saved_scope: bool=false) -> void:
	if not _resident_mode or not _started or half_days<1 or money<0 or _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty(): return
	if not _player_plan_pending_start.is_empty(): return
	if kernel.is_busy() or kernel.has_pending_reads() or _resident_adapter.is_busy() or _resident_adapter.auto_running or not _resident_short_action.is_empty() or _resident_step_after_plan: return
	var context: Dictionary=kernel.action_context()
	if context.get("action_kind") not in ActivityContinuationControls.KINDS or context.get("action_id","").is_empty(): return
	if _activity_continuation_needs_input() or context.get("travel_paused",false) or kernel.study_activity().get("paused",false): return
	var quote: Dictionary=kernel.preview("advance")
	if not quote.get("ok",false):
		if quote.get("code")=="NativeAsyncRequired": _render()
		else: _resident_failed(quote)
		return
	_resident_short_action=""; _resident_step_after_plan=false; _rest_auto=false
	var accepted: Dictionary=_resident_adapter.start(context.actor_id,context.action_id,half_days,money,resume_saved_scope)
	if not accepted.get("ok",false): _resident_failed(accepted)
	journey_view.auto_running=_resident_adapter.auto_running

## 本人先明确提交计划预算；正常世界命令回执和稳定新页到齐以前不创建run。
func _begin_player_plan(ticks: int, money: int, source: Dictionary, scene_epoch: int) -> void:
	if not _player_plan_enabled() or not _player_plan_open or source!=kernel.source_identity() or scene_epoch!=_scene_counter or ticks<1 or money<0: return
	if _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty() or not _player_plan_pending_start.is_empty(): return
	if kernel.is_busy() or kernel.has_pending_reads() or _resident_adapter.is_busy() or _resident_adapter.auto_running or not _resident_short_action.is_empty() or _resident_step_after_plan or not _resident_result.is_empty(): return
	var view: Dictionary=kernel.demand_view("player_plan")
	if view.get("ok",true)==false or not view.get("enabled",false) or view.get("plan_id","").is_empty(): return
	var context: Dictionary=kernel.command_context()
	if not context.get("ok",false) or context.actor_id!=view.get("actor_id") or context.revision!=source.revision: return
	var request: Dictionary={"kind":"player_plan_budget","params":{"max_additional_ticks":ticks,"max_gross_spend":money},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":source.revision}
	var accepted: Dictionary=_resident_submit_frozen(request)
	if not accepted.get("ok",false): _resident_failed(accepted); return
	_player_plan_pending_start={"phase":"budget_submitted","plan_id":view.plan_id,"actor_id":context.actor_id,"ticks":ticks,"money":money,"source":source.duplicate(true),"authorization_epoch":_player_plan_start_epoch}

## 只消费本次预算提交后的同源计划页；阅读、保存、失焦或其它命令都会使启动意图失效。
func _resident_finish_player_plan_start() -> void:
	if _player_plan_pending_start.get("phase")!="budget_committed": return
	if not _player_plan_enabled() or not _player_plan_open or _player_plan_pending_start.get("authorization_epoch")!=_player_plan_start_epoch:
		_player_plan_pending_start={}; return
	if _advancing or _resident_frame_pending or _resident_read_cycle or not _resident_hold.is_empty() or not _resident_result.is_empty() or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty(): return
	if kernel.is_busy() or kernel.has_pending_reads() or _resident_adapter.is_busy(): return
	var selected: Dictionary=_player_plan_pending_start.duplicate(true)
	if selected.source!=kernel.source_identity():
		_player_plan_pending_start={}; _message="安排条件已改变，请重新确认本次预算。"; _render(); return
	var view: Dictionary=kernel.demand_view("player_plan")
	if view.get("code")=="NativeAsyncRequired": return
	var approved: Dictionary=view.get("budget",{})
	if view.get("ok",true)==false or view.get("plan_id")!=selected.plan_id or view.get("actor_id")!=selected.actor_id or not view.get("enabled",false) or approved.get("max_additional_ticks")!=selected.ticks or approved.get("max_gross_spend")!=selected.money or approved.get("used_ticks")!=0 or approved.get("gross_spend")!=0:
		_player_plan_pending_start={}; _resident_failed({"ok":false,"code":"PlayerPlanBudgetMismatch"}); return
	_player_plan_pending_start={}; _resident_short_action=""; _resident_step_after_plan=false; _rest_auto=false
	var accepted: Dictionary=_resident_adapter.start_plan(selected.actor_id,selected.plan_id,selected.ticks,selected.money)
	if not accepted.get("ok",false): _resident_failed(accepted); return
	journey_view.auto_running=_resident_adapter.auto_running

## 停止仅撤销后续许可；在途半日仍由原core完成并发布稳定画面。
func _stop_activity_continuation() -> void:
	_stop_journey_auto("cancelled"); _resident_frame_pending=true

## 保存/读取先撤销许可，再等待当前半日、取消回执与完整同源页面全部稳定。
func _queue_resident_slots(mode: String) -> void:
	if not _started or mode not in ["save","load"] or not _storage_intent.is_empty(): return
	_resident_pending_slot_mode=mode
	_stop_journey_auto("saving" if mode=="save" else "reading")
	_resident_frame_pending=true
	_message="正在等当前半日完成，然后选择存档槽。"
	if is_instance_valid(status_label): status_label.text=_message

## 收到原控制器终止信号后只关闭界面自动标记。
func _resident_stopped(_reason_text: String) -> void:
	journey_view.auto_running=false

## 读取或提交失败清理在途界面意图并显示原错误，不隐式重放世界命令。
func _resident_failed(error: Dictionary) -> void:
	if _resident_handling_failure: return
	_resident_handling_failure=true
	_player_plan_start_epoch+=1; _player_plan_pending_start={}
	_advancing=false; _resident_frame_pending=false; _resident_pending_slot_mode=""
	_resident_hold={}; _resident_quote={}; _resident_command={}; _resident_completion=Callable(); _resident_short_action=""; _resident_step_after_plan=false; _resident_journal_pending=false; _rest_auto=false; _resident_read_cycle=false
	if _resident_mode: kernel.invalidate()
	if is_instance_valid(_resident_adapter): _resident_adapter.suspend("refresh_failed")
	_resident_handling_failure=false
	_message="此时不能这样做："+_reason(error.get("code","Unknown"))
	if is_instance_valid(status_label): status_label.text=_message

## 失焦停止后续旅行许可，不取消已接纳的原子事务。
func _notification(what: int) -> void:
	if _resident_mode and (what==NOTIFICATION_WM_WINDOW_FOCUS_OUT or what==NOTIFICATION_APPLICATION_FOCUS_OUT): _stop_journey_auto("focus_lost")


## 已确认的槽仅在取消和旧读取排空后进入同一worker；列表的checksum不代替语义准入。
func _resident_begin_cold() -> void:
	if _resident_slot.get("identity")!=_slot_world_identity():
		_resident_slot_finished({"ok":false,"code":"StalePreview"}); return
	var payload: Dictionary={}; var kind: String="checkpoint" if _resident_slot.mode=="save" else "restore"
	var previous: Dictionary=_resident_slot.previous
	if previous.get("ok",false):
		var decoded: Dictionary=ResidentSlotCodec.decode_payload(previous.text)
		if not decoded.ok: _resident_slot_finished(decoded); return
		if kind=="checkpoint":
			payload.previous_checkpoint_bytes=decoded.payload; payload.previous_text_sha256=previous.text.sha256_text()
			if decoded.has("continuation_bytes"): payload.previous_continuation_bytes=decoded.continuation_bytes
		else:
			var prepared: Dictionary=_resident_prepare_load_candidate(decoded.payload,decoded.get("continuation_bytes",PackedByteArray()))
			if not prepared.ok: _resident_slot_finished(prepared); return
			if prepared.get("restored",false):
				if _resident_slot.get("identity")!=_slot_world_identity(): _resident_slot_finished({"ok":false,"code":"StalePreview"}); return
				var published: Dictionary=_resident_adopt_load_candidate()
				_resident_slot_finished({"ok":true,"generation":_resident_slot.generation} if published.get("ok",false) else published)
				return
			payload.checkpoint_bytes=decoded.payload
			if decoded.has("continuation_bytes"): payload.continuation_bytes=decoded.continuation_bytes
	var target: Node=_resident_load_candidate if kind=="restore" else kernel
	if not is_instance_valid(target): _resident_slot_finished({"ok":false,"code":"InvalidResidentCandidate"}); return
	var source: Dictionary=target.source_identity()
	var accepted: Dictionary=target.transport().enqueue_live_operation({"kind":kind,"source":source.duplicate(true),"payload":payload})
	if not accepted.get("ok",false): _resident_slot_finished(accepted); return
	_resident_cold_job={"kind":kind,"source":source.duplicate(true),"reference":accepted.reference.duplicate(true)}
	_storage_intent="resident_cold"

## 完成仍绑定原job/source；仅本次成功restore的current身份可跨它自己产生的新epoch。
func _resident_poll_cold() -> void:
	var job: Dictionary=_resident_cold_job
	var target: Node=_resident_load_candidate if job.kind=="restore" else kernel
	if not is_instance_valid(target): _resident_slot_finished({"ok":false,"code":"InvalidResidentCandidate"}); return
	var reply: Dictionary=target.transport().poll_live_operation(job.reference)
	if reply.get("ok",false) and not reply.get("ready",false): return
	_resident_cold_job={}
	if not reply.get("ok",false): _resident_slot_finished(reply); return
	if reply.get("tag")!=job.kind or reply.get("source_identity")!=job.source or not reply.get("result") is Dictionary:
		_resident_slot_finished({"ok":false,"code":"InvalidResidentReply"}); return
	var result: Dictionary=reply.result
	if not result.get("ok",false): _resident_slot_finished(result); return
	if job.kind=="restore":
		if _resident_slot.get("identity")!=_slot_world_identity():
			_resident_slot_finished({"ok":false,"code":"StalePreview"}); return
		var adopted: Dictionary=target.adopt_source(reply.current_identity)
		if not adopted.get("ok",false): _resident_slot_finished(adopted); return
		var published: Dictionary=_resident_adopt_load_candidate()
		if not published.ok: _resident_slot_finished(published); return
		_resident_slot_finished({"ok":true,"generation":_resident_slot.generation}); return
	if reply.get("current_identity")!=job.source or not result.get("checkpoint_bytes") is PackedByteArray or not result.get("continuation_bytes",PackedByteArray()) is PackedByteArray or not result.get("validation") is Dictionary:
		_resident_slot_finished({"ok":false,"code":"InvalidResidentReply"}); return
	var encoded: Dictionary=ResidentSlotCodec.encode(result.checkpoint_bytes,_resident_slot.generation+1,_resident_slot.slot,result.get("continuation_bytes",PackedByteArray()))
	if not encoded.ok: _resident_slot_finished(encoded); return
	_storage_intent=""
	var accepted: Dictionary=_request_storage("resident_write","put_resident",{"slot":_resident_slot.slot,"text":encoded.text,"expected_generation":_resident_slot.generation,"validation":result.validation.duplicate(true)})
	if not accepted.ok: _resident_slot_finished(accepted)

## 只有原生restore或磁盘提升/读回成功才更新已选代数；失败保留旧slot/world。
func _resident_slot_finished(result: Dictionary) -> void:
	last_storage_result=result.duplicate(true)
	if result.get("ok",false):
		_selected_slot=_resident_slot.get("slot",_selected_slot); _save_generation=result.get("generation",_resident_slot.get("generation",_save_generation))
		_message="已保存当前旅途。" if _resident_slot.get("mode")=="save" else "已恢复此前的旅途。"
	else: _message="存档操作未完成："+_reason(result.get("code","Unknown"))
	_storage_intent=""; _resident_slot={}; _resident_cold_job={}; _advancing=false
	if is_instance_valid(_resident_load_candidate): _resident_discard_load_candidate()
	if _resident_mode and _started:
		_resident_frame_pending=true; _advancing=true # 完整新frame更新当前玩家以前仍阻止旧页面命令。
	else: _render()


## 只匹配受信安装索引中的完整选择身份；缺失和不匹配都不得退到默认目录。
func _resident_restore_catalog(selection: Dictionary) -> Dictionary:
	if not selection.get("catalog") is Dictionary: return {"ok":false,"code":"InvalidCheckpointSelection"}
	var wanted: Dictionary=selection.catalog
	for field in ["content_id","content_version","content_digest","ruleset_id","schema_sha256","artifact_sha256"]:
		if not wanted.get(field) is String or wanted[field].is_empty(): return {"ok":false,"code":"InvalidCheckpointSelection"}
	for field in ["scenario_id","player_actor_id","founder_actor_id"]:
		if not selection.get(field) is String or selection[field].is_empty(): return {"ok":false,"code":"InvalidCheckpointSelection"}
	var installed: Dictionary=Catalog.installed_catalogs()
	if not installed.ok: return installed
	var matches: Array=[]; var known: bool=false
	for catalog in installed.catalogs:
		if catalog.content_id!=wanted.content_id: continue
		known=true
		var same: bool=true
		for field in ["content_id","content_version","content_digest","schema_sha256"]:
			if catalog.get(field)!=wanted[field]: same=false
		if catalog.has("pack_set_digest")!=wanted.has("pack_set_digest") or catalog.get("pack_set_digest")!=wanted.get("pack_set_digest"): same=false
		if same: matches.append(catalog)
	if matches.size()!=1: return {"ok":false,"code":"CheckpointCatalogMismatch" if known else "MissingContent"}
	if not matches[0].get("catalog_path") is String: return {"ok":false,"code":"ContentSetNotInstalled"}
	return {"ok":true,"catalog":matches[0]}

## 确认物理槽后才建独立候选；原世界、当前美术与存档银行在完整restore前保持不动。
func _resident_prepare_load_candidate(bytes: PackedByteArray, continuation_bytes: PackedByteArray=PackedByteArray()) -> Dictionary:
	if is_instance_valid(_resident_load_candidate): return {"ok":false,"code":"ResidentCandidateBusy"}
	var backend_script=load("res://ui/native_resident_backend.gd")
	if backend_script==null: return {"ok":false,"code":"ResidentBackendUnavailable"}
	var selection: Dictionary=backend_script.inspect_checkpoint_selection(bytes)
	if not selection.get("ok",false): return selection
	var matched: Dictionary=_resident_restore_catalog(selection)
	if not matched.ok: return matched
	var loaded: Dictionary=Catalog.read_prepared(matched.catalog.catalog_path)
	if not loaded.get("ok",false): return loaded
	if loaded.get("data",{}).get("scenarios",{}).get(selection.scenario_id,{}).has("dynamic_world"):
		var art: Dictionary=Art.prepare_content(loaded)
		if not art.get("ok",false): return art
		var backend=backend_script.new(); add_child(backend)
		var installed: Dictionary=backend.install_checkpoint(loaded,bytes,continuation_bytes)
		if not installed.get("ok",false): backend.queue_free(); return installed
		_resident_load_candidate=backend; _resident_pending_art=art; _resident_bootstrap_loading=not _started
		return {"ok":true,"restored":true}
	var candidate=Kernel.new()
	var prepared: Dictionary=candidate.new_game(selection.scenario_id,matched.catalog.catalog_path,{},selection.founder_actor_id)
	if not prepared.get("ok",false): return prepared
	var art: Dictionary=Art.prepare_content(candidate.content())
	if not art.get("ok",false): return art
	var backend=backend_script.new(); add_child(backend)
	var installed: Dictionary=backend.install(candidate.content(),candidate.snapshot())
	if not installed.get("ok",false): backend.queue_free(); return installed
	_resident_load_candidate=backend; _resident_pending_art=art; _resident_bootstrap_loading=not _started
	return {"ok":true}

## 完整原生restore成功且新控制器绑定成功后才替换旧世界；不继承旧自动许可或页面缓存。
func _resident_adopt_load_candidate() -> Dictionary:
	if not is_instance_valid(_resident_load_candidate): return {"ok":false,"code":"InvalidResidentCandidate"}
	var backend: Node=_resident_load_candidate
	var adapter=ResidentJourneyAdapter.new(); add_child(adapter)
	var bound: Dictionary=adapter.bind_context(backend.transport(),backend.source_identity(),_scene_counter)
	if not bound.ok: adapter.queue_free(); return bound
	if _resident_mode:
		_resident_adapter.reset(); _resident_adapter.queue_free(); kernel.invalidate(); kernel.queue_free()
	kernel=backend; _resident_adapter=adapter; _resident_mode=true
	if kernel.has_method("invalidate_continuation_state"): kernel.invalidate_continuation_state()
	_resident_adapter.operation_completed.connect(_resident_run_completed)
	_resident_adapter.failed.connect(_resident_failed); _resident_adapter.stopped.connect(_resident_stopped)
	kernel.frame_ready.connect(_resident_frame_ready); kernel.command_completed.connect(_resident_command_completed)
	kernel.preview_ready.connect(_resident_preview_ready); kernel.page_reads_ready.connect(_resident_page_reads_ready); kernel.failed.connect(_resident_failed)
	Art.commit_content(_resident_pending_art)
	_resident_load_candidate=null; _resident_pending_art={}; _resident_bootstrap_loading=false
	_data=kernel.presentation_data(); storage.configure(kernel.content(),null,true)
	set_meta("personal_battle_generation",int(get_meta("personal_battle_generation",0))+1)
	_reset_player_plan_entry()
	_reset_trade_view(); journey_view.reset(); journey_view.opened=true
	_activity_draft={}; _resident_pending_slot_mode=""; set_meta("system_menu_open",false)
	_dynamic_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _study_teaching_open=false; _estate_id=""; _family_open=false; _military_open=false; _courses_open=false; _rest_auto=false
	_resident_command={}; _resident_result={}; _resident_quote={}; _resident_hold={}
	_resident_completion=Callable(); _resident_short_action=""; _resident_step_after_plan=false; _resident_journal_pending=false; _resident_provider_pending=""; _resident_trade_receipt_pending=""; _resident_page_error={}; _resident_read_cycle=false
	_started=true; _continuing=false
	return {"ok":true}

## 失败只退休未发布候选；绝不清除当前活世界、当前目录或已选槽，也不改磁盘银行。
func _resident_discard_load_candidate() -> void:
	if is_instance_valid(_resident_load_candidate):
		_resident_load_candidate.invalidate(); _resident_load_candidate.queue_free()
	_resident_load_candidate=null; _resident_pending_art={}; _resident_bootstrap_loading=false

## 已由当前页面核验的请求原样送入唯一异步提交，回调只有成功真实回执后才能执行。
func _resident_submit_frozen(command: Dictionary, completion: Callable=Callable()) -> Dictionary:
	if not _resident_mode or not _started: return {"ok":false,"code":"NoWorld"}
	if _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty() or not _resident_command.is_empty() or kernel.is_busy() or _resident_adapter.is_busy(): return {"ok":false,"code":"ActorBusy"}
	_resident_command=command.duplicate(true); _resident_completion=completion
	if command.get("kind")!="advance":
		_resident_short_action=""; _rest_auto=false
	_stop_journey_auto(); _advancing=true
	return {"ok":true,"pending":true}

## 页读与preview只在原发起场景仍当前时重绘；过期读取不能重新提交或抢占新页面。
func _resident_page_reads_ready(source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or not _started or source!=kernel.source_identity() or scene_epoch!=_scene_counter: return
	if kernel.has_pending_reads(): return
	if _resident_journal_pending: _resident_finish_journal()
	_resident_read_cycle=false; _advancing=false
	_resident_rendering=true; _render(); _resident_rendering=false
	_resident_adapter.update_context(source,_scene_counter)
	if not _resident_hold.is_empty() and not kernel.has_pending_reads():
		_resident_adapter.release_refresh(_resident_hold); _resident_hold={}
	_resident_continue_short()

## 只续同一已提交短行动；每半日经真实回执和输入门，手动窗口或页面读取会暂停。
func _resident_continue_short() -> void:
	if _resident_adapter.auto_running or not _resident_pending_slot_mode.is_empty() or not _player_plan_pending_start.is_empty(): return
	if not _resident_page_error.is_empty() or kernel.is_busy() or kernel.has_pending_reads() or not _storage_intent.is_empty(): return
	if _resident_step_after_plan:
		_resident_step_after_plan=false; call_deferred("_resident_perform","advance",{}); return
	if _resident_short_action.is_empty(): return
	var context: Dictionary=kernel.action_context()
	if context.get("action_id","")!=_resident_short_action:
		_resident_short_action=""; return
	if kernel.action_manual_window():
		_resident_short_action=""; return
	if kernel.has_pending_reads(): return
	call_deferred("_finish_short")

## 已知合同/行囊/近事的有限原生文本到达后才开窗；旧代次可取消，失败不显示空的成功手记。
func _resident_finish_journal() -> bool:
	if not _resident_journal_pending: return true
	var view: Dictionary=kernel.demand_view("journal_text")
	if view.get("code")=="NativeAsyncRequired": return false
	_resident_journal_pending=false
	if not view.get("ok",false) or not view.get("text") is String:
		_resident_failed(view if view.get("ok")==false else {"ok":false,"code":"InvalidJournalView"})
		return true
	_show_journal(view.text)
	return true

## 本地分页草案绑定完整来源与实际场所，读档同revision也不能继承旧控件来源。
func _regional_draft() -> Dictionary:
	var source: Dictionary=kernel.source_identity()
	var place: String=kernel.presentation_location().get("place_id","")
	var draft: Dictionary=get_meta("regional_life_draft",{})
	if draft.get("source")!=source or draft.get("place_id")!=place:
		draft={"source":source,"place_id":place,"cursor":0}
		set_meta("regional_life_draft",draft.duplicate(true))
	return draft

## 分页只改变可见列表，旧来源不能重新打开另一地点或推进半日。
func _regional_page(cursor: int, source: Dictionary, scene_epoch: int) -> void:
	if not _resident_mode or not _started or _advancing or scene_epoch!=_scene_counter or source!=kernel.source_identity(): return
	var draft: Dictionary=_regional_draft(); draft.cursor=cursor
	set_meta("regional_life_draft",draft.duplicate(true)); _render()

## 对白身份只保存公开继续点，既不保存整世界也不复制会话内部任务绑定。
func _dialogue_identity(view: Dictionary) -> Dictionary:
	return {"phase":view.get("phase","scene"),"context":view.get("context",{}).duplicate(true),"node_id":view.get("node_id",""),"command":view.get("command",{}).duplicate(true),"pending_action_id":view.get("pending_action_id","")}

## 新地方选择只有同源真报价才可点击；pending始终禁用，不读取尚未创建的quote字段。
func _regional_button(parent: Node, label: String, kind: String, target: String, params: Dictionary, dialogue: Dictionary={}, disabled: bool=false) -> Button:
	var context: Dictionary=kernel.command_context()
	var source: Dictionary=kernel.source_identity()
	var action: Dictionary={"command":kind,"params":params.duplicate(true)}
	var quote: Dictionary=kernel.preview(kind,params,context.get("actor_id",""))
	var button: Button=_button(parent,label,kind,target,func(): pass,disabled or _advancing or not quote.get("ok",false),false)
	button.set_meta("regional_action",action.duplicate(true)); button.set_meta("regional_source",source.duplicate(true))
	button.set_meta("regional_quote",quote.duplicate(true))
	button.tooltip_text="正在核对当前条件…" if quote.get("code")=="NativeAsyncRequired" else _reason(quote.get("code",""))
	button.pressed.connect(_regional_request.bind(button,action,source,_scene_counter,context.get("actor_id",""),dialogue.duplicate(true)))
	return button

## 旧控件可已释放，先以Variant接收再核有效性；来源与对白守卫仍禁止旧按钮重绑新机会。
func _regional_request(button: Variant, action: Dictionary, source: Dictionary, scene_epoch: int, actor_id: String, dialogue: Dictionary) -> Dictionary:
	var enabled: bool=_regional_life_enabled() or (_politics_enabled() and action.get("command","") in ["political_appoint","political_answer_appointment","political_arrive","political_withdraw_appointment","political_dismiss","political_set_tax","move_local","travel"])
	enabled=enabled or (kernel.has_method("treaty_passage_enabled") and kernel.treaty_passage_enabled() and action.get("command","") in ["treaty_passage_present","treaty_passage_sign"])
	enabled=enabled or (kernel.has_method("political_military_appointments_enabled") and kernel.political_military_appointments_enabled() and action.get("command","") in ["political_military_officer_issue","political_military_officer_answer","political_military_officer_withdraw","military_officer_assign"])
	enabled=enabled or (kernel.has_method("political_field_appointments_enabled") and kernel.political_field_appointments_enabled() and action.get("command","") in ["political_field_appointment_issue","political_field_appointment_answer","political_field_appointment_withdraw","military_field_command_assign"])
	enabled=enabled or (kernel.has_method("political_occupation_fiscal_enabled") and kernel.political_occupation_fiscal_enabled() and action.get("command","")=="political_set_occupation_tax")
	enabled=enabled or (kernel.has_method("political_takeover_enabled") and kernel.political_takeover_enabled() and action.get("command","")=="political_assume_public_responsibilities")
	if not enabled or not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return {"ok":false,"code":"StaleSceneRequest"}
	if _advancing or not _storage_intent.is_empty() or scene_epoch!=_scene_counter or source!=kernel.source_identity(): return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor_id or context.get("revision")!=source.get("revision"): return {"ok":false,"code":"StalePreview"}
	if not dialogue.is_empty() and _dialogue_identity(kernel.interaction_view())!=dialogue: return {"ok":false,"code":"StaleDialogueChoice"}
	var quote: Dictionary=kernel.preview(action.command,action.params,actor_id)
	if not quote.get("ok",false):
		button.disabled=true; _message="此时不能这样做："+_reason(quote.get("code","Unknown")); _render(); return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	return _resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":source.revision})

## 非resident和旧Kernel没有地方查询；只询问当前backend已选剧本的显式能力。
func _regional_life_enabled() -> bool:
	return _resident_mode and _started and kernel.has_method("regional_life_enabled") and kernel.regional_life_enabled()

## 具名任职由本局明确能力开启；界面不从已有成员或作者名字猜测权限。
func _named_membership_enabled() -> bool:
	return _resident_mode and _started and kernel.has_method("named_membership_enabled") and kernel.named_membership_enabled()

## 政治入口只认当前原生情景能力；不从组织成员或军事占领推定治权。
func _politics_enabled() -> bool:
	return _resident_mode and _started and kernel.has_method("politics_enabled") and kernel.politics_enabled()

## 教师页保留原学生会话，只展示这次明确有限请求；接受仍由原唯一命令办理。
func _study_teaching_enabled() -> bool:
	return _resident_mode and _started and kernel.has_method("study_teaching_enabled") and kernel.study_teaching_enabled()

func _study_teaching_view() -> Dictionary:
	var args: Dictionary={"page":_study_teaching_page}
	var context: Variant=kernel.interaction_view().get("context")
	if context is Dictionary and not context.is_empty(): args.context=context.duplicate(true)
	return kernel.study_teaching(args)

func _open_study_teaching() -> void:
	if not _study_teaching_enabled() or _advancing or not _storage_intent.is_empty() or not _resident_pending_slot_mode.is_empty(): return
	if not kernel.personal_battle_view().get("battle_id","").is_empty() or kernel.military_battle_page().get("active",false) or not kernel.siege_page().get("siege_id","").is_empty(): return
	_stop_journey_auto(); _rest_auto=false; journey_view.opened=false
	_player_plan_open=false; _dynamic_open=false; _self_profile_open=false; _self_profile_view={}; _growth_open=false; _ordinary_skills_open=false; _cards_open=false; _courses_open=false; _military_open=false; _family_open=false; _estate_id=""
	_study_teaching_page=0; _study_teaching_open=true; _render()

func _close_study_teaching() -> void:
	_study_teaching_open=false; _render()

func _study_teaching_change_page(value: int) -> void:
	if not _study_teaching_open or _advancing or value<0 or value>255: return
	_study_teaching_page=value; _render()

func _render_study_teaching(parent: Control) -> void:
	StudyTeachingScreen.build(self,parent,_study_teaching_view(),_close_study_teaching,_study_teaching_change_page)
