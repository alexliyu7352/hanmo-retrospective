## 围绕统一世界状态组合场景与交互目标，表现层不计算游戏结果。
extends RefCounted
const TacticLessonScreen=preload("res://ui/tactic_lesson_screen.gd")
const PersonalRecoveryScreen=preload("res://ui/personal_recovery_screen.gd")
const StudyScreen=preload("res://ui/study_screen.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const InteractionTopics=preload("res://core/interaction_topics.gd")
const TimeRules=preload("res://core/base/time_rules.gd")
const FamilyVoluntaryPathsScreen=preload("res://ui/family_voluntary_paths_screen.gd")
const VoluntarySuccessionScreen=preload("res://ui/voluntary_succession_screen.gd")
const AdultAdoptionScreen=preload("res://ui/adult_adoption_screen.gd")
const FamilyDeathScreen=preload("res://ui/family_death_screen.gd")
const ChildCareReliefScreen=preload("res://ui/child_care_relief_screen.gd")
const FamilyBirthScreen=preload("res://ui/family_birth_screen.gd")
const ApprenticeshipScreen=preload("res://ui/apprenticeship_screen.gd")
const FamilyMarriageScreen=preload("res://ui/family_marriage_screen.gd")
const FamilyInheritanceScreen=preload("res://ui/family_inheritance_screen.gd")
const FamilyBequestScreen=preload("res://ui/family_bequest_screen.gd")
const FamilyLegacyScreen=preload("res://ui/family_legacy_screen.gd")
const FamilyScreen=preload("res://ui/family_screen.gd")
const EstateScreen=preload("res://ui/estate_screen.gd")
const Location = preload("res://core/world_location.gd")
const Art = preload("res://ui/art_catalog.gd")
const MilitaryCamp=preload("res://ui/military_camp_screen.gd")
const MilitaryBattle=preload("res://ui/military_battle_screen.gd")
const NavalBattle=preload("res://ui/military_naval_battle_screen.gd")
const SiegeScreen=preload("res://ui/siege_screen.gd")
const PersonalBattle = preload("res://ui/personal_battle_screen.gd")
const OrdinarySkills=preload("res://ui/ordinary_skills_screen.gd")
const Growth=preload("res://ui/growth_screen.gd")
const GrowthTeachingText=preload("res://ui/growth_teaching_text.gd")
const SelfProfile=preload("res://ui/self_profile_screen.gd")
const SceneHost=preload("res://ui/scene_host.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
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
const NamedMembershipEntry=preload("res://ui/named_membership_entry.gd")
const PoliticsEntry=preload("res://ui/politics_entry.gd")
const DiplomacyEntry=preload("res://ui/diplomacy_entry.gd")
const PlayerPlanScreen=preload("res://ui/player_plan_screen.gd")
const PlayerPlanControls=preload("res://ui/player_plan_controls.gd")
## 使用归一化屏幕坐标放置布局，不改变人物在世界中的位置。
static func stack(parent: Control, rect: Rect2) -> VBoxContainer:
	var box = VBoxContainer.new()
	parent.add_child(box)
	box.anchor_left = rect.position.x; box.anchor_top = rect.position.y
	box.anchor_right = rect.end.x; box.anchor_bottom = rect.end.y
	box.add_theme_constant_override("separation", 10)
	return box
## 叠加只读遮罩提高文字对比度，不遮断场景交互。
static func shade(parent: Control, rect: Rect2, color: Color) -> void:
	var overlay = ColorRect.new(); overlay.color = color
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(overlay)
	overlay.anchor_left=rect.position.x; overlay.anchor_top=rect.position.y
	overlay.anchor_right=rect.end.x; overlay.anchor_bottom=rect.end.y
## 重建表现控件，保留世界、请求序号和存储服务。
static func render(m) -> void:
	m._scene_counter += 1
	for child in m.page.get_children():
		m.page.remove_child(child); child.queue_free()
	var stage = m.Landscape.new()
	stage.name = "PlaceStage" if m._started else "TitleStage"
	stage.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	stage.size_flags_vertical = Control.SIZE_EXPAND_FILL
	# 本人有限页遵守实际视口，不继承旧场所680像素下限而把返回或底部内容挤出窄窗。
	stage.custom_minimum_size = Vector2(0, 0 if m.get("_self_profile_open")==true else 680)
	if not m._started:
		stage.visual_tag="title_landscape" if "title_landscape" in Art.catalog().ids("backgrounds") else str(Art.catalog().describe().get("default_background","")); m.page.add_child(stage); title(m,stage); return
	if m._resident_mode and m.kernel.has_method("family_life_enabled") and m.kernel.family_life_enabled():
		var death: Dictionary=m.kernel.family_death_page()
		if death.get("control_confirmation_required",false):
			m._rest_auto=false; m._stop_journey_auto(); stage.visual_tag="city"; m.page.add_child(stage)
			if m.kernel.has_method("family_legacy_enabled") and m.kernel.family_legacy_enabled(): FamilyLegacyScreen.render(m,stage,true)
			else: FamilyDeathScreen.build(m,stage,death,m._save,m._title_prompt)
			return
	var personal_view: Dictionary=m.kernel.personal_battle_view()
	var battle_id: String=personal_view.get("battle_id", "")
	if not battle_id.is_empty():
		stage.visual_tag=m._data.personal_battlefields[personal_view.battle.battlefield_id].get("visual_tag", "city")
		m.page.add_child(stage); hud(m,stage,m.kernel.hud_view(battle_id),false)
		PersonalBattle.render_view(m,stage,personal_view)
		return
	var naval_page: Dictionary=m.kernel.naval_battle_page()
	if naval_page.get("active",false):
		stage.visual_tag=naval_page.geometry.get("visual_tag","")
		m.page.add_child(stage); hud(m,stage,m.kernel.hud_view(),false)
		NavalBattle.render(m,stage,naval_page)
		return
	var military_page: Dictionary=m.kernel.military_battle_page()
	if military_page.get("active",false):
		stage.visual_tag=military_page.header.stage_visual_tag
		m.page.add_child(stage); hud(m,stage,m.kernel.hud_view(),false)
		MilitaryBattle.render(m,stage,military_page)
		return
	var siege_page: Dictionary=m.kernel.siege_page()
	if not siege_page.get("siege_id","").is_empty():
		stage.visual_tag="camp"; m.page.add_child(stage); hud(m,stage,m.kernel.hud_view(),false)
		SiegeScreen.render_view(m,stage,siege_page)
		return
	var location: Dictionary=m.kernel.presentation_location()
	var on_road: bool=location.kind in ["OnRoute","Wilderness","Following","InArmy"]
	var place: Dictionary={} if on_road else m._data.places[location.place_id]
	stage.visual_tag="city" if on_road else place.get("visual_tag","city")
	m.page.add_child(stage)
	hud(m,stage,m.kernel.hud_view())
	if m._resident_mode and m.kernel.has_method("tactic_lessons_enabled") and m.kernel.tactic_lessons_enabled():
		var lesson_entries: Control=stage.get_node("SystemMenu/SystemMenuScroll").get_child(0)
		m._button(lesson_entries,"战法授课","tactic_lessons_open","",func(): TacticLessonScreen.open(m),m._advancing).name="TacticLessonsOpen"
		if TacticLessonScreen.selected(m): TacticLessonScreen.render(m,stage); return
	if m._resident_mode and m.kernel.has_method("personal_recovery_enabled") and m.kernel.personal_recovery_enabled():
		var recovery_entries: Control=stage.get_node("SystemMenu/SystemMenuScroll").get_child(0)
		m._button(recovery_entries,"本人休养","personal_recovery_open","",func(): PersonalRecoveryScreen.open(m),m._advancing).name="PersonalRecoveryOpen"
		if PersonalRecoveryScreen.selected(m): PersonalRecoveryScreen.render(m,stage); return

	var notice=plaque(stage,Rect2(.325,.095,.42,.075))
	m.status_label=m._text(notice,m._message,17)
	notice.get_parent().visible=not m._message.is_empty()
	if m.get("_study_teaching_open")==true:
		m._render_study_teaching(stage)
		return
	if m.get("_cards_open")==true:
		m._render_cards(stage)
		return
	if m.get("_self_profile_open")==true:
		SelfProfile.build(m,stage,m._self_profile_view,m._close_self_profile,m._navigate_self_profile)
		return
	if PlayerPlanControls.enabled(m) and m.get("_player_plan_open")==true:
		PlayerPlanScreen.build(m,stage,m.kernel.demand_view("player_plan"))
		return
	if m._ordinary_skills_open:
		OrdinarySkills.build(m,stage,m.kernel.ordinary_skills(m._ordinary_skill_id))
		return
	var activity: Dictionary=m.kernel.study_activity()
	if activity.get("paused",false):
		var menu_entries: Control=stage.get_node("SystemMenu/SystemMenuScroll").get_child(0)
		m._button(menu_entries,"暂停的课程","courses_open","",m._open_courses,m._advancing).name="PausedStudyMenuOpen"
	if activity.active and (not activity.get("paused",false) or m._courses_open):
		var body: VBoxContainer=_activity_body(stage)
		StudyScreen.render_active_view(m,body,activity)
		if activity.get("paused",false): m._button(body,"返回当前场所","courses_close","",m._close_courses).name="StudyPausedClose"
		return
	if m._dynamic_open and not on_road:
		m.DynamicWorldPanel.build(m,stage)
		return
	if m._courses_open and not on_road:
		var body: VBoxContainer=_activity_body(stage); var count: int=0
		for entry in m.kernel.study_course_contexts():
			var role: Dictionary=m._data.roles[entry.role_id]
			count+=1; m._text(body,m._name(role),23)
			StudyScreen.render_courses(m,body,entry.context)
		if count==0: m._text(body,"本处没有明确提供的普通技艺课程。",18)
		m._button(body,"返回场所","courses_close","",m._close_courses)
		return
	if m._military_open and (not on_road or location.kind=="InArmy"):
		MilitaryCamp.render(m,stage,m.kernel.military_camp_page(m.get_meta("camp_tab","orders"),MilitaryCamp.selection(m),m._military_context))
		return
	if m._family_open and not on_road:
		if ChildCareReliefScreen.selected(m):
			ChildCareReliefScreen.render(m,stage)
			return
		if FamilyBirthScreen.selected(m):
			FamilyBirthScreen.render(m,stage)
			return
		if ApprenticeshipScreen.selected(m):
			ApprenticeshipScreen.render(m,stage)
			return
		if FamilyMarriageScreen.selected(m):
			FamilyMarriageScreen.render(m,stage)
			return
		if FamilyInheritanceScreen.selected(m):
			FamilyInheritanceScreen.render(m,stage)
			return
		if FamilyBequestScreen.selected(m):
			FamilyBequestScreen.render(m,stage)
			return
		if FamilyLegacyScreen.selected(m):
			FamilyLegacyScreen.render(m,stage)
			return
		if FamilyVoluntaryPathsScreen.selected(m):
			FamilyVoluntaryPathsScreen.render(m,stage)
			return
		if VoluntarySuccessionScreen.selected(m):
			VoluntarySuccessionScreen.render(m,stage)
			return
		if AdultAdoptionScreen.selected(m):
			AdultAdoptionScreen.render(m,stage)
			return
		FamilyScreen.render(m,stage,m.kernel.family_page(m.get_meta("family_tab","family"),m._family_context))
		return
	if not m._estate_id.is_empty() and m.kernel.estate_available(m._estate_id):
		EstateScreen.render(m,stage,m.kernel.estate_page(m._estate_id,m.get_meta("estate_tab","people")))
		return
	if m._growth_open:
		Growth.build(m,stage,m.kernel.character_summary(m.get_meta("growth_page","skills")))
		return
	if on_road or m.journey_view.opened:
		m.WorldMapScreen.render(m,stage,m.kernel.journey_sidebar())
		if m._resident_mode and PartyParticipationPanel.visible_here(m): PartyParticipationPanel.build(m,_sidebar_section(m,stage,"PartyParticipationArea","同行"))
		if m._resident_mode and TaskPublicAccountPanel.visible_here(m): TaskPublicAccountPanel.build(m,_sidebar_section(m,stage,"TaskPublicAccountArea","公账履责"))
		if m._resident_mode and TaskMessageHandoffPanel.visible_here(m): TaskMessageHandoffPanel.build(m,_sidebar_section(m,stage,"TaskMessageHandoffArea","文书交接"))
		if m._resident_mode and TaskDeathNoticePanel.visible_here(m): TaskDeathNoticePanel.build(m,_sidebar_section(m,stage,"TaskDeathNoticeArea","已收死亡文书"))
		if m._resident_mode and TaskStaticAssetPanel.visible_here(m): TaskStaticAssetPanel.build(m,_sidebar_section(m,stage,"TaskStaticAssetArea","本人实物明细"))
		if m._resident_mode and TaskExtensionPanel.visible_here(m): TaskExtensionPanel.build(m,_sidebar_section(m,stage,"TaskExtensionArea","本人私人委托延期"))
		if m._resident_mode and TaskMessageReturnsPanel.visible_here(m): TaskMessageReturnsPanel.build(m,_sidebar_section(m,stage,"TaskMessageReturnsArea","原任务回书"))
		if m._resident_mode and TaskPersonalMessagesPanel.visible_here(m): TaskPersonalMessagesPanel.build(m,_sidebar_section(m,stage,"TaskPersonalMessagesArea","原信件当面交付"))
		if m._resident_mode and TaskPersonalReportsPanel.visible_here(m): TaskPersonalReportsPanel.build(m,_sidebar_section(m,stage,"TaskPersonalReportsArea","原信件向本人返报"))
		if m._resident_mode and TaskPersonalAcceptancesPanel.visible_here(m): TaskPersonalAcceptancesPanel.build(m,_sidebar_section(m,stage,"TaskPersonalAcceptancesArea","原信件当面受领"))
		if m._resident_mode and TaskPersonalCancellationsPanel.visible_here(m): TaskPersonalCancellationsPanel.build(m,_sidebar_section(m,stage,"TaskPersonalCancellationsArea","原信件当面销约"))
		if m._resident_mode and PersonalAppointmentPanel.visible_here(m): PersonalAppointmentPanel.build(m,_sidebar_section(m,stage,"PersonalAppointmentArea","本人会面约定"))
		if m._resident_mode and m.kernel.has_method("contact_inquiries_enabled") and m.kernel.contact_inquiries_enabled() and ContactInquiriesPanel.visible_here(m): ContactInquiriesPanel.build(m,_sidebar_section(m,stage,"ContactInquiriesArea","当面问去向"))
		if m._resident_mode and m.kernel.has_method("visit_message_replies_enabled") and m.kernel.visit_message_replies_enabled() and VisitMessageRepliesPanel.visible_here(m): VisitMessageRepliesPanel.build(m,_sidebar_section(m,stage,"VisitMessageRepliesArea","拜帖答复"))
		if m._resident_mode and m.kernel.has_method("visit_appointments_enabled") and m.kernel.visit_appointments_enabled() and VisitAppointmentsPanel.visible_here(m): VisitAppointmentsPanel.build(m,_sidebar_section(m,stage,"VisitAppointmentsArea","拜帖约期"))
		if m._resident_mode and m.kernel.has_method("born_personal_dialogues_enabled") and m.kernel.born_personal_dialogues_enabled() and BornPersonalDialoguesPanel.visible_here(m): BornPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"BornPersonalDialoguesArea","本人私人交谈"))
		if m._resident_mode and m.kernel.has_method("known_personal_dialogues_enabled") and m.kernel.known_personal_dialogues_enabled() and KnownPersonalDialoguesPanel.visible_here(m): KnownPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"KnownPersonalDialoguesArea","当面私人交谈"))
		if m._resident_mode and m.kernel.has_method("known_contact_locations_enabled") and m.kernel.known_contact_locations_enabled() and KnownContactLocationPanel.visible_here(m): KnownContactLocationPanel.build(m,_sidebar_section(m,stage,"KnownContactLocationArea","已知去向"))
		if m._resident_mode and VisitMessagePanel.visible_here(m): VisitMessagePanel.build(m,_sidebar_section(m,stage,"VisitMessageArea","本人拜帖"))
		if m._resident_mode and TaskEmploymentPanel.visible_here(m): TaskEmploymentPanel.build(m,_sidebar_section(m,stage,"TaskEmploymentArea","私人雇用"))
		if m._resident_mode: _private_shop_panels(m,stage,str(location.get("place_id","")))
		if m._resident_mode and TaskAdvancePanel.visible_here(m): TaskAdvancePanel.build(m,_sidebar_section(m,stage,"TaskAdvanceArea","受托经费"))
		return
	var view: Dictionary=m.kernel.interaction_view()
	var sidebar: Dictionary=m.kernel.place_sidebar()
	var scene_mode: String=_mount_scene_view(m,stage,place,view)
	var place_title=m._text(plaque(stage,Rect2(.025,.095,.28,.068)),m._name(m._data.settlements[place.settlement_id])+" · "+m._name(place),23)
	place_title.name="PlaceTitle"
	if scene_mode=="legacy": inhabitants(m,stage,sidebar,place,view)
	if m._regional_life_enabled(): regional_life(m,stage,view,scene_mode)
	elif m._politics_enabled() and view.get("phase")=="scene": PoliticsEntry.local_affairs(m,_sidebar_section(m,stage,"PoliticalAffairs","当地公务"))
	if m._resident_mode and PartyParticipationPanel.visible_here(m): PartyParticipationPanel.build(m,_sidebar_section(m,stage,"PartyParticipationArea","同行"))
	if m._resident_mode and TaskPublicAccountPanel.visible_here(m): TaskPublicAccountPanel.build(m,_sidebar_section(m,stage,"TaskPublicAccountArea","公账履责"))
	if m._resident_mode and TaskMessageHandoffPanel.visible_here(m): TaskMessageHandoffPanel.build(m,_sidebar_section(m,stage,"TaskMessageHandoffArea","文书交接"))
	if m._resident_mode and TaskDeathNoticePanel.visible_here(m): TaskDeathNoticePanel.build(m,_sidebar_section(m,stage,"TaskDeathNoticeArea","已收死亡文书"))
	if m._resident_mode and TaskStaticAssetPanel.visible_here(m): TaskStaticAssetPanel.build(m,_sidebar_section(m,stage,"TaskStaticAssetArea","本人实物明细"))
	if m._resident_mode and TaskExtensionPanel.visible_here(m): TaskExtensionPanel.build(m,_sidebar_section(m,stage,"TaskExtensionArea","本人私人委托延期"))
	if m._resident_mode and TaskMessageReturnsPanel.visible_here(m): TaskMessageReturnsPanel.build(m,_sidebar_section(m,stage,"TaskMessageReturnsArea","原任务回书"))
	if m._resident_mode and TaskPersonalMessagesPanel.visible_here(m): TaskPersonalMessagesPanel.build(m,_sidebar_section(m,stage,"TaskPersonalMessagesArea","原信件当面交付"))
	if m._resident_mode and TaskPersonalReportsPanel.visible_here(m): TaskPersonalReportsPanel.build(m,_sidebar_section(m,stage,"TaskPersonalReportsArea","原信件向本人返报"))
	if m._resident_mode and TaskPersonalAcceptancesPanel.visible_here(m): TaskPersonalAcceptancesPanel.build(m,_sidebar_section(m,stage,"TaskPersonalAcceptancesArea","原信件当面受领"))
	if m._resident_mode and TaskPersonalCancellationsPanel.visible_here(m): TaskPersonalCancellationsPanel.build(m,_sidebar_section(m,stage,"TaskPersonalCancellationsArea","原信件当面销约"))
	if m._resident_mode and PersonalAppointmentPanel.visible_here(m): PersonalAppointmentPanel.build(m,_sidebar_section(m,stage,"PersonalAppointmentArea","本人会面约定"))
	if m._resident_mode and m.kernel.has_method("contact_inquiries_enabled") and m.kernel.contact_inquiries_enabled() and ContactInquiriesPanel.visible_here(m): ContactInquiriesPanel.build(m,_sidebar_section(m,stage,"ContactInquiriesArea","当面问去向"))
	if m._resident_mode and m.kernel.has_method("visit_message_replies_enabled") and m.kernel.visit_message_replies_enabled() and VisitMessageRepliesPanel.visible_here(m): VisitMessageRepliesPanel.build(m,_sidebar_section(m,stage,"VisitMessageRepliesArea","拜帖答复"))
	if m._resident_mode and m.kernel.has_method("visit_appointments_enabled") and m.kernel.visit_appointments_enabled() and VisitAppointmentsPanel.visible_here(m): VisitAppointmentsPanel.build(m,_sidebar_section(m,stage,"VisitAppointmentsArea","拜帖约期"))
	if m._resident_mode and m.kernel.has_method("born_personal_dialogues_enabled") and m.kernel.born_personal_dialogues_enabled() and BornPersonalDialoguesPanel.visible_here(m): BornPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"BornPersonalDialoguesArea","本人私人交谈"))
	if m._resident_mode and m.kernel.has_method("known_personal_dialogues_enabled") and m.kernel.known_personal_dialogues_enabled() and KnownPersonalDialoguesPanel.visible_here(m): KnownPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"KnownPersonalDialoguesArea","当面私人交谈"))
	if m._resident_mode and m.kernel.has_method("known_contact_locations_enabled") and m.kernel.known_contact_locations_enabled() and KnownContactLocationPanel.visible_here(m): KnownContactLocationPanel.build(m,_sidebar_section(m,stage,"KnownContactLocationArea","已知去向"))
	if m._resident_mode and VisitMessagePanel.visible_here(m): VisitMessagePanel.build(m,_sidebar_section(m,stage,"VisitMessageArea","本人拜帖"))
	if m._resident_mode and TaskEmploymentPanel.visible_here(m): TaskEmploymentPanel.build(m,_sidebar_section(m,stage,"TaskEmploymentArea","私人雇用"))
	if m._resident_mode: _private_shop_panels(m,stage,str(location.get("place_id","")))
	if m._resident_mode and TaskAdvancePanel.visible_here(m): TaskAdvancePanel.build(m,_sidebar_section(m,stage,"TaskAdvanceArea","受托经费"))
	if view.get("phase")=="scene" and m._resident_mode and m.kernel.has_method("diplomacy_enabled") and m.kernel.diplomacy_enabled(): DiplomacyEntry.local_affairs(m,_sidebar_section(m,stage,"DiplomaticAffairs","外交往来"))
	if view.phase!="scene": conversation(m,stage,view,sidebar.dialogue_speaker.display_name)
	else:
		var caption=plaque(stage,Rect2(.235,.775,.345,.095))
		m._text(caption,m._data.texts.get("flavour."+place.id,"街巷深处，各有各的日子。"),18)
		navigation(m,stage,sidebar,place,scene_mode!="legacy")
		for event_id in sidebar.queued_event_ids:
			if m.kernel.preview("open_event",{"event_id":event_id}).ok:
				var hint=stack(stage,Rect2(.40,.68,.48,.09))
				m._button(hint,"窗边的客人朝你点了点头……","open_event",event_id,m._perform.bind("open_event",{"event_id":event_id}))


## 必要课程输入复用现有内容区域和纵向滚动；不改变场所或人物世界位置。
static func _activity_body(stage: Control) -> VBoxContainer:
	var frame: VBoxContainer=plaque(stage,Rect2(.025,.18,.95,.78))
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; frame.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	return body

## 仅明确映射启用同一只读宿主；坏模板留诊断及合法列表，无映射保留原屏幕。
static func _mount_scene(m, stage: Control, state: Dictionary, place: Dictionary, view: Dictionary) -> String:
	return _mount_scene_view(m,stage,place,view)

## 实际外壳不再传整世界；旧五参夹具只转同一宿主装配，不读取兼容state。
static func _mount_scene_view(m, stage: Control, place: Dictionary, view: Dictionary) -> String:
	var host: Control=SceneHost.new()
	host.embedded_list=view.phase=="scene"
	stage.add_child(host); stage.move_child(host,0)
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var kind: String="city" if place.parent_place==null else "interior"
	var result: Dictionary=host.configure_view(m.kernel.scene_view(place.id,kind),m._data,place.id,kind,m.kernel.preview,m.kernel.character_input)
	var mode: String=result.get("mode","error")
	if mode=="legacy":
		stage.remove_child(host); host.queue_free()
		return mode
	stage.set_layered_mode(true)
	host.set_meta("render_result",result.duplicate(true))
	host.set_input_enabled(view.phase=="scene" and not m._advancing and m._storage_intent.is_empty())
	host.command_requested.connect(m._scene_request.bind(host))
	return mode


## 墨牌承载有限文字，在复杂绘景上提供稳定对比度。
static func plaque(parent: Control, rect: Rect2) -> VBoxContainer:
	var panel=PanelContainer.new(); parent.add_child(panel)
	panel.anchor_left=rect.position.x; panel.anchor_top=rect.position.y; panel.anchor_right=rect.end.x; panel.anchor_bottom=rect.end.y
	panel.add_theme_stylebox_override("panel",Art.ink())
	var box=VBoxContainer.new(); box.add_theme_constant_override("separation",6); panel.add_child(box)
	return box

## 独立字稿与绘景组成标题；菜单文字和存储入口仍是可操作控件。
static func title(m, stage: Control) -> void:
	var heading=PanelContainer.new(); stage.add_child(heading)
	heading.anchor_left=.055; heading.anchor_right=.43; heading.anchor_top=.14; heading.anchor_bottom=.34
	heading.add_theme_stylebox_override("panel",Art.paper())
	var brand=VBoxContainer.new(); brand.set_meta("paper",true); heading.add_child(brand)
	var mark: Texture2D=Art.visible_region(Art.background("ui_title"))
	if mark!=null:
		var logo=TextureRect.new(); logo.name="TitleMark"; logo.texture=mark; logo.custom_minimum_size.y=112; logo.expand_mode=TextureRect.EXPAND_IGNORE_SIZE; logo.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED; brand.add_child(logo)
	else:
		var name=m._text(brand,"汉末行记",68); name.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
	var subtitle=m._text(brand,"山河未定，行路方始。",22); subtitle.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
	var box=stack(stage,Rect2(.105,.405,.275,.42)); box.add_theme_constant_override("separation",12)
	for entry in [["启程","start",m._start],["继续旅途","continue",m._continue],["继续地方旅途","continue_dynamic",m._continue_dynamic],["导入行旅存档","import",m._choose_import],["离开","quit",m.get_tree().quit]]:
		var button: Button=m._button(box,entry[0],entry[1],"",entry[2]); button.custom_minimum_size.y=48
	m._ui_font_scale.make_selector(box)
	if not m._message.is_empty(): m._text(box,m._message,17)
	m._text(stack(stage,Rect2(.04,.925,.7,.05)),"汉末行记　"+str(ProjectSettings.get_setting("application/config/version")),16)

## 顶栏只消费同次有限HUD投影；按钮、声音、字体和菜单仍由原本地UI状态控制。
static func hud(m, stage: Control, view: Dictionary, allow_ordinary_skills: bool=true) -> void:
	shade(stage,Rect2(0,0,1,.075),Color("141c20ee"))
	var brand=m._text(stack(stage,Rect2(.023,.015,.16,.047)),"汉末行记",25)
	brand.add_theme_color_override("font_color",Color("ddc492"))
	var label=m._text(stack(stage,Rect2(.20,.020,.56,.045)),"第%d日%s　%s　 钱%d　体力%d"%[view.day_index+1,"上半日" if view.half_day==0 else "下半日",view.display_name,view.balance,view.hp],18)
	label.name="WorldStatus"
	var bar=HBoxContainer.new(); bar.name="HudActions"; bar.grow_horizontal=Control.GROW_DIRECTION_BEGIN; stage.add_child(bar); bar.anchor_left=.765; bar.anchor_right=.98; bar.anchor_top=.012; bar.anchor_bottom=.067; bar.add_theme_constant_override("separation",5)
	for spec in [["地图","map",m._open_map],["手记","journal",m._journal]]:
		var button: Button=m._button(bar,spec[0],spec[1],"",spec[2],m._advancing); button.custom_minimum_size=Vector2(76,38); button.add_theme_font_size_override("font_size",17)
	var menu=PanelContainer.new(); menu.name="SystemMenu"; stage.add_child(menu); menu.z_index=20
	menu.anchor_left=.76; menu.anchor_right=.98; menu.anchor_top=.085; menu.anchor_bottom=.84; menu.add_theme_stylebox_override("panel",Art.ink())
	var scroll=ScrollContainer.new(); scroll.name="SystemMenuScroll"; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; menu.add_child(scroll)
	var entries=VBoxContainer.new(); entries.size_flags_horizontal=Control.SIZE_EXPAND_FILL; entries.add_theme_constant_override("separation",6); scroll.add_child(entries)
	if m._resident_mode:
		var profile: Button=m._button(entries,"本人档案","self_profile_open","",m._open_self_profile,m._advancing or not allow_ordinary_skills)
		profile.name="SelfProfileMenuOpen"
		if not allow_ordinary_skills: profile.tooltip_text="请先处理当前战斗与结果，再查看本人档案。"
	if "cultivation" in m._data.get("required_capabilities",[]):
		var ordinary: Button=m._button(entries,"普通技艺","ordinary_skills_open","",m._open_ordinary_skills,m._advancing or not allow_ordinary_skills)
		ordinary.name="OrdinarySkillsMenuOpen"
		if not allow_ordinary_skills: ordinary.tooltip_text="请先处理当前战斗与结果，再查看普通技艺。"
	if m.has_method("_study_teaching_enabled") and m._study_teaching_enabled():
		m._button(entries,"授课与学生请求","study_teaching_open","",m._open_study_teaching,m._advancing or not allow_ordinary_skills).name="StudyTeachingMenuOpen"
	if m._cards_available():
		m._button(entries,"卡片收藏","growth_cards_open","",m._open_cards,m._advancing or not allow_ordinary_skills).name="GrowthCardsMenuOpen"
	for spec in [["保存行旅","save",m._save],["载入行旅","load",m._load],["导出存档","export",m._choose_export],["导入存档","import",m._choose_import],["声音 · 开" if m._ambience.enabled else "声音 · 关","audio",m._toggle_audio],["返回标题","title",m._title_prompt]]:
		m._button(entries,spec[0],spec[1],"",spec[2],m._advancing)
	m._ui_font_scale.make_selector(entries)
	menu.visible=m.get_meta("system_menu_open",false) if m._resident_mode else false
	var toggle: Button=m._button(bar,"菜单","system_menu","",_toggle_menu.bind(menu,m)); toggle.custom_minimum_size=Vector2(76,38); toggle.add_theme_font_size_override("font_size",17)

## 菜单开合只改变可见性，不递增请求号、时间或重建当前对白。
static func _toggle_menu(menu: Control, host=null) -> void:
	menu.visible=not menu.visible
	if host!=null and host._resident_mode: host.set_meta("system_menu_open",menu.visible)
	if menu.visible: menu.get_parent().move_child(menu,-1)

## 旧场景的名单与通道共用有界滚动列，按独立分组自然排布，不能互相覆盖输入。
static func _sidebar_section(m, stage: Control, section_name: String, heading: String) -> VBoxContainer:
	var scroll: ScrollContainer=stage.get_node_or_null("SceneSidebar") as ScrollContainer
	if scroll==null:
		scroll=ScrollContainer.new(); scroll.name="SceneSidebar"
		scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED
		scroll.follow_focus=true
		stage.add_child(scroll)
		scroll.anchor_left=.025; scroll.anchor_right=.23; scroll.anchor_top=.205; scroll.anchor_bottom=.765
		var content=VBoxContainer.new(); content.name="Groups"
		content.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		content.add_theme_constant_override("separation",16); scroll.add_child(content)
	var groups: VBoxContainer=scroll.get_node("Groups") as VBoxContainer
	var section: VBoxContainer=groups.get_node_or_null(section_name) as VBoxContainer
	if section==null:
		section=VBoxContainer.new(); section.name=section_name
		section.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		section.add_theme_constant_override("separation",10); groups.add_child(section)
		m._text(section,heading,17)
	return section

## 按当前人物分组展示全部岗位主题；不可用第一岗位不能遮住同人的另一合法入口。
static func inhabitants(m, stage: Control, sidebar: Dictionary, place: Dictionary, view: Dictionary) -> void:
	if view.phase!="scene": return
	var roster: VBoxContainer=null
	var portrait: Control=null; var name_label: Label=null
	var portrait_context: Dictionary={"resolution":_portrait_resolution(stage,Rect2(.54,.17,.40,.69))}
	for person in sidebar.people:
		if portrait==null:
			portrait=_portrait(stage,Art.character_input(m.kernel.character_input(person.actor_id),"half_body",portrait_context),Rect2(.54,.17,.40,.69),"ScenePersonPortrait")
			name_label=m._text(plaque(stage,Rect2(.60,.79,.28,.063)),person.display_name,22); name_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
		for topic in person.topics:
			if roster==null: roster=_sidebar_section(m,stage,"PeopleActions","人物与事务")
			var role: Dictionary=m._data.roles[topic.role_id]
			var preview: Dictionary=m.kernel.preview("talk",{"role_id":role.id})
			var caption: String=person.display_name
			if person.topics.size()>1: caption+=" · "+m._name(role)
			var target: Button=m._button(roster,caption,"talk",role.id,m._topic_request.bind(role.id,sidebar.actor_id,sidebar.revision,m._scene_counter),not topic.available or not preview.ok or m._advancing,false)
			target.custom_minimum_size=Vector2(0,44)
			target.tooltip_text=m._name(role)+(" · 点击交谈" if preview.ok else " · "+m._reason(preview.get("code","NoDialogueForRole")))
			target.mouse_entered.connect(_focus_person.bind(portrait,name_label,Art.character_input(m.kernel.character_input(person.actor_id),"half_body",portrait_context),person.display_name))
	for prop in m._data.props.values():
		if prop.place_id!=place.id: continue
		if roster==null: roster=_sidebar_section(m,stage,"PeopleActions","人物与事务")
		m._button(roster,m._name(prop),"inspect",prop.id,m._perform.bind("inspect",{"prop_id":prop.id}),m._advancing)

## 悬停人物更换只读焦点；点击时仍提交该人的真实岗位上下文。
static func _focus_person(portrait: Control, label: Label, descriptor: Dictionary, caption: String) -> void:
	var result: Dictionary=portrait.configure(descriptor,Art.catalog())
	portrait.set_meta("render_result",result); label.text=caption
	# 控件失败时保留上一帧供重试，但换人物焦点不能把旧脸配到新姓名。
	portrait.visible=result.ok
	portrait.tooltip_text="" if result.ok else "人物图像暂不可用"

## 按实际视口与人物区域请求像素，避免把默认小预览放大成模糊半身；仍遵守1024上限。
static func _portrait_resolution(stage: Control, rect: Rect2) -> Array:
	var viewport: Vector2=stage.get_viewport_rect().size
	return [clampi(ceili(viewport.x*rect.size.x),1,1024),clampi(ceili(viewport.y*rect.size.y),1,1024)]

## 透明立绘等比放入独立区域，缺图时保持空位而不把背景截图当人物。
static func _portrait(stage: Control, descriptor: Dictionary, rect: Rect2, node_name: String) -> Control:
	var portrait: Control=Art.character_control(descriptor); portrait.name=node_name
	stage.add_child(portrait); portrait.anchor_left=rect.position.x; portrait.anchor_top=rect.position.y; portrait.anchor_right=rect.end.x; portrait.anchor_bottom=rect.end.y
	portrait.modulate.a=0; portrait.create_tween().tween_property(portrait,"modulate:a",1.0,.15)
	return portrait

## 对白保留场景与当前真实说话者；纸笺位于下方，货单需要更大的可滚动区域。
static func conversation(m, stage: Control, view: Dictionary, speaker_name: Variant=null) -> void:
	var speaker: String=view.get("speaker",view.context.provider_actor_id)
	_portrait(stage,Art.character_input(m.kernel.character_input(speaker),"half_body",{"resolution":_portrait_resolution(stage,Rect2(.58,.17,.38,.74))}),Rect2(.58,.17,.38,.74),"SpeakerPortrait")
	var scroll=ScrollContainer.new(); scroll.name="DialogueScroll"; stage.add_child(scroll)
	scroll.anchor_left=.235; scroll.anchor_right=.69 if view.phase=="trade" else .955; scroll.anchor_top=.22 if view.phase=="trade" else .60; scroll.anchor_bottom=.965
	scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED
	m.main_column=VBoxContainer.new(); m.main_column.size_flags_horizontal=Control.SIZE_EXPAND_FILL; m.main_column.add_theme_constant_override("separation",9); scroll.add_child(m.main_column)
	if speaker_name==null: m._conversation(view)
	else: m._conversation(view,speaker_name)

## 复用对白运行器原有阶段与宿主报价；只把文本和选项排成纸笺与两列墨牌。
static func conversation_body(m, view: Dictionary, speaker_name: Variant=null) -> void:
	var holder=PanelContainer.new(); holder.name="DialoguePaper"; m.main_column.add_child(holder); holder.add_theme_stylebox_override("panel",Art.paper())
	var box=VBoxContainer.new(); box.set_meta("paper",true); box.add_theme_constant_override("separation",8); holder.add_child(box)
	var speaker: String=view.get("speaker",view.context.provider_actor_id)
	var title=m._text(box,m._name(m._actor(speaker)) if speaker_name==null else speaker_name,22); title.add_theme_color_override("font_color",Color("79532a"))
	if m._regional_life_enabled():
		var draft: Dictionary=m._regional_draft()
		var regional: Dictionary=m.kernel.regional_life_topics(draft.place_id,draft.cursor)
		if not regional.get("conversation_text","").is_empty():
			var detail: Label=m._text(box,regional.conversation_text,17); detail.name="RegionalDialogueSource"
			detail.set_meta("regional_source",m.kernel.source_identity())
	NamedMembershipEntry.conversation(m,box,view)
	InteractionTasksPanel.conversation(m,box,view)
	PartyRecruitmentPanel.build(m,box)
	VisitMessagePanel.build(m,box)
	RuntimeTaskOffersPanel.conversation(m,box,view)
	if view.phase in ["say","choice"]: DiplomacyEntry.local_affairs(m,box)
	var options=GridContainer.new(); options.name="DialogueChoices"; options.columns=2; options.add_theme_constant_override("h_separation",10); options.add_theme_constant_override("v_separation",8)
	match view.phase:
		"say":
			m._text(box,view.text,21)
			_dialogue_button(m,options,"继续","dialogue_next","",{},view,m._advancing)
		"choice":
			m._text(box,view.text,21)
			for option in view.options: _dialogue_button(m,options,option.text,"dialogue_choose",option.id,{"option_id":option.id},view,m._advancing)
		"action":
			var preview: Dictionary=m.kernel.preview("dialogue_confirm"); var words: String="应承之前，再核一遍约定。"; var request: Dictionary=view.get("command",{})
			if request.get("kind")=="join_party" and preview.ok: words="同行每个实际行走半日额外%d钱，由你支付；停步不收路费。"%preview.cost_per_tick
			if request.get("kind")=="service": words="店家已说明价钱。" if request.get("activity")=="rest" else "帮店家做半日活，工钱办完结清。"
			if request.get("kind")=="duel_start" and preview.ok: words="以实际随身武器切磋，场地与对手已经约定。交付%d钱，胜负均不另发报酬。"%preview.fee
			if request.get("kind")=="learn_skill" and preview.ok: words="向眼前的授艺人学习%s，束脩%d钱。取得招式后仍须自行修炼。"%[m._name(m._data.personal_skills[request.skill_id]),preview.cost]
			var teaching_words: String=GrowthTeachingText.confirmation(m,request,preview)
			if not teaching_words.is_empty(): words=teaching_words
			if preview.get("ok",false) and preview.get("contract_terms") is Dictionary:
				var terms: Dictionary=preview.contract_terms
				words="承接%d份；采办经费上限%d钱，约定报酬%d钱；须在半日%d前交付实际货物。"%[terms.quantity,terms.purchase_budget_cap,terms.reward_total,terms.deadline_tick]
			m._text(box,words+("　耗时%d个半日"%preview.get("duration",0) if preview.ok else "　"+m._reason(preview.get("code","Unknown"))),20)
			if request.get("kind")=="service" and request.get("activity")=="work":
				PlayerPlanControls.append(m,box,"work",{"context":view.context.duplicate(true)},"把这份短工安排到稍后")
			_dialogue_button(m,options,"确认办理","dialogue_confirm","",{},view,not preview.ok or m._advancing)
			_dialogue_button(m,options,"暂不办理","dialogue_cancel","",{},view,m._advancing)
		"waiting":
			m._text(box,"已经谈妥，正在办理……",21)
			_dialogue_button(m,options,"继续半日","advance","",{},view,m._advancing or not m.kernel.preview("advance").ok).name="DialogueAdvance"
		"trade": m._trade(box,view)
	if "npc_society" in m._data.required_capabilities and view.phase in ["say","choice"]: _dialogue_button(m,options,"问问近事","ask_news","",{},view,m._advancing or not m.kernel.preview("ask_news").ok)
	if view.phase!="choice" or m._regional_life_enabled(): _dialogue_button(m,options,"结束交谈","end_interaction","",{},view,m._advancing or view.phase=="waiting")
	m.main_column.add_child(options)
	for button in options.get_children():
		button.size_flags_horizontal=Control.SIZE_EXPAND_FILL; button.custom_minimum_size.y=44; button.alignment=HORIZONTAL_ALIGNMENT_LEFT; button.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART

## 左侧建筑入口对应实际通行地点；本人未结军令可在城内查阅，具体办理仍由真实地点和Kernel核验。
static func navigation(m, stage: Control, sidebar: Dictionary, place: Dictionary, mapped_scene: bool=false) -> void:
	# 折行仍留在原底栏范围；内容超高时滚动，键盘焦点也能带出最后一行，不让最小高度撑出视口。
	var footer=ScrollContainer.new(); footer.name="SceneNavigationScroll"
	footer.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; footer.follow_focus=true
	stage.add_child(footer); footer.anchor_left=.235; footer.anchor_right=.975; footer.anchor_top=.90; footer.anchor_bottom=.99
	var paths=HFlowContainer.new(); paths.name="SceneNavigationActions"; paths.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	paths.add_theme_constant_override("h_separation",8); footer.add_child(paths)
	# 地方近况加入既有可折行底栏，避免覆盖场景作者安排的人物入口。
	if m._resident_mode and m.kernel.dynamic_world_enabled():
		m._button(paths,"查看地方近况","dynamic_open",place.id,m._open_dynamic_world,m._advancing).name="DynamicWorldOpen"
	if PlayerPlanControls.enabled(m):
		m._button(paths,"查看生活安排","player_plan_open","",m._open_player_plan,m._advancing).name="PlayerPlanOpen"
		for destination in place.passages:
			PlayerPlanControls.append(m,paths,"move_local",{"place_id":destination},"稍后去 · "+m._name(m._data.places[destination]))
	var buildings: VBoxContainer=null
	if not mapped_scene and place.parent_place==null:
		buildings=_sidebar_section(m,stage,"PlaceActions","通往何处")
	for destination in ([] if mapped_scene else place.passages):
		var preview: Dictionary=m.kernel.preview("move_local",{"place_id":destination})
		var target_place: Dictionary=m._data.places[destination]
		var city_view: bool=place.parent_place==null
		var parent: Control=buildings if city_view else paths
		var label: String=m._name(target_place) if city_view else "离开 · "+m._name(target_place)
		var b: Button=m._button(parent,label,"move_local",destination,m._perform.bind("move_local",{"place_id":destination}),not preview.ok or m._advancing)
		if city_view:
			b.custom_minimum_size=Vector2(0,44); b.alignment=HORIZONTAL_ALIGNMENT_LEFT
			b.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	if place.id==m._data.settlements[place.settlement_id].entry_place:
		for dest in m._data.settlements.values():
			if dest.id==place.settlement_id:continue
			var p: Dictionary=m.kernel.preview("travel",{"destination":dest.id})
			if p.ok:m._button(paths,"出城 · "+m._name(dest)+"（%d个半日）"%p.duration,"travel",dest.id,m._perform.bind("travel",{"destination":dest.id}),m._advancing or sidebar.action_busy)
	m._button(paths,"等候片刻","wait","",m._perform.bind("wait",{}),m._advancing or sidebar.action_busy)
	for id in sidebar.estate_ids:
		m._button(paths,"查看庄务","estate_open",id,m._open_estate.bind(id),m._advancing)
	var military_here: bool=sidebar.military_here
	if military_here and "military" in m._data.required_capabilities:
		m._button(paths,"军务与部曲","camp_open","",m._open_military_camp,m._advancing)
	if "family" in m._data.required_capabilities:
		m._button(paths,"家事与休整","family_open","",m._open_family,m._advancing)
	if "cultivation" in m._data.required_capabilities:
		m._button(paths,"普通技艺","ordinary_skills_open","",m._open_ordinary_skills,m._advancing).name="OrdinarySkillsOpen"
		m._button(paths,"本处技艺课程","courses_open","",m._open_courses,m._advancing)
		m._button(paths,"武学与行囊","growth","",m._open_growth,m._advancing)
	for campaign in sidebar.operating_campaigns:
		var params: Dictionary={"campaign_id":campaign.campaign_id}
		if m.kernel.preview("siege_start",params).ok:
			m._button(paths,"展开围城军议","siege_start",campaign.campaign_id,m._perform.bind("siege_start",params),m._advancing)
		# 当前军令目标已有围城时只显示真实报价允许的援军入口，不在UI重算组织或行动资格。
		var siege_id: String=campaign.target_siege_id
		if not siege_id.is_empty():
			var join_params: Dictionary={"siege_id":siege_id,"campaign_id":campaign.campaign_id}
			if m.kernel.preview("siege_join",join_params).ok:
				m._button(paths,"加入围城军议","siege_join",siege_id,m._perform.bind("siege_join",join_params),m._advancing)

## 地方对白沿原phase和命令，新增来源守卫只作用于显式新章节。
static func _dialogue_button(m, parent: Node, label: String, kind: String, target: String, params: Dictionary, view: Dictionary, disabled: bool=false) -> Button:
	if m._regional_life_enabled():
		return m._regional_button(parent,label,kind,target,params,m._dialogue_identity(view),disabled)
	return m._button(parent,label,kind,target,m._perform.bind(kind,params),disabled)

## 只向原人物名单加入当场生活和岗位说明；有模板时复用其已有滚动容器。
static func regional_life(m, stage: Control, interaction: Dictionary, scene_mode: String) -> void:
	if interaction.get("phase")!="scene": return
	var draft: Dictionary=m._regional_draft()
	var view: Dictionary=m.kernel.regional_life_topics(draft.place_id,draft.cursor)
	if not view.get("ok",false): return
	var parent: VBoxContainer=null
	if scene_mode!="legacy":
		var host: Control=stage.find_child("SceneHost",true,false)
		if host!=null: parent=host.current_candidate_container()
	if parent==null: parent=_sidebar_section(m,stage,"RegionalLife","眼前近况")
	NamedMembershipEntry.local_people(m,parent)
	PoliticsEntry.local_affairs(m,parent)
	for row in view.rows:
		if row.get("kind")=="actor":
			var label: Label=m._text(parent,row.display_name+" · "+row.text,17)
			label.set_meta("regional_row",row.duplicate(true)); label.name="RegionalActorActivity"
		elif row.get("action") is Dictionary:
			var action: Dictionary=row.action
			var button: Button=m._regional_button(parent,row.text,action.command,row.topic_id,action.params,{},not row.get("available",false))
			button.set_meta("regional_row",row.duplicate(true)); button.name="RegionalTopic"
		else:
			var code: String=row.get("code","")
			var reason: String="眼下没有新的可分享近况。" if code=="RegionalSourceUnavailable" else "这段近况刚聊过，稍后再问。" if code=="RegionalTopicCoolingDown" else m._reason(code)
			var label: Label=m._text(parent,row.text+" · "+reason,16); label.set_meta("regional_row",row.duplicate(true))
	if draft.cursor>0: m._button(parent,"上一段近况","regional_previous","",m._regional_page.bind(maxi(0,draft.cursor-64),draft.source,m._scene_counter),m._advancing)
	if view.next_cursor>=0: m._button(parent,"下一段近况","regional_next","",m._regional_page.bind(view.next_cursor,draft.source,m._scene_counter),m._advancing)


## 经营/店单可看本人旧责任，法律柜台严格传当前地点；所有决定仍原quote与命令。
static func _private_shop_panels(m, stage: Control, current_place: String) -> void:
	var kernel=m.kernel
	if kernel.has_method("private_shop_liquidations_enabled") and kernel.private_shop_liquidations_enabled():
		PrivateShopLiquidationsPanel.build(m,_sidebar_section(m,stage,"PrivateShopLiquidationsArea","店铺委托与清算"))
	if kernel.has_method("private_shop_daily_rent_enabled") and kernel.private_shop_daily_rent_enabled() and not PrivateShopDailyRentPanel._current(m).is_empty():
		PrivateShopDailyRentPanel.build(m,_sidebar_section(m,stage,"PrivateShopDailyRentArea","原租期与自愿付款"))
	if kernel.has_method("private_shops_enabled") and kernel.private_shops_enabled():
		PrivateShopPanel.build(m,_sidebar_section(m,stage,"PrivateShopArea","本人商铺"))
	if kernel.has_method("task_shop_orders_enabled") and kernel.task_shop_orders_enabled() and TaskShopOrdersPanel.visible_here(m):
		TaskShopOrdersPanel.build(m,_sidebar_section(m,stage,"TaskShopOrdersArea","本人店单"))
	if not current_place.is_empty() and kernel.has_method("political_shop_licensing_enabled") and kernel.political_shop_licensing_enabled():
		var page: Dictionary=kernel.political_shop_licensing(current_place,int(m.get_meta("shop_licensing_cursor",0)))
		PoliticalShopLicensingPanel.render(m,_sidebar_section(m,stage,"PoliticalShopLicensingArea","私铺许可"),page,current_place)
	if not current_place.is_empty() and kernel.has_method("political_shop_registry_successions_enabled") and kernel.political_shop_registry_successions_enabled():
		PoliticalShopRegistryPanel.render(m,_sidebar_section(m,stage,"PoliticalShopRegistryArea","司法接册"),kernel.political_shop_registry_successions(current_place,int(m.get_meta("shop_registry_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_governance_continuations_enabled") and kernel.political_governance_continuations_enabled():
		GovernanceContinuationPanel.render(m,_sidebar_section(m,stage,"GovernanceContinuationArea","安民责任续办"),kernel.political_governance_continuations(current_place,int(m.get_meta("governance_continuation_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("governance_invitation_replies_enabled") and kernel.governance_invitation_replies_enabled():
		GovernanceInvitationReplyPanel.render(m,_sidebar_section(m,stage,"GovernanceInvitationReplyArea","安民邀请回复"),kernel.governance_invitation_replies(current_place,int(m.get_meta("governance_invitation_reply_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_head_handovers_enabled") and kernel.political_head_handovers_enabled():
		PoliticalHeadHandoverPanel.render(m,_sidebar_section(m,stage,"PoliticalHeadHandoverArea","原首职生前交接"),kernel.political_head_handovers(current_place,int(m.get_meta("political_head_handover_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_successor_rounds_enabled") and kernel.political_successor_rounds_enabled():
		PoliticalSuccessorRoundPanel.render(m,_sidebar_section(m,stage,"PoliticalSuccessorRoundArea","后任首长新轮继位"),kernel.political_successor_rounds(current_place,int(m.get_meta("political_successor_round_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_term_handovers_enabled") and kernel.political_term_handovers_enabled():
		PoliticalTermHandoverPanel.render(m,_sidebar_section(m,stage,"PoliticalTermHandoverArea","后任首职生前交接"),kernel.political_term_handovers(current_place,int(m.get_meta("political_term_handover_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_office_receptions_enabled") and kernel.political_office_receptions_enabled():
		PoliticalOfficeReceptionPanel.render(m,_sidebar_section(m,stage,"PoliticalOfficeReceptionArea","公共接待岗位"),kernel.political_office_receptions(current_place,int(m.get_meta("political_office_reception_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_office_resignations_enabled") and kernel.political_office_resignations_enabled():
		PoliticalOfficeResignationPanel.render(m,_sidebar_section(m,stage,"PoliticalOfficeResignationArea","本人辞任办理"),kernel.political_office_resignations(current_place,int(m.get_meta("political_office_resignation_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_subordinate_appointments_enabled") and kernel.political_subordinate_appointments_enabled():
		PoliticalSubordinateAppointmentPanel.render(m,_sidebar_section(m,stage,"PoliticalSubordinateAppointmentArea","下属空缺补任"),kernel.political_subordinate_appointments(current_place,int(m.get_meta("political_subordinate_appointment_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_governance_agencies_enabled") and kernel.political_governance_agencies_enabled():
		PoliticalGovernanceAgencyPanel.render(m,_sidebar_section(m,stage,"PoliticalGovernanceAgencyArea","治理代理签受"),kernel.political_governance_agencies(current_place,int(m.get_meta("political_governance_agency_cursor",0))),current_place)
	if not current_place.is_empty() and kernel.has_method("political_successions_enabled") and kernel.political_successions_enabled():
		PoliticalSuccessionPanel.render(m,_sidebar_section(m,stage,"PoliticalSuccessionArea","原首职正式继位"),kernel.political_successions(current_place,int(m.get_meta("political_succession_cursor",0))),current_place)
