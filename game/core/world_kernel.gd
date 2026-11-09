## 内容驱动世界的唯一提交入口；只有完整通过校验的候选状态才能替换真实世界。
extends RefCounted
const CommandContext=preload("res://core/backends/legacy_command_context.gd")
const ActionContext=preload("res://core/views/action_context.gd")
const LegacyBackend=preload("res://core/backends/legacy_world_backend.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
const CharacterSummary=preload("res://core/views/character_summary.gd")
const TradeSheet=preload("res://core/views/trade_sheet.gd")
const SceneQuery=preload("res://core/views/scene_projection.gd")
const MapView=preload("res://core/views/map_view.gd")
const HudView=preload("res://core/views/hud_view.gd")
const JourneyNotes=preload("res://core/views/journey_notes.gd")
const Validation=preload("res://core/runtime/world_validation.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const TransactionContext=preload("res://core/runtime/transaction_context.gd")
const DomainObservers=preload("res://core/runtime/domain_observers.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const ExplanationQuery=preload("res://core/runtime/explanation_query.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Training=preload("res://core/domains/military/training_rules.gd")
const MilitaryModule=preload("res://core/domains/military/military_module.gd")
const MilitarySchedule=preload("res://core/military_schedule.gd")
const Military=preload("res://core/military_world.gd")
const MilitaryCoordination=preload("res://core/domains/military/military_coordination.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const Combat=preload("res://core/combat_session.gd")
const Personal=preload("res://core/personal_combat.gd")
const PersonalCoordination=preload("res://core/domains/personal/personal_coordination.gd")
const Escort=preload("res://core/escort_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Rest=preload("res://core/long_rest.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const BargainQuotes=preload("res://core/domains/economy/bargain_quote.gd")
const SkillStudy=preload("res://core/domains/cultivation/skill_study_rules.gd")
const EncounterModule=preload("res://core/domains/interaction/encounter_module.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Schedule=preload("res://core/travel_schedule.gd")
const Parties=preload("res://core/party_rules.gd")
const Location=preload("res://core/world_location.gd")
const Rules = preload("res://core/rules_migration.gd")
const Catalog = preload("res://core/content_catalog.gd")
const Factory = preload("res://core/world_factory.gd")
const Schema = preload("res://core/state_schema.gd")
const Conditions = preload("res://core/condition_rules.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const ActionRuntime=preload("res://core/runtime/action_runtime.gd")
const Inventory = preload("res://core/inventory_rules.gd")
const Travel = preload("res://core/travel_rules.gd")
const Step = preload("res://core/world_step.gd")
const Journey = preload("res://core/journey_commands.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const JourneyModule=preload("res://core/domains/travel/journey_module.gd")
const Dialogue = preload("res://core/dialogue_runner.gd")
const Services = preload("res://core/service_rules.gd")
const Events = preload("res://core/event_rules.gd")
var _backend=LegacyBackend.new()
# 受信兼容入口只别名同一后端三元组；不复制第二份状态，也不宣称脚本私有写屏障。
var _catalog: Dictionary:
	get: return _backend.catalog
	set(value):
		_backend.catalog=value
		_backend.invalidate_native_growth_read()
var _state: Dictionary:
	get: return _backend.state
	set(value):
		_backend.state=value
		_backend.invalidate_native_growth_read()
var _registry: Dictionary:
	get: return _backend.registry
	set(value):
		_backend.registry=value
		_backend.invalidate_native_growth_read()

## 只提供发行代码绑定的描述；测试可继承替换以验证启动失败保留旧世界。
func _module_descriptors() -> Array:
	return Builtin.descriptors()

## 读取已准备内容并建立剧本；任一步失败均保留原有世界。
func new_game(scenario_id: String = "scenario.two_stories", content_path: String = "res://data/generated/two_stories.hmcatalog.json", appearance_draft: Dictionary={}, player_actor_id: String="") -> Dictionary:
	return _backend.new_game(_module_descriptors,scenario_id,content_path,appearance_draft,player_actor_id)

## 返回隔离快照，防止界面或测试通过共享引用篡改真实状态。
func snapshot() -> Dictionary:
	return _backend.snapshot()

## 只读投影当前人物和命令序号；不复制完整世界，也不预占请求或替代原提交守卫。
func command_context(actor_id: String="") -> Dictionary:
	return _backend.command_context(actor_id)

## 基础行动值不预读战斗或课程窗口；当前控制器决定何时查询，不由此授予自动操作。
func action_context() -> Dictionary:
	return _backend.action_context()

## 只在调用方原短路位置判定旅行输入边界，保原Combat/Siege/Encounter顺序。
func action_needs_input() -> bool:
	return _backend.action_needs_input()

## 原手动课程窗口按需读取，不在基础上下文中提前访问行动事实。
func action_manual_window() -> bool:
	return _backend.action_manual_window()

## 只投影调用方已确定的单人物表现输入，不复制整世界，也不授予可见性权限。
func character_input(actor_id: String) -> Dictionary:
	return _backend.character_input(actor_id)

## 只返回当前玩家成长窗口的指定页有限数据，报价与交互仍由原命令入口判断。
func character_summary(page: String="skills") -> Dictionary:
	return _backend.character_summary(page)

## 新本人档案仅使用已支持后端的有限投影；旧后端不从整态伪造新DTO。
func self_profile() -> Dictionary:
	return _backend.self_profile() if _backend.has_method("self_profile") else {"ok":false,"code":"UnsupportedNativeCapability"}

## 本人普通技艺只走后端有限投影；不读取整态或触发课程结算。
func ordinary_skills(skill_id: String="") -> Dictionary:
	return _backend.ordinary_skills(skill_id)

## 军政纯算法只替换提案来源；原预览、逐项子候选和外层发布顺序保持。
func configure_native_military_schedule(enabled: bool, expected_build: Dictionary={}) -> void:
	_backend.configure_native_military_schedule(enabled,expected_build)

## 显式选择原半日末旅行算法；仍由同一Backend候选与旧命令执行器发布。
func configure_native_travel_schedule(enabled: bool, expected_build: Dictionary={}) -> void:
	_backend.configure_native_travel_schedule(enabled,expected_build)

## 显式开发配置只启用按需原生行动读取，不构建投影或改变命令权限。
func configure_native_growth_read(enabled: bool, source_commit: String="") -> void:
	_backend.configure_native_growth_read(enabled,source_commit)

## 受信调用方原地修改投影来源后显式通知；私有来源代次不进入世界或存档。
func invalidate_native_growth_read() -> void:
	_backend.invalidate_native_growth_read()

## 返回隔离的开发计数与计时，不暴露可写投影或世界引用。
func native_growth_read_diagnostics() -> Dictionary:
	return _backend.native_growth_read_diagnostics()

## 开窗时由实际后端读取首个家事岗位，不在页面刷新时重选provider。
func family_provider_context() -> Dictionary:
	return _backend.family_provider_context()

## 后端提供当前玩家家事/休整指定页，保留原窗口context和命令入口。
func family_page(tab: String="family", provider_context: Dictionary={}) -> Dictionary:
	return _backend.family_page(tab,provider_context)

## 庄务导航仅问原存在与同地条件，不提前读取整页或添加owner守卫。
func estate_available(estate_id: String) -> bool:
	return _backend.estate_available(estate_id)

## 后端返回当前庄务指定页的有限输入，不在门面解释账本或计算资格。
func estate_page(estate_id: String, tab: String="people") -> Dictionary:
	return _backend.estate_page(estate_id,tab)

## 当前个人战页面只接后端有限输入，保留原选择器和独立命令预览。
func personal_battle_view() -> Dictionary:
	return _backend.personal_battle_view()

## 在旧输入守卫时点读取同一会话身份，不刷新旧页面绑定。
func personal_battle_context(battle_id: String) -> Dictionary:
	return _backend.personal_battle_context(battle_id)

## 仅为画布初始或旧回退分支提供后端同源几何。
func personal_battle_geometry(battle_id: String, draft: Dictionary) -> Dictionary:
	return _backend.personal_battle_geometry(battle_id,draft)

## 只投影当前玩家的指定场景；保留既有知闻与错误，不新增世界ID或游标语义。
func scene_view(place_id: String, view_kind: String) -> Dictionary:
	return _backend.scene_view(place_id,view_kind)

## 读取原HUD当前玩家摘要；个人战仅采用调用方已选战场，不重选分支或授予行动许可。
func hud_view(personal_battle_id: String="") -> Dictionary:
	return _backend.hud_view(personal_battle_id)

## 返回地图画布所需的有限动态值；既有全作者地图语义不扩大为新知闻或行动许可。
func map_view() -> Dictionary:
	return _backend.map_view()

## 返回原有效货单所需的有限运行数据；不报价、预留、授权或额外校验人物外貌。
func trade_view(shop_id: String, provider_actor_id: String, receipt_action_id: String="") -> Dictionary:
	return _backend.trade_view(shop_id,provider_actor_id,receipt_action_id)

## 按当前已开始世界生成原行旅手记；只返回文本，不复制整态或扩展知识范围。
func journal_text() -> String:
	return _backend.journal_text()

## 提供内容定义副本，供表现层和带版本校验的存储适配器读取。
func content() -> Dictionary:
	return _backend.content()

## 一次复制保存所需世界、目录及受信注册表；调用方不能修改活跃注册对象。
func save_context() -> Dictionary:
	return _backend.save_context()

## 只读解释已提交状态；observer由受信调用方指定而非query授权，本入口不提供外部调用者身份认证。
func explain(query: Dictionary, observer_id: String="") -> Dictionary:
	return _backend.explain(query,observer_id)

## 只读当前实际岗位的普通技艺课程；领域完成在场、权源与资格判断，查询不自动开课。
func study_courses(actor_id: String, context: Dictionary, ticks: int=2) -> Dictionary:
	return _backend.study_courses(actor_id,context,ticks)

## 只读本人当前课程公开题面，不读取其它人物实例或改变答题轮次。
func study_etiquette(actor_id: String, action_id: String) -> Dictionary:
	return _backend.study_etiquette(actor_id,action_id)

## 读取本人当前课程题面；通用练习仅返回候选表现，不应用交易折扣。
func study_challenge(actor_id: String, action_id: String) -> Dictionary:
	return _backend.study_challenge(actor_id,action_id)

## 只读本人军阵课程公开实例，不推进随机、课时或真实部队。
func military_drill(actor_id: String, action_id: String) -> Dictionary:
	return _backend.military_drill(actor_id,action_id)

## 仅恢复内容和引擎兼容的状态，不重放已经提交的消费或奖励。
func restore(saved: Dictionary) -> Dictionary:
	return _backend.restore(saved)

## 冷恢复只装配受信注册合同，不新建默认世界、不分配实例或消耗随机流。
func prepare_restore_registry() -> Dictionary:
	return _backend.prepare_restore_registry(_module_descriptors)

## 接收已经单次投影的目录与当前规则正文；终验通过才原子替换世界、目录和注册，不重新开局或发货。
func restore_with_catalog(saved: Dictionary, catalog: Dictionary, registry: Variant=null) -> Dictionary:
	return _backend.restore_with_catalog(saved,catalog,registry)

## 按当前版本执行纯查询预览，不消费随机数或预扣资源。
func preview(kind: String, params: Dictionary = {}, actor_id: String = "") -> Dictionary:
	return _backend.preview(_preview,kind,params,actor_id)

## 原子提交带版本的命令；重复请求返回原结果，不再次执行。
func submit(command: Dictionary) -> Dictionary:
	return _backend.submit(command,_preview,_apply)

## 校验已支持命令及真实世界前提，不能仅相信界面是否显示按钮。
func _preview(state: Dictionary, kind: String, params: Dictionary, actor_id: String) -> Dictionary:
	return _backend.preview_command(state,kind,params,actor_id,_preview)

## 仅在隔离候选状态执行已校验规则，动画和界面回调不直接写入事实。
func _apply(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary) -> Dictionary:
	return _backend.apply_command(state,kind,params,actor_id,offer,_advance,_begin_action,_drive_dialogue,_apply)

## 建立行动并占用实际参与者；全部不变量校验通过后才由调用者提交。
func _begin_action(state: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	return _backend.begin_action(state,actor_id,kind,params,offer)

## 委托统一半日协调器推进一次世界，再恢复已完成行动的对白和场景事件。
func _advance(state: Dictionary, hold: Array = []) -> Dictionary:
	return _backend.advance_world(state,hold,_drive_dialogue,_military_journey,_schedule_travel,_schedule_military)

## 只释放指定行动持有的人物占用和资源预留，不触碰其他行动。
func _release_action(state: Dictionary, action: Dictionary) -> void:
	_backend.release_action(state,action)

## 便捷半日调用仍走同一版本化命令入口，不建立第二套模拟时钟。
func advance_one_tick() -> Dictionary:
	if _state.is_empty(): return {"ok":false,"code":"NoWorld"}
	return submit({"kind":"advance","params":{},"actor_id":_state.player_actor_id,"request_seq":_state.request_sequences[_state.player_actor_id]+1,"expected_revision":_state.revision})

## 有限步数推进自动对白节点；只有登记的零时长线索发现可免除再次确认。
func _drive_dialogue(state: Dictionary, actor_id: String) -> Dictionary:
	return _backend.drive_dialogue(state,actor_id,_preview,_apply)

## 返回当前会话的只读视图，查询期间不执行对白节点。
func interaction_view(actor_id: String = "") -> Dictionary:
	return _backend.interaction_view(actor_id)

## 半日末处理旅人提案；仍先校验正式旅行规则，起步不提前消耗下一半日。
func _schedule_travel(state: Dictionary) -> Dictionary:
	return _backend.schedule_travel(state,_preview,_begin_action)

## 军令只请求实际地图行旅；抵达本城后仍须沿场所通道入营，缺钱或不在城口不能瞬移。
func _military_journey(state: Dictionary, request: Dictionary) -> Dictionary:
	return _backend.military_journey(state,request,_begin_action)

## NPC军政提案复用正式命令，不生成玩家请求序列或绕开同一状态校验。
func _schedule_military(state: Dictionary) -> Dictionary:
	return _backend.schedule_military(state,_preview,_apply)

## 按原人物和报价摘要定位唯一既有实例；不刷新报价、校验授权或扫描给界面全部事实。
func trade_bargain_source(actor_id: String, basis_sha256: String) -> Dictionary:
	return _backend.trade_bargain_source(actor_id,basis_sha256)

## 真实交易题面只读本人同一冻结报价，不替调用者刷新市场时效或创建另一个seed。
func trade_bargain(actor_id: String, quote_id: String) -> Dictionary:
	return _backend.trade_bargain(actor_id,quote_id)

## 后端提供普通场所的有限显示输入，按钮继续原实时preview与请求协议。
func place_sidebar() -> Dictionary:
	return _backend.place_sidebar()

## 后端提供本人行旅侧栏，保持会面、同行、实际行程和供粮的原分工。
func journey_sidebar() -> Dictionary:
	return _backend.journey_sidebar()

## 开窗办理人沿后端原选择，保持窗口context寿命。
func military_camp_provider_context() -> Dictionary:
	return _backend.military_camp_provider_context()

## 营务当前页只读有限输入，预览与数量编辑仍走原公开入口。
func military_camp_page(tab: String="orders", selection: Dictionary={}, provider_context: Dictionary={}) -> Dictionary:
	return _backend.military_camp_page(tab,selection,provider_context)

## 保原动态public preview顺序组合展示报价；只在原成功分支读取出发和姓名来源。
func military_camp_offer(kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=preview(kind,params)
	if kind=="campaign_depart" and quote.ok:
		var source: Dictionary=_backend.military_camp_departure_source(params.campaign_id)
		if not source.at_target:
			var travel: Dictionary=preview("travel",{"destination":source.destination},source.commander_actor_id)
			if not travel.ok: quote=travel
			else: quote.duration=travel.get("duration",0)
	var result: Dictionary={"quote":quote,"participant_names":[],"departure_guide_place_id":""}
	if quote.ok and not quote.get("participants",[]).is_empty():
		result.participant_names=_backend.military_camp_participant_names(quote.participants)
	if kind=="campaign_depart" and quote.get("code")=="NotAtCityExit":
		result.departure_guide_place_id=_backend.military_camp_departure_guide_place()
	return result

## 后端提供原选择顺序的有限野战页面，不把完整会话或AI带进UI。
func military_battle_page(battle_id: String="") -> Dictionary:
	return _backend.military_battle_page(battle_id)

## 舟战页面只消费本人参战只读投影；新命令仍走同一事务。
func naval_battle_page(battle_id: String="") -> Dictionary:
	return _backend.naval_battle_page(battle_id)
func naval_battle_draft(battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary:
	return _backend.naval_battle_draft(battle_id,selected_id,orders)

## 当前整套草案反馈来自后端同一规则源，UI只接受与本页身份一致的读取。
func military_battle_draft(battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary:
	return _backend.military_battle_draft(battle_id,selected_id,orders)

## 后端提供当场普通课程岗位，时长与实际开课仍走旧有限课程查询。
func study_course_contexts() -> Array:
	return _backend.study_course_contexts()

## 当前课程显示用原事实付款摘要，不构造新的成长或题面状态。
func study_activity() -> Dictionary:
	return _backend.study_activity()

## 活动会话只取原世界与内容身份标量；节点代次和Kernel实例仍由UI核验。
func activity_identity() -> Dictionary:
	return _backend.activity_identity()

## 非战场路由只取原raw位置标量，完整页面仍由各自有限入口提供。
func presentation_location() -> Dictionary:
	return _backend.presentation_location()

## 本地货单缓存按后端原交谈阶段保留，不将当前屏幕当作交易结束。
func trade_cache_context() -> Dictionary:
	return _backend.trade_cache_context()

## 实际成功trade的收据仅供显示，支付与货物权威仍是原提交结果。
func trade_action_receipt(action_id: String) -> Dictionary:
	return _backend.trade_action_receipt(action_id)

## 后端复用原围城选择器，外壳不扫描所有战争或传递完整观察。
func siege_page() -> Dictionary:
	return _backend.siege_page()

## 原草案请求只在当前来源一致时返回读取，军议/战略仍由preview与submit授权。
func siege_draft(identity: Dictionary, requests: Array, first_ok: bool=false) -> Dictionary:
	return _backend.siege_draft(identity,requests,first_ok)
