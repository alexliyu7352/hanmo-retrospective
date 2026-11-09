## 单个常驻Bridge的有限前端读缓存；install后不保GD世界、不提交GD规则、不用debug export热刷新。
extends Node
signal frame_ready(source: Dictionary, scene_epoch: int)
signal command_completed(kind: String, result: Dictionary, source: Dictionary, current: Dictionary)
signal preview_ready(kind: String, params: Dictionary, result: Dictionary, source: Dictionary, scene_epoch: int)
signal page_reads_ready(source: Dictionary, scene_epoch: int)
signal failed(error: Dictionary)
const Presenter=preload("res://ui/resident_journey_presenter.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const BASE_QUERIES: Array[String]=["command_context","action_context","screen_context","hud_view","journey_input_need","interaction_view"]
var _bridge: Object
var _presenter: Node
var _installed: bool=false
var _catalog: Dictionary={}
var _presentation_data: Dictionary={}
var _source: Dictionary={}
var _actor_id: String=""
var _scenario_id: String=""
var _scene_epoch: int=0
var _generation: int=0
var _refreshing: bool=false
var _job: Dictionary={}
var _stage: Dictionary={}
var _seed: Dictionary={}
var _published: Dictionary={}
var _last_error: Dictionary={}
var _preview_cache: Dictionary={}
var _character_ids: Array[String]=[]
var _quote_requests: Array=[]
var _page_cache: Dictionary={}
var _pending_reads: Array=[]
var _pending_read_keys: Dictionary={}
var _continuation_read_generation: int=0
var _catalog_source_sha256: String=""
const MAX_PAGE_READS: int=4096

## 唯一创建点便于孤立协议测试替换transport；产品始终创建同一个真实Bridge实例。
func _create_bridge() -> Object:
	return ClassDB.instantiate("HanmoResidentBridge") if ClassDB.class_exists("HanmoResidentBridge") else null

## 独立未安装Bridge只读取校验后的选择提示；不把目录提示当成完整存档准入。
static func inspect_checkpoint_selection(bytes: PackedByteArray) -> Dictionary:
	if not ClassDB.class_exists("HanmoResidentBridge"): return {"ok":false,"code":"UnsupportedNativeCapability","capability":"inspect_checkpoint_selection"}
	var inspector: Object=ClassDB.instantiate("HanmoResidentBridge")
	if not is_instance_valid(inspector) or not inspector.has_method("inspect_checkpoint_selection"):
		return {"ok":false,"code":"UnsupportedNativeCapability","capability":"inspect_checkpoint_selection"}
	var result: Variant=inspector.inspect_checkpoint_selection(bytes)
	if not result is Dictionary: return {"ok":false,"code":"InvalidCheckpointSelection"}
	return result.duplicate(true)

## 目录与seed只在一次冷JSON边界进入core；保原字段序和int64/float标签，成功后只留目录。
func install(catalog: Dictionary, seed: Dictionary) -> Dictionary:
	if _installed: return _error("ResidentAlreadyInstalled")
	if is_busy(): return _error("ResidentBackendBusy")
	if not is_instance_valid(_bridge): _bridge=_create_bridge()
	if not is_instance_valid(_bridge): return _error("UnsupportedNativeCapability","install")
	for method in ["install","enqueue_live_operation","poll_live_operation"]:
		if not _bridge.has_method(method): return _error("UnsupportedNativeCapability",method)
	var result: Dictionary=_bridge.install(JSON.stringify(catalog,"",false,true).to_utf8_buffer(),JSON.stringify(seed,"",false,true).to_utf8_buffer())
	if not result.get("ok",false): return result.duplicate(true)
	if not _valid_source(result.get("identity")) or not seed.get("player_actor_id") is String or not seed.get("scenario_id") is String: return _error("InvalidResidentInstallReply")
	_source=result.identity.duplicate(true); _actor_id=seed.player_actor_id; _scenario_id=seed.scenario_id; _catalog=catalog.duplicate(true); _installed=true
	_presentation_data=catalog.get("data",{}).duplicate(true)
	_catalog_source_sha256=JSON.stringify(_catalog.get("data",{}),"",true).sha256_text()
	_presenter=Presenter.new(); add_child(_presenter); _presenter.set_process(false)
	_presenter.frame_ready.connect(_presenter_ready); _presenter.failed.connect(_presenter_failed)
	return result.duplicate(true)

## 原生profile的冷请求绑定完整原目录身份；不用GD工厂创建世界或生成另一份人口。
static func native_request(catalog: Dictionary, scenario_id: String, founder: String="", appearance: Dictionary={}, unlocked_actor_ids: Variant=null, character_creation_request: Dictionary={}) -> Dictionary:
	var definition: Dictionary=catalog.get("data",{}).get("scenarios",{}).get(scenario_id,{})
	var expected: Dictionary={"content_id":catalog.get("content_id",""),"content_version":catalog.get("content_version",""),"content_digest":catalog.get("content_digest",""),"ruleset_id":"hanmo.native.local_society/1","schema_sha256":catalog.get("schema_sha256",""),"artifact_sha256":JSON.stringify(catalog,"",false,true).sha256_text(),"module_versions":[{"name":"dynamic_world","version":"local_society_v1"},{"name":"local_economy","version":"1"},{"name":"organization","version":"2"},{"name":"affairs","version":"1"},{"name":"director","version":"1"}]}
	if definition.get("dynamic_world",{}).has("population_military"):
		expected["module_versions"][1]["version"]="2"
		expected["module_versions"].append({"name":"population_military","version":"2" if definition.get("dynamic_world",{}).has("siege_integration") else "1"})
	if definition.get("dynamic_world",{}).has("regional_life"):
		expected["module_versions"][4]["version"]="2"
		expected["module_versions"].append({"name":"regional_life","version":"1"})
	if definition.get("dynamic_world",{}).has("siege_integration"):
		expected["module_versions"].append({"name":"siege_integration","version":"1"})
	if definition.get("dynamic_world",{}).get("organization",{}).get("named_membership_enabled",false):
		for version in expected["module_versions"]:
			if version.name in ["local_economy","organization","director"]: version["version"]="3"
		expected["module_versions"].append({"name":"named_membership","version":"1"})
	# 单一显式外交块共同产生两个必需模块；无块的旧情景保持原身份。
	if definition.get("dynamic_world",{}).has("diplomacy"):
		for module in ["diplomacy","envoy_tasks"]: expected["module_versions"].append({"name":module,"version":str(definition["dynamic_world"]["diplomacy"].get("version",0))})
	# 新显式能力各有模块身份，不改旧情景或全局能力摘要。
	if definition.get("dynamic_world",{}).has("politics"): expected["module_versions"].append({"name":"politics","version":"1"})
	if definition.get("dynamic_world",{}).has("political_fiscal"): expected["module_versions"].append({"name":"political_fiscal","version":"1"})
	if definition.get("dynamic_world",{}).has("treaty_passage"):
		expected["module_versions"].append({"name":"treaty_passage","version":"1"})
	if definition.get("dynamic_world",{}).has("political_military_appointments"):
		expected["module_versions"].append({"name":"political_military_appointments","version":"1"})
	if definition.get("dynamic_world",{}).has("political_field_appointments"):
		expected["module_versions"].append({"name":"political_field_appointments","version":"1"})
	if definition.get("dynamic_world",{}).has("political_occupation_fiscal"):
		for module in ["political_occupation_fiscal","occupation_fiscal_settlement"]: expected["module_versions"].append({"name":module,"version":"1"})
	if definition.get("dynamic_world",{}).has("political_takeover"):
		for module in ["political_takeover","economic_takeover_execution"]: expected["module_versions"].append({"name":module,"version":"1"})
		expected["module_versions"].append({"name":"task_takeover_execution","version":str(definition["dynamic_world"]["political_takeover"].get("task_execution_version",1))})
	var economic_definition: Dictionary=definition.get("dynamic_world",{}).get("local_economy",{})
	if economic_definition.has("production_management"):
		expected["module_versions"][1]["version"]="4"
		expected["module_versions"].append({"name":"production_management","version":"1"})
	if economic_definition.has("household_provisioning"):
		expected["module_versions"][1]["version"]="5"
		expected["module_versions"].append({"name":"household_provisioning","version":"1"})
	if definition.get("dynamic_world",{}).has("fiscal_settlement"): expected["module_versions"].append({"name":"fiscal_settlement","version":"1"})
	if definition.get("dynamic_world",{}).has("fiscal_taxpayer_continuity"): expected["module_versions"].append({"name":"fiscal_taxpayer_continuity","version":"1"})
	# 三个模块同启；部分作者配置仍由原生严格准入拒绝。
	if definition.get("dynamic_world",{}).has("politics_governance") or definition.get("dynamic_world",{}).has("public_relief"):
		for module in ["politics_governance","governance_tasks","public_relief"]: expected["module_versions"].append({"name":module,"version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).get("registry_version",0)==1:
		expected["module_versions"][4]["version"]="4"
		expected["module_versions"].append({"name":"director_registry","version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).get("diplomatic_response_version",0)==1: expected["module_versions"].append({"name":"diplomatic_response","version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).has("authored_task_continuation"): expected["module_versions"].append({"name":"authored_task_continuation","version":str(definition["dynamic_world"]["director"]["authored_task_continuation"]["version"])})
	if definition.get("dynamic_world",{}).get("affairs",{}).get("obligation_needs_version",0)==1:
		expected["module_versions"][3]["version"]="2"
		expected["module_versions"].append({"name":"obligation_needs","version":"1"})
	# 财政需求只由显式作者块启用，独立或与工资共存均沿原生Affairs3。
	if definition.get("dynamic_world",{}).get("affairs",{}).has("fiscal_need_policies"):
		expected["module_versions"][3]["version"]="3"
		expected["module_versions"].append({"name":"fiscal_need_policies","version":"1"})
	if definition.get("dynamic_world",{}).has("occupation_fiscal_needs"): expected["module_versions"].append({"name":"occupation_fiscal_needs","version":"1"})
	# 匿名工资不自动启用具名工资或财政映射。
	if definition.get("dynamic_world",{}).get("affairs",{}).has("cohort_wage_need_policies"):
		expected["module_versions"][3]["version"]="4"
		expected["module_versions"].append({"name":"cohort_wage_need_policies","version":"1"})
	if not definition.get("dynamic_world",{}).get("regional_life",{}).get("outcome_topics",[]).is_empty(): expected["module_versions"].append({"name":"regional_outcome","version":"1"})
	var segmented: bool=false
	for role_id in catalog.get("data",{}).get("roles",{}):
		var role: Dictionary=catalog.data.roles[role_id]
		for course in role.get("skill_courses",[]):
			if course.get("settlement_policy","")=="per_lesson_v1": segmented=true
			if course.get("settlement_policy","")=="innate_per_lesson_v1" and role_id in definition.get("enabled",{}).get("roles",[]): segmented=true
	if segmented: expected["module_versions"].append({"name":"study_segments","version":"1"})
	if definition.get("player_plan_profile","")=="known_life_v1": expected["module_versions"].append({"name":"player_plan","version":"1"})
	var actor_policy: String=definition.get("dynamic_world",{}).get("founder_policy","")
	if actor_policy in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"]: expected["module_versions"].append({"name":"selected_existing_actor","version":"2" if actor_policy=="selected_existing_actor_roster_v1" else "1"})
	if definition.get("skill_profile","")=="full_sixteen_v1": expected["module_versions"].append({"name":"skills_extended","version":"1"})
	if definition.get("growth_cards",{}).get("profile","")=="learned_ability_cards_v1": expected["module_versions"].append({"name":"growth_cards","version":"1"})
	if definition.get("task_offer_response_profile","") in ["known_offers_v1","known_offers_v2"]: expected["module_versions"].append({"name":"task_offer_responses","version":"2" if definition.task_offer_response_profile=="known_offers_v2" else "1"})
	if definition.get("dynamic_world",{}).has("task_personal_advances"): expected["module_versions"].append({"name":"task_personal_advances","version":"1"})
	if definition.has("task_message_handoff_profile"): expected["module_versions"].append({"name":"task_message_handoffs","version":"4" if definition.task_message_handoff_profile=="known_personal_delivery_and_sender_report_v4" else "3" if definition.task_message_handoff_profile=="known_personal_delivery_and_report_v3" else "2" if definition.task_message_handoff_profile=="observed_letter_delivery_and_report_v2" else "1"})
	if definition.get("party_invitation_profile","")=="authored_node_v1": expected["module_versions"].append({"name":"party_invitations","version":"1"})
	if definition.get("party_recruitment_profile","")=="provider_declaration_v1": expected["module_versions"].append({"name":"party_recruitment_consents","version":"1"})
	var actor_card_profile: String=definition.get("actor_card_unlocks",{}).get("profile","")
	if actor_card_profile in ["completed_procurement_actor_cards_v1","completed_task_actor_cards_v2"]: expected["module_versions"].append({"name":"actor_card_unlocks","version":"2" if actor_card_profile=="completed_task_actor_cards_v2" else "1"})
	if definition.get("teaching_requests",{}).get("profile","")=="authored_teaching_requests_v1": expected["module_versions"].append({"name":"teaching_requests","version":"1"})
	if definition.get("study_teaching_requests",{}).get("profile","")=="authored_study_teaching_requests_v1": expected["module_versions"].append({"name":"study_teaching_requests","version":"1"})
	if definition.get("military_request_policy",{}).get("profile","")=="request_policy/1": expected["module_versions"].append({"name":"military_requests","version":"1"})
	if definition.get("military_officer_assignments",{}).get("profile","")=="political_appointment_v1": expected["module_versions"].append({"name":"military_officer_assignments","version":"1"})
	if definition.get("military_field_commands",{}).get("profile","")=="casualty_appointment_v1": expected["module_versions"].append({"name":"military_field_commands","version":"1"})
	if definition.get("military_wounded_recovery",{}).get("profile","")=="supplied_wound_batches_v1": expected["module_versions"].append({"name":"military_wounded_recovery","version":"1"})
	if definition.get("military_supply_attrition",{}).get("profile","")=="anonymous_shortage_v1": expected["module_versions"].append({"name":"military_supply_attrition","version":"1"})
	if definition.get("military_rout_retreat",{}).get("profile","")=="two_blocked_rounds_v1": expected["module_versions"].append({"name":"military_rout_retreat","version":"1"})
	if definition.get("military_command_shock",{}).get("profile","")=="army_commander_once_v1": expected["module_versions"].append({"name":"military_command_shock","version":"1"})
	if definition.get("military_garrison_rest",{}).get("profile","")=="supplied_garrison_rest_v1": expected["module_versions"].append({"name":"military_garrison_rest","version":"1"})
	if definition.get("military_combat_conditions",{}).get("profile","")=="entry_fatigue_03_v1": expected["module_versions"].append({"name":"military_combat_conditions","version":"1"})
	if definition.get("military_commanded_damage",{}).get("profile","")=="entry_commanded_damage_05_v1": expected["module_versions"].append({"name":"military_commanded_damage","version":"1"})
	if definition.get("military_sequential_rounds",{}).get("profile","")=="entry_initiative_05_v1": expected["module_versions"].append({"name":"military_sequential_rounds","version":"1"})
	if definition.get("military_field_terrain",{}).get("profile","")=="terrain_and_zones_05_v1": expected["module_versions"].append({"name":"military_field_terrain","version":"1"})
	if definition.get("military_command_resources",{}).get("profile","")=="entry_tactics_recovery_05_v1": expected["module_versions"].append({"name":"military_command_resources","version":"1"})
	if definition.get("military_tactic_learning",{}).get("version",0)==1: expected["module_versions"].append({"name":"military_tactic_learning","version":"1"})
	if definition.get("military_siege_engineering",{}).get("version",0)==1: expected["module_versions"].append({"name":"military_siege_engineering","version":"1"})
	if definition.get("insight_cards",{}).get("profile","")=="completed_camp_training_insights_v1": expected["module_versions"].append({"name":"growth_insights","version":"1"})
	if definition.get("military_naval",{}).has("repair_policy"): expected["module_versions"].append({"name":"naval_repairs","version":"1"})
	if definition.get("military_naval",{}).has("repair_charge_policy"): expected["module_versions"].append({"name":"naval_repair_charges","version":"1"})
	if definition.get("military_naval",{}).has("navigation_policy"): expected["module_versions"].append({"name":"naval_navigation_wear","version":"1"})
	if definition.get("military_naval",{}).get("combat_policy",{}).has("tactical_navigation"): expected["module_versions"].append({"name":"naval_tactical_navigation","version":"1"})
	if definition.get("military_naval",{}).get("combat_policy",{}).has("gunnery"): expected["module_versions"].append({"name":"naval_gunnery","version":"1"})
	if definition.get("military_naval",{}).get("profile","")=="afloat_inland_v1": expected["module_versions"].append({"name":"military_naval","version":"6" if definition.get("military_naval",{}).get("combat_policy",{}).has("gunnery") else "5" if definition.get("military_naval",{}).has("navigation_policy") else "4" if definition.get("military_naval",{}).has("repair_charge_policy") else "3" if definition.get("military_naval",{}).has("repair_policy") else "2" if definition.get("military_naval",{}).has("combat_policy") else "1"})
	if definition.get("household_control",{}).get("profile","")=="initial_household_stewardship_v1": expected["module_versions"].append({"name":"household_control","version":"1"})
	if definition.get("dynamic_world",{}).has("household_provision_management"): expected["module_versions"].append({"name":"household_provision_management","version":"1"})
	if definition.get("character_creation",{}).get("profile")=="point_buy_character_v1": expected["module_versions"].append({"name":"character_creation","version":"1"})
	if definition.get("dynamic_world",{}).has("production_disruptions"): expected["module_versions"].append({"name":"production_disruptions","version":"1"})
	if definition.get("adult_adoption",{}).get("profile")=="authored_adult_adoption_requests_v1": expected["module_versions"].append({"name":"adult_adoption","version":"1"})
	if definition.get("family_life",{}).get("profile")=="verified_military_deaths_v1": expected["module_versions"].append({"name":"family_life","version":"1"})
	var legacy_profile := String(definition.get("family_legacy",{}).get("profile",""))
	if legacy_profile in ["authored_posthumous_control_consent_v1","posthumous_control_sources_v2","posthumous_control_sources_v3"]: expected["module_versions"].append({"name":"family_legacy","version":"3" if legacy_profile=="posthumous_control_sources_v3" else ("2" if legacy_profile=="posthumous_control_sources_v2" else "1")})
	if definition.get("family_bequest",{}).get("profile")=="revocable_personal_bequest_v1": expected["module_versions"].append({"name":"family_bequest","version":"1"})
	if definition.has("task_message_notices"): expected["module_versions"].append({"name":"task_message_notices","version":"1"})
	if definition.get("family_inheritance",{}).get("profile")=="disclosed_personal_bequest_v1": expected["module_versions"].append({"name":"family_inheritance","version":"1"})
	if definition.get("family_marriage",{}).get("profile")=="personal_marriage_separate_assets_v1": expected["module_versions"].append({"name":"family_marriage","version":"1"})
	if definition.has("task_static_asset_profile"): expected["module_versions"].append({"name":"task_static_assets","version":"1"})
	if definition.get("dynamic_world",{}).has("estate_employment"): expected["module_versions"].append({"name":"estate_employment","version":"1"})
	if definition.get("dynamic_world",{}).has("economic_supply_policies"):
		for module in ["economic_supply_policies","political_supply_adoptions"]: expected["module_versions"].append({"name":module,"version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).has("political_supply_renewals"): expected["module_versions"].append({"name":"political_supply_renewals","version":"1"})
	if definition.get("voluntary_succession",{}).get("profile") in ["authored_voluntary_succession_v1","living_accepted_family_paths_v2"]: expected["module_versions"].append({"name":"voluntary_succession","version":"2" if definition.get("voluntary_succession",{}).get("profile")=="living_accepted_family_paths_v2" else "1"})
	if definition.get("family_birth",{}).get("profile")=="adult_birth_personal_care_v1" and definition.get("family_lineage",{}).get("profile")=="biological_origin_innate_v1":
		expected["module_versions"].append({"name":"family_lineage","version":"1"})
		expected["module_versions"].append({"name":"family_birth","version":"1"})
	if definition.get("visit_message_profile","")=="witnessed_reception_v1": expected["module_versions"].append({"name":"visit_messages","version":"1"})
	if definition.get("family_innate_learning",{}).get("profile")=="lineage_per_lesson_v1": expected["module_versions"].append({"name":"family_innate_learning","version":"1"})
	if definition.get("visit_reception_profile","")=="authored_local_reception_v1": expected["module_versions"].append({"name":"visit_reception_intents","version":"1"})
	if definition.get("visit_reception_profile","")=="authored_local_reception_v1": expected["module_versions"].append({"name":"visit_reception_claims","version":"1"})
	if definition.get("family_care_relief",{}).get("profile")=="living_parent_private_relief_v1": expected["module_versions"].append({"name":"family_care_relief","version":"1"})
	for module in ["growth_market_history","political_shop_licensing","private_shops"]:
		if definition.get("dynamic_world",{}).has(module): expected["module_versions"].append({"name":module,"version":"1"})
	if definition.get("task_shop_order_profile","")=="private_shop_orders_v1": expected["module_versions"].append({"name":"task_shop_orders","version":"1"})
	if definition.has("task_extensions"): expected["module_versions"].append({"name":"task_extensions","version":"1"})
	if definition.get("dynamic_world",{}).has("political_shop_registry_successions"): expected["module_versions"].append({"name":"political_shop_registry_successions","version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).has("political_shop_registry_intents"): expected["module_versions"].append({"name":"political_shop_registry_intents","version":"1"})
	if definition.get("personal_appointment_profile","")=="witnessed_personal_meetings_v1": expected["module_versions"].append({"name":"personal_appointments","version":"1"})
	if definition.get("dynamic_world",{}).has("personal_injury_recovery"): expected["module_versions"].append({"name":"personal_injury_recovery","version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).has("personal_appointment_attendance"): expected["module_versions"].append({"name":"personal_appointment_attendance","version":"1"})
	if definition.get("dynamic_world",{}).has("political_shop_licensing_receipts"): expected["module_versions"].append({"name":"political_shop_licensing_receipts","version":"1"})
	if definition.get("dynamic_world",{}).has("private_shop_liquidations"):
		expected["module_versions"].append({"name":"private_shop_liquidations","version":"1"})
		expected["module_versions"].append({"name":"task_shop_order_estate_cancellations","version":"1"})
	if definition.has("family_apprenticeship"): expected["module_versions"].append({"name":"family_apprenticeship","version":"1"})
	if definition.get("dynamic_world",{}).has("political_governance_continuations"): expected["module_versions"].append({"name":"political_governance_continuations","version":"1"})
	if definition.get("contact_inquiry_profile","")=="witnessed_personal_inquiries_v1": expected["module_versions"].append({"name":"contact_inquiries","version":"1"})
	if definition.get("dynamic_world",{}).get("director",{}).has("political_governance_intents"): expected["module_versions"].append({"name":"political_governance_intents","version":"1"})
	if definition.get("visit_reply_profile","") in ["witnessed_visit_replies_v1","witnessed_authored_visit_replies_v2"]: expected["module_versions"].append({"name":"visit_message_replies","version":"2" if definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2" else "1"})
	if definition.has("family_disciple_legacy"): expected["module_versions"].append({"name":"family_disciple_legacy","version":"1"})
	if definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2": expected["module_versions"].append({"name":"visit_reply_intents","version":"1"})
	if definition.get("dynamic_world",{}).has("political_governance_invitation_replies"): expected["module_versions"].append({"name":"political_governance_invitation_replies","version":"1"})
	if definition.get("dynamic_world",{}).has("private_shop_daily_rent"): expected["module_versions"].append({"name":"private_shop_daily_rent","version":"1"})
	if definition.get("dynamic_world",{}).has("born_personal_dialogues"): expected["module_versions"].append({"name":"born_personal_dialogues","version":"1"})
	if definition.has("family_biological_legacy"): expected["module_versions"].append({"name":"family_biological_legacy","version":"1"})
	if definition.get("dynamic_world",{}).has("political_successions"): expected["module_versions"].append({"name":"political_successions","version":"1"})
	if definition.get("visit_appointment_profile","")=="witnessed_visit_appointments_v1":
		expected["module_versions"].append({"name":"visit_appointment_invitations","version":"1"})
		expected["module_versions"].append({"name":"visit_appointment_meetings","version":"1"})
		if definition.has("visit_appointment_intents"): expected["module_versions"].append({"name":"visit_appointment_intents","version":"1"})
	if definition.get("dynamic_world",{}).has("political_head_handovers"): expected["module_versions"].append({"name":"political_head_handovers","version":"1"})
	if definition.get("dynamic_world",{}).has("private_shop_command_results"): expected["module_versions"].append({"name":"private_shop_command_results","version":"1"})
	if definition.has("visit_appointment_attendance"): expected["module_versions"].append({"name":"visit_appointment_attendance","version":"1"})
	if definition.get("dynamic_world",{}).has("growth_tactic_lessons"): expected["module_versions"].append({"name":"growth_tactic_lessons","version":"1"})
	if definition.get("dynamic_world",{}).has("political_successor_rounds"): expected["module_versions"].append({"name":"political_successor_rounds","version":"1"})
	if definition.get("dynamic_world",{}).has("political_term_handovers"): expected["module_versions"].append({"name":"political_term_handovers","version":"1"})
	if str(definition.get("visit_message_receipt_profile", "")) == "permanent_visit_message_results_v1": expected["module_versions"].append({"name":"visit_message_receipts","version":"1"})
	if definition.get("dynamic_world",{}).has("known_personal_dialogues"): expected["module_versions"].append({"name":"known_personal_dialogues","version":"1"})
	if definition.get("dynamic_world",{}).has("political_office_receptions"): expected["module_versions"].append({"name":"political_office_receptions","version":"1"})
	if str(definition.get("task_shop_order_contact_profile",""))=="original_shop_order_contacts_v1": expected["module_versions"].append({"name":"task_shop_order_contacts","version":"1"})
	if definition.has("family_collateral_legacy"): expected["module_versions"].append({"name":"family_collateral_legacy","version":"1"})
	if definition.get("dynamic_world",{}).has("political_governance_agencies"):
		expected["module_versions"].append({"name":"political_governance_agencies","version":str(definition["dynamic_world"]["political_governance_agencies"].get("version",0))})
		if definition["dynamic_world"].get("director",{}).has("governance_agency_intents"): expected["module_versions"].append({"name":"political_governance_agency_intents","version":"1"})
	if definition.has("personal_simultaneous_outcomes"): expected["module_versions"].append({"name":"personal_simultaneous_outcomes","version":str(definition["personal_simultaneous_outcomes"].get("version",0))})
	if definition.has("task_message_returns"): expected["module_versions"].append({"name":"task_message_returns","version":"1"})
	if definition.has("family_voluntary_paths"): expected["module_versions"].append({"name":"family_voluntary_paths","version":str(definition["family_voluntary_paths"].get("version",0))})
	if definition.get("dynamic_world",{}).has("task_personal_message_deliveries"): expected["module_versions"].append({"name":"task_personal_message_deliveries","version":"1"})
	if definition.get("dynamic_world",{}).has("task_personal_message_reports"): expected["module_versions"].append({"name":"task_personal_message_reports","version":"1"})
	if definition.get("dynamic_world",{}).has("task_personal_message_cancellations"): expected["module_versions"].append({"name":"task_personal_message_cancellations","version":"1"})
	if definition.get("dynamic_world",{}).has("political_office_resignations"):
		expected["module_versions"].append({"name":"political_office_resignations","version":str(definition["dynamic_world"]["political_office_resignations"].get("version",0))})
		if definition["dynamic_world"].get("director",{}).has("office_resignation_responses"): expected["module_versions"].append({"name":"political_office_resignation_responses","version":"1"})

	if definition.get("dynamic_world",{}).has("political_subordinate_appointments"):
		expected["module_versions"].append({"name":"political_subordinate_appointments","version":"1"})
	if definition.get("dynamic_world",{}).has("political_subordinate_appointment_intents"):
		expected["module_versions"].append({"name":"political_subordinate_appointment_intents","version":"1"})
	if definition.get("dynamic_world",{}).has("task_personal_message_acceptances"): expected["module_versions"].append({"name":"task_personal_message_acceptances","version":"1"})

	if definition.get("dynamic_world",{}).has("political_dismissal_sources"):
		expected["module_versions"].append({"name":"political_dismissal_sources","version":"1"})

	if catalog.has("pack_set_digest"): expected["pack_set_digest"]=catalog["pack_set_digest"]
	var request: Dictionary={"scenario_id":scenario_id,"world_seed":definition.get("seed_text",""),"expected_catalog_identity":expected}
	if not founder.is_empty(): request["founder_selection"]=founder
	if not appearance.is_empty(): request["appearance_request"]=appearance.duplicate(true)
	if not character_creation_request.is_empty(): request["character_creation_request"]=character_creation_request.duplicate(true)
	if unlocked_actor_ids!=null: request["unlocked_actor_ids"]=unlocked_actor_ids.duplicate(true) if unlocked_actor_ids is Array else unlocked_actor_ids
	return request

## 已安装身份和选择只来自Bridge成功结果；失败不能把目录或人物留作半个活世界。
func _accept_native_install(catalog: Dictionary, result: Dictionary) -> Dictionary:
	if not result.get("ok",false):
		# 冷边界拒绝保留原code/path/阶段诊断；不打印目录、请求或人物资产正文。
		print("RESIDENT_NATIVE_INSTALL_REJECTED "+JSON.stringify(result))
		return result.duplicate(true)
	if not _valid_source(result.get("identity")) or not result.get("player_actor_id") is String or not result.get("scenario_id") is String: return _error("InvalidResidentInstallReply")
	var effective: Dictionary=Snapshot.resolve(catalog,result["scenario_id"])
	if not effective.get("ok",false): return effective.duplicate(true)
	_source=result["identity"].duplicate(true); _actor_id=result["player_actor_id"]; _scenario_id=result["scenario_id"]; _catalog=catalog.duplicate(true); _installed=true
	_presentation_data=effective["catalog"]["data"].duplicate(true)
	# 原生冷目录保留完整被动定义；只补齐名称/模板，不扩展已选路线、岗位或实际人物集合。
	for table in Snapshot.PASSIVE_TABLES:
		_presentation_data[table]=catalog["data"].get(table,{}).duplicate(true)
	var actor_definitions: Dictionary=catalog["data"].get("actors",{}).duplicate(true)
	# 当前剧本的人物初态覆盖仍与原生投影一致，其余原型只作为不可变定义可供展示。
	actor_definitions.merge(_presentation_data.get("actors",{}),true)
	_presentation_data["actors"]=actor_definitions
	_catalog_source_sha256=JSON.stringify(_catalog.get("data",{}),"",true).sha256_text()
	_presenter=Presenter.new(); add_child(_presenter); _presenter.set_process(false)
	_presenter.frame_ready.connect(_presenter_ready); _presenter.failed.connect(_presenter_failed)
	return result.duplicate(true)

## 新局只走目录原生入口；缺能力明确拒绝，不调用legacy工厂补空seed。
func install_native(catalog: Dictionary, request: Dictionary) -> Dictionary:
	if _installed: return _error("ResidentAlreadyInstalled")
	if is_busy(): return _error("ResidentBackendBusy")
	if not is_instance_valid(_bridge): _bridge=_create_bridge()
	if not is_instance_valid(_bridge): return _error("UnsupportedNativeCapability","install_native")
	for method in ["install_native","enqueue_live_operation","poll_live_operation"]:
		if not _bridge.has_method(method): return _error("UnsupportedNativeCapability",method)
	var result: Variant=_bridge.install_native(JSON.stringify(catalog,"",false,true).to_utf8_buffer(),JSON.stringify(request,"",false,true).to_utf8_buffer())
	if not result is Dictionary: return _error("InvalidResidentInstallReply")
	return _accept_native_install(catalog,result)

## 冷恢复直接准入原检查点全部领域，绝不先新建世界或重放一期经济。
func install_checkpoint(catalog: Dictionary, bytes: PackedByteArray, continuation_bytes: PackedByteArray=PackedByteArray()) -> Dictionary:
	if _installed: return _error("ResidentAlreadyInstalled")
	if is_busy(): return _error("ResidentBackendBusy")
	if not is_instance_valid(_bridge): _bridge=_create_bridge()
	if not is_instance_valid(_bridge) or not _bridge.has_method("install_checkpoint"): return _error("UnsupportedNativeCapability","install_checkpoint")
	var result: Variant=_bridge.install_checkpoint(JSON.stringify(catalog,"",false,true).to_utf8_buffer(),bytes,continuation_bytes)
	if not result is Dictionary: return _error("InvalidResidentInstallReply")
	return _accept_native_install(catalog,result)

## 许可的有限读遵循原页面异步队列，不导出世界或推进时间。
func continuation_state() -> Dictionary:
	return demand_view("continuation_state")

## 同一世界版本内begin/cancel也会改许可；只撤销这一有限读的旧缓存。
func invalidate_continuation_state() -> void:
	var key: String="query:"+JSON.stringify({"query_id":"continuation_state","args":{}},"",false,true)
	_continuation_read_generation+=1
	_page_cache.erase(key)

## 页面显式检查所选情景profile；非空领域表不代表已启用宏观运行。
func dynamic_world_enabled() -> bool:
	return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("profile","")=="local_society_v1"

## 计划仅在当前情景明确启用；旧目录无flag时不显示或制造默认计划。
func player_plan_enabled() -> bool:
	return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("player_plan_profile","")=="known_life_v1"

## 只读当前场所中可观察的有限行，来源仍由统一page缓存严格绑定。
func dynamic_place(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("dynamic_place",{"place_id":place_id,"cursor":cursor})

## 新登记能力的有限观察页；游标只作同源值转交，不取得整个世界或补全未知信息。
## 地方经济读口只附当前页选择，原生复核本人地点与真实公职授权。
func diplomacy_enabled() -> bool:
	var definition: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("diplomacy",{})
	return _installed and definition.get("enabled",false) and definition.get("version",0) in [1,2]

func diplomacy_entries(place_id: String) -> Dictionary:
	return demand_view("diplomacy",{"place_id":place_id})

## 新税政和实际税账同一显式配置，不由已有财政页隐式启用。
## 双签通行只有显式独立配置，旧外交接受不自动升级。
func treaty_passage_enabled() -> bool:
	var config: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("treaty_passage",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1
## 政治签受必须与同局Military6调任共同启用，旧任命不自动取得军权。
func political_military_appointments_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	var config: Dictionary=scenario.get("dynamic_world",{}).get("political_military_appointments",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1 and scenario.get("military_officer_assignments",{}).get("profile","")=="political_appointment_v1"

## 野外换帅另有专项配对；驻营调任能力不能冒野外授权。
func political_field_appointments_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	var config: Dictionary=scenario.get("dynamic_world",{}).get("political_field_appointments",{})
	return political_military_appointments_enabled() and config.get("enabled",false) and config.get("version",0)==1 and scenario.get("military_field_commands",{}).get("profile","")=="casualty_appointment_v1"

func political_field_appointments(cursor: int=0) -> Dictionary:
	return demand_view("political_field_appointments",{"cursor":cursor})

func political_occupation_fiscal_enabled() -> bool:
	var config: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("political_occupation_fiscal",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1

## 接任后的真实税账使用自己的游标，不混入原财政1历史。
func fiscal_taxpayer_continuity(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("fiscal_taxpayer_continuity",args)

func occupation_fiscal_settlement(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("occupation_fiscal_settlement",args)

func political_occupation_fiscal_entries(place_id: String) -> Dictionary:
	return demand_view("political_occupation_fiscal",{"place_id":place_id})

func political_takeover_enabled() -> bool:
	var config: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("political_takeover",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1

func political_takeover_entries(place_id: String) -> Dictionary:
	return demand_view("political_takeover",{"place_id":place_id})

func politics_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("politics",{}).get("enabled",false)

func politics_entries(place_id: String) -> Dictionary:
	return demand_view("politics_entries",{"place_id":place_id})

## 财政能力单独显式开启；只有公职任免的情景不请求未注册的税页。
func political_fiscal_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("political_fiscal",{}).get("enabled",false)

func political_fiscal_entries(place_id: String) -> Dictionary:
	return demand_view("political_fiscal_entries",{"place_id":place_id})

## 治理必须由政治与救济共同显式启用，旧情景不发新的读请求。
func politics_governance_enabled() -> bool:
	var dynamic: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{})
	return _installed and dynamic.get("politics_governance",{}).get("enabled",false) and dynamic.get("public_relief",{}).get("version",0)==1

func politics_governance(place_id: String) -> Dictionary:
	return demand_view("politics_governance",{"place_id":place_id})

## 财政仅转交当前本人页面参数；空游标/身份省略，原生严格验证合法源。
func fiscal_settlement(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("fiscal_settlement",args)

func economic_supply_policies_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("economic_supply_policies")

func economic_supply_policies(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("economic_supply_policies",args)

func local_economy(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("local_economy",args)

func world_observation(place_id: String, cursor: Dictionary={}) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	return demand_view("world_observation",args)

## 新版只读观察页须由当前已安装情景显式启用，旧目录不发新增读取。
func world_observation_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("director",{}).get("registry_version",0)==1

## 合同详情只允许读取本人的运行任务，不接受调用方扩大观察主体。
func dynamic_contract(task_id: String) -> Dictionary:
	return demand_view("dynamic_contract",{"task_id":task_id})

## 内容是不可变作者目录的副本，不是已安装世界的快照。
func content() -> Dictionary: return _catalog.duplicate(true)

## Main只取得原ScenarioSnapshot投影的静态表现集合；原目录仍单独保留用于精确checkpoint身份。
func presentation_data() -> Dictionary: return _presentation_data.duplicate(true)

## controller与冷存储复用此唯一transport，不创建第二个worker或resident世界。
func transport() -> Object: return _bridge

## 只返回四字段执行来源；调用者不得把它替代原命令request/revision规则。
func source_identity() -> Dictionary: return _source.duplicate(true)

## 原生完成后显式交接来源；旧已排队读取只失去发布资格，不重绑为新revision执行。
func adopt_source(current: Dictionary) -> Dictionary:
	if not _installed: return _error("NoWorld")
	if not _valid_source(current): return _error("InvalidResidentSource")
	if current==_source: return {"ok":true}
	if current.execution_epoch<_source.execution_epoch: return _error("StaleWorld")
	if current.execution_epoch==_source.execution_epoch:
		if current.world_id!=_source.world_id: return _error("StaleWorld")
		if current.revision<_source.revision or current.tick<_source.tick: return _error("StalePreview")
	var changed_world: bool=current.world_id!=_source.world_id or current.execution_epoch!=_source.execution_epoch
	invalidate(); _page_cache.clear(); _source=current.duplicate(true)
	if changed_world: _published={}
	return {"ok":true}

## 一次读取闭合玩家首页包；只有缺少的后续页再跨worker，Main全程持有同源refresh hold。
func refresh(scene_epoch: int) -> Dictionary:
	if not _installed: return _error("NoWorld")
	if is_busy(): return _error("ResidentBackendBusy")
	_generation+=1; _scene_epoch=scene_epoch; _refreshing=true; _last_error={}; _seed={}
	_stage={"source":_source.duplicate(true),"scene_epoch":scene_epoch,"views":{}}
	var accepted: Dictionary=_issue("query",{"query_id":"player_frame_seed","args":{}},_scene_epoch,{"query_id":"player_frame_seed"})
	if not accepted.get("ok",false): _fail(accepted)
	return accepted

## 原命令只深复制并交真实core；不先preview、不修改序号、不替core抢先拒绝旧revision重放。
func submit_async(raw_command: Dictionary, scene_epoch: int) -> Dictionary:
	var kind: String=raw_command.get("kind","") if raw_command.get("kind","") is String else ""
	return _enqueue("command",raw_command,scene_epoch,{"command_kind":kind})

## 纯预览始终是独立signed job，不借同步submit制造报价；原参数随回执深复制返回。
func preview_async(kind: String, params: Dictionary, actor_id: String, scene_epoch: int) -> Dictionary:
	return _enqueue("preview",{"actor_id":actor_id,"kind":kind,"params":params.duplicate(true)},scene_epoch,{"command_kind":kind,"params":params.duplicate(true),"actor_id":actor_id})

## 焦点/阅读/选择失效时停止读链；已接收ticket仍要消费，已接收command不能被静默丢掉。
func invalidate() -> void:
	_generation+=1; _refreshing=false; _stage={}; _seed={}; _preview_cache={}; _character_ids=[]; _quote_requests=[]; _pending_reads=[]; _pending_read_keys={}
	if is_instance_valid(_presenter): _presenter.invalidate()

## 只有本叶或presenter的现有工作算忙，不建立普通请求队列或忙时隐式重试。
func is_busy() -> bool:
	return _refreshing or not _job.is_empty() or is_instance_valid(_presenter) and _presenter.is_busy()

## 每帧消费至多一条有限结果；本叶和presenter绝不同时拥有普通槽。
func poll() -> void:
	if not _job.is_empty():
		var job: Dictionary=_job
		var reply: Dictionary=_bridge.poll_live_operation(job.reference.duplicate(true))
		if reply.get("ok",false) and not reply.get("ready",false): return
		_job={}
		if not reply.get("ok",false):
			if job.kind=="command" or job.generation==_generation: _fail(reply)
			return
		if reply.get("tag")!=job.kind or reply.get("source_identity")!=job.source or not _valid_source(reply.get("current_identity")) or not reply.get("result") is Dictionary:
			_fail(_error("InvalidResidentReply")); return
		if job.kind=="command":
			adopt_source(reply.current_identity)
			command_completed.emit(job.command_kind,reply.result.duplicate(true),job.source.duplicate(true),reply.current_identity.duplicate(true))
			return
		if job.generation!=_generation: return
		if reply.current_identity!=job.source or job.source!=_source:
			_fail(_error("StalePreview")); return
		if job.get("page_read",false):
			if job.scene_epoch!=_scene_epoch: return
			# 优先停止可能先于旧同版本读回调；旧许可结果只能消费，不得重新入缓存。
			if job.get("query_id","")=="continuation_state" and job.get("continuation_generation",-1)!=_continuation_read_generation:
				_pending_read_keys.erase(job.read_key)
				continuation_state()
				return
			_page_cache[job.read_key]={"source":job.source.duplicate(true),"result":reply.result.duplicate(true)}
			_pending_read_keys.erase(job.read_key)
			page_reads_ready.emit(job.source.duplicate(true),job.scene_epoch)
			return
		if job.kind=="preview":
			if typeof(reply.result.get("ok"))!=TYPE_BOOL: _fail(_error("InvalidResidentPreview")); return
			_preview_cache={"kind":job.command_kind,"actor_id":job.actor_id,"params":job.params.duplicate(true),"params_json":JSON.stringify(job.params,"",false,true),"result":reply.result.duplicate(true),"source":job.source.duplicate(true),"scene_epoch":job.scene_epoch}
			preview_ready.emit(job.command_kind,job.params.duplicate(true),reply.result.duplicate(true),job.source.duplicate(true),job.scene_epoch)
			return
		if not _refreshing: return
		if reply.result.has("ok") and reply.result.ok!=true: _fail(reply.result); return
		if job.query_id=="player_frame_seed":
			var seeded: Dictionary=_accept_seed(reply.result,job.source)
			# B的同步failed回调可重入refresh；旧返回只能处理自己仍拥有的批次。
			if not seeded.get("ok",false) and _refreshing and job.generation==_generation and job.source==_source: _fail(seeded)
			return
		if job.query_id!="interaction_view": _fail(_error("UnsupportedLiveQuery")); return
		var checked: Dictionary=_accept_base(job.query_id,reply.result)
		if not checked.ok: _fail(checked); return
		if checked.get("continued",false): return
		var resumed: Dictionary=_start_presenter()
		if not resumed.get("ok",false) and _refreshing and job.generation==_generation and job.source==_source: _fail(resumed)
	elif is_instance_valid(_presenter) and _presenter.is_busy():
		_presenter.poll()

## 接纳后才保存小票据/预览参数，拒绝不覆盖既有工作或旧完整frame。
func _enqueue(kind: String, payload: Dictionary, scene_epoch: int, context: Dictionary={}) -> Dictionary:
	if not _installed: return _error("NoWorld")
	if is_busy(): return _error("ResidentBackendBusy")
	var accepted: Dictionary=_issue(kind,payload,scene_epoch,context)
	if accepted.get("ok",false): _scene_epoch=scene_epoch
	return accepted

## 所有包都冻结发起时来源；Bridge返回的JobRef必须逐项绑定同一world/epoch。
func _issue(kind: String, payload: Dictionary, scene_epoch: int, context: Dictionary) -> Dictionary:
	var source: Dictionary=_source.duplicate(true)
	var accepted: Dictionary=_bridge.enqueue_live_operation({"kind":kind,"source":source,"payload":payload.duplicate(true)})
	if not accepted.get("ok",false): return accepted.duplicate(true)
	var reference: Variant=accepted.get("reference")
	if not reference is Dictionary or reference.get("world_id")!=source.world_id or reference.get("execution_epoch")!=source.execution_epoch or typeof(reference.get("operation_id"))!=TYPE_INT or reference.operation_id<1: return _error("InvalidResidentReference")
	_job={"kind":kind,"source":source,"reference":reference.duplicate(true),"generation":_generation,"scene_epoch":scene_epoch}
	_job.merge(context.duplicate(true),false)
	return accepted.duplicate(true)

## 闭合首页只有既定五个根字段；原生2MiB上限在transport执行，不序列化整包另做热路径副本。
func _accept_seed(value: Dictionary, verified_source: Dictionary) -> Dictionary:
	if value.size()!=5 or value.get("ok")!=true or not value.get("views") is Dictionary or not value.get("journey") is Dictionary or not value.get("characters") is Dictionary or not value.get("quotes") is Array: return _error("InvalidResidentFrameSeed")
	if value.views.size()!=BASE_QUERIES.size(): return _error("InvalidResidentFrameSeed")
	for query in BASE_QUERIES:
		if not value.views.get(query) is Dictionary: return _error("InvalidResidentFrameSeed")
	_seed={"source":verified_source.duplicate(true),"journey":value.journey.duplicate(true),"characters":value.characters.duplicate(true),"quotes":value.quotes.duplicate(true)}
	for query in BASE_QUERIES:
		var child: Dictionary=value.views[query]
		if child.get("ok")==false: return child.duplicate(true)
		var checked: Dictionary=_accept_base(query,child)
		if not checked.get("ok",false): return checked
		# interaction在固定顺序末尾；只有它的后续页可能仍占唯一普通槽。
		if checked.get("continued",false): return checked
	return _start_presenter()

## 交给B的首页包装仍带已核外层source；后续页不能借当前身份重绑旧seed。
func _start_presenter() -> Dictionary:
	if _seed.get("source")!=_source: return _error("StalePreview")
	var actor: String=_stage.views.command_context.actor_id
	var bound: Dictionary=_presenter.bind_context(_bridge,_source,actor,_scene_epoch)
	if not bound.get("ok",false): return bound
	return _presenter.refresh_from_seed(_source,_scene_epoch,{"source":_seed.source.duplicate(true),"pages":_seed.journey})

## 基础缓存只收已签收的有限query结果；分页选项连续收齐，绝不静默截断。
func _accept_base(query: String, value: Dictionary) -> Dictionary:
	match query:
		"command_context":
			if value.get("ok")!=true or not value.get("actor_id") is String or typeof(value.get("revision"))!=TYPE_INT or value.revision!=_source.revision or typeof(value.get("next_request_seq"))!=TYPE_INT: return _error("InvalidCommandContext")
		"action_context":
			if value.get("ok")!=true or value.get("actor_id")!=_stage.views.command_context.actor_id or value.get("revision")!=_source.revision or not value.get("action_id") is String or not value.get("action_kind") is String or not value.get("travel_paused") is bool: return _error("InvalidActionContext")
		"hud_view":
			if value.get("ok")!=true or value.get("actor_id")!=_stage.views.command_context.actor_id or value.get("revision")!=_source.revision or not value.get("display_name") is String: return _error("InvalidHudViewSource")
			for key in ["day_index","half_day","balance","hp"]:
				if typeof(value.get(key))!=TYPE_INT: return _error("InvalidHudViewSource")
		"journey_input_need":
			if value.get("ok")!=true or not value.get("needs_input") is bool: return _error("InvalidJourneyInputNeed")
		"interaction_view":
			if not value.get("phase") is String: return _error("InvalidInteractionView")
			if value.has("page"): return _accept_interaction_page(value)
		"screen_context":
			var created: bool=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("character_creation",{}).get("profile")=="point_buy_character_v1"
			if value.get("ok")!=true or value.size()!=7+int(growth_cards_enabled())+int(actor_cards_enabled())+int(insight_cards_enabled())+int(created)+int(naval_combat_enabled()) or not value.get("world_scene_legacy") is bool: return _error("InvalidScreenContext")
			if naval_combat_enabled():
				var naval: Variant=value.get("naval_battle_page")
				if not naval is Dictionary or not naval.get("active") is bool: return _error("InvalidNavalBattlePage")
				if naval.active and (naval.get("source")!=_source or naval.get("actor_id")!=_stage.views.command_context.actor_id or not naval.get("battle_id") is String or not naval.get("units") is Dictionary or naval.units.size()>128): return _error("InvalidNavalBattlePage")
			if created:
				var origin: Variant=value.get("character_creation")
				if not origin is Dictionary or origin.size()!=1 or not origin.get("mode") in ["standard","free"]: return _error("InvalidCharacterCreationSource")
			if growth_cards_enabled():
				var cards: Variant=value.get("growth_cards_summary")
				if not cards is Dictionary or cards.size()!=3 or cards.get("enabled")!=true or cards.get("actor_id")!=_stage.views.command_context.actor_id or typeof(cards.get("acquired_count"))!=TYPE_INT or cards.acquired_count<0 or cards.acquired_count>4096: return _error("InvalidCardsSummary")
			if insight_cards_enabled():
				var insights: Variant=value.get("insight_cards_summary")
				if not insights is Dictionary or insights.size()!=3 or insights.get("enabled")!=true or insights.get("actor_id")!=_stage.views.command_context.actor_id or typeof(insights.get("acquired_count"))!=TYPE_INT or insights.acquired_count<0 or insights.acquired_count>4096: return _error("InvalidInsightCardsSummary")
			if actor_cards_enabled():
				var actor_cards: Variant=value.get("actor_cards_summary")
				var actor_card_v2: bool=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("actor_card_unlocks",{}).get("profile")=="completed_task_actor_cards_v2"
				if actor_card_v2:
					if not actor_cards is Dictionary or actor_cards.size()!=5 or actor_cards.get("enabled")!=true or actor_cards.get("actor_id")!=_stage.views.command_context.actor_id or typeof(actor_cards.get("source_version"))!=TYPE_INT or actor_cards.get("source_version")!=2 or typeof(actor_cards.get("total"))!=TYPE_INT or actor_cards.total<1 or actor_cards.total>4096 or typeof(actor_cards.get("acquired_count"))!=TYPE_INT or actor_cards.acquired_count<0 or actor_cards.acquired_count>actor_cards.total: return _error("InvalidActorCardsSummary")
				elif not actor_cards is Dictionary or actor_cards.size()!=4 or actor_cards.get("enabled")!=true or actor_cards.get("actor_id")!=_stage.views.command_context.actor_id or typeof(actor_cards.get("completed_count"))!=TYPE_INT or actor_cards.completed_count<0 or actor_cards.completed_count>2 or typeof(actor_cards.get("acquired_count"))!=TYPE_INT or actor_cards.acquired_count<0 or actor_cards.acquired_count>4096 or (actor_cards.completed_count<2 and actor_cards.acquired_count!=0): return _error("InvalidActorCardsSummary")
			for key in ["personal_battle_view","military_battle_page","siege_page","study_activity","presentation_location"]:
				if not value.get(key) is Dictionary: return _error("InvalidScreenContext")
		_: return _unsupported(query)
	_stage.views[query]=value.duplicate(true)
	return {"ok":true}

## 只收当前对白的至多4096可见选项；超出有限前端预算明确失败，header/来源不得跨页变化。
func _accept_interaction_page(value: Dictionary) -> Dictionary:
	var page: Variant=value.get("page")
	if not page is Dictionary or typeof(page.get("cursor"))!=TYPE_INT or typeof(page.get("total_rows"))!=TYPE_INT or not value.get("options") is Array: return _error("InvalidInteractionPage")
	if page.total_rows<0 or page.total_rows>4096: return _error("ResidentFrameLimit")
	var rows: Array=_stage.get("_interaction_rows",[])
	if page.cursor!=rows.size() or value.options.size()!=mini(64,page.total_rows-page.cursor) or page.cursor>page.total_rows: return _error("InvalidInteractionPage")
	var header: Dictionary=value.duplicate(true); header.erase("page"); header.erase("options")
	if rows.is_empty() and not _stage.has("_interaction_first"):
		_stage._interaction_first=value.duplicate(true); _stage._interaction_header=header; _stage._interaction_total=page.total_rows
	elif header!=_stage._interaction_header or page.total_rows!=_stage._interaction_total: return _error("InvalidInteractionPage")
	for row in value.options:
		if not row is Dictionary or row.size()!=2 or not row.get("id") is String or not row.get("text") is String: return _error("InvalidInteractionPage")
		rows.append(row.duplicate(true))
	_stage._interaction_rows=rows
	if rows.size()<page.total_rows:
		if typeof(page.get("next_cursor"))!=TYPE_INT or page.next_cursor!=rows.size(): return _error("InvalidInteractionPage")
		var accepted: Dictionary=_issue("query",{"query_id":"interaction_view","args":{"cursor":page.next_cursor}},_scene_epoch,{"query_id":"interaction_view"})
		return {"ok":true,"continued":true} if accepted.get("ok",false) else accepted
	if page.has("next_cursor"): return _error("InvalidInteractionPage")
	var complete: Dictionary=_stage._interaction_first.duplicate(true); complete.erase("page"); complete.options=rows
	_stage.views.interaction_view=complete
	for key in ["_interaction_first","_interaction_header","_interaction_total","_interaction_rows"]: _stage.erase(key)
	return {"ok":true}

## presenter完整同源后才一次发布所有getter；旧scene/revision绝不混入新frame。
func _presenter_ready(source: Dictionary, scene_epoch: int, map: Dictionary, sidebar: Dictionary) -> void:
	if not _refreshing or source!=_source or source!=_stage.get("source") or scene_epoch!=_scene_epoch or scene_epoch!=_stage.get("scene_epoch"): return
	_stage.map_view=map.duplicate(true); _stage.journey_sidebar=sidebar.duplicate(true); _stage.characters={}
	_character_ids=[]; _select_character(_stage.views.command_context.actor_id)
	if not sidebar.get("encounter",{}).is_empty(): _select_character(sidebar.encounter.get("provider_actor_id",""))
	var interaction: Dictionary=_stage.views.interaction_view
	if interaction.get("phase")!="scene": _select_character(interaction.get("speaker",interaction.get("context",{}).get("provider_actor_id","")))
	if _seed.characters.keys()!=_character_ids: _fail(_error("InvalidCharacterView")); return
	for actor in _character_ids:
		if not _seed.characters[actor] is Dictionary or not _valid_character(_seed.characters[actor],actor): _fail(_error("InvalidCharacterView")); return
		_stage.characters[actor]=_seed.characters[actor].duplicate(true)
	_start_quotes()

## 仅收实际portrait消费者self/peer/speaker，名单只显示名字；core仍逐个执行可见性筛选。
func _select_character(actor_id: String) -> void:
	if not actor_id.is_empty() and not actor_id in _character_ids: _character_ids.append(actor_id)

## 固定控制仍按原列表核验一次同source纯预览；普通拒绝照原quote保存，不推断按钮权限。
func _start_quotes() -> void:
	_quote_requests=[]; _stage.quotes=[]
	for kind in ["advance","pause_travel","resume_travel","travel_wait","return","rescue","wait"]: _quote_requests.append({"kind":kind,"params":{}})
	var encounter: Dictionary=_stage.journey_sidebar.get("encounter",{})
	if not encounter.is_empty():
		for kind in ["encounter_talk","encounter_fight"]: _quote_requests.append({"kind":kind,"params":{"encounter_id":encounter.encounter_id}})
	var phase: String=_stage.views.interaction_view.phase
	if phase=="action": _quote_requests.append({"kind":"dialogue_confirm","params":{}})
	if phase in ["say","choice"] and "npc_society" in _catalog.get("data",{}).get("required_capabilities",[]): _quote_requests.append({"kind":"ask_news","params":{}})
	if phase!="scene" and phase!="choice": _quote_requests.append({"kind":"end_interaction","params":{}})
	if _seed.quotes.size()!=_quote_requests.size(): _fail(_error("InvalidResidentPreview")); return
	var actor: String=_stage.views.command_context.actor_id
	for index in range(_quote_requests.size()):
		var request: Dictionary=_quote_requests[index]; var quote: Variant=_seed.quotes[index]
		if not quote is Dictionary or quote.size()!=4 or quote.get("kind")!=request.kind or quote.get("actor_id")!=actor or not quote.get("params") is Dictionary or not quote.get("result") is Dictionary or typeof(quote.result.get("ok"))!=TYPE_BOOL:
			_fail(_error("InvalidResidentPreview")); return
		if JSON.stringify(quote.params,"",false,true)!=JSON.stringify(request.params,"",false,true): _fail(_error("InvalidResidentPreview")); return
		_stage.quotes.append(_quote_record({"command_kind":quote.kind,"actor_id":actor,"params":quote.params,"source":_seed.source,"scene_epoch":_scene_epoch},quote.result))
	_publish_frame()

## 首页及全部后续页核验完成才原子发布；过程中没有grant/submit或隐式GD规则执行。
func _publish_frame() -> void:
	_published=_stage; _actor_id=_published.views.command_context.actor_id; _stage={}; _seed={}; _refreshing=false; _character_ids=[]; _quote_requests=[]
	frame_ready.emit(_source.duplicate(true),_scene_epoch)

## 预览身份保留原参数键序及数值标签；只持有有限当前输入，不成为规则权威。
func _quote_record(job: Dictionary, result: Dictionary) -> Dictionary:
	return {"kind":job.command_kind,"actor_id":job.actor_id,"params_json":JSON.stringify(job.params,"",false,true),"result":result.duplicate(true),"source":job.source.duplicate(true),"scene_epoch":job.scene_epoch}

## 接收A的固定六+五白名单，私密seed/parents等附加字段无法进入前端公开缓存。
func _valid_character(value: Dictionary, actor: String) -> bool:
	if value.size()!=3 or value.get("ok")!=true or not value.get("actor_view") is Dictionary or not value.get("appearance_view") is Dictionary: return false
	var person: Dictionary=value.actor_view; var appearance: Dictionary=value.appearance_view
	if person.size()!=6 or person.get("actor_id")!=actor or typeof(person.get("age_years"))!=TYPE_INT or typeof(person.get("visibility_version"))!=TYPE_INT or person.visibility_version!=_source.revision: return false
	for key in ["display_name","presentation_profile_id","portrait_kind"]:
		if not person.get(key) is String: return false
	if appearance.size()!=5 or typeof(appearance.get("format_version"))!=TYPE_INT or not appearance.get("generator_version") is String or not appearance.get("attire_style") is String or not appearance.get("legacy") is bool or not appearance.get("base_features") is Dictionary: return false
	var features: Dictionary=appearance.base_features
	if appearance.legacy: return features.is_empty()
	if features.size()!=9: return false
	for key in ["face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"]:
		if not features.get(key) is String: return false
	return true

## 只接纳当前批次的失败；过期的已排队读仍可消费，但不能改新页面说明。
func _presenter_failed(error: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if _refreshing and source==_source and scene_epoch==_scene_epoch: _fail(error)

## 失败保留旧完整cache及其旧身份，不发布部分新值，不自动重发任何命令或许可。
func _fail(error: Dictionary) -> void:
	_last_error=error.duplicate(true); _refreshing=false; _stage={}; _seed={}; _character_ids=[]; _quote_requests=[]; _pending_reads=[]; _pending_read_keys={}
	if is_instance_valid(_presenter): _presenter.invalidate()
	failed.emit(error.duplicate(true))

## 同步getter从不排队、推进或全态导出；只复用完整同source的frame，scene只约束异步发布。
func _cached(key: String, base: bool=true) -> Dictionary:
	if not _installed: return _error("NoWorld")
	if _published.is_empty(): return _last_error.duplicate(true) if not _last_error.is_empty() else _error("ResidentFrameUnavailable")
	if _published.source!=_source: return _error("StalePreview")
	var values: Dictionary=_published.views if base else _published
	return values[key].duplicate(true) if values.get(key) is Dictionary else _unsupported(key)

## 当前控制人之外不从前端缓存猜序号；必要时必须另经受信原生查询。
func command_context(actor_id: String="") -> Dictionary:
	var value: Dictionary=_cached("command_context")
	if not actor_id.is_empty() and actor_id!=value.get("actor_id"): return _unsupported("command_context.actor")
	return value

## 基础行动值只读已发布同源回执，不推断占用或自动推进资格。
func action_context() -> Dictionary: return _cached("action_context")
## 地图只取presenter已经收齐的同源页。
func map_view() -> Dictionary: return _cached("map_view",false)
## 侧栏只取同批真实会面、同行和行程结果。
func journey_sidebar() -> Dictionary: return _cached("journey_sidebar",false)
## 非个人战HUD复用原有限回执；额外战场参数尚无对应cache时明确拒绝。
func hud_view(personal_battle_id: String="") -> Dictionary:
	return _cached("hud_view") if personal_battle_id.is_empty() else demand_view("hud_view",{"personal_battle_id":personal_battle_id})
## 只提供本次权威玩家会话，不借同步getter读取另一人物。
func interaction_view(actor_id: String="") -> Dictionary:
	return _cached("interaction_view") if actor_id.is_empty() or actor_id==_actor_id else _unsupported("interaction_view.actor")

## screen子视图只透传core实际返回的对象，缺失不伪造inactive空页。
func _screen(key: String) -> Dictionary:
	var screen: Dictionary=_cached("screen_context")
	if screen.get("ok")!=true: return screen
	return screen[key].duplicate(true) if screen.get(key) is Dictionary else _unsupported(key)

## 位置来自core原raw-location投影，不猜Following或InArmy的场所。
func presentation_location() -> Dictionary: return _screen("presentation_location")
## 个人战分支只有core已核的原getter结果，不能默认无战斗。
func personal_battle_view() -> Dictionary: return _screen("personal_battle_view")
## 军事战场分支只有core已核的原getter结果，不能默认inactive。
func military_battle_page(battle_id: String="") -> Dictionary:
	return _screen("military_battle_page") if battle_id.is_empty() else demand_view("military_battle_page",{"battle_id":battle_id})
## 舟战能力只来自已选择情景；旧资产段1始终没有战斗页。
func naval_combat_enabled() -> bool:
	return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("military_naval",{}).get("combat_policy",{}).get("kind","")=="boarding_ram_v1"
## 当前参战与草案消费同源有限读页，读取不提交军令或推进。
func naval_battle_page(battle_id: String="") -> Dictionary:
	if not naval_combat_enabled(): return {"active":false}
	return _screen("naval_battle_page") if battle_id.is_empty() else demand_view("naval_battle_page",{"battle_id":battle_id})
func naval_battle_draft(battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary:
	return demand_view("naval_battle_draft",{"battle_id":battle_id,"selected_id":selected_id,"orders":orders.duplicate(true)})
## 围城分支只有core已核的原getter结果，不能默认为空。
func siege_page() -> Dictionary: return _screen("siege_page")
## 课程分支只有core已核的原getter结果，不能默认无活动。
func study_activity() -> Dictionary: return _screen("study_activity")

## 缺必要读值时保守阻止自动推进；失败仍能由实际getter/failed信号明确诊断。
func action_needs_input() -> bool:
	var value: Dictionary=_cached("journey_input_need")
	return value.needs_input if value.get("ok")==true and value.get("needs_input") is bool else true

## 未接入的同步规则/全态入口明确失败，不调用旧Kernel或伪装成只读snapshot。
func submit(_command: Dictionary) -> Dictionary: return _error("NativeAsyncRequired","submit")
## 动态quote必须匹配最新请求scene；固定控制只复用同源完整frame内原scene的quote。
func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
	var actor: String=actor_id if not actor_id.is_empty() else _actor_id
	var signature: String=JSON.stringify(params,"",false,true)
	if not _preview_cache.is_empty() and _preview_cache.source==_source and _preview_cache.scene_epoch==_scene_epoch and _preview_cache.kind==kind and _preview_cache.actor_id==actor and _preview_cache.params_json==signature: return _preview_cache.result.duplicate(true)
	if not _published.is_empty() and _published.source==_source:
		for quote in _published.get("quotes",[]):
			if quote.source==_source and quote.scene_epoch==_published.scene_epoch and quote.kind==kind and quote.actor_id==actor and quote.params_json==signature: return quote.result.duplicate(true)
	var pending: Dictionary=_demand_read("preview",{"actor_id":actor,"kind":kind,"params":params.duplicate(true)})
	if pending.get("code")=="NativeAsyncRequired": pending.revision=_source.revision
	return pending
## 原snapshot不能转成debug export，也不返回缩略世界冒充完整状态。
func snapshot() -> Dictionary: return _unsupported("snapshot")
## 冷保存必须由同worker checkpoint路径完成，不提供GD世界副本。
func save_context() -> Dictionary: return _unsupported("save_context")
## 旧Dictionary恢复不能绕开同worker的typed checkpoint验证与epoch切换。
func restore(_state: Dictionary) -> Dictionary: return _unsupported("restore")
## 新局Factory只允许由中央冷装配调用，install后本叶不另造世界。
func new_game(_scenario: String="", _path: String="", _appearance: Dictionary={}, _player: String="") -> Dictionary: return _unsupported("new_game")
## 仅在core已核无模板且请求匹配当前world位置时供SceneHost走原legacy分支。
func scene_view(place_id: String, view_kind: String) -> Dictionary:
	var screen: Dictionary=_cached("screen_context")
	if screen.get("ok")!=true: return screen
	if view_kind!="world": return demand_view("scene_view",{"place_id":place_id,"view_kind":view_kind})
	if screen.get("world_scene_legacy")!=true: return demand_view("scene_view",{"place_id":place_id,"view_kind":view_kind})
	var location: Dictionary=screen.presentation_location
	if location.get("kind") not in ["Settlement","Wilderness","OnRoute","Following","InArmy"] or place_id!=location.get("place_id",""): return _unsupported("scene_view.location")
	# 当前城内原通路由同源核心给出；原野与在途没有场所时保留旧地图回退。
	if location.kind=="Settlement" and not place_id.is_empty(): return demand_view("scene_view",{"place_id":place_id,"view_kind":view_kind})
	return {"ok":true,"visibility":"current"}
## 同步人物只返回C已授权且本帧已完成的六+五字段，不在getter补查任意ID。
func character_input(actor_id: String) -> Dictionary:
	var characters: Dictionary=_cached("characters",false)
	if characters.get(actor_id) is Dictionary: return characters[actor_id].duplicate(true)
	return characters if characters.get("ok")==false else demand_view("character_input",{"actor_id":actor_id})

## 小来源合同不接受缺省world或浮点时钟，且不保存额外身份/世界字段。
func _valid_source(value: Variant) -> bool:
	if not value is Dictionary or value.size()!=4 or not value.get("world_id") is String or value.world_id.length()!=32: return false
	for index in range(32):
		var digit: int=value.world_id.unicode_at(index)
		if not (digit>=48 and digit<=57 or digit>=97 and digit<=102): return false
	for key in ["execution_epoch","revision","tick"]:
		if typeof(value.get(key))!=TYPE_INT: return false
	return value.execution_epoch>0 and value.revision>=0 and value.tick>=0

## 未实现能力与本地协议失败是诊断结果，不是空的成功玩法对象。
func _unsupported(capability: String) -> Dictionary: return _error("UnsupportedNativeCapability",capability)
## 诊断仅包含错误和具体入口，不携带被拒绝的世界或请求正文。
func _error(code: String, capability: String="") -> Dictionary:
	var result: Dictionary={"ok":false,"code":code}
	if not capability.is_empty(): result.capability=capability
	return result

## SceneTree每帧只消费有限缓存，绝不等待worker或授予下一半日。
func _process(_delta: float) -> void: poll()

## 有限页面读只记录需求；Main持有刷新许可后逐条发给唯一worker，不在getter同步执行。
func _demand_read(kind: String, payload: Dictionary) -> Dictionary:
	if not _installed: return _error("NoWorld")
	var key: String=kind+":"+JSON.stringify(payload,"",false,true)
	if _page_cache.get(key) is Dictionary and _page_cache[key].source==_source: return _page_cache[key].result.duplicate(true)
	if not _pending_read_keys.has(key):
		if _pending_read_keys.size()+_page_cache.size()>=MAX_PAGE_READS: return _error("ResidentFrameLimit")
		_pending_reads.append({"kind":kind,"payload":payload.duplicate(true),"key":key})
		_pending_read_keys[key]=true
	return _error("NativeAsyncRequired",payload.get("query_id",payload.get("kind","")))

## 页面选择参数始终完整签名，不以最新参数覆盖旧已发起请求。
func demand_view(query: String, args: Dictionary={}) -> Dictionary:
	return _demand_read("query",{"query_id":query,"args":args.duplicate(true)})

## 仅反映本页已声明的纯读需求；这不是普通命令队列，也不会提交或自动推进。
func has_pending_reads() -> bool: return not _pending_reads.is_empty()

## 调用者先停止推进并持刷新token；一条读完成后才准许Main发布或发下一条。
func begin_pending_read(scene_epoch: int) -> Dictionary:
	if is_busy(): return _error("ResidentBackendBusy")
	if _pending_reads.is_empty(): return {"ok":true,"empty":true}
	var request: Dictionary=_pending_reads.pop_front()
	var context: Dictionary={"page_read":true,"read_key":request.key}
	if request.kind=="query": context.query_id=request.payload.query_id
	if context.get("query_id","")=="continuation_state": context["continuation_generation"]=_continuation_read_generation
	var result: Dictionary=_enqueue(request.kind,request.payload,scene_epoch,context)
	if not result.get("ok",false): _pending_read_keys.erase(request.key)
	return result

## 活动票据从同源人物和不变目录形成，额外epoch防止同revision读档后旧控件重放。
func activity_identity() -> Dictionary:
	var context: Dictionary=command_context()
	return {"revision":context.get("revision",-1),"actor_id":context.get("actor_id",""),"content_digest":_catalog.get("content_digest",""),"pack_set_digest":_catalog.get("pack_set_digest",""),"source_sha256":_catalog_source_sha256,"world_id":_source.get("world_id",""),"execution_epoch":_source.get("execution_epoch",0)}

## 手动窗口只信原核心有限查询；未取得结果时保守停止自动短行动。
func action_manual_window() -> bool:
	var value: Dictionary=demand_view("action_manual_window")
	return value.get("manual",true) if value.get("ok")==true else true

## 所有旧页面getter只读取当前页的同源有限值，缺值记录纯读需求并明确返回pending。
## 本人档案只排入同一有限读取队列，不复制世界或发命令。
func self_profile() -> Dictionary: return demand_view("self_profile")

## 只有当前显式情景启用收藏；旧目录不因被动卡片定义而开放新页面。
func growth_cards_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("growth_cards",{}).get("profile","")=="learned_ability_cards_v1"
## 小摘要来自同一基础frame，只有取得数改变才要求分页图鉴同步。
func growth_cards_summary() -> Dictionary: return _screen("growth_cards_summary") if growth_cards_enabled() else {"enabled":false}
## 本人卡片页面仍走唯一异步有限读队列，不同步调用Bridge。
func growth_cards(args: Dictionary={}) -> Dictionary: return demand_view("growth_cards",args)

## 人物卡资格独立于本局武功；来源只来自原生本人已结算委托投影。
func actor_cards_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("actor_card_unlocks",{}).get("profile","") in ["completed_procurement_actor_cards_v1","completed_task_actor_cards_v2"]
func actor_cards_creation_mode() -> String:
	if _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).has("character_creation"): return str(_screen("character_creation").get("mode",""))
	return preload("res://ui/actor_unlock_start.gd").creation_mode(_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}))
func actor_cards_summary() -> Dictionary: return _screen("actor_cards_summary") if actor_cards_enabled() else {"enabled":false}
func actor_cards(args: Dictionary={}) -> Dictionary: return demand_view("actor_cards",args)

## 本人技艺页只读所选skill_id，不预览或补发课程经验。
func ordinary_skills(skill_id: String="") -> Dictionary: return demand_view("ordinary_skills",{"skill_id":skill_id})
## 成长页按原skills/inventory分支查询，不读取未选中页的全人物。
func character_summary(page: String="skills") -> Dictionary: return demand_view("character_summary",{"page":page})
## 家事开窗上下文来自本地首岗位，不猜测提供人的可用性。
func family_provider_context() -> Dictionary: return demand_view("family_provider_context")
## 家庭页只请求当前页签与原冻结办理上下文。
func family_page(tab: String="family", provider_context: Dictionary={}) -> Dictionary: return demand_view("family_page",{"tab":tab,"provider_context":provider_context})
## 庄园在场许可只采纳原生布尔结果，pending不冒充可用。
func estate_available(estate_id: String) -> bool: return demand_view("estate_available",{"estate_id":estate_id}).get("available",false)
## 庄务页绑定选中庄园与页签，不枚举远方资产。
func estate_page(estate_id: String, tab: String="people") -> Dictionary: return demand_view("estate_page",{"estate_id":estate_id,"tab":tab})
## 个人战草案实时对比原核心会话身份，不从本地元数据推断。
func personal_battle_context(battle_id: String) -> Dictionary: return demand_view("personal_battle_context",{"battle_id":battle_id})
## 个人战几何草案只调用原纯查询，绝不执行AI或回合。
func personal_battle_geometry(battle_id: String, draft: Dictionary) -> Dictionary: return demand_view("personal_battle_geometry",{"battle_id":battle_id,"draft":draft})
## 营务开窗提供人由实际岗位选择。
func military_camp_provider_context() -> Dictionary: return demand_view("military_camp_provider_context")
## 营务输入保存选中部队和原岗位上下文。
func military_camp_page(tab: String="orders", selection: Dictionary={}, provider_context: Dictionary={}) -> Dictionary: return demand_view("military_camp_page",{"tab":tab,"selection":selection,"provider_context":provider_context})
## 营务字段变更只请求原核心纯报价所需输入。
func military_camp_offer(kind: String, params: Dictionary) -> Dictionary: return demand_view("military_camp_offer",{"kind":kind,"params":params})
## 军阵草案绑定本人战场和有序命令，不执行战斗。
func military_battle_draft(battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary: return demand_view("military_battle_draft",{"battle_id":battle_id,"selected_id":selected_id,"orders":orders})
## 围城候选保原身份与请求列表，不发放出击或谈判。
func siege_draft(identity: Dictionary, requests: Array, first_ok: bool=false) -> Dictionary: return demand_view("siege_draft",{"identity":identity,"requests":requests,"first_ok":first_ok})
## 课程列表只读指定本地岗位和本人选中课时。
func study_courses(actor_id: String, context: Dictionary, ticks: int=2) -> Dictionary: return demand_view("study_courses",{"actor_id":actor_id,"context":context,"ticks":ticks})
## 学习练习只读取本人当前action_id的既有题面。
func study_challenge(actor_id: String, action_id: String) -> Dictionary: return demand_view("study_challenge",{"actor_id":actor_id,"action_id":action_id})
## 礼法练习沿原单独入口读取，不生成第二题面。
func study_etiquette(actor_id: String, action_id: String) -> Dictionary: return demand_view("study_etiquette",{"actor_id":actor_id,"action_id":action_id})
## 驻军操练只请求当前行动公开输入，不暴露裁判状态。
func military_drill(actor_id: String, action_id: String) -> Dictionary: return demand_view("military_drill",{"actor_id":actor_id,"action_id":action_id})
## 岗位上下文的wire数组显式拆包，不把缺页伪造成可办理岗位。
func study_course_contexts() -> Array: return demand_view("study_course_contexts").get("rows",[])
## 货单读取真实库存与本人任务公款选项，不计算成交价格。
func trade_view(shop_id: String, provider_actor_id: String, receipt_action_id: String="") -> Dictionary: return demand_view("trade_view",{"shop_id":shop_id,"provider_actor_id":provider_actor_id,"receipt_action_id":receipt_action_id})
## 货单缓存寿命只采纳当前原生对白阶段。
func trade_cache_context() -> Dictionary: return demand_view("trade_cache_context")
## 成功交易显示只读该行动实际收据，不补造历史。
func trade_action_receipt(action_id: String) -> Dictionary: return demand_view("trade_action_receipt",{"action_id":action_id})
## 议价来源按原basis身份查询，旧报价不能借新人物重绑。
func trade_bargain_source(actor_id: String, basis_sha256: String) -> Dictionary: return demand_view("trade_bargain_source",{"actor_id":actor_id,"basis_sha256":basis_sha256})
## 议价题面仅查询既有quote_id，不创建报价。
func trade_bargain(actor_id: String, quote_id: String) -> Dictionary: return demand_view("trade_bargain",{"actor_id":actor_id,"quote_id":quote_id})
## 场所侧栏通过原生可见角色与事件投影，不枚举完整世界。
func place_sidebar() -> Dictionary: return demand_view("place_sidebar")

## 地方近况只取当前位置的有限页，不在前端扫描NPC计划或远方资源。
func regional_life_topics(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("regional_life_topics",{"place_id":place_id,"cursor":cursor})

## 只启用当前已安装剧本明确声明的地方生活，组合包的能力并集不授予其他剧本入口。
func regional_life_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("regional_life")

## 任职只启用当前剧本的显式能力；查询只投影眼前的人与本人已知邀请。
func named_membership_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("organization",{}).get("named_membership_enabled",false)

## 同一worker返回来源与动作，不从前端反推NPC、工资或组织权限。
func named_membership_entries(place_id: String, provider_actor_id: String="") -> Dictionary:
	return demand_view("named_membership_entries",{"place_id":place_id,"provider_actor_id":provider_actor_id})

## 见闻只消费本情景显式目录与本人完成事实；创建来源仅认当前已实现的正式人物政策。
func insight_cards_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("insight_cards",{}).get("profile","")=="completed_camp_training_insights_v1"
func insight_cards_creation_mode() -> String:
	if insight_cards_enabled() and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).has("character_creation"): return str(_screen("character_creation").get("mode",""))
	var policy: String=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("founder_policy","")
	return "standard" if insight_cards_enabled() and policy in ["fixed_actor","fixed_actor_appearance_v1","fixed_actor_appearance_composition_v1","selected_existing_actor_v1","selected_existing_actor_roster_v1"] else ""
func insight_cards_summary() -> Dictionary: return _screen("insight_cards_summary") if insight_cards_enabled() else {"enabled":false}
func insight_cards(args: Dictionary={}) -> Dictionary: return demand_view("insight_cards",args)

## 普通授课只读本人当前明确请求与有限既有课程。
func study_teaching_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("study_teaching_requests",{}).get("profile","")=="authored_study_teaching_requests_v1"
func study_teaching(args: Dictionary={}) -> Dictionary: return demand_view("study_teaching",args)

func adult_adoption_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("adult_adoption",{}).get("profile")=="authored_adult_adoption_requests_v1"
func adult_adoption_requests(page: int=0) -> Dictionary: return demand_view("adult_adoption_requests",{"page":page})

## 只由所选正式作者profile启用接续界面，普通收养不自动授权。
func voluntary_succession_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("voluntary_succession",{}).get("profile") in ["authored_voluntary_succession_v1","living_accepted_family_paths_v2"]
## 有限页沿原只读队列获取，按钮提交仍走共同命令入口。
func voluntary_succession_page(page: int=0) -> Dictionary: return demand_view("voluntary_succession_page",{"page":page})
# 生前路径仅由完整显式目录启用；报价和命令沿原请求入口。
func family_voluntary_paths_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).has("family_voluntary_paths")
func family_voluntary_paths_view(page: int=0) -> Dictionary: return demand_view("family_voluntary_paths",{"page":page})

## 生命终点来自正式有限查询；存档与标题仍使用原存储服务。
func family_life_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_life",{}).get("profile")=="verified_military_deaths_v1"
func family_death_page() -> Dictionary: return demand_view("family_death_page",{})

## 独立条件许可仅由正式有限页面与具名命令办理。
func family_legacy_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_legacy",{}).get("profile") in ["authored_posthumous_control_consent_v1","posthumous_control_sources_v2","posthumous_control_sources_v3"]
func family_collateral_legacy_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).has("family_collateral_legacy")
func family_collateral_page(page: int=0) -> Dictionary: return demand_view("family_collateral_page",{"page":page})
func family_biological_legacy_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).has("family_biological_legacy")
func family_biological_page(page: int=0) -> Dictionary: return demand_view("family_biological_page",{"page":page})

## 页面不包含全世界死亡账或任何遗产处分捷径。
func family_legacy_page(page: int=0) -> Dictionary: return demand_view("family_legacy_page",{"page":page})

## 显式遗赠声明能力才开放入口，不从旧家庭能力推断处分权。
func family_bequest_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_bequest",{}).get("profile")=="revocable_personal_bequest_v1"

## 仅请求当前玩家的有限私产与声明页，不传递完整世界。
func family_bequest_page(page: int=0) -> Dictionary: return demand_view("family_bequest_page",{"page":page})

## 独立受领能力才允许办理，原家庭与声明不会自动开放财产交割。
func family_inheritance_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_inheritance",{}).get("profile")=="disclosed_personal_bequest_v1"

## 页面仅请求本人已知条款、精确原信和有限回执，不向表现层传完整世界。
func family_inheritance_page(page: int=0,lot_page: int=0,notice_binding_fact_id: String="") -> Dictionary: return demand_view("family_inheritance_page",{"page":page,"lot_page":lot_page,"notice_binding_fact_id":notice_binding_fact_id})

## 明确婚议策略才启用个人婚姻入口，不让旧家庭profile隐式增加配偶状态。
func estate_employment_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("estate_employment")

func task_employment(cursor: int=0, policy_cursor: int=0) -> Dictionary: return demand_view("task_employment",{"cursor":cursor,"policy_cursor":policy_cursor})

func family_marriage_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_marriage",{}).get("profile")=="personal_marriage_separate_assets_v1"

## 有限本人页只含实际听到的意愿和自身经历，不导出全账或远方死亡真值。
func family_marriage_page(page: int=0) -> Dictionary: return demand_view("family_marriage_page",{"page":page})

## 生育与谱系显式配对，旧家庭不自动得到生育、禀赋或儿童身份。
func family_birth_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	return scenario.get("family_birth",{}).get("profile")=="adult_birth_personal_care_v1" and scenario.get("family_lineage",{}).get("profile")=="biological_origin_innate_v1"

## 只请求原生有限本人经历页；不会扫描全局出生或远方儿童状态。
func family_birth_page(page: int=0) -> Dictionary: return demand_view("family_birth_page",{"page":page})

## 独立有限援助profile；原出生/生命史由原生配对验证。
func family_care_relief_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	return scenario.get("family_care_relief",{}).get("profile")=="living_parent_private_relief_v1"

## 只读取本人有来源的有限条款/实际照护，不扫描他人私粮或远方死亡。
func family_care_relief_page(page: int=0) -> Dictionary: return demand_view("family_care_relief_page",{"page":page})

## 私铺页面只发有限本人查询；真实经营/资格仍由同一原生候选重核。
func private_shops_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("private_shops")
## 只所选作者显式清算能力开放页面；实际执行仍由原Native资格报价决定。
func private_shop_liquidations_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("private_shop_liquidations")
func private_shop_business(tab: String="shops", shop_id: String="", cursor: int=0, fund_amount: int=100, restock_quantity: int=1) -> Dictionary:
	return demand_view("private_shop_business",{"tab":tab,"shop_id":shop_id,"cursor":cursor,"fund_amount":fund_amount,"restock_quantity":restock_quantity})
## 许可查询限实际地点与有界游标，不在GD推断官署权限。
func political_shop_licensing_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_shop_licensing")
func political_shop_registry_successions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_shop_registry_successions")
## 原生完整profile控制有限本地续办查询。
func political_governance_continuations_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_governance_continuations")
func political_governance_continuations(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_governance_continuations",{"place_id":place_id,"cursor":cursor})

## 原双方本地只读邀请回复；命令仍经原生preview。
func governance_invitation_replies_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_governance_invitation_replies")
func governance_invitation_replies(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("governance_invitation_replies",{"place_id":place_id,"cursor":cursor})

## 本人原首职继位的有限本地页，真实提名、接受和就任仍走原生命令。
func political_head_handovers_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_head_handovers")
func political_head_handovers(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_head_handovers",{"place_id":place_id,"cursor":cursor})

## 本任首长及候选本人新轮办理，仍只调用原生纯页。
func political_successor_rounds_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_successor_rounds")
func political_successor_rounds(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_successor_rounds",{"place_id":place_id,"cursor":cursor})

## 已正式就任者的本人让职页。
func political_term_handovers_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_term_handovers")
func political_term_handovers(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_term_handovers",{"place_id":place_id,"cursor":cursor})

## 公共接待岗位本人认领页。
func political_office_receptions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_office_receptions")
func political_office_receptions(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_office_receptions",{"place_id":place_id,"cursor":cursor})

## 本人原首职继位的有限本地页，真实提名、接受和就任仍走原生命令。
func political_successions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_successions")
func political_successions(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_successions",{"place_id":place_id,"cursor":cursor})
func political_shop_registry_successions(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_shop_registry_successions",{"place_id":place_id,"cursor":cursor})
func political_shop_licensing(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_shop_licensing",{"place_id":place_id,"cursor":cursor})
## 店单仍由原Task生成本人可见页，未知订单不从目录补条款。
func task_shop_orders_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("task_shop_order_profile","")=="private_shop_orders_v1"
func task_shop_orders(cursor: int=0, order_id: String="") -> Dictionary:
	return demand_view("task_shop_orders",{"cursor":cursor,"order_id":order_id})

## 当前选定情景的显式个人康复，旧情景不推断新模块。
func personal_recovery_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	return scenario.get("dynamic_world",{}).get("personal_injury_recovery",{}).get("version")==1

## 只读本人有限履约记录，任何身体改变仍由原服务命令推进。
func personal_recovery_page(page: int=0) -> Dictionary: return demand_view("personal_recovery",{"page":page})

## 复用已选原会面或文书能力；此页只说明最后亲见，不新增来源或位置信息。
func known_contact_locations_enabled() -> bool:
	var scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
	return _installed and (scenario.get("personal_appointment_profile","")=="witnessed_personal_meetings_v1" or scenario.get("task_message_handoff_profile","") in ["observed_letter_delivery_v1","observed_letter_delivery_and_report_v2","known_personal_delivery_and_report_v3","known_personal_delivery_and_sender_report_v4"])

# 只呈现当前所选情景的明确师承能力，所有决定仍由原生本人命令校验。
func family_apprenticeship_enabled() -> bool:
	return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("family_apprenticeship",{}).get("profile")=="adult_personal_apprenticeship_v1"
# 有界师承页不自动代签双方意愿。
func family_apprenticeship_page(page: int=0) -> Dictionary:
	return demand_view("family_apprenticeship",{"page":page})
# 只沿真实已接受师承列原学习课，不复制经验或授予控制权。
func family_apprenticeship_courses(bond_id: String,page: int=0) -> Dictionary:
	return demand_view("family_apprenticeship_courses",{"bond_id":bond_id,"page":page})

## 只显式同版本目录启用问去向；原亲见模块另由Native完整配对。
func contact_inquiries_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("contact_inquiry_profile","")=="witnessed_personal_inquiries_v1"

## 本人回信独立启用，原消息及独立冷段由Native严格配对。
func visit_message_replies_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("visit_reply_profile","") in ["witnessed_visit_replies_v1","witnessed_authored_visit_replies_v2"]

## 只原作者显式租期开放；支付和复业仍由原生同候选报价决定。
func private_shop_daily_rent_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("private_shop_daily_rent")

## 独立出生模板只开真实本人会话；完整Birth与来源配对仍由Native核。
func born_personal_dialogues_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("born_personal_dialogues",{}).get("version",0)==1

func visit_appointments_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("visit_appointment_profile","")=="witnessed_visit_appointments_v1"

# 本人军阵教学只查询原有限页，所有承诺与学费经正式命令确认。
func tactic_lessons_enabled() -> bool:
	return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("growth_tactic_lessons",{}).get("version")==1

func tactic_lessons_page(page: int=0) -> Dictionary:
	return demand_view("growth_tactic_lesson_view",{"page":page})

func known_personal_dialogues_enabled() -> bool:
	return _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("known_personal_dialogues",{}).get("version",0)==1

func political_governance_agencies_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_governance_agencies")
func political_governance_agencies(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_governance_agencies",{"place_id":place_id,"cursor":cursor})

func political_office_resignations_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_office_resignations")
func political_office_resignations(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_office_resignations",{"place_id":place_id,"cursor":cursor})

func political_subordinate_appointments_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_subordinate_appointments")
func political_subordinate_appointments(place_id: String, cursor: int=0) -> Dictionary:
	return demand_view("political_subordinate_appointments",{"place_id":place_id,"cursor":cursor})
