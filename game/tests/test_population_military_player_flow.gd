## 正式1.2情景的唯一Main军籍操作片；中央提供已安装世界与精确输入身份，不注入人口、军团、行旅或战果。
extends RefCounted
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")
const REQUIRED_SCRIPTS=["res://tests/test_population_military_player_flow.gd","res://tests/test_dynamic_world_player_flow.gd","res://ui/main.gd","res://ui/military_camp_screen.gd","res://ui/population_reserve_panel.gd","res://ui/dynamic_world_panel.gd"]

## 文件全部由中央显式metadata提供；不猜相邻目录、旧回执命名或库文件名，摘要不符即拒绝开始。
static func verify_inputs(harness, main, options: Dictionary) -> bool:
	for field in ["scenario_id","catalog_path","source_commit","files","script_sha256"]:
		if not Flow.check(harness,options.has(field),"reserve_input_"+field): return false
	if not Flow.check(harness,options["source_commit"] is String and options["source_commit"].length()==40 and options["source_commit"].is_valid_hex_number(false),"reserve_source_commit_identity"): return false
	var files: Dictionary=options["files"]
	for role in ["catalog","pack_lock","schema","capabilities","native_library","binding"]:
		var record: Dictionary=files.get(role,{})
		if not Flow.check(harness,record.get("path") is String and record.get("sha256") is String and record["sha256"].length()==64 and FileAccess.file_exists(record["path"]) and FileAccess.get_sha256(record["path"])==record["sha256"],"reserve_file_"+role): return false
	for path in REQUIRED_SCRIPTS:
		if not Flow.check(harness,options["script_sha256"].get(path,"").length()==64 and FileAccess.get_sha256(path)==options["script_sha256"][path],"reserve_script_"+path.get_file()): return false
	var binding: Variant=JSON.parse_string(FileAccess.get_file_as_string(files["binding"]["path"]))
	if not Flow.check(harness,binding is Dictionary and binding.get("source_commit")==options["source_commit"] and binding.get("native_extension",{}).get("path")==files["native_library"]["path"] and binding.get("native_extension",{}).get("sha256")==files["native_library"]["sha256"],"reserve_native_binding"): return false
	if not Flow.check(harness,ProjectSettings.globalize_path(files["native_library"]["path"]).begins_with(ProjectSettings.globalize_path("res://")),"reserve_library_in_current_stage"): return false
	var header: Variant=JSON.parse_string(FileAccess.get_file_as_string(files["catalog"]["path"]))
	var content: Dictionary=main.kernel.content()
	if not Flow.check(harness,header is Dictionary and header.get("catalog_sha256")==content.get("content_digest") and header.get("pack_set_digest")==content.get("pack_set_digest") and header.get("schema_sha256")==files["schema"]["sha256"] and header.get("rule_capabilities_sha256")==files["capabilities"]["sha256"] and header.get("pack_set_digest")==files["pack_lock"]["sha256"],"reserve_installed_catalog_identity"): return false
	if not Flow.check(harness,ProjectSettings.globalize_path(options["catalog_path"])==ProjectSettings.globalize_path(files["catalog"]["path"]),"reserve_exact_catalog_path"): return false
	var scenario: Dictionary=main._data.get("scenarios",{}).get(options["scenario_id"],{})
	if not Flow.check(harness,scenario.get("dynamic_world",{}).get("population_military",{}).get("enabled",false) and main.kernel.command_context().get("actor_id")==scenario.get("player_actor"),"reserve_formal_authored_actor"): return false
	harness.rows.append({"stage":"population_military_input_identity","inputs":options.duplicate(true),"source":main.kernel.source_identity()})
	return true

## 只取实际显示行；没有源行时失败，不从完整世界快照推算当前军籍。
static func source_row(root: Node, source_id: String) -> Dictionary:
	if root.has_meta("population_military_row"):
		var row: Dictionary=root.get_meta("population_military_row")
		if row.get("source_id")==source_id: return row.duplicate(true)
	for child in root.get_children():
		var row: Dictionary=source_row(child,source_id)
		if not row.is_empty(): return row
	return {}

## 根据既有稳定metadata找真实编组勾选框，不假设其他控件的兄弟顺序或名字。
static func unit_control(root: Node, unit_id: String) -> CheckBox:
	if root is CheckBox and root.get_meta("unit_id","")==unit_id: return root
	for child in root.get_children():
		var control: CheckBox=unit_control(child,unit_id)
		if control!=null: return control
	return null

## 读取当前营务页仍走原异步有限查询，只用于核验军令阶段与实际军团身份。
static func camp_page(harness, main, tab: String) -> Dictionary:
	var view: Dictionary=main.kernel.military_camp_page(tab,{},main._military_context)
	if view.get("code")=="NativeAsyncRequired":
		if not Flow.check(harness,await harness.settled(main),"reserve_camp_read_settled"): return {}
		view=main.kernel.military_camp_page(tab,{},main._military_context)
	if not Flow.check(harness,view.get("ok",true) and view.get("code")!="NativeAsyncRequired","reserve_camp_read_available"): return {}
	return view

## 只完成既有真实行旅，不跳钟到目的地；期限来自本次军令，途中额外输入交回中央处理。
static func finish_travel(harness, main, destination: String, deadline: int) -> bool:
	var before: int=main.kernel.source_identity()["tick"]
	if not Flow.check(harness,main.kernel.action_context().get("action_kind")=="travel","reserve_real_travel_started"): return false
	if not await Flow.finish_action(harness,main,"travel",deadline): return false
	var location: Dictionary=main.kernel.presentation_location()
	var place: Dictionary=main._data.get("places",{}).get(location.get("place_id",""),{})
	if not Flow.check(harness,place.get("settlement_id")==destination and main.kernel.source_identity()["tick"]>before,"reserve_real_travel_arrived"): return false
	if not main._military_open:
		if not await Flow.click(harness,main,"camp_open"): return false
	return true

## 一条真实闭环依次训练、任命、登记、原编组、实际输粮往返、归营、回报和显式健康退籍。
static func run(harness, main, options: Dictionary={}) -> bool:
	if not Flow.check(harness,await harness.settled(main) and main._resident_mode and main.kernel.dynamic_world_enabled(),"reserve_native_main_ready"): return false
	if not verify_inputs(harness,main,options): return false
	var prefix: String=options.get("namespace","society")+"::"
	var source_id: String="source.city_reserve"
	var office_id: String=prefix+"office.city_reserve_captain"
	if not Flow.check(harness,main.kernel.source_identity()["tick"]==0 and main.kernel.presentation_location().get("place_id")==prefix+"place.courtyard","reserve_new_authored_start"): return false
	if not await Flow.click(harness,main,"dynamic_open"): return false
	var training: Dictionary={}
	for row in Flow.rows(main.page):
		if row.get("kind")=="recruitment" and row.get("source_cohort_id")=="cohort.city": training=row; break
	if not Flow.check(harness,not training.is_empty() and training.get("count")==1 and training.get("labor_count")==1,"reserve_actual_voluntary_training_entry"): return false
	var request: Dictionary=training["action"]["params"]
	var training_ticks: int=main._data["scenarios"][options["scenario_id"]]["dynamic_world"]["organization"]["activity_terms"][request["activity_terms_id"]]["duration_ticks"]
	if not await Flow.click(harness,main,"organization_recruit",training["id"]): return false
	var training_deadline: int=main.kernel.source_identity()["tick"]+training_ticks
	while main.kernel.source_identity()["tick"]<training_deadline:
		if not await Flow.wait_halfday(harness,main,training_deadline): return false
	if not await Flow.click(harness,main,"dynamic_close"): return false
	if not await Flow.click(harness,main,"camp_open"): return false
	if not await Flow.click(harness,main,"office_accept",office_id): return false
	if not await Flow.finish_action(harness,main,"office_accept",main.kernel.source_identity()["tick"]+1): return false
	if not await Flow.click(harness,main,"camp_tab","population"): return false
	var trained: Dictionary=source_row(main.page,source_id)
	if not Flow.check(harness,trained.get("reserve_status")=="trained_reserve" and trained.get("reserve_count")==1 and trained.get("serving_count")==0 and trained.has("enroll_action"),"reserve_training_is_not_enrollment"): return false
	var before_enroll: int=main.kernel.source_identity()["tick"]
	if not await Flow.click(harness,main,"population_enroll",source_id): return false
	if not await Flow.finish_action(harness,main,"population_enroll",before_enroll+1): return false
	var enrolled: Dictionary=source_row(main.page,source_id)
	if not Flow.check(harness,main.kernel.source_identity()["tick"]==before_enroll+1 and enrolled.get("reserve_count")==0 and enrolled.get("serving_count")==1 and enrolled.get("reserve_status")=="trained_reserve" and not enrolled.get("military_food_sources",[]).is_empty(),"reserve_one_halfday_source_backed_enrollment"): return false
	var boundary: Dictionary={"trained":trained.duplicate(true),"enrolled":enrolled.duplicate(true)}
	if options.has("after_enrollment"):
		if not Flow.check(harness,options["after_enrollment"] is Callable,"reserve_boundary_callback_valid"): return false
		if not await options["after_enrollment"].call(boundary): return false
	return await continue_service(harness,main,options,boundary)

## 已登记稳定边界的同一原军旅末段；冷恢复者先经真实页面回到人口页，历史观察只用于回执而不安装世界。
static func continue_service(harness, main, options: Dictionary, boundary: Dictionary) -> bool:
	if not Flow.check(harness,await harness.settled(main) and main._resident_mode and main.kernel.dynamic_world_enabled(),"reserve_continuation_native_main_ready"): return false
	if not verify_inputs(harness,main,options): return false
	var prefix: String=options.get("namespace","society")+"::"
	var source_id: String="source.city_reserve"
	var unit_id: String=prefix+"unit.city_reserve"
	var trained: Dictionary=boundary.get("trained",{})
	var enrolled: Dictionary=source_row(main.page,source_id)
	if not Flow.check(harness,not trained.is_empty() and enrolled.get("reserve_count")==0 and enrolled.get("serving_count")==1 and enrolled.get("reserve_status")=="trained_reserve" and not enrolled.get("military_food_sources",[]).is_empty() and main.kernel.action_context().get("action_id","").is_empty(),"reserve_continue_from_actual_stable_service"): return false
	if not await Flow.click(harness,main,"camp_tab","orders"): return false
	if not await Flow.click(harness,main,"campaign_accept",prefix+"order.city_outpost_supply"): return false
	var orders: Dictionary=await camp_page(harness,main,"orders")
	var campaign: Dictionary={}
	for row in orders.get("campaigns",[]):
		if row.get("template_id")==prefix+"order.city_outpost_supply": campaign=row; break
	if not Flow.check(harness,not campaign.is_empty() and campaign.get("status")=="accepted","reserve_actual_campaign_accepted"): return false
	var campaign_id: String=campaign["campaign_id"]
	var deadline: int=main.kernel.source_identity()["tick"]+campaign["deadline_remaining"]
	if not await Flow.click(harness,main,"camp_tab","formation"): return false
	var control: CheckBox=unit_control(main.page,unit_id)
	if not Flow.check(harness,control!=null and not control.disabled,"reserve_original_unit_selection"): return false
	control.button_pressed=true
	if not Flow.check(harness,await harness.settled(main),"reserve_unit_selection_settled"): return false
	var food: SpinBox=main.find_child("CampFoodQuantity",true,false)
	var money: SpinBox=main.find_child("CampMoneyQuantity",true,false)
	if not Flow.check(harness,food!=null and money!=null,"reserve_original_supply_controls"): return false
	food.value=28; money.value=40
	if not Flow.check(harness,await harness.settled(main),"reserve_supplies_quoted"): return false
	var form: Button=Flow.button(main.page,"army_form",campaign_id)
	if not Flow.check(harness,form!=null and form.get_meta("camp_params",{}).get("unit_ids")==[unit_id] and form.get_meta("camp_params",{}).get("food_quantity")==28 and form.get_meta("camp_params",{}).get("money")==40,"reserve_original_formation_request"): return false
	var before_form: int=main.kernel.source_identity()["tick"]
	if not await Flow.click(harness,main,"army_form",campaign_id): return false
	if not await Flow.finish_action(harness,main,"army_form",before_form+1): return false
	if not Flow.check(harness,main.kernel.source_identity()["tick"]==before_form+1,"reserve_original_formation_halfday"): return false
	if not await Flow.click(harness,main,"camp_tab","orders"): return false
	if not await Flow.click(harness,main,"campaign_depart",campaign_id): return false
	if not await finish_travel(harness,main,prefix+"settlement.city_outpost",deadline): return false
	if not await Flow.click(harness,main,"campaign_deliver",campaign_id): return false
	if not await Flow.click(harness,main,"campaign_return",campaign_id): return false
	if not await finish_travel(harness,main,prefix+"settlement.qingchuan",deadline): return false
	if not await Flow.click(harness,main,"camp_tab","supply"): return false
	var supply: Dictionary=await camp_page(harness,main,"supply")
	if not Flow.check(harness,not supply.get("selected_army_id","").is_empty(),"reserve_actual_returning_army"): return false
	if not await Flow.click(harness,main,"army_return",supply["selected_army_id"]): return false
	if not await Flow.finish_action(harness,main,"army_return",deadline): return false
	if not await Flow.click(harness,main,"camp_tab","population"): return false
	var returned: Dictionary=source_row(main.page,source_id)
	if not Flow.check(harness,returned.get("serving_count")==1 and returned.get("reserve_count")==0,"reserve_army_return_keeps_service"): return false
	if not await Flow.click(harness,main,"camp_tab","orders"): return false
	if not await Flow.click(harness,main,"campaign_report",campaign_id): return false
	if not await Flow.click(harness,main,"camp_tab","population"): return false
	returned=source_row(main.page,source_id)
	if not Flow.check(harness,returned.get("release_actions",[]).size()==1,"reserve_explicit_fit_release_available"): return false
	var release: Dictionary=returned["release_actions"][0]
	var before_release: int=main.kernel.source_identity()["tick"]
	if not await Flow.click(harness,main,"population_release",release["enrollment_id"]): return false
	if not await Flow.finish_action(harness,main,"population_release",before_release+1): return false
	var released: Dictionary=source_row(main.page,source_id)
	if not Flow.check(harness,main.kernel.source_identity()["tick"]==before_release+1 and released.get("serving_count")==0 and released.get("reserve_count")==1 and released.get("reserve_labor_count")==1 and released.get("allocatable_labor_count")==1 and released.get("allocated_labor_count")==0 and released.get("reserve_status")=="trained_reserve","reserve_explicit_release_without_auto_farm_allocation"): return false
	harness.rows.append({"stage":"population_military_real_main_roundtrip","source":main.kernel.source_identity(),"trained":trained,"enrolled":enrolled,"returned":returned,"released":released,"campaign_id":campaign_id,"input_identity":options.duplicate(true)})
	return true
