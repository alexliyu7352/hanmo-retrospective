## 当前真实页面的有限读取计划；只请求原getter，不提交命令、推导权限或复制世界状态。
extends RefCounted
const TacticLessonScreen=preload("res://ui/tactic_lesson_screen.gd")
const PersonalRecoveryScreen=preload("res://ui/personal_recovery_screen.gd")
const FamilyVoluntaryPathsScreen=preload("res://ui/family_voluntary_paths_screen.gd")
const VoluntarySuccessionScreen=preload("res://ui/voluntary_succession_screen.gd")
const ChildCareReliefScreen=preload("res://ui/child_care_relief_screen.gd")
const FamilyBirthScreen=preload("res://ui/family_birth_screen.gd")
const FamilyMarriageScreen=preload("res://ui/family_marriage_screen.gd")
const FamilyInheritanceScreen=preload("res://ui/family_inheritance_screen.gd")
const FamilyBequestScreen=preload("res://ui/family_bequest_screen.gd")
const FamilyLegacyScreen=preload("res://ui/family_legacy_screen.gd")
const FieldAppointments=preload("res://ui/political_field_appointment_panel.gd")
const PoliticalTakeoverEntry=preload("res://ui/political_takeover_entry.gd")
const AdultAdoptionScreen=preload("res://ui/adult_adoption_screen.gd")
const InteractionTasksPanel=preload("res://ui/interaction_tasks_panel.gd")
const RuntimeTaskOffersPanel=preload("res://ui/runtime_task_offers_panel.gd")
const PartyParticipationPanel=preload("res://ui/party_participation_panel.gd")
const PartyRecruitmentPanel=preload("res://ui/party_recruitment_panel.gd")
const VisitMessagePanel=preload("res://ui/visit_message_panel.gd")
const TaskPublicAccountPanel=preload("res://ui/task_public_account_panel.gd")
const TaskMessageHandoffPanel=preload("res://ui/task_message_handoff_panel.gd")
const TaskDeathNoticePanel=preload("res://ui/task_death_notice_panel.gd")
const TaskStaticAssetPanel=preload("res://ui/task_static_asset_panel.gd")
const TaskExtensionPanel=preload("res://ui/task_extension_panel.gd")
const TaskMessageReturnsPanel=preload("res://ui/task_message_returns_panel.gd")
const TaskPersonalMessagesPanel=preload("res://ui/task_personal_messages_panel.gd")
const TaskPersonalReportsPanel=preload("res://ui/task_personal_reports_panel.gd")
const TaskPersonalAcceptancesPanel=preload("res://ui/task_personal_acceptances_panel.gd")
const TaskPersonalCancellationsPanel=preload("res://ui/task_personal_cancellations_panel.gd")
const PersonalAppointmentPanel=preload("res://ui/personal_appointment_panel.gd")
const ContactInquiriesPanel=preload("res://ui/contact_inquiries_panel.gd")
const VisitMessageRepliesPanel=preload("res://ui/visit_message_replies_panel.gd")
const VisitAppointmentsPanel=preload("res://ui/visit_appointments_panel.gd")
const BornPersonalDialoguesPanel=preload("res://ui/born_personal_dialogues_panel.gd")
const KnownPersonalDialoguesPanel=preload("res://ui/known_personal_dialogues_panel.gd")
const KnownContactLocationPanel=preload("res://ui/known_contact_location_panel.gd")
const TaskEmploymentPanel=preload("res://ui/task_employment_panel.gd")
const PrivateShopPanel=preload("res://ui/private_shop_panel.gd")
const PrivateShopDailyRentPanel=preload("res://ui/private_shop_daily_rent_panel.gd")
const TaskShopOrdersPanel=preload("res://ui/task_shop_orders_panel.gd")
const PrivateShopLiquidationsPanel=preload("res://ui/private_shop_liquidations_panel.gd")
const PoliticalShopLicensingPanel=preload("res://ui/political_shop_licensing_panel.gd")
const PoliticalShopRegistryPanel=preload("res://ui/political_shop_registry_panel.gd")
const GovernanceContinuationPanel=preload("res://ui/political_governance_continuation_panel.gd")
const GovernanceInvitationReplyPanel=preload("res://ui/governance_invitation_reply_panel.gd")
const PoliticalHeadHandoverPanel=preload("res://ui/political_head_handover_panel.gd")
const PoliticalSuccessorRoundPanel=preload("res://ui/political_successor_round_panel.gd")
const PoliticalTermHandoverPanel=preload("res://ui/political_term_handover_panel.gd")
const PoliticalOfficeReceptionPanel=preload("res://ui/political_office_reception_panel.gd")
const PoliticalOfficeResignationPanel=preload("res://ui/political_office_resignation_panel.gd")
const PoliticalSubordinateAppointmentPanel=preload("res://ui/political_subordinate_appointment_panel.gd")
const PoliticalGovernanceAgencyPanel=preload("res://ui/political_governance_agency_panel.gd")
const PoliticalSuccessionPanel=preload("res://ui/political_succession_panel.gd")
const TaskAdvancePanel=preload("res://ui/task_personal_advance_panel.gd")
const WorldMapTransport=preload("res://ui/world_map_transport.gd")
const MapPresence=preload("res://ui/world_map_presence.gd")
const MapCompany=preload("res://ui/world_map_company.gd")
const MapObligations=preload("res://ui/world_map_obligations.gd")
const MilitaryCamp=preload("res://ui/military_camp_screen.gd")

## 必要页尚未齐全时保持旧完整页面，失败留精确诊断，不将错误字典交旧绘表器当成功值。
static func _ready(host, value: Dictionary) -> bool:
	if value.get("code")=="NativeAsyncRequired": return false
	if value.get("ok",true)==false:
		host._resident_page_error=value.duplicate(true)
		return false
	return true

## 场景模板先由原核心决定人物与候选；当前报价齐备才发布，普通规则拒绝仍保留原点击反馈。
static func _scene_ready(host, place_id: String, kind: String) -> bool:
	var view: Dictionary=host.kernel.scene_view(place_id,kind)
	if not _ready(host,view): return false
	var all_ready: bool=true
	for person in view.get("people",[]):
		all_ready=_ready(host,host.kernel.character_input(person.actor_id)) and all_ready
	if view.get("visibility")=="current":
		for candidate in view.get("hotspot_candidates",[]):
			if not view.has("template_id") and not (kind=="world" and not place_id.is_empty() and candidate.get("command_kind")=="move_local" and candidate.get("target_ref",{}).get("kind")=="place"): continue
			var quote: Dictionary=host.kernel.preview(candidate.command_kind,candidate.params,view.observer_id)
			if quote.get("code")=="NativeAsyncRequired": all_ready=false
			elif quote.get("kind")=="internal": all_ready=_ready(host,quote) and all_ready
	return all_ready

## 任职动作报价在同一页面发布前齐备；普通拒绝保留禁用提示，异步读取不能漏成空白控件。
static func _membership_ready(host, place_id: String, provider: String="") -> bool:
	var view: Dictionary=host.kernel.named_membership_entries(place_id,provider)
	if not _ready(host,view): return false
	var ready: bool=true
	for row in view.get("rows",[]):
		for action in row.get("actions",[]):
			var quote: Dictionary=host.kernel.preview(action.command,action.params)
			if quote.get("code")=="NativeAsyncRequired": ready=false
			elif quote.get("kind")=="internal": ready=_ready(host,quote) and ready
	return ready

## 公务与税页必须先读齐同源有限行及原报价；未启用财政不制造默认税页。
static func _politics_ready(host, place_id: String) -> bool:
	var pages: Array=[host.kernel.politics_entries(place_id)]
	if host.kernel.has_method("political_occupation_fiscal_enabled") and host.kernel.political_occupation_fiscal_enabled():
		pages.append(host.kernel.political_occupation_fiscal_entries(place_id))
	if host.kernel.has_method("political_fiscal_enabled") and host.kernel.political_fiscal_enabled():
		pages.append(host.kernel.political_fiscal_entries(place_id))
	if host.kernel.has_method("politics_governance_enabled") and host.kernel.politics_governance_enabled():
		pages.append(host.kernel.politics_governance(place_id))
	var ready: bool=true
	for page in pages:
		if not _ready(host,page):
			ready=false
			continue
		for row in page.get("rows",[]):
			for action in row.get("actions",[]):
				ready=_quote_ready(host,action.command,action.params) and ready
	if host.kernel.has_method("political_takeover_enabled") and host.kernel.political_takeover_enabled():
		var takeover: Dictionary=host.kernel.political_takeover_entries(place_id)
		if not _ready(host,takeover): ready=false
		else:
			for row in takeover.get("registrations",[]):
				ready=_quote_ready(host,PoliticalTakeoverEntry.COMMAND,PoliticalTakeoverEntry.command_params(host,row)) and ready
	return ready

## 当面递交与答复沿当前say/choice会话，先读齐有限行及同源原报价。
static func _diplomacy_ready(host, place_id: String) -> bool:
	if not host.kernel.has_method("diplomacy_enabled") or not host.kernel.diplomacy_enabled(): return true
	var view: Dictionary=host.kernel.diplomacy_entries(place_id)
	if not _ready(host,view): return false
	var ready: bool=true
	for row in view.get("rows",[]):
		for action in row.get("actions",[]):
			ready=_quote_ready(host,action.command,action.params) and ready
	return ready

## 可见动作只准备原参数报价；异步等待阻挡页面，普通规则拒绝仍交原控件显示。
static func _quote_ready(host, kind: String, params: Dictionary={}, actor_id: String="") -> bool:
	var quote: Dictionary=host.kernel.preview(kind,params,actor_id)
	if quote.get("code")=="NativeAsyncRequired": return false
	return _ready(host,quote) if quote.get("kind")=="internal" else true

## 当前修业页的原Session按钮报价同批到齐，预算确认不会因页面装配又产生纯读而空转。
static func _study_ready(host, activity: Dictionary) -> bool:
	if activity.get("kind")!="study_skill": return true
	var actor_id: String=activity.actor_id
	var params: Dictionary={"action_id":activity.action_id}
	var ready: bool=true
	var paused: bool=activity.get("paused",false)
	if activity.get("pause_supported",false):
		ready=_quote_ready(host,"study_resume" if paused else "study_pause",params,actor_id) and ready
	if not paused:
		ready=_quote_ready(host,"advance",{},actor_id) and ready
		if host._player_plan_enabled():
			ready=_quote_ready(host,"player_plan_add",{"kind":"continue_action","params":params,"authorized":false},actor_id) and ready
		if "study" in activity.get("challenge_scopes",[]):
			var challenge: Dictionary=host.kernel.study_challenge(actor_id,activity.action_id)
			if challenge.get("code")=="NativeAsyncRequired": ready=false
			elif challenge.get("kind")=="internal": ready=_ready(host,challenge) and ready
			elif challenge.get("ok",false) and not challenge.get("consumed",false) and challenge.get("phase")!="completed":
				for option in challenge.get("options",[]):
					var choice: Dictionary={"action_id":activity.action_id,"step_id":challenge.get("node_id",challenge.get("round_id","")),"option_id":option.id}
					ready=_quote_ready(host,"study_challenge_choose",choice,actor_id) and ready
				ready=_quote_ready(host,"study_challenge_exit",params,actor_id) and ready
	return _quote_ready(host,"study_cancel",params,actor_id) and ready

## 只按原屏幕优先级选择一个页面；表内画像或课程上下文仍先由对应原生叶选定。
static func prepare(host) -> bool:
	var kernel=host.kernel
	host._resident_page_error={}
	# 新页面仍在读取时保留旧完整画面，但名单与真实命中控件都不能继续发起旧场景输入。
	for scene in host.find_children("SceneHost","",true,false):
		if scene.has_method("set_input_enabled"): scene.set_input_enabled(false)
	if kernel.has_method("family_life_enabled") and kernel.family_life_enabled():
		var death: Dictionary=kernel.family_death_page()
		if not _ready(host,death): return false
		if death.get("control_confirmation_required",false):
			return _legacy_ready(host) if kernel.has_method("family_legacy_enabled") and kernel.family_legacy_enabled() else true
	var battle: Dictionary=kernel.personal_battle_view()
	if not _ready(host,battle): return false
	if not battle.get("battle_id","").is_empty():
		var hud_ready: bool=_ready(host,kernel.hud_view(battle.battle_id))
		var context_ready: bool=_ready(host,kernel.personal_battle_context(battle.battle_id))
		return hud_ready and context_ready
	var military: Dictionary=kernel.military_battle_page()
	if not _ready(host,military): return false
	if military.get("active",false): return true
	var siege: Dictionary=kernel.siege_page()
	if not _ready(host,siege): return false
	if not siege.get("siege_id","").is_empty(): return true
	if not host._resident_trade_receipt_pending.is_empty():
		var receipt: Dictionary=kernel.trade_action_receipt(host._resident_trade_receipt_pending)
		if not _ready(host,receipt): return false
		host._trade_receipt=receipt; host._resident_trade_receipt_pending=""
	var manual: Dictionary=kernel.demand_view("action_manual_window")
	if not _ready(host,manual): return false
	if TacticLessonScreen.selected(host):
		var lessons: Dictionary=kernel.tactic_lessons_page(int(host.get_meta("tactic_lessons_page",0)))
		if not _ready(host,lessons): return false
		var ready: bool=true
		for offer in lessons.get("offers",[]):
			if offer.get("eligible",false): ready=_quote_ready(host,"tactic_lesson_start",offer.params) and ready
		if lessons.get("active_course") is Dictionary and lessons.active_course.get("can_cancel",false): ready=_quote_ready(host,"tactic_lesson_cancel",lessons.active_course.cancel_params) and ready
		return ready
	if PersonalRecoveryScreen.selected(host):
		var recovery: Dictionary=kernel.personal_recovery_page(int(host.get_meta("personal_recovery_page",0)))
		if not _ready(host,recovery): return false
		if recovery.get("active") is Dictionary:
			if not _quote_ready(host,"recovery_cancel",{"action_id":recovery.active.action_id}): return false
		return true
	if host.get("_study_teaching_open")==true:
		var teaching: Dictionary=host._study_teaching_view()
		if not _ready(host,teaching): return false
		var ready: bool=true
		for row in teaching.get("requests",[]): ready=_quote_ready(host,"teach_study",row.params) and ready
		var course: Dictionary=teaching.get("active_course",{})
		if course.get("active",false):
			var params: Dictionary={"action_id":course.action_id}
			if course.get("pause_supported",false): ready=_quote_ready(host,"study_resume" if course.get("paused",false) else "study_pause",params) and ready
			if not course.get("paused",false): ready=_quote_ready(host,"advance",{}) and ready
			ready=_quote_ready(host,"study_cancel",params) and ready
		return ready
	if host.get("_cards_open")==true:
		var cards: Dictionary=host._cards_view()
		if not _ready(host,cards): return false
		var ready: bool=true
		for row in cards.get("cards",[]):
			if row.get("category")=="heart" and row.get("learned",false): ready=_quote_ready(host,"configure_heart",{"skill_id":"" if row.get("configured",false) else row.personal_skill_id}) and ready
		for params in preload("res://ui/growth_cards_practice.gd").requests(host,cards.get("cards",[])): ready=_quote_ready(host,"practice_skill",params) and ready
		return ready
	if host._self_profile_open:
		var prepared: Dictionary=host.SelfProfileAdapter.prepare(kernel)
		if prepared.get("code")=="NativeAsyncRequired":
			host._self_profile_view={}; return false
		if not prepared.get("ready",false):
			host._self_profile_view={"ok":false,"code":prepared.get("code","InvalidSelfProfile")}; return true
		host._self_profile_view=prepared.profile.duplicate(true)
		var labels: Dictionary={}
		for role in host._self_profile_view.roles:
			var definition: Dictionary=host._data.get("roles",{}).get(role.role_id,{})
			labels[role.role_id]=host._data.get("texts",{}).get(definition.get("name_key",""),role.role_id)
		host._self_profile_view["role_names"]=labels
		return true
	if kernel.player_plan_enabled() and host.get("_player_plan_open")==true: return _ready(host,kernel.demand_view("player_plan"))
	if host._ordinary_skills_open: return _ready(host,kernel.ordinary_skills(host._ordinary_skill_id))
	if kernel.action_context().get("action_kind") in host.ActivityContinuationControls.KINDS and kernel.has_method("continuation_state"):
		if not _ready(host,kernel.continuation_state()): return false
		if not _quote_ready(host,"advance"): return false
	var activity: Dictionary=kernel.study_activity()
	if not _ready(host,activity): return false
	if activity.get("active",false) and (not activity.get("paused",false) or host._courses_open): return _study_ready(host,activity)
	var location: Dictionary=kernel.presentation_location()
	var on_road: bool=location.get("kind") in ["Wilderness","OnRoute","Following","InArmy"]
	if host._dynamic_open and not on_road: return host.DynamicWorldPanel.prepare(host)
	if host._courses_open and not on_road:
		var courses: Dictionary=kernel.demand_view("study_course_contexts")
		if not _ready(host,courses): return false
		var all_ready: bool=true
		var selected: Dictionary=host.get_meta("study_course_ticks",{})
		for entry in courses.rows:
			var ready: bool=_ready(host,kernel.study_courses(kernel.command_context().actor_id,entry.context,selected.get(entry.role_id,2)))
			all_ready=ready and all_ready
		return all_ready
	if host._military_open and (not on_road or location.get("kind")=="InArmy"):
		if host.get_meta("camp_tab","orders")=="field_appointments" and not FieldAppointments.prepare(host): return false
		if host._resident_provider_pending=="military":
			var provider: Dictionary=kernel.military_camp_provider_context()
			if not _ready(host,provider): return false
			host._military_context=provider; host._resident_provider_pending=""
		var selection: Dictionary=MilitaryCamp.selection(host)
		var camp: Dictionary=kernel.military_camp_page(host.get_meta("camp_tab","orders"),selection,host._military_context)
		if not _ready(host,camp): return false
		if not camp.get("selected_army_id","").is_empty():
			var resolved: Dictionary=MilitaryCamp.selection(host,camp.selected_army_id)
			if resolved!=selection: return _ready(host,kernel.military_camp_page(host.get_meta("camp_tab","orders"),resolved,camp.entry_provider.get("context",{})))
		return true
	if host._family_open and not on_road:
		if ChildCareReliefScreen.selected(host): return _care_relief_ready(host)
		if FamilyBirthScreen.selected(host): return _birth_ready(host)
		if FamilyMarriageScreen.selected(host): return _marriage_ready(host)
		if FamilyInheritanceScreen.selected(host): return _inheritance_ready(host)
		if FamilyBequestScreen.selected(host): return _bequest_ready(host)
		if FamilyLegacyScreen.selected(host): return _legacy_ready(host)
		if FamilyVoluntaryPathsScreen.selected(host):
			var living: Dictionary=FamilyVoluntaryPathsScreen.page(host)
			if not _ready(host,living): return false
			for row in living.get("rows",[]):
				for operation in row.get("actions",[]):
					if operation.get("available",false) and not _ready(host,kernel.preview(operation.command,operation.params)): return false
			return true
		if VoluntarySuccessionScreen.selected(host):
			var succession: Dictionary=kernel.voluntary_succession_page(int(host.get_meta("succession_page",0)))
			if not _ready(host,succession): return false
			for request in succession.get("requests",[]):
				var operation: Dictionary=request.accept_action
				if operation.get("available",false) and not _ready(host,kernel.preview(operation.command,operation.params)): return false
			for plan in succession.get("plans",[]):
				for operation in plan.get("actions",[]):
					if operation.get("available",false) and not _ready(host,kernel.preview(operation.command,operation.params)): return false
			return true
		if AdultAdoptionScreen.selected(host):
			var adoption: Dictionary=kernel.adult_adoption_requests(int(host.get_meta("adult_adoption_page",0)))
			if not _ready(host,adoption): return false
			for entry in adoption.get("requests",[]):
				if entry.get("eligible",false) and not _ready(host,kernel.preview("family_adopt_existing",entry.params)): return false
			return true
		if host._resident_provider_pending=="family":
			var provider: Dictionary=kernel.family_provider_context()
			if not _ready(host,provider): return false
			host._family_context=provider; host._resident_provider_pending=""
		return _ready(host,kernel.family_page(host.get_meta("family_tab","family"),host._family_context))
	if not host._estate_id.is_empty():
		var available: Dictionary=kernel.demand_view("estate_available",{"estate_id":host._estate_id})
		if not _ready(host,available): return false
		if available.get("available",false): return _ready(host,kernel.estate_page(host._estate_id,host.get_meta("estate_tab","people")))
	if host._growth_open: return _ready(host,kernel.character_summary(host.get_meta("growth_page","skills")))
	if not PartyParticipationPanel.prepare(host): return false
	if not PartyRecruitmentPanel.prepare(host): return false
	if not VisitMessagePanel.prepare(host): return false
	if not TaskPublicAccountPanel.prepare(host): return false
	if not TaskMessageHandoffPanel.prepare(host): return false
	if not TaskDeathNoticePanel.prepare(host): return false
	if not TaskStaticAssetPanel.prepare(host): return false
	if not TaskExtensionPanel.prepare(host): return false
	if not TaskMessageReturnsPanel.prepare(host): return false
	if not TaskPersonalMessagesPanel.prepare(host): return false
	if not TaskPersonalReportsPanel.prepare(host): return false
	if not TaskPersonalAcceptancesPanel.prepare(host): return false
	if not TaskPersonalCancellationsPanel.prepare(host): return false
	if not PersonalAppointmentPanel.prepare(host): return false
	if host.kernel.has_method("contact_inquiries_enabled") and host.kernel.contact_inquiries_enabled() and not ContactInquiriesPanel.prepare(host): return false
	if host.kernel.has_method("visit_message_replies_enabled") and host.kernel.visit_message_replies_enabled() and not VisitMessageRepliesPanel.prepare(host): return false
	if host.kernel.has_method("visit_appointments_enabled") and host.kernel.visit_appointments_enabled() and not VisitAppointmentsPanel.prepare(host): return false
	if host.kernel.has_method("born_personal_dialogues_enabled") and host.kernel.born_personal_dialogues_enabled() and not BornPersonalDialoguesPanel.prepare(host): return false
	if host.kernel.has_method("known_personal_dialogues_enabled") and host.kernel.known_personal_dialogues_enabled() and not KnownPersonalDialoguesPanel.prepare(host): return false
	if kernel.has_method("known_contact_locations_enabled") and kernel.known_contact_locations_enabled():
		if not KnownContactLocationPanel.prepare(host): return false
	if not TaskEmploymentPanel.prepare(host): return false
	if kernel.has_method("private_shop_liquidations_enabled") and kernel.private_shop_liquidations_enabled():
		if not PrivateShopLiquidationsPanel.prepare(host): return false
	if kernel.has_method("private_shop_daily_rent_enabled") and kernel.private_shop_daily_rent_enabled():
		if not PrivateShopDailyRentPanel.prepare(host): return false
	if kernel.has_method("private_shops_enabled") and kernel.private_shops_enabled():
		if not PrivateShopPanel.prepare(host): return false
	if kernel.has_method("task_shop_orders_enabled") and kernel.task_shop_orders_enabled():
		if not TaskShopOrdersPanel.prepare(host): return false
	if not str(location.get("place_id","")).is_empty() and kernel.has_method("political_shop_licensing_enabled") and kernel.political_shop_licensing_enabled():
		if not PoliticalShopLicensingPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalShopRegistryPanel.prepare(host,str(location.get("place_id",""))): return false
	if not str(location.get("place_id","")).is_empty():
		if not GovernanceContinuationPanel.prepare(host,str(location.get("place_id",""))): return false
		if not GovernanceInvitationReplyPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalHeadHandoverPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalSuccessorRoundPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalTermHandoverPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalOfficeReceptionPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalOfficeResignationPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalSubordinateAppointmentPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalGovernanceAgencyPanel.prepare(host,str(location.get("place_id",""))): return false
		if not PoliticalSuccessionPanel.prepare(host,str(location.get("place_id",""))): return false
	if not TaskAdvancePanel.prepare(host): return false
	if on_road or host.journey_view.opened:
		if not _scene_ready(host,location.get("place_id",""),"world"): return false
		if not MapPresence.prepare(host): return false
		if not WorldMapTransport.prepare(host): return false
		if not MapCompany.prepare(host): return false
		return MapObligations.prepare(host)
	if host._regional_life_enabled():
		var regional_draft: Dictionary=host._regional_draft()
		if not _ready(host,kernel.regional_life_topics(location.place_id,regional_draft.cursor)): return false
	if host.has_method("_politics_enabled") and host._politics_enabled():
		if not _politics_ready(host,location.place_id): return false
	if not _diplomacy_ready(host,location.place_id): return false
	var sidebar: Dictionary=kernel.place_sidebar()
	if not _ready(host,sidebar): return false
	var view: Dictionary=kernel.interaction_view()
	if not InteractionTasksPanel.prepare(host,view): return false
	if not RuntimeTaskOffersPanel.prepare(host,view): return false
	if host._named_membership_enabled():
		var membership_ready: bool=_membership_ready(host,location.place_id)
		if view.get("phase") in ["say","choice","end"]:
			membership_ready=_membership_ready(host,location.place_id,view.get("context",{}).get("provider_actor_id","")) and membership_ready
		if not membership_ready: return false
	var place: Dictionary=host._data.places[location.place_id]
	var kind: String="city" if place.get("parent_place")==null else "interior"
	var all_ready: bool=_scene_ready(host,location.place_id,kind)
	for person in sidebar.get("people",[]):
		all_ready=_ready(host,kernel.character_input(person.actor_id)) and all_ready
	if view.get("phase")!="trade" and not host._trade_request.is_empty(): host._reset_trade_view()
	if view.get("phase")=="trade":
		all_ready=_ready(host,kernel.trade_view(view.command.shop_id,view.context.provider_actor_id,host._trade_receipt.get("action_id",""))) and all_ready
	return all_ready

## 终点选择与生前许可共用当前有限页；仅为实际可用动作取得正式报价。
static func _legacy_ready(host) -> bool:
	var page: Dictionary=FamilyLegacyScreen.page(host)
	if not _ready(host,page): return false
	for request in page.get("requests",[]):
		var operation: Dictionary=request.action
		if operation.get("available",false) and not _ready(host,host.kernel.preview(operation.command,operation.params)): return false
	for consent in page.get("consents",[]):
		for operation in consent.get("actions",[]):
			if operation.get("available",false) and not _ready(host,host.kernel.preview(operation.command,operation.params)): return false
	return true

## 遗赠正文先取得有限当前页；可编辑金额按本人点击取得原生正式报价。
static func _bequest_ready(host) -> bool:
	var page: Dictionary=host.kernel.family_bequest_page(int(host.get_meta("bequest_page",0)))
	if not _ready(host,page): return false
	var active: Dictionary=page.get("active",{})
	if not active.is_empty() and active.get("revoke_available",false):
		var quote: Dictionary=host.kernel.preview("family_bequest_revoke",{"declaration_id":active.declaration_id})
		if quote.get("code")=="NativeAsyncRequired": return false
	return true

## 原通知和有限条款同时到齐才渲染；动态分项数量仍由点击请求正式异步报价。
static func _inheritance_ready(host) -> bool:
	FamilyInheritanceScreen.navigation(host)
	var binding: String=String(host.get_meta("inheritance_notice",""))
	var page: Dictionary=host.kernel.family_inheritance_page(int(host.get_meta("inheritance_page",0)),int(host.get_meta("inheritance_lot_page",0)),binding)
	var notices: Dictionary=host.kernel.demand_view("task_death_notices",{"cursor":int(host.get_meta("inheritance_notice_page",0))*16})
	if not _ready(host,page) or not _ready(host,notices): return false
	for row in page.get("own_declarations",[]):
		if row.get("disclose_available",false):
			var quote: Dictionary=host.kernel.preview("family_bequest_disclose",{"declaration_id":row.declaration_id})
			if quote.get("code")=="NativeAsyncRequired": return false
	for row in page.get("known_bequests",[]):
		if row.get("accept_available",false):
			var quote: Dictionary=host.kernel.preview("family_inheritance_accept",{"disclosure_fact_id":row.disclosure_fact_id,"notice_binding_fact_id":binding})
			if quote.get("code")=="NativeAsyncRequired": return false
	return true

## 正式有限页和精确可用报价到齐才渲染，不在等待异步时自动作出本人决定。
static func _marriage_ready(host) -> bool:
	var page: Dictionary=host.kernel.family_marriage_page(int(host.get_meta("marriage_page",0)))
	if not _ready(host,page): return false
	for row in page.get("current_statements",[]):
		if row.get("available",false):
			var params={"statement_id":row.statement_id}
			if row.command=="family_marriage_dissolve": params["marriage_id"]=row.marriage_id
			var quote: Dictionary=host.kernel.preview(row.command,params)
			if quote.get("code")=="NativeAsyncRequired": return false
	for row in page.get("proposals",[]):
		for answer in row.get("options",[]):
			if answer.get("available",false):
				var quote: Dictionary=host.kernel.preview("family_marriage_reply",{"proposal_id":row.proposal_id,"decision":answer.decision})
				if quote.get("code")=="NativeAsyncRequired": return false
	return true

## 原照护ID与有限本人页到齐才预读报价；预读不接受同意、不启Action或消耗粮。
static func _birth_ready(host) -> bool:
	var page: Dictionary=host.kernel.family_birth_page(int(host.get_meta("birth_page",0)))
	if not _ready(host,page): return false
	for row in page.get("current_statements",[]):
		if row.get("option",{}).get("available",false):
			var quote: Dictionary=host.kernel.preview("family_birth_receive",{"arrangement_id":row.arrangement_id})
			if quote.get("code")=="NativeAsyncRequired": return false
	for row in page.get("proposals",[]):
		for answer in row.get("options",[]):
			if answer.get("available",false):
				var quote: Dictionary=host.kernel.preview("family_birth_reply",{"proposal_id":row.proposal_id,"decision":answer.decision})
				if quote.get("code")=="NativeAsyncRequired": return false
	for row in page.get("arrangements",[]):
		for option in row.get("options",[]):
			if option.get("available",false):
				var params={"accepted_id":row.accepted_id}
				if option.kind=="family_child_care_cancel": params={"action_id":option.action_id}
				var quote: Dictionary=host.kernel.preview(option.kind,params)
				if quote.get("code")=="NativeAsyncRequired": return false
	return true

## 异步预读只采用原本人页原ID，不代本人接受、开工或消费粮食。
static func _care_relief_ready(host) -> bool:
	var page: Dictionary=host.kernel.family_care_relief_page(int(host.get_meta("care_relief_page",0)))
	if not _ready(host,page): return false
	for row in page.get("current_offers",[]):
		if row.get("option",{}).get("available",false):
			var quote: Dictionary=host.kernel.preview("family_care_relief_accept",{"offer_id":row.offer_id,"child_actor_id":row.child.actor_id})
			if quote.get("code")=="NativeAsyncRequired": return false
	for row in page.get("agreements",[]):
		for option in row.get("options",[]):
			if option.get("available",false):
				var params={"action_id":option.action_id} if option.kind=="family_care_relief_cancel" else {"accepted_id":row.accepted_id}
				var quote: Dictionary=host.kernel.preview(option.kind,params)
				if quote.get("code")=="NativeAsyncRequired": return false
	return true
