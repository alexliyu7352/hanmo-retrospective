#!/usr/bin/env python3
"""唯一的架构及发行验证清单；独立保存实际执行结果，不把导出当作玩家验收。"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET

try:
    from tools import build
    from tools.verification_report import (catalog_identity, file_set_identity, report_exit_code,
                                           runtime_inputs_identity, schema_identity, sha256_file, source_identity, write_report)
except ModuleNotFoundError:
    import build
    from verification_report import (catalog_identity, file_set_identity, report_exit_code,
                                     runtime_inputs_identity, schema_identity, sha256_file, source_identity, write_report)

ROOT = Path(__file__).resolve().parents[1]
ENGINE_BUILD = '4.7.2.stable.official.ed1daf0bf'
COMMENT_BASELINE = '6baae04'
NATIVE_PROFILES = {'native-growth', 'native-travel'}
PROFILES = {'linux', 'web', 'architecture', 'release'} | NATIVE_PROFILES
WEB_STAGES = {'source_web_static', 'node_version', 'node_storage', 'python_web_storage'}
WEB_DEFERRED_REASON = '用户要求先完成并发布 Linux 游戏，再开展 HTML/Web 发行；本平台门延期且未执行'
# 恢复的独立协议、控件与纯规则门沿原默认入口运行；专用银行/安装输入仍明确隔离。
TESTS = ['test_political_dismissal_sources', 'test_political_subordinate_appointment_panel', 'test_political_office_resignation_authoring', 'test_political_office_resignation_registration', 'test_political_office_resignation_response_registration', 'test_political_office_resignation_responses', 'test_political_office_resignation_panel', 'test_family_voluntary_paths_screen', 'test_task_message_returns_catalog', 'test_task_message_returns_panel_guard', 'test_task_message_return_task_hooks', 'test_task_message_returns_registration', 'test_task_personal_messages_catalog', 'test_task_personal_messages_panel_guard', 'test_task_personal_messages_registration', 'test_task_personal_reports_catalog', 'test_task_personal_reports_panel_guard', 'test_task_personal_reports_registration', 'test_task_personal_acceptances_catalog', 'test_task_personal_acceptances_panel_guard', 'test_task_personal_acceptances_registration', 'test_task_personal_cancellations_catalog', 'test_task_personal_cancellations_panel_guard', 'test_task_personal_cancellations_registration', 'test_political_governance_agency_panel', 'test_political_office_reception_panel', 'test_political_term_handover_panel', 'test_military_siege_engineering', 'test_political_successor_round_panel', 'test_political_head_handover_panel', 'test_political_succession_panel', 'test_military_field_terrain', "test_military_tactic_learning", 'test_tactic_lesson_screen', "test_military_command_resources", 'test_military_sequential', 'test_private_shop_daily_rent_panel_guard', 'test_governance_invitation_reply_panel', 'test_military_commanded_damage', 'test_military_combat_conditions', 'test_military_garrison_rest', 'test_military_command_shock', 'test_apprenticeship_screen', 'test_military_rout_retreat', 'test_known_contact_location_panel', 'test_private_shop_liquidations_panel_guard', 'test_political_governance_continuation_panel', 'test_military_supply_attrition', 'test_military_wounded_recovery', 'test_political_shop_registry_panel', 'test_private_shop_panel_guard', 'test_task_shop_orders_panel_guard', 'test_political_shop_licensing_panel', 'test_military_naval_gunnery', 'test_child_care_relief_screen', 'test_military_naval_tactical_navigation', 'test_military_naval_navigation', 'test_military_naval_repairs', 'test_family_birth_screen', 'test_party_recruitment_panel_guard', 'test_visit_message_panel_guard', 'test_supply_policy_panel_guard','test_political_supply_choice_panel', 'test_task_employment_panel_guard', 'test_task_static_asset_panel_guard', 'test_task_extension_panel_guard', 'test_personal_appointment_panel_guard', 'test_contact_inquiries_panel_guard', 'test_visit_message_replies_panel_guard', 'test_visit_appointments_panel_guard', 'test_born_personal_dialogues_panel_guard', 'test_known_personal_dialogues_panel_guard', 'test_family_marriage_screen', 'test_political_military_entry', 'test_political_field_appointment_panel', 'test_fiscal_taxpayer_continuity_panel', 'test_family_inheritance_screen', 'test_occupation_fiscal_panel', 'test_family_bequest_screen', 'test_family_legacy_screen', 'test_family_death_screen', 'test_military_naval_battle_screen', 'test_military_naval_panel','test_study_teaching_navigation', 'test_study_teaching_screen', 'test_party_participation_panel', 'test_insight_discovery_profile', 'test_insight_cards_controller', 'test_insight_cards_screen', 'test_diplomacy_entry_lifecycle', 'test_diplomacy_page_readiness', 'test_world_map_attached_page', 'test_world_map_label_levels', 'test_world_map_route_context', 'test_growth_cards_practice', 'test_world_map_scene_backend', 'test_politics_entry_lifecycle', 'test_politics_page_readiness', 'test_fiscal_settlement_panel', 'test_world_map_company', 'test_world_map_local_passages', 'test_growth_cards_late_callbacks', 'test_economic_freed_callbacks', 'test_interaction_tasks_freed', 'test_world_map_released_callbacks', 'test_world_map_crossings', 'test_world_map_following_context', 'test_world_map_transport', 'test_world_map_transport_pointer', 'test_growth_cards_screen', 'test_card_discovery_profile', 'test_growth_cards_controller', 'test_economic_world_panel', 'test_economic_production_context', 'test_interaction_tasks_panel', 'test_interaction_tasks_prepare', 'test_world_map_presence', 'test_world_map_presence_pointer', 'test_world_map_search_session', 'test_world_map_places', 'test_world_map_region_registry', 'test_world_map_symbol_scope', 'test_world_map_region_frame', 'test_adult_blade_pose_player', 'test_estate_overview', 'test_world_map_scene', 'test_adult_sword_full_arm_sample', 'test_adult_sword_body_rig', 'test_adult_sword_body_layers',
    'test_activity_decision_guidance',
    'test_alpha_letter_escort_continuity',
    'test_character_appearance_adapter',
    'test_city_building_parts',
    'test_city_building_states',
    'test_city_layout_editor',
    'test_city_layout_system',
    'test_city_road_nodes',
    'test_city_shared_views',
    'test_city_showcase',
    'test_military_battle_draft_rules',
    'test_military_preview_feedback',
    'test_military_session_isolation',
    'test_native_resident_backend',
    'test_native_resident_page_reads',
    'test_player_plan_controls',
    'test_qingchuan_grain_scene',
    'test_regional_appearance_composition',
    'test_regional_appearance_picker_recovery',
    'test_regional_appearance_sources',
    'test_resident_journey_adapter',
    'test_resident_journey_presenter',
    'test_resident_main_routing',
    'test_resident_map_affordances',
    'test_resident_native_storage',
    'test_resident_scene_readiness',
    'test_resident_slot_codec',
    'test_self_profile_adapter',
    'test_self_profile_main_edges',
    'test_self_profile_resident_reads',
    'test_self_profile_screen',
    'test_self_profile_skills',
    'test_personal_playback_input', 'test_personal_playback', 'test_personal_playback_canvas', 'test_native_author_preview_panel', 'test_skill_study_segments', 'test_activity_flow_policy', 'test_activity_views', 'test_personal_tactical_projection', 'test_personal_npc_withdrawal', 'test_personal_withdrawal_history', 'test_personal_stamina_round', 'test_personal_strike_conditions', 'test_personal_area_strike', 'test_personal_guard_focus', 'test_personal_escort_exit', 'test_personal_escort_exit_kernel', 'test_personal_ai_scoring', 'test_personal_initial_stamina', 'test_drill_challenge', 'test_training_drill', 'test_military_training_drill', 'test_military_training', 'test_military_condition_history', 'test_military_march_conditions', 'test_military_training_supply', 'test_military_training_occupation', 'test_personal_armor_inventory', 'test_personal_armor_battle', 'test_bargain_challenge', 'test_military_damage_cover', 'test_military_damage_sources', 'test_military_wall_bridge', 'test_personal_hit_entry', 'test_personal_hit_kernel', 'test_personal_hit_leaf', 'test_personal_hit_round', 'test_siege_engine_use', 'test_siege_fortification_context', 'test_study_bargain', 'test_trade_bargain', 'test_dynamic_actor_schedule', 'test_time_rules', 'test_registered_state', 'test_world_phase_graph', 'test_action_lifecycle', 'test_module_registry', 'test_builtin_command_contract', 'test_builtin_phase_contract', 'test_registered_core_commands', 'test_registered_knowledge', 'test_registered_movement', 'test_registered_dialogue', 'test_module_coverage', 'test_compat_action_contracts', 'test_registered_parties', 'test_registered_encounters', 'test_registered_cultivation', 'test_registered_family', 'test_registered_estates', 'test_registered_personal', 'test_registered_personal_contracts', 'test_registered_personal_early_guards', 'test_journey_registration', 'test_registered_rest', 'test_early_gate_integration', 'test_registered_trade_contracts', 'test_registered_trade', 'test_registered_tasks', 'test_registered_military_contracts', 'test_registered_military', 'test_second_military_lifecycle', 'test_registered_campaign', 'test_registered_military_combat', 'test_registered_siege', 'test_registered_siege_join', 'test_siege_npc_result_guard', 'test_capability_contract', 'test_event_policy', 'test_task_policy', 'test_catalog', 'test_foundation', 'test_rules', 'test_storage', 'test_save_v2_decoding', 'test_scenario_menu', 'test_save_v2_encoding', 'test_scenario_active_identity', 'test_scenario_calendar', 'test_storage_metadata', 'test_storage_callback_epoch', 'test_slot_file_metadata', 'test_storage_slot_inspection', 'test_scenario_factory_contract', 'test_scenario_kernel_startup', 'test_scenario_initial_goods', 'test_scenario_snapshot', 'test_pack_set_restore', 'test_scene',
         'test_playable', 'test_life_services', 'test_lived_scene', 'test_editor_client',
         'test_editor_panel', 'test_editor_lived', 'test_editor_failure', 'test_f05_art', 'test_f05_playable', 'test_f05_notes', 'test_presentation_registry', 'test_layered_presentation', 'test_feature_bindings', 'test_character_presentation', 'test_character_visible_projection', 'test_character_input_callers', 'test_person_token_hidden_setup', 'test_trade_sheet_view', 'test_character_summary', 'test_bargain_source_view', 'test_family_estate_views', 'test_military_camp_views', 'test_character_presentation_edges', 'test_character_battle_identity', 'test_character_screen_integration', 'test_transaction_observers', 'test_observer_integration', 'test_explanation_visibility', 'test_kernel_explanation', 'test_scene_projection', 'test_scene_binding_contract', 'test_scene_presentation', 'test_appearance_picker', 'test_editor_appearance', 'test_character_auxiliary_screens', 'test_scene_screen_integration', 'test_scene_draw_order', 'test_visual_entrypoints', 'test_ui_font_scale', 'test_editor_presentation', 'test_travel_control', 'test_route_graph', 'test_world_step', 'test_road_conditions', 'test_journey_migration', 'test_world_map', 'test_auto_travel', 'test_map_geometry', 'test_editor_roads', 'test_parties', 'test_road_contacts', 'test_encounters', 'test_mobility_migration', 'test_mobility_integrity', 'test_companion_scene', 'test_editor_companions', 'test_companion_reuse',
         'test_editor_structured', 'test_personal_simultaneous_outcomes', 'test_personal_geometry', 'test_personal_content', 'test_personal_round', 'test_personal_directional_guard', 'test_personal_attack_commit', 'test_personal_withdrawal_rules', 'test_personal_escort_withdrawal', 'test_personal_ai_survival', 'test_personal_timeout_outcomes', 'test_military_archer_contact', 'test_military_march', 'test_military_flank', 'test_military_feint', 'test_military_suppress', 'test_personal_stance_contract', 'test_personal_condition_snapshot', 'test_personal_counter', 'test_personal_facing_damage', 'test_personal_qualified_snapshot', 'test_military_support', 'test_combat_rule_versions', 'test_cultivation_rules', 'test_cultivation_rule_versions', 'test_skill_study', 'test_skill_study_query', 'test_ordinary_skills_view', 'test_ordinary_skills_screen', 'test_skill_study_cancel', 'test_skill_study_progress', 'test_skill_eligibility', 'test_etiquette_challenge', 'test_study_etiquette', 'test_personal_world', 'test_escort_world',
         'test_social_rules', 'test_social_world', 'test_actor_identity', 'test_actor_appearance', 'test_actor_appearance_world', 'test_identity_world', 'test_growth_screen', 'test_personal_battle_screen', 'test_world_terrain', 'test_terrain_journey', 'test_terrain_waypoint_types', 'test_terrain_contact', 'test_military_hex', 'test_military_round', 'test_estate_rules', 'test_family_rules', 'test_world_journeys', 'test_world_life_screen', 'test_military_battle_screen', 'test_military_assets', 'test_long_rest', 'test_family_screen', 'test_family_world', 'test_military_content', 'test_military_combat', 'test_military_receipt_history', 'test_campaign_rules', 'test_siege_rules', 'test_siege_node_eligibility', 'test_editor_local_references', 'test_military_campaign_world',
         'test_campaign_return', 'test_garrison_ownership', 'test_military_contact',
         'test_military_expiry_world', 'test_military_world', 'test_retainer_world',
         'test_siege_world', 'test_siege_screen', 'test_siege_timeout_return', 'test_alpha_entry',
         'test_trade_inventory_screen', 'test_military_departure_screen', 'test_auto_travel_boundaries',
         'test_military_logistics', 'test_military_logistics_screen', 'test_architecture_replay', 'test_architecture_save_regression']
TESTS += ['test_actor_unlock_profile', 'test_actor_unlock_cards', 'test_actor_card_registration', 'test_actor_unlock_envoy_profile']

TESTS += ['test_actor_unlock_start_gate']

EXCLUDED_TESTS = {
    'test_personal_appointment_attendance_catalog': '须正规1.1作者索引和匹配schema；仅核目录与三producer身份，不裸跑或替代真实NPC赴约。',
    'test_visit_appointment_attendance_catalog': '须正规书面赴约1.1作者索引和匹配schema；只核原Snapshot与三producer身份，不冒实际NPC赴约。',
    'test_visit_message_receipts_catalog': '须正规拜帖1.2作者索引与匹配schema；只核原Snapshot与独立回执声明，不冒历史裁剪或实际NPC运行。',
    'test_task_shop_order_contacts_catalog': '须正规私店接触作者与同代schema；只验原Installed显式来源，不冒实际成交或历史生命验收。',
    'test_visit_reception_catalog': '需正规1.11完整作者索引与匹配Schema两参数；仅核Snapshot/native_request身份和目录保留，实际NPC原生门另行同代运行，不裸跑或冒World。',
    'test_world_map_obligations_main': '需正式Installed同源舞台及S3真实受领bank；只读本人期限行与来源，不裸跑或代GUI。',
    'test_world_map_travel_slots_main': '需正式关渡世界、独立XDG与新证据目录，produce/consume两进程验证原manual-1在途继续；不可裸跑。',
    'test_party_participation_main': '正式本人入离队Main helper；必须指定mode、同源真实bank/page与独立output，沿原控件和冷恢复，不裸跑或造初态。',
    'test_diplomatic_response_player_flow': '正式NPC外交答复Main helper；须显式diplomatic_response模块、新prepared目录与真实城吏，参数(main, evidence_path)，不裸跑或代替World冷验证。',
    'test_insight_card_catalog': '须显式传入正式清溪1.2与见闻1.0两包compose索引和同源Schema两个参数；只核Catalog/Snapshot/native_request及旧profile缺席，不创建World或代替实际四课训练。',
    'test_insight_cards_main': '须同源见闻两包正式stage与独占XDG；produce消费Native实际四课完成bank，consume另进程从真实manual-1冷读新人生；不裸跑或注入经历。',
    'test_diplomacy_player_flow': '正式外交Main helper；须diplomacy/envoy_tasks模块、prepared目录与真实城吏，参数(main, evidence_path)，不裸跑。',
    'test_teaching_request_catalog': '须显式传入正式七包邀学compose索引和同源Schema两个参数；只核Catalog/Snapshot/native_request字段及旧表缺席，不创建World或代替实际师生教学。',
    'test_actor_card_main': '需正式六包人物卡stage、同源新ABI扩展与独占XDG；produce带actor-card-bank/proof/evidence三个实源路径，consume在新进程用同evidence读真实manual-1；原生两采购先成功，不裸跑或注入World。',
    'test_task_responses_main': '须显式传入新回应profile实际bank、期望本人投影与输出目录；仅匹配正式stage可运行，不能裸跑。',
    'test_governance_player_flow': '治理正式情景Main helper；须新prepared目录、三治理模块及真实城吏，参数(main, evidence_path)，不裸跑或冒GUI。',
    'test_actor_card_catalog': '须显式传入正式五包人物卡compose目录索引和匹配Schema两个参数；仅Catalog/Snapshot/native_request准入，不代替安装信任链或真实World两采购。',
    'test_actor_unlock_native_observation': '须显式传入同源C++实际投影DTO原件一个路径参数；脚本自建独占跨局profile，这是跨语言组件消费，不能冒正式World两次采购完成。',
    'test_actor_unlock_start': '须显式传入正式三人作者catalog、同源schema和人物卡作者片段；仅检验原选择控件与独立资格文件，不替代完整World采购解锁。',
    'test_growth_cards_practice_main': '需正式卡片组合stage、原生同源库和全新独占manual-1；producer/新进程consumer分别运行，真实学习修炼配置与物理槽，禁止覆盖旧槽或裸聚合。',
    'test_existing_actor_roster_main': '需真实installed三人roster情景与匹配Main，核标题名单/外貌选择取消/第三人真实建局；不能裸跑或沿用旧两人夹具。',
    'test_world_map_crossings_main': '需正式installed crossings作者样本与同源库；实际地形选点报价/行旅遇人自然停住，不裸跑、不以check-only冒运行。',

    'test_world_map_local_passages_main': '须同源正式native stage与真实当前场所候选/原报价；只验无模板通路一次MoveLocal和旧来源拒绝，不裸跑或代GUI。',
    'test_world_map_cargo_main': '需正式native-resident stage、同源受领采购bank及实际页面预期输入；只验Main候选恢复/采用后的非空携物与纯读，不裸跑默认聚合，UI槽与GUI另列。',
    'test_growth_cards_main': '须原native_resident_profile正式冻结stage与两包卡片情景、同源SO/receipt及初次资源导入；外部脚本复用唯一stage、独立图鉴路径，真实Main学习收藏配置，不裸跑或称GUI。',
    'test_world_map_company_main': '须原清溪正式native stage/原Installed情景，只验本人军团空态与准入，不裸跑或冒GUI。',
    'test_world_map_company_formed_main': '须原清溪正式native stage，真实受任/军令/原单位编组；产生游戏bank，独立存储，不裸跑。',
    'test_world_map_company_travel_main': '须原清溪真实编组bank及匹配stage，原Main采用→军务→原军令行军，不重演编组或冒完整军战。',
    'test_economic_management_main': '须正式原生管理profile、已安装作者包及原Main；只验生产配置/撤回/财政读取，不裸跑或冒GUI与槽存读。',
    'test_political_career_player_flow': '政治正式情景原生helper；须新prepared目录、真实Office/Grant与冷恢复，不裸跑或代旧矩阵',
    'test_growth_cards_catalog': '显式传入真实组合作者索引与同源schema两参数；仅GD目录准入与旧表缺失语义，正式安装信任链和Main另用同源stage验证，不裸跑默认聚合。',
    'test_economic_world_navigation': '需显式传入生产经济DTO，验证原dynamic页切换与来源守卫；保持同源fixture身份，不裸跑默认空参数。',
    'test_world_map_transport_codec': '需显式S3生产codec真实DTO样本；逐行验证task/lot/quantity/保留状态，不用界面自造fixture冒原生输入。',
    'test_world_map_presence_main': '需原prepare冻结源、原包、实际SO、隔离storage的正常Main；原资源闭包恢复后同stage执行，不裸跑或回退规则。',
    'test_economic_world_configuration': '显式传入同源原生管理DTO文件，实际SpinBox/preview/确认链不能以空夹具替代；绑定DTO/生产叶身份单列执行。',
    'test_extended_skill_author_contract': '显式传入当前作者索引与内容schema两参数；验证16技能/开局名单作者及原生请求合同，须绑定输入SHA，不裸跑默认聚合。',
    'test_map_traffic_native': '须标准prepare_profile冻结本线源码、native包与同源SO以及独占storage；真实Main地图搜索/旅程中断恢复，不裸跑或回退GD。',
    'test_configurable_actor_roster': '专用真实快照/名单picker门，需两个显式作者正文与中央Schema路径；仅控件/源不变/旧回调与新旧政策局部，不能裸跑或代替新游戏及银行验收。',
    'test_shared_world_foundation_schema': '专用纯Schema门，需显式当前或指定历史Schema绝对路径；固定引擎真实Shape/Strict读取，8项准入/旧新互斥/typed引用/字典上限，不是世界或作者包全体验。',
    'test_world_map_pointer_dispatch': '专用headless Viewport标准输入分发，需明确--input真实1.10有限render输入、可选--canvas-source修复叶；6项滚轮/点击/拖动/键盘证据分列，不是OS或GUI，不裸跑通用矩阵。',

    'test_world_map_native_region': '专用当前Main和新1.10区域取景包，须精确ec891cc源、4adf5b8e安装代及frozen165扩展；19项纯读地图门不代旧旅程或GUI，原件evidence/world-map-line/native110-map-receipt.json。',
    'test_adult_blade_pose_alpha_main': '专用标准Alpha标题/武馆/真实一轮与有限回放，须独立完整安装stage和显式输出；旧faba正确回退的25/2原红保留，新表现锁待后继，不作为裸脚本矩阵。',

    'test_adult_blade_pose_binding': '专用真实Registry与8源PNG/原Alpha traveller，需显式--fragment及隔离导入工程；45项证据在verification/adult-blade-binding-v1/fixture-green，人工公开片段不代Main/GUI。',
    'test_world_map_native_19': '专用当前Main与local_society1.9严格compose/profile及frozen165真实扩展；需独占stage/存储与显式选择1.9，不能裸跑或回退GD。验证正常待选择相逢，不自动代答；原件见evidence/world-map-line/native19-main-first.log。',

    'test_estate_overview_main': '专用原Main._perform/Kernel/短行动门，需完整Alpha catalog与实际庄务作者及独占headless准备；不依当前所选native包裸跑。证据见verification/residence-environments-20261008/ui-main-second.log，不代GUI或存读。',
    'test_world_map_main_current': '专用当前Main+标准Alpha GD代表链，临时关闭resident且设置独占storage_profile，需完整alpha_life.hmcatalog与原资源；证据见evidence/world-map-line/main-current-fixed.log，不代native1.9或GUI。',

    'test_accepted_adult_presentation': '专用test_accepted_adult_presentation.gd入口需唯一候选presentation JSON绝对路径及其原十二PNG，并在输入旁写报告；不能用通用profile/content参数替代候选输入',
    'test_adult_appearance_compositions': '专用test_adult_appearance_compositions.gd入口需独立工坊192组合RGBA参照JSON，可附真实installed catalog；会建立/破坏/还原安装资源负例，须隔离工程；现有verification/formal-appearance-runtime-recovery-20261007保留实际运行',
    'test_existing_actor_start_picker': '专用test_existing_actor_start_picker.gd入口严格接收作者正文、中央schema、完整presentation三个路径；需对应原PNG，不接受通用profile/content参数',
    'test_installed_adult_appearance': '专用test_installed_adult_appearance.gd入口需正式compose→stage的catalog及prepared/receipt两输出路径；实际安装读者见verification/formal-pack-entry-recovered-20261007/installed-reader-run.json',
    'test_military_preview_joint': 'tools/military_preview_parity/prepare.py准备正式隔离军旅stage，test_military_preview_joint.gd以--input/--output及可选--main运行，再由compare.py核同源GD/C++；必须匹配冻结Bridge与精确输入，不能裸跑或代替完整军阵验收',
    'test_military_session_main': 'tools/military_session_node/run_node.py提供真实Main绑定JSON、输出目录、原作者接敌输入及固定native165；脚本严格两位置参数并含冷进程/物理银行故障，不能共享工程裸跑',
    'test_qingchuan_region_player_flow': 'RefCounted验证助手，无SceneTree独立入口；由tools/player_life_plan_main_node/qingchuan_region_main.gd及run_node.py驱动，需原青川1.6银行、冻结stage和实际Main，不能作为--script裸跑',
    'test_resident_main_cache_epoch': '继承test_resident_main_title_restore.gd；专用入口需--evidence下已产生的title-manifest/原银行及匹配profile、冻结Bridge，限定同世界只读恢复；不可新造银行替代',
    'test_resident_main_encounter': '专用test_resident_main_encounter.gd入口需--fixture/--workspace/--evidence与真实resident扩展、原会面存档；只冷恢复指定世界，不可用通用lived_city输入替代',
    'test_resident_main_journey': '专用test_resident_main_journey.gd入口需匹配冻结resident Bridge与正式Alpha独立stage，真实Main/GD对照推进一半日并写读新银行；现有resident-main-checkpoint专门门不由通用legacy工程裸跑',
    'test_resident_main_preview': '专用test_resident_main_preview.gd入口需匹配冻结resident Bridge与正式Alpha独立stage；核真实异步报价取消/零推进，通用legacy工程无该扩展准入，不可冒充GUI验收',
    'test_resident_main_title_restore': '专用test_resident_main_title_restore.gd入口需独立user根、--title-phase=produce/consume和--evidence两进程，匹配Bridge及installed Alpha；实际标题银行不可共享profile裸跑',
    'test_resident_slot_roundtrip': '专用test_resident_slot_roundtrip.gd入口严格接收prepared manifest与全新evidence根两个参数，需匹配Bridge/原catalog/初态/命令；真实checkpoint与物理槽推进一半日，不可裸跑',

    'test_personal_playback_main': '专用真实Main回放节点入口；需显式source-freeze、原planning银行副本及固定native165，独立验证零写/冷恢复，不裸跑或冒称GUI通过',
    'test_named_membership_player_flow': '具名任职联合节点helper；须正式内容、真实native Main和菜单银行独立继续，不裸跑或代旧矩阵',
    'test_regional_life_player_flow': '地方生计与事实对白联合Main helper；需正式1.3及同源原生，含私有钱货/真实入口/物理存读，不裸跑或伪造工作成果',
    'test_siege_integration_player_flow': '协商整编联合Main helper；需正式围城内容及同源原生，含逐将决议、实物交接与物理存读，不裸跑或伪造接受',
    'test_population_military_player_flow': '预备人口军籍联合Main helper；需正式1.2作者与同源原生，核登记/原军旅/退籍/当前物理存读，不裸跑或伪造战损',
    'test_dynamic_obligation_player_flow': '组织履约和真实路遇货损联合Main helper；使用正式后继内容与同源原生，不裸跑或伪造战果',
    'test_dynamic_world_player_flow': '原生动态世界联合节点helper；须正式两地内容、真实native Main和独立进程银行，不裸跑或作为GD规则oracle',
    'test_personal_round_report': '需正式Alpha资源与个人view夹具；--actual-only走原Main/Kernel开战到返回，默认门核简报键鼠、焦点、字号；不裸跑或代完整GUI验收',
    'test_armor_equipment_controls': '需armor_equipment_controls薄作者fixture，核成长页护具原命令/产权/预留/失效；独立受影响节点门，不裸跑或代全存读',
    'test_combat_armor_feedback': '需原个人战有限view fixture与护具战斗源，核本人冻结反馈/可见性/布局及旧oracle窄归一；不裸跑或代真实GUI',
    'test_native_military_schedule': '需独立native_military_schedule真实作者、旧提案oracle及冻结军政Bridge；只在专门有限提案与候选回滚门运行，不裸跑或代战争/存读全量',
    'test_native_travel_profile': '由verify --profile native-travel在唯一显式完整stage及正常GUI导入后运行；保默认关闭、不代旧231矩阵或全世界原生验收',
    'test_native_travel_schedule': '需native_travel_schedule/prepare_fixture.py原动态人物来源、冻结旧oracle与精确新Bridge；只核同义提案/半日候选原子性，不裸跑或代完整Director/存读',
    'test_native_travel_provider_adapter': '独立纯GD有限协议适配门，覆盖候选int64/scope/build及全批坏返；与travel专用fixture核来源，不共享裸跑或代真实native/GUI',
    'test_native_growth_profile': '由verify --profile native-growth在显式完整stage与GUI导入后运行代表门；默认不跑、不代全量/存读或自动冷headless导入',
    'test_live_native_growth_action': '默认不跑；verify --profile native-growth的--native-suite可显式选已有fixture。需live_native_growth_action/prepare_fixture.py与固定Bridge；开发成长页按需来源代次/失败缓存/真实GUI分证，默认OFF，不代完整M1或存档迁移',
    'test_native_actor_commitments_bridge': '默认不跑；verify --profile native-growth的--native-suite可显式选已有fixture。需native_actor_commitments_bridge/prepare_fixture.py及固定原生候选，复用20冻结工件和纯值oracle；显式profile/深转换隔离门，headless与真实GUI分证，默认OFF，不代在线后端或完整M1',
    'test_study_segments_player_flow': '逐课成长与手动晚答Main helper；需正式Qingstudy1.6及同源原生，含原题暂停和物理存读，不裸跑',
    'test_course_pause_player_flow': '统一节点RefCounted helper；需真实自动课程首半日后的同源Main/原Kernel，核暂停/移动拒绝/原师续学/完成；不裸跑或代全存读',
    'test_skill_study_pause': '普通课程受影响生命周期fixture，已由课程片完成必要规则检查；在合适节点显式选择，不因新文件自动重跑',
    'test_military_battle_condition_view': '默认以显式领域战斗边界核真实训练/行军来源；可追加已核完整世界作GD/native有限页精确比较，不代完整Main战役或像素验收',
    'test_siege_engine_player_flow': '统一节点RefCounted helper；需同源native Main/原Kernel真实未缩短围城前序，制造/选择/取消/真实半日反馈；不能裸跑或代像素验收',
    'test_kernel_siege_page': '需kernel_siege_page/prepare_fixture.py真实三军围城来源与固定旧Screen oracle；限定合法存活页同源前提/只读draft，不裸跑或代GUI/存读/整战争验收',
    'test_presentation_shell_routing': '需固定外壳路由/货单缓存oracle与独立Main夹具，围城正式来源另接；阶段检查不裸跑或冒称完整外壳/GUI/存读通过',
    'test_study_activity_input': '需study_activity_input/prepare_fixture.py正式学习/军阵/货单作者与固定旧Main身份oracle；只核有限来源、原票据guard/quote/晚序号，不裸跑或冒称全成长/存读',
    'test_military_battle_presentation_input': '需military_battle_presentation_input/prepare_fixture.py固定旧Screen/Canvas与正式tiny采购军事输入；只核有限页/完整draft/真实Main命令短链，不裸跑或代GUI/存读',
    'test_scene_journey_sidebar': '需scene_journey_sidebar/prepare_fixture.py显式三作者与--fixture/--oracle/--require-query；真实Main侧栏及原地图/话题边界，不能裸跑或冒称GUI/存读通过',
    'test_native_inventory_bridge': '默认不跑；verify --profile native-growth的--native-suite可显式选已有fixture。需native_inventory_bridge/prepare_fixture.py及精确本地原生库，先纯GD导入后隔离headless显式load；默认OFF，headless冷editor首自动发现134仍BLOCKED、图形流程另证，不共享裸跑或代正式game兼容',
    'test_kernel_personal_battle_view': '需kernel_personal_battle_view/prepare_fixture.py固定旧Screen/Canvas oracle及真实Alpha；仅有限页面/身份/几何按需门，不共享裸跑或代GUI/存读验收',
    'test_legacy_backend_queries': '需显式--content/--scenario正式ea9类输入与原448 --oracle、--backend-required；核15原查询同形及隔离，不裸跑或伪造未有题面成功实例',
    'test_kernel_action_context': '需kernel_action_context/prepare_fixture.py固定旧Main30时序oracle与显式控制边界替身；真实Kernel短链另列，不裸跑或冒称全部自动旅行/休整验收',
    'test_legacy_backend_coordination': '需legacy_backend_coordination/prepare_fixture.py原正式军旅及调度变体，显式--content/--schedule-content/--oracle与--backend-required；核同候选递归/尾协调，不裸跑或重完整存读',
    'test_kernel_hud_view': '需kernel_hud_view/prepare_fixture.py原HUD文字/控件与Personal.active oracle，核有限标量和原个人战hp上下文，不共享裸跑或代损坏世界GUI容错',
    'test_kernel_map_view': '需kernel_map_view/prepare_fixture.py正式tiny与固定旧Canvas绘图/输入oracle，核有限动态值与实际caller，不共享裸跑或代全GUI',
    'test_legacy_backend_lifecycle': '需显式--content/--scenario正式prepared输入与原7652 --oracle，--ownership-required核8公开委托/原三元组安装，不共享裸跑或扩Codec存读',
    'test_kernel_scene_view': '需kernel_scene_view/prepare_fixture.py正式tiny及原Host/两caller oracle，核有限projection和epoch/候选/人物回调，不共享裸跑或称全UI无snapshot',
    'test_kernel_journal_text': '需显式--content正式prepared目录及原ed27文字--oracle，核真实Main文本/知识边界和少专用snapshot，不共享裸跑或扩完整history/存读',
    'test_legacy_activity_projection': '需legacy_activity_projection/prepare_fixture.py既有正式军事tiny，核原行动/任职来源与只读隔离，不共享裸跑或称授权/完整NativeProjection',
    'test_legacy_asset_projection': '需legacy_asset_projection/prepare_fixture.py正式采购tiny三元组，核task产权/真实拆批/零预留和只读隔离，不共享裸跑或代NativeProjection/玩家授权',
    'test_legacy_backend_ownership': '需显式--content/--scenario正式prepared内容及从精确03旧Kernel首次生成的--oracle普通值证据，--ownership-required核真实事务/别名；不共享裸跑或扩Codec存读',
    'test_kernel_character_input': '需kernel_character_input/prepare_fixture.py正式tiny与旧conversation oracle，核真实caller少一次snapshot/单人输入隔离，不共享裸跑或代全UI验收',
    'test_m0_scene_command_context': '需m0_scene_context/prepare_fixture.py正式tiny与真实入树SceneHost/Layered候选；核原guard和context，不共享裸跑或代全GUI/存读验收',
    'test_siege_join_entry': '需siege_supply_sources/prepare_fixture.py的join-player与join-player-foreign正式作者变体；核真实navigation/Button和当前规则拒绝，不共享裸跑或代全GUI验收',
    'test_m0_projection_compatibility': '需m0_projection/prepare_fixture.py正式tiny与固定旧源码SHA临时oracle，核旧UI转发和原几何，不共享裸跑或代视觉验收',
    'test_siege_start_occupation': '需siege_supply_sources/prepare_fixture.py显式occupation作者变体；核真实开局/加入占用与失败原子性，不共享裸跑或扩整编',
    'test_personal_stamina_authored': '需personal_stamina_authored/prepare_fixture.py独立双版本compose/stage工程；只核旧四场初态等价与第五场round_v1真实入口，不共享裸跑或代替GUI验收',
    'test_m0_topic_command_context': '需M0正式tiny procurement隔离Main/Kernel工程；只核默认玩家与原topic epoch/revision/preview守卫，不共享裸跑或代GUI验收',
    'test_study_failure_messages': '需独立轻Main/UI脚本及原Study夹具（可用personal_load准备器）；只核真实离树说明和原资格，不共享裸跑或代GUI验收',
    'test_siege_supply_sources': '需siege_supply_sources/prepare_fixture.py正式tiny隔离Kernel工程；只核真实粮源/非粮/一次消费/尾回滚，不共享裸跑或扩大存读迁移',
    'test_personal_defeat_delay': '需personal_load正式tiny隔离Kernel工程；只核既定遭遇败北两半日travel hold/同行/世界继续，不裸跑或扩大存读迁移',
    'test_personal_result_consistency': '需personal_load/prepare_fixture.py正式tiny隔离工程；只核既定个人终态一致性，不共享裸跑或代替完整存读迁移',
    'test_world_command_context': '需正式tiny procurement prepare/stage隔离工程与Main节点；只核M0只读上下文/原提交守卫，不在共享目录裸跑或代替原生后端/GUI验收',
    'test_activity_kernel_views': '需正式军阵tiny安装锁与隔离profile的新控件/真实Kernel接缝；不在共享目录裸跑或当Main实窗验收',
    'test_personal_input_adapter': '需个人owner隔离小工程的真实Kernel与显示身份节点门；无图形/截图结论，不共享裸跑',
    'test_personal_load_draft': '需personal_load/prepare_fixture.py的正式compose/stage独立工程与隔离profile；验证真实Main读档、UI代次和完整世界不变，不裸跑或代替图形验收',
    'test_personal_models_kernel': '需skill_study_kernel/prepare_fixture.py --personal新锁；只核typed模型/一次同步/v2/公开预览，不裸跑或代完整个人战',
    'test_military_drill_kernel': '需skill_study_kernel/prepare_fixture.py --drill的真实新锁与隔离银行；军阵busy输入/首课系数/v2，不裸跑或代图形验收',
    'test_military_training_kernel': '需skill_study_kernel/prepare_fixture.py --military真实作者新锁；仅任官/供给/取消/v2与四课，不裸跑或替代军阵验收',
    'test_military_march_kernel': '需military_march_kernel/prepare_fixture.py的正式tiny隔离Kernel工程；只核显式地形行军来源/混合课程/接敌回滚和存读，不裸跑或代替旧道路/完整战斗模型',
    'test_armor_kernel': '需skill_study_kernel/prepare_fixture.py --armor正式作者初货/新锁；只验装备共享入口与真实售出，不裸跑',
    'test_trade_bargain_kernel': '需skill_study_kernel/prepare_fixture.py --trade正式新锁工程；只验报价/v2/真实成交尾拒与幂等，不在共享目录裸跑',
    'test_military_damage': '需--baseline-path指向逐SHA核实的真27e5旧源码14病例捕获；没有原件不能裸跑或另造oracle',
    'test_siege_safe_departure': '需siege_safe_departure/prepare_fixture.py的正式多配置隔离工程；仅真实退军/产权/回滚门，不在共享源目录裸跑',
    'test_siege_strategy_contract': '需siege_safe_departure/prepare_fixture.py的正式tiny隔离Kernel输入；只核战略字段拒绝/回放/冷恢复，不共享裸跑或代表完整整编',
    'test_siege_integration_contract': '需siege_safe_departure/prepare_fixture.py的正式tiny隔离Kernel工程；只读提议/未激活接受来源拒绝，不裸跑或代表整编执行',
    'test_siege_departure_allocation': '需同攻城prepare_fixture及精确allocation作者反例；只核有界全队分配，不扩大游戏规模或裸跑',
    'test_skill_study_kernel': '需skill_study_kernel/prepare_fixture.py正式tiny组合与隔离Kernel工程；仅课程全Schema/中途v2/取消事务，不在共享目录裸跑',
    'test_ordinary_skill_routes': '需ordinary_skill_routes/prepare_fixture.py、精确原1.2作者与原tiny图的独立compose/stage工程；只核十技艺课程/旧三章初态等价，不裸跑或代替完整普通技能玩法',
    'test_editor_dependency_context': '需独立Editor/worker工程和base/addon作者fixture；只核显式依赖字段/只读候选，不在共享工程裸跑或代集中作者体验',
    'test_scene_sidebar_layout': '需scene_sidebar_layout/prepare_fixture.py的正式隔离安装工程；侧栏尺寸/滚动及真实岗位命令已有定点证据，集中OS验收延期',
    'test_editor_scenario_snapshot': '需a25_snapshot_authoring/prepare_fixture.py的独立Editor/worker多入口工程；显式选择/预览/确认及undo/save可改测试作者，不能裸跑或代实际工坊输入',
    'test_pack_effective_adoption': '需a2_effective_adoption/prepare_fixture.py真实新模式目录及独立profile，以--mode=create/restore和--evidence两进程执行2tick正式收养/冷恢复；不可裸参数、直造fact或替代长期家族',
    'test_pack_appearance_identity': '需a2_locked_appearance/prepare_fixture.py正式compose/stage的双锁tiny分层工程；正常行旅/相遇/个人战和Native v2冷恢复有独立profile，只证节点身份合同，不可共享工程裸跑或代儿童美术验收',
    'test_editor_nullable_references': '需真实包Editor节点/worker独立工程与tiny作者fixture，含保存/重开；专门核Schema可空引用，不可共享工程裸跑或代OS作者输入',
    'test_pack_activity_restore': '需a2_activity_restore/prepare_fixture.py正式双包正逆compose/stage；以--mode=create/start/waiting/completed及--evidence独立目录四进程运行，含真实临时v2保存，不可裸参数或重复长矩阵',
    'test_editor_resource_import': '需author_resource_import/prepare_fixture.py生成精确Panel/client与实际项目原图的独立Editor工程；含真实worker写入/partial故障与专用profile，不可共享工程裸跑，非OS作者输入',
    'test_editor_packs': '需独立Editor节点工程及真实worker/pack fixture，正常保存/compose可写作者草稿；只专用节点门，不替代真实工坊输入',
    'test_flat_slot_compatibility': '需真实六canonical安装目录与平面旧采购目录的独立工程；仅分类微门，不得用合成sidecar冒充或共享目录裸跑',
    'test_slot_selection_menu': '需slot_selection/prepare_fixture.py经正式compose/stage准备双包独立工程、profile与只读历史输入；选择/确认含真实文件写，不可共享工程裸跑',
    'test_title_catalog': '需独立namespaced真实安装工程和标题资源；只测返回标题节点/纹理/中文确认，不能在共享目录裸跑或替代OS复验',
    'test_installed_scenario_menu': '需独立工程及真实installed-content目录；测试UI节点选择与取消，不在共享工程裸跑或冒称OS验收',
    'test_multi_role_topics': '需interaction_topics/prepare_fixture.py正常准备legacy/multiple/explicit目录及对应Schema；单独受影响短门',
    'test_multi_role_menu': '需同双岗位正式prepare目录、真实UI资源与隔离profile；event-only分支单独验证，不冒称OS实际输入',
    'test_restore_menu': '需prepare_restore_menu_test.py从精确源码与已安装内容集准备独立工程和profile；真实UI信号/Native回调，不可在共享运行目录裸跑或当OS验收',
    'test_installed_content': '必须在真实compose→stage的独立Godot工程运行；测试会修改并原样还原该工程索引/PNG，禁止在玩家或共享生成目录裸跑，局部21项已验不代替UI继续验收',
    'test_vertical_slice_representatives': '需prepare_vertical_slice.py --representatives及HANMO_REAL_ART_PROJECT/HANMO_REPRESENTATIVE_OUTPUT，专用局部门只读CPU合成，非OS实际输入',
    'test_vertical_slice_age_boundary': '需prepare_vertical_slice.py --representatives及HANMO_REAL_ART_PROJECT/HANMO_AGE_OUTPUT；单次有界720wait且外层timeout240，不能裸参数或例行重复',
    'test_vertical_slice_layout': '需正式双配置prepare及HANMO_REAL_ART_PROJECT/HANMO_LAYOUT_OUTPUT，专用局部门验证只读布局；不可裸参数或冒充字号版本全窗口矩阵',
    'test_vertical_slice_real_art': '需prepare_vertical_slice双配置及HANMO_REAL_ART_PROJECT/OUTPUT，由专用局部门验证，不能裸参数或冒充原生视觉验收',
    'test_presentation_vertical_slice': '由tools/verify_presentation_slice.py单独准备双作者目录并跨进程存读；不可省略专用输入或把原生输入/视觉BLOCKED冒充PASS',
    'test_export_font': '导出阶段单独准备并验证内嵌字体，通用无图形门禁不宣称字体已验收',
    'test_family_succession_screen': '需要图形输入与跨进程继任存档验收',
    'test_localized_file_dialog': '需要图形窗口及中文文件对话框验收',
    'test_military_camp_screen': '需要图形输入及营务界面验收',
    'test_visual_shell': '需要图形渲染与截图审查',
    'test_dialog_lifecycle': '需要图形窗口与对话生命周期输入验收',
}


def validate_test_registry(game: Path, *, tests: list[str] | None = None, excluded: dict | None = None) -> dict:
    """每个真实测试脚本须唯一归入执行或明确排除清单；漏测和陈旧名称均阻止发布。"""
    tests = TESTS if tests is None else tests
    reasons = EXCLUDED_TESTS if excluded is None else excluded
    scripts = {path.stem for path in (game / 'tests').glob('test_*.gd')}
    registered = set(tests)
    excluded = set(reasons)
    problems = []
    if len(tests) != len(registered): problems.append('执行清单含重复项')
    if registered & excluded: problems.append('重复分类：' + ', '.join(sorted(registered & excluded)))
    if scripts - registered - excluded: problems.append('未分类：' + ', '.join(sorted(scripts - registered - excluded)))
    if (registered | excluded) - scripts: problems.append('脚本不存在：' + ', '.join(sorted((registered | excluded) - scripts)))
    if any(not reason.strip() for reason in reasons.values()): problems.append('排除项缺少原因')
    if problems: raise RuntimeError('发行测试清单无效：' + '；'.join(problems))
    return {test: {'result': 'NOT_RUN', 'reason': reason} for test, reason in reasons.items()}


def prepare_military_content(folder: Path) -> Path:
    """用共享测试作者夹具经过正式准备器生成目录，不能复用旧验证资料冒充当前内容。"""
    build.run_checked([sys.executable, '-m', 'tests.military_content_fixture', str(folder)],
                      log=folder / 'prepare.log')
    output = folder / 'military-world.hmcatalog.json'
    if not output.is_file(): raise RuntimeError('军事全流程测试缺少准备目录：' + str(output))
    return output


def test_arguments(test: str, *, military_content: Path | None = None, dynamic_content: Path | None = None, lifecycle_content: Path | None = None, scenario_content: Path | None = None, goods_folder: Path | None = None) -> list[str]:
    """每次测试使用独立槽，避免旧内容测试档阻断保存；旧场景仍明确回归原内容。"""
    if test in {'test_save_v2_decoding', 'test_scenario_menu', 'test_save_v2_encoding', 'test_scenario_active_identity', 'test_scenario_calendar', 'test_storage_metadata', 'test_storage_callback_epoch', 'test_slot_file_metadata', 'test_storage_slot_inspection', 'test_scenario_factory_contract', 'test_scenario_snapshot', 'test_pack_set_restore', 'test_alpha_entry', 'test_feature_bindings', 'test_character_presentation', 'test_character_visible_projection', 'test_character_input_callers', 'test_person_token_hidden_setup', 'test_trade_sheet_view', 'test_character_summary', 'test_bargain_source_view', 'test_family_estate_views', 'test_military_camp_views', 'test_character_presentation_edges', 'test_character_battle_identity', 'test_character_screen_integration', 'test_transaction_observers', 'test_observer_integration', 'test_explanation_visibility', 'test_kernel_explanation', 'test_scene_projection', 'test_scene_binding_contract', 'test_scene_presentation', 'test_appearance_picker', 'test_editor_appearance', 'test_character_auxiliary_screens', 'test_scene_screen_integration', 'test_scene_draw_order', 'test_visual_entrypoints'}: return []
    if test == 'test_scenario_kernel_startup':
        if scenario_content is None: raise RuntimeError('双年代新局门必须提供正式prepare的专用目录')
        return ['--', '--scenario-content=' + str(scenario_content)]
    if test == 'test_scenario_initial_goods':
        if goods_folder is None: raise RuntimeError('显式个人初货门必须提供两份正式prepare目录')
        return ['--', '--goods-content='+str(goods_folder/'goods-a.hmcatalog.json'), '--goods-content='+str(goods_folder/'goods-b.hmcatalog.json')]
    arguments = ["--", "--profile=tests-" + test.replace("_", "-") + '-' + uuid.uuid4().hex[:8],
                 "--content=res://data/generated/lived_city.hmcatalog.json",
                 "--scenario=scenario.lived_city"]
    if test == 'test_editor_local_references': arguments.append('--verify-world')
    if test in {'test_military_world', 'test_registered_military_contracts', 'test_registered_military', 'test_registered_campaign', 'test_journey_registration'}:
        if military_content is None: raise RuntimeError('军事全流程测试必须提供当前准备的作者目录')
        arguments.append('--military-content=' + str(military_content))
    if test == 'test_second_military_lifecycle':
        if lifecycle_content is None: raise RuntimeError('第二军事生命周期必须提供专用占城作者目录，不能复用军需delivery夹具')
        arguments.append('--military-content=' + str(lifecycle_content))
    if test in {'test_registered_personal', 'test_registered_personal_early_guards'}:
        arguments.append('--legacy-kernel=res://tests/fixtures/journey_94e1477/world_kernel.gd')
    if test in {'test_registered_core_commands', 'test_registered_knowledge', 'test_registered_movement', 'test_registered_dialogue'}:
        arguments.append('--legacy-kernel=res://tests/fixtures/core_5a02a9e/world_kernel.gd')
    if test == 'test_dynamic_actor_schedule':
        if dynamic_content is None: raise RuntimeError('动态调度测试必须提供当前准备的作者目录')
        arguments.append('--dynamic-content=' + str(dynamic_content))
    return arguments


def assertion_count(output: str) -> int:
    """拒绝缺失、空白或失败的断言汇总，即使进程退出码为零。"""
    matches = re.findall(r'CHECKS=(\d+)(?: COMPARED_COMMANDS=\d+)? FAILURES=(\d+)', output)
    if not matches or any(int(n) == 0 or int(f) != 0 for n, f in matches) or 'SCRIPT ERROR:' in output or '\nFAIL:' in output:
        raise RuntimeError('Missing or failed runtime assertion summary')
    return sum(int(n) for n, _ in matches)


def optional_stages() -> dict:
    """图形、作者操作与原生/Web 实际输入各自记录；无人执行时保留具体原因。"""
    reasons = {
        'graphics': '未运行图形渲染、中文显示与截图人工审查',
        'author_operations': '未执行实际作者界面新增、编辑、撤销、保存及重开操作',
        'native_actual_input': '未运行实际原生包鼠标/键盘输入与代表存档互通',
        'web_actual_input': '未运行实际 Web 游戏鼠标/键盘输入；Node/存储单测与导出不提供此证据',
        'native_export': '本 profile 不生成原生发行包，实际导出及产物摘要由发行入口另行记录',
        'web_export': '本 profile 不生成 Web 发行包，导出成功不证明 Web 实际输入通过',
    }
    return {name: {'status': 'NOT_RUN', 'required': False, 'reason': reason, 'scope': reason,
                   'command': [], 'exit_code': None, 'log': None, 'log_sha256': None,
                   'log_summary': ''} for name, reason in reasons.items()}


def stage_plan(root: Path, run: Path, engine: str, profile: str = 'architecture') -> dict:
    """单清单同时声明身份及平台门；共享与 Web pytest 精确分组，旧 profile 保持全平台必需。"""
    if profile not in PROFILES:
        raise ValueError('未知验证 profile：' + profile)
    if profile in NATIVE_PROFILES:
        raise ValueError('native-growth需显式staged-project，使用独立的限定阶段入口')
    python = sys.executable
    internal = [python, str(root / 'tools/verify.py'), '--root', str(root)]
    commands = {
        'engine_version': ([engine, '--version'], '固定引擎精确构建身份', 30),
        'node_version': (['node', '--version'], '实际 Node 版本', 30),
        'source_static': ([python, str(root / 'tools/check_source.py'), '--root', str(root),
                           '--output', str(run / 'source_check.json'), '--scope', 'shared'], '受控递归静态引用、Python 语法与 GDScript 职责注释；不执行 JavaScript', 180),
        'source_web_static': ([python, str(root / 'tools/check_source.py'), '--root', str(root),
                               '--output', str(run / 'source_web_check.json'), '--scope', 'web'],
                              'Web 必需文件与真实 Node JavaScript 语法', 180),
        'comments': ([python, str(root / 'tools/check_comments.py'), '--base', COMMENT_BASELINE], '固定中文注释基线 6baae04', 180),
        'presentation': (internal + ['--stage', 'presentation'], '表现清单、实际美术文件与作者表现引用', 180),
        'content_validation': (internal + ['--stage', 'content'], '所有当前 foundation 作者包经真实 prepare 校验和准备', 180),
        'architecture_dependencies': ([python, str(root / 'tools/check_architecture.py'), '--root', str(root)], '受信模块版本、状态归属及逐边核心依赖守卫；不宣称静态写屏障', 180),
        'runtime_registry': (internal + ['--stage', 'registry'], '所有 Godot 测试须唯一执行或明确独立排除', 180),
        'node_storage': (['node', '--test', str(root / 'tests/storage_format.test.cjs')], '实际 Node 存档封装协议；非 Web 游戏输入', 180),
        'scenario_goods_fixture': ([python, '-m', 'tests.scenario_content_fixture', str(run / 'scenario-goods'), '--goods'], '正式准备显式个人初货及逆序的两份目录', 180),
        'scenario_fixture': ([python, '-m', 'tests.scenario_content_fixture', str(run / 'scenario-fixtures')], '正式准备双年代有效世界短门目录；不复用旧默认剧本', 180),
        'dynamic_actor_fixture': ([python, '-m', 'tests.dynamic_actor_content_fixture', str(run / 'dynamic-fixtures')], '正式准备显式动态行旅与军政权限独立作者夹具', 180),
        'military_fixture': ([python, '-m', 'tests.military_content_fixture', str(run / 'fixtures')], '重新准备共享 Python 军事作者夹具', 180),
        'military_lifecycle_fixture': ([python, '-m', 'tests.military_content_fixture', str(run / 'lifecycle-fixtures'), '--lifecycle'], '正式准备第二军制占城/撤军完整链独立作者夹具，不复用军需投递输入', 180),
        'godot_import': ([engine, '--headless', '--path', str(root / 'game'), '--editor', '--quit'], '固定 Godot 导入与 GDScript 实际编译', 300),
        'python_pytest': ([python, '-m', 'pytest', 'tests', '-q', '--ignore=tests/test_web_storage.py', '--junitxml=' + str(run / 'python-pytest.xml')],
                          '共享/原生 pytest，含 tests/content、原生发行配置与军事跨语言入口；仅独立 Web 文件延期', 1200),
        'python_web_storage': ([python, '-m', 'pytest', 'tests/test_web_storage.py', '-q',
                                '--junitxml=' + str(run / 'python-web-storage.xml')],
                               '真实 Chromium IndexedDB 全部 30 个浏览器存储用例；非 Web 游戏输入', 1200),
        'military_cross_language': ([python, '-m', 'pytest',
                                    'tests/test_military_content.py::test_godot_uses_prepared_author_definitions',
                                    '-q', '--junitxml=' + str(run / 'military-cross-language.xml')],
                                   '共享 Python 生成有效/坏军事目录，再交真实 Godot 校验', 180),
    }
    fixture = run / 'fixtures/military-world.hmcatalog.json'
    for test in TESTS:
        commands['godot_' + test] = ([engine, '--headless', '--audio-driver', 'Dummy', '--path', str(root / 'game'),
                                     '--script', f'res://tests/{test}.gd'] + test_arguments(test, military_content=fixture, dynamic_content=run / 'dynamic-fixtures/dynamic-actor-fixtures.json', lifecycle_content=run / 'lifecycle-fixtures/military-lifecycle.hmcatalog.json', scenario_content=run / 'scenario-fixtures/two-eras.hmcatalog.json', goods_folder=run / 'scenario-goods'),
                                    '实际 Godot 脚本断言；不等同真实玩家操作', 600 if test == 'test_family_world' else 120)
    commands['source_unchanged'] = ([], '验证起止源树/Schema/作者包身份一致', 0)
    commands['runtime_inputs_unchanged'] = ([], '真实准备后至引擎结束，运行 catalog/Schema 与字体/许可输入字节一致', 0)
    return {name: {'command': command, 'scope': scope, 'timeout': timeout,
                   'required': not (profile == 'linux' and name in WEB_STAGES),
                   **({'deferred_reason': WEB_DEFERRED_REASON} if profile == 'linux' and name in WEB_STAGES else {})}
            for name, (command, scope, timeout) in commands.items()}


def run_stage(name: str, specification: dict, run: Path, blocked: str | None = None) -> dict:
    """沿用 run_checked 拒绝非零退出与 SCRIPT ERROR，失败仍保留独立日志和退出码。"""
    if specification.get('deferred_reason'):
        return {**specification, 'status': 'NOT_RUN', 'exit_code': None, 'log': None,
                'log_sha256': None, 'log_summary': '', 'reason': specification['deferred_reason']}
    log = run / (name + '.log')
    row = {**specification, 'status': 'BLOCKED' if blocked else 'NOT_RUN', 'exit_code': None,
           'log': str(log), 'log_sha256': None, 'log_summary': '', 'reason': blocked or ''}
    if blocked:
        log.write_text(blocked + '\n', encoding='utf-8')
    else:
        try:
            output = build.run_checked(specification['command'], timeout=specification['timeout'], log=log)
            if not log.exists():
                log.write_text(output, encoding='utf-8')
            row.update(status='PASS', exit_code=0)
            if name.startswith('godot_test_'):
                row['assertions'] = assertion_count(output)
                row['startup_unicode_messages'] = output.count('Unicode parsing error')
        except RuntimeError as exc:
            message = str(exc)
            match = re.search(r'Command failed with status (-?\d+):', message)
            row.update(status='FAIL', exit_code=int(match.group(1)) if match else 0 if 'Engine diagnostic detected:' in message else row['exit_code'],
                       reason=message)
            if isinstance(exc.__cause__, OSError):
                row['status'] = 'BLOCKED'
            if not log.exists():
                partial = ''
                if isinstance(exc.__cause__, subprocess.TimeoutExpired):
                    for value in [exc.__cause__.stdout, exc.__cause__.stderr]:
                        partial += value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
                log.write_text(partial + message + '\n', encoding='utf-8')
    output = log.read_text(encoding='utf-8', errors='replace')
    row['log_sha256'] = sha256_file(log)
    row['log_summary'] = output[-2000:]
    return row



def static_dependencies(row: dict, receipt: Path) -> None:
    """仅凭当前静态回执的明确无源错误/有缺依赖证据分类阻断；不能遮蔽真实失败。"""
    if row['status'] != 'FAIL' or row['exit_code'] != 2 or not receipt.is_file():
        return
    try:
        result = json.loads(receipt.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return
    if result.get('status') == 'BLOCKED' and not result.get('errors') and result.get('blocked'):
        row['status'] = 'BLOCKED'
        row['reason'] = '静态 JavaScript 必需依赖阻断：' + '; '.join(result['blocked'])


def pytest_dependencies(row: dict, junit: Path, engine_available: bool, engine: str) -> None:
    """仅逐条确认的依赖/权限失败转为 BLOCKED；未知失败、断言失败与漏测保留 FAIL。"""
    if row['status'] not in {'PASS', 'FAIL'} or not junit.is_file():
        return
    try:
        tree = ET.parse(junit)
    except (OSError, ET.ParseError):
        return
    blocked = []; failed = []; skipped = []
    for case in tree.iter('testcase'):
        name = case.get('classname', '') + '::' + case.get('name', '')
        if case.find('skipped') is not None:
            skipped.append(name)
        for error in list(case.findall('failure')) + list(case.findall('error')):
            text = error.get('message', '') + '\n' + (error.text or '')
            missing_engine = not engine_available and name in {
                'tests.test_military_content::test_godot_uses_prepared_author_definitions',
                'tests.test_storage_profiles::test_godot_forwards_release_profile_and_preserves_native_paths',
            } and ('Godot executable not found' in text or engine in text and 'FileNotFoundError' in text)
            browser_block = name.startswith('tests.test_web_storage::') and error.tag == 'error' and (
                'socket() failed: Operation not permitted' in text or "Executable doesn't exist at" in text)
            (blocked if missing_engine or browser_block else failed).append(name)
    row['blocked_cases'] = blocked; row['failed_cases'] = failed; row['skipped_cases'] = skipped
    if blocked and not failed:
        row['status'] = 'BLOCKED'
        row['reason'] = '实际 pytest 非零；以下用例被依赖/运行权限阻断：' + ', '.join(blocked)
    elif skipped and not blocked and not failed:
        row['status'] = 'NOT_RUN'; row['reason'] = '必需全量用例未执行：' + ', '.join(skipped)


def _native_note(run: Path, name: str, status: str, value: dict) -> dict:
    """纯身份检查也保留独立JSON日志，与已有阶段使用相同状态及摘要结构。"""
    log = run / (name + '.log')
    log.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {'command': [], 'scope': '显式开发工程来源检查', 'required': True,
            'status': status, 'exit_code': 0 if status == 'PASS' else None,
            'log': str(log), 'log_sha256': sha256_file(log),
            'log_summary': log.read_text()[-2000:], 'reason': value.get('reason', '')}


def _native_suite_plan(root: Path, engine: str, selected: list[str], oracle: Path | None) -> dict:
    """旧native矩阵仅显式选择现有fixture；固定脚本而不是执行输入中的任意命令。"""
    mapping = {'inventory': 'test_native_inventory_bridge', 'actor': 'test_native_actor_commitments_bridge',
               'live': 'test_live_native_growth_action'}
    plan = {}
    for selection in selected:
        name, separator, location = selection.partition('=')
        if not separator or name not in mapping or name in plan or not location:
            raise RuntimeError('--native-suite requires unique inventory|actor|live=prepared_game')
        game = Path(location).resolve(); script = game / 'tests' / (mapping[name] + '.gd')
        if not script.is_file() or script.read_bytes() != (root / 'game/tests' / script.name).read_bytes():
            raise RuntimeError('Native suite script is missing or does not match the selected source: ' + name)
        arguments = []
        if name == 'live':
            if oracle is None or not oracle.is_file():
                raise RuntimeError('live suite requires explicit --native-live-oracle')
            arguments = ['--', '--native', '--oracle=' + str(oracle.resolve())]
        plan[name] = {'command': [engine, '--headless', '--audio-driver', 'Dummy', '--path', str(game),
                                  '--script', 'res://tests/' + script.name] + arguments,
                      'scope': '显式复用已准备的旧native隔离套件：' + name, 'timeout': 300, 'required': True}
    if oracle is not None and 'live' not in plan:
        raise RuntimeError('--native-live-oracle requires the live suite')
    return plan


def _native_suite_inputs(specification: dict, oracle: Path | None = None) -> dict:
    """仅绑定显式选中的fixture运行文件与oracle；忽略引擎缓存，不访问其它工程。"""
    command = specification['command']; game = Path(command[command.index('--path') + 1]).absolute()
    if any(part.is_symlink() for part in (game, *game.parents)):
        raise RuntimeError('Native suite refuses aliased game path')
    paths = []
    for path in game.rglob('*'):
        relative = path.relative_to(game)
        if '.godot' in relative.parts:
            continue
        if path.is_symlink():
            raise RuntimeError('Native suite input symlink: ' + str(relative))
        if not path.is_file():
            continue
        if '__pycache__' in relative.parts and path.suffix in {'.pyc', '.pyo'}:
            continue
        paths.append(path)
    result = {'game': str(game.resolve()), 'files': file_set_identity(game, paths)}
    if oracle is not None:
        if any(part.is_symlink() for part in (oracle.absolute(), *oracle.absolute().parents)):
            raise RuntimeError('Native suite refuses aliased oracle')
        result['oracle'] = {'path': str(oracle.resolve()), 'sha256': sha256_file(oracle)}
    return result


def _native_import_block(game: Path, extension: str) -> str | None:
    """只检查实际已声明的导入前置缺失，不启动已知失败的冷headless editor。"""
    guide = '；请用固定Godot正常GUI打开 ' + str(game / 'project.godot') + ' 完成导入，headless冷editor134不自动尝试。'
    cache = game / '.godot'; classes = cache / 'global_script_class_cache.cfg'; listing = cache / 'extension_list.cfg'
    if not classes.is_file() or 'HMStrictJSON' not in classes.read_text():
        return '缺固定GD全局类索引' + guide
    if not listing.is_file() or extension not in listing.read_text():
        return '缺正式扩展自动发现记录' + guide
    for sidecar in game.rglob('*.import'):
        if '.godot' in sidecar.relative_to(game).parts:
            continue
        text = sidecar.read_text(encoding='utf-8')
        destinations = re.findall(r'"(res://\.godot/[^"\r\n]+)"', text)
        for destination in destinations:
            relative = Path(destination.removeprefix('res://'))
            if '..' in relative.parts or not (game / relative).is_file():
                return '缺资源导入目标：' + destination + guide
    return None


def _native_suite_assertions(name: str, output: str) -> int:
    """适配原三套件已有汇总文字，不修改其预期或以零退出替代断言通过。"""
    patterns = {'inventory': r'NATIVE_BRIDGE checks=(\d+) failures=(\d+) oracle=23',
                'actor': r'NATIVE_ACTOR_BRIDGE_CHECKS=(\d+) FAILURES=(\d+) COMPARED=60',
                'live': r'LIVE_NATIVE_GROWTH:\s*(\d+) checks,\s*(\d+) failures,\s*6 oracle rows'}
    matches = re.findall(patterns[name], output)
    if len(matches) != 1 or int(matches[0][0]) <= 0 or int(matches[0][1]) != 0:
        raise RuntimeError('Missing or failed native suite assertion summary: ' + name)
    return int(matches[0][0])


def run_native_growth_verification(root: Path, output: Path, engine: str | None,
                                   staged_project: Path | None, native_suites: list[str],
                                   native_live_oracle: Path | None, *, profile: str = 'native-growth') -> dict:
    """复用现有阶段执行与回执，只验证显式完整开发工程；不导入、不运行默认历史全套。"""
    try:
        from tools.native_growth_profile import validate_profile
    except ModuleNotFoundError:
        from native_growth_profile import validate_profile
    root = root.resolve(); output = output.resolve()
    if staged_project is not None and (output == staged_project.resolve() or staged_project.resolve() in output.parents):
        raise RuntimeError('native-growth evidence output must be outside the staged game tree')
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-' + uuid.uuid4().hex[:8]
    run = output / run_id; run.mkdir(parents=True, exist_ok=False)
    receipt = run / 'report.json'
    selected = engine or os.environ.get('GODOT_BIN') or os.environ.get('HANMO_GODOT_EDITOR') or ''
    resolved = shutil.which(selected) if selected else None
    engine_command = resolved or selected or '<未配置固定 Godot>'
    report = {'format': 'HM-VERIFY/1', 'profile': profile, 'run_id': run_id,
              'receipt': str(receipt), 'created_at': datetime.now(timezone.utc).isoformat(),
              'staged_project': str(staged_project.resolve()) if staged_project is not None else None,
              'engine_path': resolved or selected or None, 'engine_build': None, 'engine_sha256': None,
              'native_suites': list(native_suites), 'native_live_oracle': str(native_live_oracle.resolve()) if native_live_oracle else None,
              'artifacts': [], 'stages': {}, 'ok': False,
              'scope': '显式Linux开发工程，非linux/release全量或保存迁移验收'}
    prepared = None; initial_sha = None; blocked = None
    try:
        if staged_project is None:
            raise RuntimeError('native-growth requires --staged-project')
        prepared = validate_profile(root, staged_project, profile=profile)
        initial_sha = sha256_file(Path(prepared['manifest_path']))
        report.update(commit=prepared['manifest']['gd_source']['commit'], stage_manifest=prepared['manifest_path'],
                      stage_manifest_sha256=initial_sha)
        configuration = prepared['manifest']['configuration']
        report['project_configuration'] = {'stage_raw_sha256': configuration['project_sha256'],
                                           'semantic_sha256': configuration['project_semantic_sha256'],
                                           'actual_before_sha256': sha256_file(Path(prepared['game']) / 'project.godot')}
        report['stages']['native_stage_identity'] = _native_note(run, 'native_stage_identity', 'PASS',
                                                               {'manifest_sha256': initial_sha, **report['project_configuration']})
    except (RuntimeError, OSError, ValueError, KeyError, TypeError) as exc:
        blocked = str(exc)
        report['stages']['native_stage_identity'] = _native_note(run, 'native_stage_identity', 'BLOCKED', {'reason': blocked})
    version = run_stage('engine_version', {'command': [engine_command, '--version'], 'scope': '固定Linux引擎身份',
                                         'timeout': 30, 'required': True}, run, None if resolved else '缺固定Godot路径')
    if version['status'] == 'PASS':
        actual = Path(version['log']).read_text().strip()
        report.update(engine_build=actual, engine_sha256=sha256_file(Path(resolved)))
        expected = json.loads((root / 'native/dependencies.lock.json').read_text())['godot_bridge']['sdk']
        if actual != ENGINE_BUILD or report['engine_sha256'] != expected['engine_sha256']:
            version.update(status='BLOCKED', reason='固定Godot版本或字节不匹配')
    report['stages']['engine_version'] = version
    if version['status'] != 'PASS': blocked = blocked or version['reason']
    game = Path(prepared['game']) if prepared else (staged_project or run) / 'game'
    if prepared:
        blocked = blocked or _native_import_block(game, prepared['manifest']['native']['extension_path'])
    try:
        suites = _native_suite_plan(root, engine_command, native_suites, native_live_oracle)
        report['native_suite_inputs'] = {name: _native_suite_inputs(specification, native_live_oracle if name == 'live' else None)
                                         for name, specification in suites.items()}
    except (RuntimeError, OSError) as exc:
        blocked = blocked or str(exc)
        report['stages']['native_suite_selection'] = _native_note(run, 'native_suite_selection', 'FAIL', {'reason': str(exc)})
        suites = {}; report['native_suite_inputs'] = {}
    measurement = run / (profile + '-measurements.json')
    representative = 'test_' + profile.replace('-', '_') + '_profile'
    test_stage = 'godot_' + representative
    command = [engine_command, '--headless', '--audio-driver', 'Dummy', '--path', str(game),
               '--script', 'res://tests/' + representative + '.gd', '--',
               '--manifest=' + str(Path(prepared['manifest_path'])) if prepared else '--manifest=',
               '--output=' + str(measurement)]
    row = run_stage(test_stage, {'command': command, 'scope': '实际完整作者内容的首次／提交后／缓存读取',
                                                       'timeout': 300, 'required': True}, run, blocked)
    if row['status'] == 'PASS':
        if not measurement.is_file():
            row.update(status='FAIL', reason='代表内容门没有生成本次测量JSON')
        else:
            report['artifacts'].append({'path': str(measurement), 'bytes': measurement.stat().st_size, 'sha256': sha256_file(measurement)})
    report['stages'][test_stage] = row
    for name, specification in suites.items():
        row = run_stage('native_suite_' + name, specification, run, blocked)
        if row['status'] == 'PASS':
            try: row['assertions'] = _native_suite_assertions(name, Path(row['log']).read_text())
            except RuntimeError as exc: row.update(status='FAIL', reason=str(exc))
        try:
            if _native_suite_inputs(specification, native_live_oracle if name == 'live' else None) != report['native_suite_inputs'][name]:
                row.update(status='FAIL', reason='Native suite inputs changed during execution')
        except (RuntimeError, OSError) as exc:
            row.update(status='FAIL', reason=str(exc))
        report['stages']['native_suite_' + name] = row
    try:
        if prepared is None: raise RuntimeError(blocked or '缺已验证的stage')
        final = validate_profile(root, staged_project, profile=profile)
        if sha256_file(Path(final['manifest_path'])) != initial_sha:
            raise RuntimeError('开发工程manifest在验证中改变')
        report['project_configuration']['actual_after_sha256'] = sha256_file(Path(final['game']) / 'project.godot')
        report['stages']['native_stage_unchanged'] = _native_note(run, 'native_stage_unchanged', 'PASS',
                                                                {'manifest_sha256': initial_sha, **report['project_configuration']})
    except (RuntimeError, OSError, ValueError, KeyError, TypeError) as exc:
        report['stages']['native_stage_unchanged'] = _native_note(run, 'native_stage_unchanged', 'FAIL' if prepared else 'BLOCKED', {'reason': str(exc)})
    report['required_stages'] = list(report['stages'])
    report['ok'] = all(row['status'] == 'PASS' for row in report['stages'].values())
    write_report(receipt, report)
    return report


def run_verification(root: Path, profile: str, output: Path, engine: str | None, *,
                     staged_project: Path | None = None, native_suites: list[str] | None = None,
                     native_live_oracle: Path | None = None) -> dict:
    """依次执行唯一门禁，失败继续收集证据；每次新回执绑定起止源树与真实依赖身份。"""
    if profile not in PROFILES:
        raise ValueError('未知验证 profile：' + profile)
    if profile in NATIVE_PROFILES:
        return run_native_growth_verification(root, output, engine, staged_project, native_suites or [], native_live_oracle, profile=profile)
    root = root.resolve(); output = output.resolve()
    identity = source_identity(root)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-' + uuid.uuid4().hex[:8]
    run = output / run_id; run.mkdir(parents=True, exist_ok=False)
    receipt = run / 'report.json'
    selected = engine or os.environ.get('GODOT_BIN') or os.environ.get('HANMO_GODOT_EDITOR') or ''
    resolved = shutil.which(selected) if selected else None
    engine_command = resolved or selected or '<未配置固定 Godot>'
    report = {'format': 'HM-VERIFY/1', 'profile': profile, 'run_id': run_id, 'receipt': str(receipt),
              'created_at': datetime.now(timezone.utc).isoformat(), **identity,
              'engine_build': None, 'engine_expected_build': ENGINE_BUILD,
              'engine_path': resolved or selected or None, 'engine_sha256': None,
              'python_version': platform.python_version(), 'python_executable': sys.executable,
              'node_version': None, 'schema_sha256': schema_identity(root)['sha256'],
              'schemas': schema_identity(root), 'catalog': catalog_identity(root),
              'artifacts': [], 'stages': {}, 'prepared_catalogs': None, 'prepared_fonts': None, 'prepared_font_imports': None, 'ok': False}
    original_root = build.ROOT
    old_engine = os.environ.get('GODOT_BIN')
    build.ROOT = root
    os.environ['GODOT_BIN'] = engine_command
    try:
        plan = stage_plan(root, run, engine_command, profile)
        version_spec = plan['engine_version']
        missing = '固定 Godot 编辑器不存在；不使用 PATH 中其他版本：' + engine_command if not resolved else None
        version = run_stage('engine_version', version_spec, run, missing)
        report['stages']['engine_version'] = version
        if version['status'] == 'PASS':
            actual = Path(version['log']).read_text().strip()
            report['engine_build'] = actual; report['engine_sha256'] = sha256_file(Path(resolved))
            if actual != ENGINE_BUILD:
                version.update(status='BLOCKED', reason='固定引擎构建不匹配：' + actual)
        available = version['status'] == 'PASS'
        node = run_stage('node_version', plan['node_version'], run)
        report['stages']['node_version'] = node
        if node['status'] == 'PASS':
            report['node_version'] = Path(node['log']).read_text().strip()
        runtime_start = None
        for name, specification in plan.items():
            if name in {'engine_version', 'node_version', 'source_unchanged', 'runtime_inputs_unchanged'}:
                continue
            reason = None
            if (name.startswith('godot_') or name == 'military_cross_language') and not available:
                reason = '依赖固定 Godot 构建阻断：' + version['reason']
            if name == 'python_pytest' and resolved and not available:
                reason = '固定引擎身份/查询未通过；不让 pytest 间接使用非批准构建'
            if name == 'node_storage' and node['status'] != 'PASS':
                reason = 'Node 依赖不可用：' + node['reason']
            if name.startswith('godot_') and name != 'godot_import':
                dependencies = ['content_validation', 'runtime_registry', 'military_fixture', 'godot_import']
                if name == 'godot_test_scenario_initial_goods': dependencies.append('scenario_goods_fixture')
                if name == 'godot_test_scenario_kernel_startup': dependencies.append('scenario_fixture')
                if name == 'godot_test_second_military_lifecycle': dependencies.append('military_lifecycle_fixture')
                for dependency in dependencies:
                    if report['stages'].get(dependency, {}).get('status') != 'PASS':
                        reason = '前置阶段未通过：' + dependency
                        break
            if name == 'military_cross_language' and report['stages'].get('godot_import', {}).get('status') != 'PASS':
                reason = '前置阶段未通过：godot_import'
            row = run_stage(name, specification, run, reason)
            if name in {'source_static', 'source_web_static'}:
                static_dependencies(row, run / ('source_check.json' if name == 'source_static' else 'source_web_check.json'))
            if name == 'content_validation' and row['status'] == 'PASS':
                runtime_start = runtime_inputs_identity(root)
                report.update(runtime_start)
                report['font_imports_before_import'] = runtime_start['prepared_font_imports']
            if name == 'godot_import' and row['status'] == 'PASS' and runtime_start is not None:
                imports = runtime_inputs_identity(root)['prepared_font_imports']
                runtime_start['prepared_font_imports'] = imports
                report['prepared_font_imports'] = imports
            if name in {'python_pytest', 'python_web_storage', 'military_cross_language'}:
                xml = run / {'python_pytest': 'python-pytest.xml', 'python_web_storage': 'python-web-storage.xml',
                             'military_cross_language': 'military-cross-language.xml'}[name]
                pytest_dependencies(row, xml, available, engine_command)
            report['stages'][name] = row
            print(name + ': ' + row['status'], flush=True)
        report['stages'].update(optional_stages())
        report['excluded_tests'] = {test: {'result': 'NOT_RUN', 'reason': reason} for test, reason in EXCLUDED_TESTS.items()}
        end = source_identity(root)
        report['source_end_identity'] = end
        unchanged = identity == end and report['schema_sha256'] == schema_identity(root)['sha256'] and report['catalog'] == catalog_identity(root)
        report['stages']['source_unchanged'] = {'command': [], 'required': True, 'scope': '验证起止源树/Schema/作者包身份一致',
                                              'status': 'PASS' if unchanged else 'FAIL', 'exit_code': None,
                                              'log': None, 'log_sha256': None, 'log_summary': '',
                                              'reason': '' if unchanged else '运行期间源树身份变化，回执不可复用'}
        runtime_end = runtime_inputs_identity(root)
        report['runtime_inputs_end_identity'] = runtime_end
        runtime_status = 'BLOCKED' if runtime_start is None else 'PASS' if runtime_start == runtime_end else 'FAIL'
        report['stages']['runtime_inputs_unchanged'] = {
            'command': [], 'required': True, 'scope': '真实准备后至引擎结束，运行 catalog/Schema 与字体/许可输入字节一致',
            'status': runtime_status, 'exit_code': None, 'log': None, 'log_sha256': None, 'log_summary': '',
            'reason': '' if runtime_status == 'PASS' else '未完成内容准备' if runtime_start is None else '运行内容或字体输入在验证中变化'}
        report['required_stages'] = [name for name, row in report['stages'].items() if row['required']]
        report['ok'] = all(report['stages'][name]['status'] == 'PASS' for name in report['required_stages'])
        report['tests'] = {test: {'result': report['stages']['godot_' + test]['status'],
                                 **{key: report['stages']['godot_' + test][key] for key in ['assertions', 'startup_unicode_messages']
                                    if key in report['stages']['godot_' + test]}}
                           for test in TESTS}
        report['assertions'] = sum(row.get('assertions', 0) for row in report['tests'].values())
        report['engine'] = report['engine_build']
        write_report(receipt, report)
    finally:
        build.ROOT = original_root
        if old_engine is None:
            os.environ.pop('GODOT_BIN', None)
        else:
            os.environ['GODOT_BIN'] = old_engine
    return report


def _require_native_growth_receipt(path: Path, root: Path, *, profile: str = 'native-growth') -> dict:
    """开发回执只认证原隔离stage与受影响门，不能被linux/release消费为发行通过。"""
    try:
        from tools.native_growth_profile import validate_profile
    except ModuleNotFoundError:
        from native_growth_profile import validate_profile
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
        if report.get('format') != 'HM-VERIFY/1' or report.get('profile') != profile or not report.get('ok'):
            raise RuntimeError(profile + '回执未通过或格式不符')
        actual = validate_profile(root, Path(report['staged_project']), profile=profile)
        if (sha256_file(Path(actual['manifest_path'])) != report['stage_manifest_sha256'] or
                actual['manifest']['gd_source']['commit'] != report['commit']):
            raise RuntimeError(profile + '隔离来源身份已过期')
        config = actual['manifest']['configuration']; observed = report['project_configuration']
        if (observed['stage_raw_sha256'] != config['project_sha256'] or observed['semantic_sha256'] != config['project_semantic_sha256'] or
                any(not re.fullmatch(r'[0-9a-f]{64}', observed[key]) for key in ['actual_before_sha256', 'actual_after_sha256'])):
            raise RuntimeError(profile + '配置来源或规范摘要失效')
        oracle = Path(report['native_live_oracle']) if report.get('native_live_oracle') else None
        suites = _native_suite_plan(root, report['engine_path'], report['native_suites'], oracle)
        inputs = {name: _native_suite_inputs(specification, oracle if name == 'live' else None)
                  for name, specification in suites.items()}
        if inputs != report['native_suite_inputs']:
            raise RuntimeError(profile + '可选suite输入或oracle已变')
        expected = {'native_stage_identity', 'engine_version', 'godot_test_' + profile.replace('-', '_') + '_profile', 'native_stage_unchanged'}
        expected.update('native_suite_' + name for name in suites)
        if (set(report['stages']) != expected or set(report['required_stages']) != expected or
                len(report['required_stages']) != len(expected) or report_exit_code(report) != 0):
            raise RuntimeError(profile + '必需阶段缺失或没有全部通过')
        lock = json.loads((root / 'native/dependencies.lock.json').read_text())
        if (report['engine_build'] != ENGINE_BUILD or
                report['engine_sha256'] != lock['godot_bridge']['sdk']['engine_sha256'] or
                sha256_file(Path(report['engine_path'])) != report['engine_sha256']):
            raise RuntimeError(profile + '固定引擎身份已过期')
        for name, row in report['stages'].items():
            if (not row['required'] or row['status'] != 'PASS' or row['exit_code'] != 0 or
                    sha256_file(Path(row['log'])) != row['log_sha256']):
                raise RuntimeError(profile + '阶段日志或状态失效：' + name)
        if len(report['artifacts']) != 1:
            raise RuntimeError(profile + '缺本次代表内容测量工件')
        for artifact in report['artifacts']:
            target = Path(artifact['path'])
            if target.stat().st_size != artifact['bytes'] or sha256_file(target) != artifact['sha256']:
                raise RuntimeError(profile + '测量工件已变')
        return report
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise RuntimeError(profile + '回执缺失或不完整：' + str(path)) from exc


def require_receipt(path: Path, root: Path, profile: str) -> dict:
    """发行仅消费当前身份且全部必需阶段 PASS 的完整回执；缺失、陈旧与改日志均拒绝。"""
    if profile in NATIVE_PROFILES:
        return _require_native_growth_receipt(path, root, profile=profile)
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
        identity = source_identity(root)
        if report['format'] != 'HM-VERIFY/1' or report['profile'] != profile:
            raise RuntimeError('验证回执格式或 profile 不匹配')
        if any(report[key] != identity[key] for key in ['commit', 'dirty_diff_sha256']):
            raise RuntimeError('验证回执源树身份已过期')
        if report['schema_sha256'] != schema_identity(root)['sha256'] or report['catalog'] != catalog_identity(root):
            raise RuntimeError('验证回执 Schema/内容身份已过期')
        actual_inputs = runtime_inputs_identity(root)
        if report['prepared_catalogs'] != actual_inputs['prepared_catalogs']:
            raise RuntimeError('验证回执运行内容输入身份已过期')
        if report['prepared_fonts'] != actual_inputs['prepared_fonts']:
            raise RuntimeError('验证回执字体输入身份已过期')
        if report['prepared_font_imports'] != actual_inputs['prepared_font_imports']:
            raise RuntimeError('验证回执字体导入配置身份已过期')
        plan = stage_plan(root, path.parent, report.get('engine_path') or '', profile)
        expected = {name for name, specification in plan.items() if specification['required']}
        declared = {name for name, stage in report['stages'].items() if stage['required']}
        if (set(report['required_stages']) != expected or len(report['required_stages']) != len(expected)
                or declared != expected or not report['ok'] or report_exit_code(report) != 0):
            raise RuntimeError('验证回执必需项未全通过：BLOCKED/FAIL/NOT_RUN')
        if report['engine_build'] != ENGINE_BUILD:
            raise RuntimeError('验证回执固定引擎身份不匹配')
        engine_path = Path(report['engine_path'])
        if not engine_path.is_file() or sha256_file(engine_path) != report['engine_sha256']:
            raise RuntimeError('验证回执引擎二进制身份已过期')
        for name, stage in report['stages'].items():
            if name in {'source_unchanged', 'runtime_inputs_unchanged'} or stage['status'] != 'PASS':
                continue
            log = Path(stage['log'])
            if stage['exit_code'] != 0 or not log.is_file() or sha256_file(log) != stage['log_sha256']:
                raise RuntimeError('验证回执日志缺失或摘要改变：' + name)
        for artifact in report['artifacts']:
            target = Path(artifact['path'])
            if not target.is_file() or target.stat().st_size != artifact['bytes'] or sha256_file(target) != artifact['sha256']:
                raise RuntimeError('验证回执构建产物缺失或摘要改变：' + str(target))
        return report
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise RuntimeError('验证回执缺失或不完整：' + str(path)) from exc



def record_build_artifacts(report: dict, root: Path, artifacts: list[Path], platform_name: str,
                           command: list[str], log: Path, output: Path) -> dict:
    """从当前通过回执派生独立构建证据；导出日志与文件摘要不改变真实输入的 NOT_RUN。"""
    if platform_name not in {'native', 'web'}:
        raise ValueError('未知构建平台：' + platform_name)
    allowed = {'native': {'linux', 'release'}, 'web': {'web', 'release'}}
    if report.get('profile') not in allowed[platform_name]:
        raise RuntimeError('构建平台与验证 profile 不匹配：' + platform_name)
    original = require_receipt(Path(report['receipt']), root, report['profile'])
    if not log.is_file() or not artifacts or any(not path.is_file() for path in artifacts):
        raise RuntimeError('缺少实际导出日志或构建产物，不能生成构建回执')
    derived = json.loads(json.dumps(original))
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-build-' + uuid.uuid4().hex[:8]
    run = output.resolve() / run_id; run.mkdir(parents=True, exist_ok=False)
    receipt = run / 'report.json'
    derived.update(run_id=run_id, receipt=str(receipt), parent_receipt=original['receipt'],
                   created_at=datetime.now(timezone.utc).isoformat())
    derived['artifacts'] = [{'path': str(path.resolve()), 'file': path.name, 'bytes': path.stat().st_size,
                             'sha256': sha256_file(path)} for path in sorted(artifacts)]
    derived['stages'][platform_name + '_export'] = {
        'status': 'PASS', 'required': False, 'reason': '', 'scope': '实际导出及构建摘要；独立玩家输入未执行',
        'command': command, 'exit_code': 0, 'log': str(log.resolve()), 'log_sha256': sha256_file(log),
        'log_summary': log.read_text(encoding='utf-8', errors='replace')[-2000:]}
    write_report(receipt, derived)
    return derived


def internal_stage(root: Path, stage: str) -> int:
    """给共享执行器提供现有工具的真实入口，不在此重写内容或表现业务规则。"""
    if stage == 'presentation':
        try:
            from tools.presentation import check_project
        except ModuleNotFoundError:
            from presentation import check_project
        print(json.dumps(check_project(root), ensure_ascii=False))
    elif stage == 'content':
        build.stage_content(root)
        print(json.dumps({'sources': [str(path.relative_to(root)) for path in build.discover_author_sources(root)], 'ok': True}))
    else:
        print(json.dumps(validate_test_registry(root / 'game'), ensure_ascii=False))
    return 0


def main(argv: list[str] | None = None) -> int:
    """退出码零仅代表必需项全通过；一为失败，二为阻断或未执行，未知 profile 不接受。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), default='architecture')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--engine')
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--stage', choices=['presentation', 'content', 'registry'])
    parser.add_argument('--staged-project', type=Path, help='Prepared output/game directory returned by build stage')
    parser.add_argument('--native-suite', action='append', default=[], metavar='NAME=PREPARED_GAME')
    parser.add_argument('--native-live-oracle', type=Path)
    args = parser.parse_args(argv)
    if (args.staged_project is not None or args.native_suite or args.native_live_oracle is not None) and (args.profile not in NATIVE_PROFILES or args.stage):
        parser.error('Native development arguments require --profile native-growth without --stage')
    if args.profile in NATIVE_PROFILES and (args.stage or args.staged_project is None):
        parser.error('native-growth requires --staged-project and does not accept --stage')
    if args.stage:
        return internal_stage(args.root, args.stage)
    if args.output is None:
        parser.error('--output 必须指向验证父目录；每次运行在其中创建唯一目录')
    if args.profile in NATIVE_PROFILES:
        report = run_verification(args.root, args.profile, args.output, args.engine, staged_project=args.staged_project,
                                  native_suites=args.native_suite, native_live_oracle=args.native_live_oracle)
    else:
        report = run_verification(args.root, args.profile, args.output, args.engine)
    print(json.dumps({'ok': report['ok'], 'receipt': report['receipt'], 'exit_code': report_exit_code(report)}, ensure_ascii=False))
    return report_exit_code(report)


if __name__ == '__main__':
    raise SystemExit(main())
