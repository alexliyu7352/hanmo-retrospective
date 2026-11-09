"""目录身份有序向量的窄源码契约门；不替代原生准入或引擎运行。"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def actual_submit_tail_arguments(submit):
    """按真实具名形参核实参，不把尾参文本相邻视为业务绑定。"""
    signature=submit.split('Status commit_tail(',1)[1].split('){',1)[0]
    names=[re.search(r'(\w+)\s*$',part).group(1) for part in signature.split(',')]
    expression=submit.split('auto completed=commit_tail(',1)[1].split(';',1)[0][:-1]
    arguments=[];begin=0;depth=0
    for i,char in enumerate(expression):
        if char in '([{':depth+=1
        elif char in ')]}':depth-=1
        elif char==',' and depth==0:arguments.append(expression[begin:i].strip());begin=i+1
    arguments.append(expression[begin:].strip())
    if len(names)!=len(arguments) or len(set(names))!=len(names):
        raise ValueError("submit signature and actual argument shape mismatch")
    return dict(zip(names,arguments))


class NativeModuleIdentityOrderTest(unittest.TestCase):
    """三个生产者须给同一显式模块组按同一顺序追加，不能按集合比较。"""

    def test_personal_message_plan_source_and_commit_order(self):
        """私信Continue只借原SAY和实际结果；早形状与晚全源分门。"""
        bundle=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertIn("family_voluntary_paths_enabled()||catalog->task_personal_messages||catalog->task_personal_reports)?validate_player_plan_shape",bundle)
        late=bundle.index("(catalog->task_personal_messages||catalog->task_personal_reports)?&tasks:nullptr")
        self.assertLess(bundle.index("valid=validate_task_personal_message_world(*this)"),late)
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        self.assertLess(submit.index("auto personal_bound=bind_task_personal_message_result_world"),submit.index("finish_player_plan_commit("))
        execution=(ROOT/"native/world/src/resident/domains/player_plan/plan_execution.cpp").read_text()
        self.assertIn("if(personal_message&&!player_plan_personal_message_meetings_match",execution)
        self.assertIn('if(action->kind=="deliver_message")',execution)
        self.assertIn("!player_plan_personal_message_action(w,*step->action_id)&&",execution)
        page=(ROOT/"native/world/src/resident/domains/player_plan/plan_page.cpp").read_text()
        self.assertIn("if(!player_plan_personal_message_meetings_match",page)
        cmake=(ROOT/"native/world/resident.cmake").read_text()
        self.assertEqual(cmake.count("    src/resident/domains/player_plan/plan_personal_message.cpp"),2)

    def test_personal_report_order_actual_tail_and_plan(self):
        """私人返报与辞任同代并存，完整Task原史后才允许Plan消耗结果。"""
        self._check_group(["task_personal_message_deliveries","task_personal_message_reports","political_office_resignations","political_office_resignation_responses"])
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        actual=actual_submit_tail_arguments(submit)
        self.assertEqual(actual["personal_report_sources"],"personal_report_sources.value")
        self.assertEqual(actual["political_office_resignation_source_id"],"political_office_resignation_source_id")
        bundle=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertLess(bundle.index("validate_task_personal_report_world(*this)"),bundle.index("(catalog->task_personal_messages||catalog->task_personal_reports)?&tasks:nullptr"))
        execution=(ROOT/"native/world/src/resident/domains/player_plan/plan_execution.cpp").read_text()
        self.assertIn("if(personal_report&&!player_plan_personal_report_meetings_match",execution)
        self.assertIn("!player_plan_personal_report_action(w,*step->action_id)&&",execution)

    def _positions(self, path, prefix, names):
        """只取实际追加表达式，重复或漏注册均失败，不推断条件是否成立。"""
        text = (ROOT / path).read_text(encoding="utf-8")
        positions = []
        for name in names:
            if prefix == "gd":
                expression = r'expected\["module_versions"\]\.append\(\{"name":\s*"' + name + r'"'
            else:
                key = {'occupation_fiscal_needs': r'occupation_fiscal_need_module', 'fiscal_taxpayer_continuity': r'fiscal_taxpayer_continuity_module', 'political_military_appointments': r'military_officer_political_module', 'military_officer_assignments': r'military_officer_module', 'military_field_commands': r'military_field_command_module', 'military_wounded_recovery': r'military_recovery_module', 'military_supply_attrition': r'military_supply_attrition_module', 'military_rout_retreat': r'military_rout_module', 'military_command_shock': r'military_command_shock_module', 'military_garrison_rest': r'military_garrison_rest_module', 'military_combat_conditions': r'military_combat_conditions_module', 'military_commanded_damage': r'military_commanded_damage_module', 'military_sequential_rounds': r'military_sequential_module', 'military_field_terrain': r'military_field_terrain_module', 'military_command_resources': r'military_command_resources_module', 'military_tactic_learning': r'military_tactic_learning_module', 'military_siege_engineering': r'military_siege_engineering_module', 'political_field_appointments': r'military_field_command_political_module', 'private_shop_daily_rent': r'private_shop_daily_rent_module', 'private_shop_command_results': r'private_shop_command_results_module', 'political_governance_invitation_replies': r'governance_invitation_reply_module', 'political_head_handovers': r'political_head_handover_domain', 'political_successor_rounds': r'political_successor_rounds_domain', 'political_term_handovers': r'political_term_handover_domain', 'political_office_receptions': r'political_office_reception_domain', 'visit_message_receipts': r'visit_message_receipt_module', 'task_shop_order_contacts': r'task_shop_order_contact_module', 'task_personal_message_deliveries': r'task_personal_message_module', 'task_personal_message_reports': r'task_personal_report_module', 'political_office_resignations': r'political_office_resignation_module', 'political_office_resignation_responses': r'political_office_resignation_responses_module'}.get(name, '"' + name + '"')
                expression = re.escape(prefix) + r'\.module_versions\.emplace_back\(' + key + r','
            # 显式有序字面量循环与逐项append等价，只解析当前有限模块列表，不猜动态值。
            if prefix == "gd" and name in ("economic_supply_policies", "political_supply_adoptions"):
                loop = r'for module in \[([^\]]+)\]: expected\["module_versions"\]\.append\(\{"name":module,"version":"1"\}\)'
                found = [m for m in re.finditer(loop, text) if '"' + name + '"' in m.group(1)]
                self.assertEqual(len(found), 1, f"{path}: {name} must occur in exactly one literal producer")
                positions.append(found[0].start(1) + found[0].group(1).index('"' + name + '"'))
                continue
            matches = list(re.finditer(expression, text))
            self.assertEqual(len(matches), 1, f"{path}: {name} must be appended exactly once")
            positions.append(matches[0].start())
        return positions

    def _check_group(self, names):
        """按已冻结原生顺序核三份源码；不改字节合同来迎合前端。"""
        for path, prefix in [
            ("game/ui/native_resident_backend.gd", "gd"),
            ("native/world/src/resident/kernel/native_initial_state.cpp", "admitted"),
            ("native/world/src/resident/kernel/world_codec.cpp", "id"),
        ]:
            with self.subTest(path=path, modules=names):
                positions = self._positions(path, prefix, names)
                self.assertEqual(positions, sorted(positions), f"{path}: ordered identity differs")

    def test_office_resignation_npc_tail_and_actual_result(self):
        """Q01与辞任完整同启时三目录尾同序，玩家回执不被NPC尾参覆盖。"""
        self._check_group(["family_voluntary_paths", "task_personal_message_deliveries", "political_office_resignations", "political_office_resignation_responses"])
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        actual=actual_submit_tail_arguments(submit)
        self.assertEqual(actual["personal_message_sources"],"personal_message_sources.value")
        self.assertEqual(actual["political_office_resignation_source_id"],"political_office_resignation_source_id")
        self.assertLess(submit.index("bind_political_office_resignation_result_world"),submit.index("finish_player_plan_commit"))
        source=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        first=source.index("valid=validate_political_office_resignation_world(*this)")
        self.assertLess(source.index("validate_family_voluntary_paths_world(*this)"),first)
        self.assertLess(first,source.index("validate_political_office_resignation_response_world(*this)"))
        self.assertLess(source.index("validate_political_office_resignation_response_world(*this)"),source.index("validate_political_governance_director_world(*this)"))

    def test_military_field_terrain_order(self):
        self._check_group(["military_sequential_rounds", "military_field_terrain", "military_command_resources", "military_naval"])

    def test_innate_profile_and_late_growth(self):
        """先天课程须出生全史后核Growth，再核真实计划来源；旧profile早序不变。"""
        self._check_group(["family_lineage", "family_birth", "visit_messages", "family_innate_learning"])
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        birth=source.index("valid=validate_family_birth_world(*this)")
        late=source.index("validate_growth(",birth)
        plan=source.index("validate_player_plan_world(*this)",birth)
        self.assertLess(birth,late)
        self.assertLess(late,plan)
        early=source[:birth]
        self.assertIn("!catalog->growth->innate_learning",early)
        codec=(ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text()
        self.assertIn('name=="growth"&&w.catalog->growth->innate_learning',codec)
        self.assertIn('section.domain=="growth"&&catalog.growth->innate_learning',codec)
        self.assertEqual(codec.count('writer.schema_features|=innate_study_checkpoint_feature'),1)
        self.assertEqual(codec.count('reader.schema_features|=innate_study_checkpoint_feature'),1)

    def test_child_care_relief_independent_tail(self):
        """新照护不依赖拜帖；标题尾与全读写同序，完整出生/照护先于计划绑定。"""
        self._check_group(["family_birth", "visit_messages", "family_innate_learning", "visit_reception_intents", "visit_reception_claims", "family_care_relief"])
        codec=(ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text()
        title=codec.split('if(has("visit_messages"))',1)[1].split('if(has("family_care_relief"))',1)[0]
        self.assertTrue(title.lstrip().startswith('{if(!dynamic)fail("VisitMessageProfileMismatch","checkpoint/module_versions");domains.push_back({"visit_messages",1});}'))
        self.assertIn('domains.push_back({"visit_reception_claims",1});',title)
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        birth=source.index("valid=validate_family_birth_world(*this)")
        relief=source.index("validate_child_care_relief_world(*this)",birth)
        growth=source.index("validate_growth(",birth)
        plan=source.index("validate_player_plan_world(*this)",birth)
        self.assertLess(birth,relief)
        self.assertLess(relief,growth)
        self.assertLess(growth,plan)

    def test_visit_message_order(self):
        """拜帖独立尾只在真实显式profile追加，三目录生产者须保持同序。"""
        self._check_group(["voluntary_succession", "family_lineage", "family_birth", "visit_messages"])

    def test_message_handoff_order(self):
        """原交付凭据放在垫款之后，保持新旧送信profile同序。"""
        self._check_group(["task_offer_responses", "task_personal_advances", "task_message_handoffs", "party_invitations"])

    def test_party_consent_order(self):
        """本人招伴声明无新冷段，身份只在旧邀请之后统一追加。"""
        self._check_group(["selected_existing_actor", "party_invitations", "party_recruitment_consents"])

    def test_family_bequest_order(self):
        """同启死亡、生活控制和私产遗赠时仍按原生有序目录登记。"""
        self._check_group(["family_life", "family_legacy", "family_bequest", "task_message_notices", "family_inheritance", "family_marriage", "voluntary_succession"])

    def test_treaty_passage_order(self):
        """新双签身份位于旧财政之后，不能随旧送信错序移动。"""
        self._check_group(["politics", "political_fiscal", "treaty_passage", "task_takeover_execution", "production_management"])

    def test_taxpayer_continuity_order(self):
        """资产易手的显式标记紧接旧财政，三处有序身份保持一致。"""
        self._check_group(["household_provisioning", "fiscal_settlement", "fiscal_taxpayer_continuity", "director_registry"])

    def test_political_military_appointment_order(self):
        """新专项签受位于双签之后，不因新增尾段改变目录顺序。"""
        self._check_group(["political_fiscal", "treaty_passage", "political_military_appointments", "task_takeover_execution"])

    def test_known_and_message_receipts_merged_identity_order(self):
        """实际两条独立尾在现有完整后继之后，三producer不靠集合判同。"""
        self._check_group(['private_shop_command_results','visit_appointment_attendance','growth_tactic_lessons','political_successor_rounds','political_term_handovers','visit_message_receipts','known_personal_dialogues'])

    def test_term_handover_identity_order(self):
        """任期交接必须在原逐轮继位之后，三生产者同序。"""
        self._check_group(['political_successor_rounds','political_term_handovers','political_office_receptions'])

    def test_shop_command_results_actual_writer_and_late_control(self):
        """原真实before与result精确绑定，完整控制和Task史先于经营结果授权。"""
        self._check_group(["political_head_handovers","private_shop_command_results","visit_appointment_attendance","growth_tactic_lessons"])
        source=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        shop=source.index('valid=validate_private_shop_command_results_world(*this)')
        for prior in ('validate_family_disciple_legacy_world','validate_family_biological_world','validate_task_shop_orders_world','validate_private_shop_liquidation_world'):
            self.assertLess(source.index('valid='+prior+'(*this)'),shop)
        player=source.index('valid=validate_private_shop_command_result_player_world(*this)')
        self.assertLess(shop,player)
        for line in source[:player].splitlines():
            if 'validate_player_plan_world(*this)' in line:self.assertIn('!private_shop_command_results_enabled()',line)
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        # 按真实形参名核原调用位置，追加别域尾参不能覆盖教学前tick或原World。
        actual=actual_submit_tail_arguments(submit)
        self.assertEqual(actual['tactic_lesson_from_tick'],'current.common.tick()')
        self.assertEqual(actual['shop_command_before'],'&current')
        self.assertEqual(actual['political_successor_round_source_id'],'political_successor_round_source_id')
        self.assertEqual(actual['political_term_handover_source_id'],'political_term_handover_source_id')
        self.assertEqual(actual['political_office_reception_source_id'],'political_office_reception_source_id')
        self.assertEqual(actual['visit_message_sources'],'visit_message_sources.value')
        self.assertEqual(actual['known_dialogue_source'],'known_dialogue_source.value')
        self.assertLess(submit.index('bind_family_biological_result_world'),submit.index('bind_private_shop_command_results_world'))
        self.assertLess(submit.index('bind_private_shop_command_results_world'),submit.index('finish_player_plan_commit'))

    def test_shop_authority_follows_original_actor_api(self):
        """权限模式只取原不可变目录，不能让新尾暗改旧multiActor API或信任存档模式。"""
        source=(ROOT/"native/world/src/resident/kernel/private_shop_command_results_integration.cpp").read_text()
        predicate='w.dynamic_enabled()&&w.catalog->director&&w.catalog->director->dynamic_subjects'
        self.assertIn(predicate+'?PrivateShopSubmissionAuthority::controlled_player:PrivateShopSubmissionAuthority::legacy_actor_api',source)
        self.assertIn('submission_authority(*before)',source)
        self.assertIn('submission_authority(w)',source)
        original=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        self.assertIn('if(w.dynamic_enabled()&&w.catalog->director->dynamic_subjects&&w.common.actor(command.actor_id)&&command.actor_id!=w.common.state().metadata.player_actor_id)',original)
        self.assertIn('if(current.dynamic_enabled()&&current.catalog->director->dynamic_subjects&&command.actor_id!=current.common.state().metadata.player_actor_id)',original)
        codec=(ROOT/"native/world/src/resident/domains/commerce/command_results/private_shop_command_results_codec.cpp").read_text()
        self.assertNotIn('submission_authority',codec)
        self.assertNotIn('PrivateShopSubmissionAuthority',codec)

    def test_tactic_lessons_late_sources_and_final_plan(self):
        """基本计划形状永不跳过；教学和唯一军事取得全史通过后才核新行动绑定。"""
        self._check_group(["military_command_resources","military_tactic_learning","military_naval"])
        self._check_group(["family_biological_legacy","visit_appointment_invitations","political_head_handovers","growth_tactic_lessons"])
        source=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertIn('if(player_plan_enabled()&&!player_plan_seed_complete)',source)
        self.assertIn('if(player_plan_enabled()){valid=(growth_tactic_lessons_enabled()||family_voluntary_paths_enabled()||catalog->task_personal_messages||catalog->task_personal_reports)?validate_player_plan_shape(player_plan):validate_player_plan(common,player_plan);',source)
        lessons=source.index('valid=validate_growth_tactic_lessons_world(*this)')
        military=source.index('valid=validate_military_tactic_learning_world_sources(*this)')
        self.assertEqual(source.count('valid=validate_military_tactic_learning_world_sources(*this)'),1)
        self.assertLess(lessons,military)
        for line in source[:lessons].splitlines():
            if 'validate_player_plan_world(*this)' in line:self.assertIn('!growth_tactic_lessons_enabled()',line)
        self.assertIn('if(growth_tactic_lessons_enabled()&&(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns&&!catalog->task_personal_messages)){auto inheritance=',source[military:])
        self.assertIn('if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns||catalog->task_personal_messages){auto inheritance=',source[military:])
        self.assertLess(source.index('valid=validate_family_inheritance(',military),source.index('valid=validate_player_plan(common,player_plan,&growth_tactic_lessons)',military))
        self.assertLess(source.index('valid=validate_player_plan(common,player_plan,&growth_tactic_lessons)',military),source.index('valid=validate_player_plan_world(*this)',military))

    def test_tactic_lesson_actual_outer_clock_and_plan_bindings(self):
        """推进可由他人或个人战引起，原outer前tick和真实cache对象必须一路保留。"""
        base=ROOT/"native/world/src/resident"
        submit=(base/"kernel/resident_world.cpp").read_text()
        self.assertIn('current.growth_tactic_lessons_enabled()?&current.growth_tactic_lessons:nullptr,current.common.tick()',submit)
        self.assertIn('bind_growth_tactic_lesson_result_world(w,tactic_lesson_before,actor,sequence,*receipt,tactic_lesson_from_tick)',submit)
        self.assertLess(submit.index('bind_growth_tactic_lesson_result_world('),submit.index('finish_player_plan_commit('))
        helper=(base/"domains/player_plan/plan_tactic_lesson.cpp").read_text()
        self.assertIn('read_growth_tactic_lesson_completions_for_result(common,lessons,actual)',helper)
        self.assertIn('source.source_fact_id!=original.value.completion->source_fact_id',helper)
        execution=(base/"domains/player_plan/plan_execution.cpp").read_text()
        external=execution.split('if(state.current->actor_id!=actor){',1)[1].split('    if(execution){',1)[0]
        self.assertIn('player_plan_tactic_lesson_completion(w,state.current->actor_id',external)
        self.assertIn('player_plan_pause(state,"PlayerPlanManualCommand")',external)
        self.assertIn('&&!player_plan_tactic_lesson_action(w,*step->action_id)&&!repair_action',execution)

    def test_successor_round_same_generation_order(self):
        """后任新轮与原首轮不同段，三producer保持同一完整有序身份。"""
        self._check_group(["political_head_handovers","visit_appointment_attendance","growth_tactic_lessons","political_successor_rounds"])

    def test_military_tactic_learning_order(self):
        """学习只有完整指挥资源后追加；三producer顺序一致，不靠能力并集隐式开启。"""
        self._check_group(["military_command_resources", "military_tactic_learning", "military_naval"])

    def test_military_officer_assignment_order(self):
        """军官接替在原军请之后、舟师之前登记，GD与原生必须一致。"""
        self._check_group(["military_requests", "military_officer_assignments", "military_naval"])

    def test_naval_repair_module_order(self):
        """显式修缮标记与Naval3选择在三处同序，旧舟师不增加空标记。"""
        self._check_group(["military_field_commands", "naval_repairs", "military_naval", "household_control"])

    def test_tactical_navigation_policy_order(self):
        """转向/水流无新冷段，只能以显式marker与原Naval目录政策严格配对。"""
        self._check_group(["naval_repairs", "naval_repair_charges", "naval_navigation_wear", "naval_tactical_navigation", "military_naval"])

    def test_supply_policy_module_order(self):
        """发行与接管采用必须同对且同序，不能改旧就业或生活模块的解释。"""
        self._check_group(["estate_employment", "economic_supply_policies", "political_supply_adoptions", "voluntary_succession"])

    def test_repair_charges_profile_order(self):
        """收费显式marker接原修缮，再追加明确Naval版本，旧3身份不混用。"""
        self._check_group(["naval_repairs", "naval_repair_charges", "military_naval"])

    def test_supply_renewal_profile_forwarding(self):
        """新续签读者保留S3责任v2与原军事profile，三个marker生产者同序。"""
        self._check_group(["economic_supply_policies", "political_supply_adoptions", "political_supply_renewals"])
        for path, call, argument in [
            ("native/world/src/resident/kernel/native_initial_state.cpp", "auto response_director=decode_director_catalog(", 'DirectorSupplyRenewalProfile{dynamic.find("economic_supply_policies")'),
            ("native/world/src/resident/kernel/world_codec.cpp", "auto director=decode_director_catalog(", "DirectorSupplyRenewalProfile{result->economic_supply_policies_enabled}"),
        ]:
            source=(ROOT/path).read_text(encoding="utf-8")
            self.assertIn(argument, source.split(call,1)[1].split(";",1)[0])
        source=(ROOT/"native/world/src/resident/domains/director/director_model.hpp").read_text(encoding="utf-8")
        self.assertIn("TaskResponsibilityDirectorContext* task_responsibility",source)
        political=(ROOT/"native/world/src/resident/domains/politics/politics_director.hpp").read_text(encoding="utf-8")
        self.assertIn("supply_renewal;",political)
        dynamic=(ROOT/"native/world/src/resident/kernel/dynamic_integration.cpp").read_text(encoding="utf-8")
        self.assertIn("politics->supply_renewal=[&w]",dynamic)

    def test_birth_marker_and_validation_order(self):
        """出生双marker同序，原血缘身份先Common、完整照护史先计划来源。"""
        self._check_group(["economic_supply_policies", "political_supply_adoptions", "voluntary_succession", "family_lineage", "family_birth"])
        source = (ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text(encoding="utf-8")
        self.assertLess(source.index("validate_family_birth_identity_world(*this)"), source.index("common.validate_common()"))
        for selected in ["if(!growth_tactic_lessons_enabled()&&player_plan_enabled()&&!catalog->contact_inquiries&&!private_shops_enabled()&&family_birth_enabled())", "if(!growth_tactic_lessons_enabled()&&private_shops_enabled()&&player_plan_enabled()&&!catalog->contact_inquiries)"]:
            self.assertEqual(source.count(selected), 1)
            self.assertLess(source.index("validate_family_birth_world(*this)"), source.index(selected))

    def test_supply_policy_receipt_feature_selection(self):
        """同一个新位同时进入writer/title/full reader，旧生产tag14仍独立选择。"""
        source = (ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text(encoding="utf-8")
        for writer in ["writer", "reader", "political_features"]:
            expression = (writer + ".schema_features|=" if writer != "political_features" else writer + "|=") + "economic_supply_policy_checkpoint_feature"
            self.assertEqual(source.count(expression), 1, writer)
        command = (ROOT / "native/world/src/resident/checkpoint/command_boundary.cpp").read_text(encoding="utf-8")
        self.assertIn('kind=="configure_local_production"&&(c.schema_features&common_checkpoint::production_configuration_receipts)', command)
        self.assertIn('kind=="political_issue_supply_policy"&&(c.schema_features&economic_supply_policy_checkpoint_feature)', command)

    def test_personal_appointment_attendance_module_order(self):
        """两个独立NPC模板有各自准入，赴约尾追加不吞接册和康复身份。"""
        self._check_group(["political_shop_registry_intents", "personal_appointments", "personal_injury_recovery", "personal_appointment_attendance"])
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertLess(source.index("validate_family_birth_world(*this)"),source.index("validate_personal_appointment_world(*this)"))
        self.assertLess(source.index("validate_personal_appointment_world(*this)"),source.index("validate_personal_appointment_attendance_world(*this)"))
        codec=(ROOT / "native/world/src/resident/domains/director/director_codec.cpp").read_text()
        self.assertIn("supply,reception,registries,attendance_enabled",codec)

    def test_registry_intents_module_order(self):
        """NPC接册显式目录紧随原继受，再保预约和个人康复原顺序。"""
        self._check_group(["political_shop_registry_successions", "political_shop_registry_intents", "personal_appointments", "personal_injury_recovery"])
    def test_supply_attrition_module_order(self):
        """Military9随完整恢复追加，保三身份生产者原顺序。"""
        self._check_group(["military_field_commands", "military_wounded_recovery", "military_supply_attrition", "military_naval"])

    def test_wounded_recovery_module_order(self):
        """Military8随原野外模块追加，三身份生产者不重排旧军事/舟师。"""
        self._check_group(["military_officer_assignments", "military_field_commands", "military_wounded_recovery", "military_naval"])

    def test_field_command_module_order(self):
        """独立野外双权源保驻营前置，军与政各目录producer同序。"""
        self._check_group(["military_officer_assignments", "military_field_commands", "military_naval"])
        self._check_group(["political_military_appointments", "political_field_appointments", "task_takeover_execution"])

    def test_field_catalog_dependency_order(self):
        """新野外目录只在旧驻营与组织目录已安装后交叉校验。"""
        source = (ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text(encoding="utf-8")
        installed = 'result->organization=std::make_shared<const OrganizationCatalog>'
        checked = 'validate_political_field_catalog(*result->common,*result->organization'
        self.assertEqual(source.count(checked), 1)
        self.assertLess(source.index(installed), source.index(checked))

    def test_officer_catalog_dependency_order(self):
        """军官目录交叉验证必须晚于组织目录安装，不能解引用尚未创建的依赖。"""
        source = (ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text(encoding="utf-8")
        installed = 'result->organization=std::make_shared<const OrganizationCatalog>'
        checked = 'validate_political_military_appointment_catalog(*result->common,*result->organization'
        self.assertEqual(source.count(installed), 1)
        self.assertEqual(source.count(checked), 1)
        self.assertLess(source.index(installed), source.index(checked))

    def test_director_profile_forwarding(self):
        """新局预读和正式目录均传显式NPC能力，旧重载默认不能遮蔽已选模块。"""
        cases = [
            ("native/world/src/resident/kernel/native_initial_state.cpp", "auto response_director=decode_director_catalog(",
             'dynamic.find("treaty_passage")', 'DirectorMilitaryAppointmentProfile{dynamic.find("political_military_appointments")'),
            ("native/world/src/resident/kernel/world_codec.cpp", "auto director=decode_director_catalog(",
             "treaty_passage_enabled(*result)", "DirectorMilitaryAppointmentProfile{political_military_appointments_enabled(*result)"),
        ]
        for path, call, treaty, officers in cases:
            source = (ROOT / path).read_text(encoding="utf-8")
            self.assertEqual(source.count(call), 1, f"{path}: original catalog call must remain unique")
            expression = source.split(call, 1)[1].split(";", 1)[0]
            self.assertIn(treaty, expression, f"{path}: treaty profile was dropped by a legacy overload")
            self.assertIn(officers, expression, f"{path}: officer profile was dropped by a legacy overload")

    def test_employment_inheritance_dependency_order(self):
        """继产免责消费真实雇用责任之前，先核同候选雇用全史与其原经济/任务来源。"""
        source = (ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text(encoding="utf-8")
        checked = "valid=validate_estate_employment_world(*this)"
        used = "valid=validate_family_inheritance("
        # 旧作者保原位置；真正出生身份必须在完整Birth之后再核实际雇用年龄。
        early = "if(!family_birth_enabled()){" + checked
        late = "if(family_birth_enabled()){" + checked
        self.assertEqual(source.count(checked), 2)
        self.assertEqual(source.count(early), 1)
        self.assertEqual(source.count(late), 1)
        self.assertEqual(source.count(used), 4)
        self.assertIn("if(!family_voluntary_paths_enabled()&&!growth_tactic_lessons_enabled()&&!family_collateral_legacy_enabled()&&(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns)){auto inheritance=",source)
        self.assertIn("if(growth_tactic_lessons_enabled()&&(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns&&!catalog->task_personal_messages)){auto inheritance=",source)
        contact=source.index("if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns||catalog->task_personal_messages){auto inheritance=")
        self.assertLess(source.index("valid=validate_task_shop_order_contact_world(*this)"),contact)
        self.assertLess(source.index("valid=validate_dynamic_tasks("), source.index(early))
        birth = source.index("valid=validate_family_birth_world(*this)")
        self.assertLess(source.index(early), birth)
        self.assertLess(birth, source.index(late))
        self.assertLess(source.index(late), source.index(used))

    def test_governance_continuation_late_pair(self):
        """新续辦尾在师承之后，工作/付款同原权源，NPC不能伪造玩家实际结果。"""
        self._check_group(["private_shop_liquidations", "task_shop_order_estate_cancellations", "family_apprenticeship", "political_governance_continuations"])
        world = (ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertEqual(world.count("valid=validate_political_governance_continuation_world(*this)"), 1)
        self.assertLess(world.index("valid=validate_family_apprenticeship_world(*this)"), world.index("valid=validate_political_governance_continuation_world(*this)"))
        integration = (ROOT / "native/world/src/resident/kernel/governance_task_integration.cpp").read_text()
        self.assertEqual(integration.count("PoliticalGovernanceContinuationWorldAccess::create("), 2)
        self.assertIn("w.governance_tasks,*effective,action_id", integration)
        self.assertIn("tasks,*effective,source.acceptance.task_id", integration)
        commit = (ROOT / "native/world/src/resident/kernel/resident_world.cpp").read_text()
        self.assertLess(commit.index("state.history.push_back("), commit.index("bind_political_governance_continuation_result("))
        self.assertLess(commit.index("bind_political_governance_continuation_result("), commit.index("finish_player_plan_commit("))
        npc = (ROOT / "native/world/src/resident/kernel/phase_plan.cpp").read_text().split("Status try_npc_command(",1)[1]
        self.assertLess(npc.index('common_failure("GovernanceContinuationPlayerCommandRequired")'), npc.index("begin_savepoint()"))

    def test_apprenticeship_independent_tail(self):
        """师承独立尾只跟真实已有清算尾追加，三生产者及二进制尾序一致。"""
        self._check_group(["political_shop_licensing_receipts", "private_shop_liquidations", "task_shop_order_estate_cancellations", "family_apprenticeship"])
        codec = (ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text()
        for prefix in ['domains.push_back({', 'section(', 'else if(section.domain==']:
            previous = prefix + '"task_shop_order_estate_cancellations"'
            added = prefix + '"family_apprenticeship"'
            self.assertEqual(codec.count(previous), codec.count(added))
            self.assertGreater(codec.count(added), 0)
        world = (ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertLess(world.index("valid=validate_family_birth_world(*this)"), world.index("valid=validate_family_apprenticeship_world(*this)"))
        commit = (ROOT / "native/world/src/resident/kernel/resident_world.cpp").read_text()
        self.assertLess(commit.index("state.history.push_back("), commit.index("bind_family_apprenticeship_result_world("))
        self.assertLess(commit.index("bind_family_apprenticeship_result_world("), commit.index("finish_player_plan_commit("))
        npc = (ROOT / "native/world/src/resident/kernel/phase_plan.cpp").read_text().split("Status try_npc_command(",1)[1]
        self.assertLess(npc.index('common_failure("ApprenticeshipDirectDecisionRequired")'), npc.index("begin_savepoint()"))

    def test_employment_module_order(self):
        """就业目录仅追加在原实物marker之后，三生产者与冷尾各自同序。"""
        self._check_group(["task_static_assets", "estate_employment"])

    def test_field_director_profile_forwarding(self):
        """野外NPC资格须传到两个真实目录入口，不能落入驻营旧重载默认值。"""
        for path, call, required in [
            ("native/world/src/resident/kernel/native_initial_state.cpp", "auto response_director=decode_director_catalog(", 'dynamic.find("political_field_appointments")!=nullptr'),
            ("native/world/src/resident/kernel/world_codec.cpp", "auto director=decode_director_catalog(", 'political_field_appointments_enabled(*result)'),
        ]:
            source = (ROOT / path).read_text(encoding="utf-8")
            expression = source.split(call, 1)[1].split(";", 1)[0]
            self.assertIn(required, expression, f"{path}: field NPC profile omitted")

    def test_independent_checkpoint_tail_order(self):
        """独立冷尾的目录、标题选择和实际写入同序；不枚举无关profile组合。"""
        source = (ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text(encoding="utf-8")
        paths = [
            ("std::vector<CheckpointDomain> domains_for(", "// 从已核封套", r"domains\.push_back\(\{"),
            ("Outcome<std::vector<CheckpointDomain>> selected_checkpoint_domains(", "// 唯一域顺序", r"domains\.push_back\(\{"),
            ("Outcome<Bytes> encode_world_checkpoint(", "// 标题选槽", r"section\("),
        ]
        names = ["family_bequest", "task_message_notices", "treaty_passage", "family_inheritance",
                 "fiscal_taxpayer_continuity", "political_military_appointments", "family_marriage", "task_static_assets", "estate_employment", "political_field_appointments", "economic_supply_policies", "political_supply_adoptions", "family_birth", "visit_messages", "visit_reception_claims", "family_care_relief"]
        for begin, end, expression in paths:
            body = source.split(begin, 1)[1].split(end, 1)[0]
            positions = []
            for name in names:
                token = {"political_military_appointments": r"military_officer_political_module", "political_field_appointments": r"military_field_command_political_module"}.get(name) or '"' + name + '"'
                matches = list(re.finditer(expression + r"\s*" + token + r"\s*,", body))
                self.assertEqual(len(matches), 1, f"{begin}: {name} must be emitted exactly once")
                positions.append(matches[0].start())
            self.assertEqual(positions, sorted(positions), f"{begin}: independent checkpoint tails differ")

    def test_visit_reception_catalog_forwarding(self):
        """NPC接待仅追加不可变目录，两个Director安装入口均传具名profile。"""
        self._check_group(["visit_messages", "family_innate_learning", "visit_reception_intents", "visit_reception_claims"])
        for path, call in [("native/world/src/resident/kernel/native_initial_state.cpp", "auto response_director=decode_director_catalog("), ("native/world/src/resident/kernel/world_codec.cpp", "auto director=decode_director_catalog(")]:
            source=(ROOT / path).read_text()
            expression=source.split(call,1)[1].split(";",1)[0]
            self.assertIn("reception", expression)
        source=(ROOT / "native/world/src/resident/kernel/dynamic_integration.cpp").read_text()
        bind=source.index("auto bound=bind_visit_reception_world(w,proposal)")
        self.assertLess(source.index("auto applied=apply_command(w,proposal.subject_id"), bind)
        self.assertLess(bind, source.index("w.common.commit_savepoint(mark.value)",bind))

    def test_military_command_resources_exact_source_and_order(self):
        """15高位/三生产者/晚兵法来源均同一显式规则，旧14不补资格。"""
        self._check_group(["military_sequential_rounds", "military_command_resources", "military_naval"])
        base=ROOT / "native/world/src/resident"
        source=(base/"kernel/world_bundle.cpp").read_text()
        late=source.index("valid=validate_military_command_resources_world(*this)")
        self.assertLess(source.index("valid=validate_military_commanded_damage_world(*this)"),late)
        self.assertLess(late,source.index("if(military_commanded_damage_enabled()&&player_plan_enabled()&&!growth_tactic_lessons_enabled())"))
        self.assertEqual(source.count("valid=validate_military_command_resources_world(*this)"),1)
        codec=(base/"kernel/world_codec.cpp").read_text()
        self.assertIn('validate_military_siege_engineering_checkpoint_layout(version("military"),versions)',codec)
        profile=(base/"domains/military/military_tactic_learning_profile.hpp").read_text()
        self.assertIn('validate_military_command_resources_checkpoint_layout(version==16?15u:version,modules)',profile)
        self.assertIn('writer.schema_features|=military_command_resources_checkpoint_feature',codec)
        self.assertIn('reader.schema_features|=military_command_resources_checkpoint_feature',codec)
        phase=(base/"kernel/phase_plan.cpp").read_text()
        self.assertIn('MilitaryTacticsWorldRead tactics_skills(w,dynamic_input)',phase)
        self.assertIn('contact_access.prepare_command_resources=',phase)

    def test_military_sequential_module_order(self):
        self._check_group(["military_commanded_damage", "military_sequential_rounds", "military_naval"])

    def test_military_commanded_damage_module_order(self):
        """M14在完整13之后追加，三个生产者保持真实向量次序。"""
        self._check_group(["military_combat_conditions", "military_commanded_damage", "military_naval"])

    def test_commanded_damage_late_qualification_and_plan(self):
        """M14早军权回放后必须补真实人物资格，全部旧Plan出口对新格式延后。"""
        base=ROOT / "native/world/src/resident"
        source=(base / "kernel/world_bundle.cpp").read_text()
        late=source.index('valid=validate_military_commanded_damage_world(*this)')
        self.assertLess(source.index('valid=validate_family_birth_world(*this)'),late)
        self.assertLess(source.index('if(catalog->growth->innate_learning){'),late)
        final=source.index('if(military_commanded_damage_enabled()&&player_plan_enabled()&&!growth_tactic_lessons_enabled())')
        self.assertLess(late,final)
        for line in source[:final].splitlines():
            if 'validate_player_plan_world(*this)' in line:self.assertIn('!military_commanded_damage_enabled()',line)
        self.assertEqual(source.count('valid=validate_military_commanded_damage_world(*this)'),1)
        phase=(base / "kernel/phase_plan.cpp").read_text()
        self.assertIn('MilitaryCommandedWorldRead commanded_skills(w,dynamic_input);auto contact_access=ma;',phase)
        self.assertIn('install_military_contact(w.common,w.military,*w.catalog->military,contact_access,',phase)
        integration=(base / "kernel/military_commanded_damage_integration.cpp").read_text()
        self.assertIn('!world_.catalog->family||!world_.catalog->growth',integration.split('commanded_skill_before_entry',1)[1])

    def test_military_combat_conditions_module_order(self):
        """Military13入战条件只在完整12后追加，三真实生产者同序。"""
        self._check_group(["military_garrison_rest", "military_combat_conditions", "military_naval"])

    def test_military_garrison_rest_module_order(self):
        self._check_group(["military_command_shock", "military_garrison_rest", "military_naval"])

    def test_military_command_shock_module_order(self):
        """Military11身份仅追加在原10之后，三生产者一致。"""
        self._check_group(["military_rout_retreat", "military_command_shock", "military_naval"])

    def test_military_rout_module_order(self):
        """Military10尾段在原9之后，三个身份生产者必须同序。"""
        self._check_group(["military_field_commands", "military_wounded_recovery", "military_supply_attrition", "military_rout_retreat", "military_naval"])

    def test_military_rout_exact_cold_gate(self):
        """新永久轮次由实际Military/M9先验后核，不能仅依今日状态。"""
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        start=source.index("valid=validate_military_state(")
        self.assertLess(start,source.index("valid=validate_military_supply_attrition_world(*this)",start))
        self.assertLess(source.index("valid=validate_military_supply_attrition_world(*this)",start),source.index("valid=validate_military_rout_world(*this)",start))
        self.assertEqual(source.count("valid=validate_military_rout_world(*this)"),1)

    def test_naval_navigation_module_order(self):
        """Naval5具名航损沿原段版本且三目录生产者保修缮/收费前置同序。"""
        self._check_group(["naval_repairs", "naval_repair_charges", "naval_navigation_wear", "military_naval"])

    def test_naval_gunnery_module_order(self):
        """Naval6仍沿原段编号，三生产者必须在旧前置之后追加同一具名技艺标记。"""
        self._check_group(["naval_repairs", "naval_repair_charges", "naval_navigation_wear", "naval_tactical_navigation", "naval_gunnery", "military_naval"])

    def test_naval_gunnery_late_skill_proof(self):
        """早机械重放不冒XP先验；同一未发布候选在完整生命/成长史后必须补晚门。"""
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        body=source.split("Status WorldBundle::validate() const {",1)[1]
        early=body.index("validate_naval_battle_structure_for_gunnery(")
        birth=body.index("valid=validate_family_birth_world(*this)")
        growth=body.index("validate_growth(",birth)
        late=body.index("valid=validate_naval_gunnery_world(*this)")
        plan=body.index("validate_player_plan_world(*this)",late)
        self.assertLess(early,birth)
        self.assertLess(birth,growth)
        self.assertLess(growth,late)
        self.assertLess(late,plan)
        self.assertEqual(body.count("valid=validate_naval_gunnery_world(*this)"),1)
        self.assertIn("!family_birth_enabled()&&!naval_gunnery_enabled()",body[:birth])
        bridge=(ROOT / "native/world/src/resident/kernel/naval_gunnery_integration.cpp").read_text()
        self.assertIn("read_growth_naval_skill_before_preparation(",bridge)
        self.assertIn("validate_naval_gunnery_history(",bridge)

    def test_growth_encoder_explicit_innate_flag(self):
        """新世界导出与装载选同一先天开关；产品静态断言另拒声明/定义漂成重载。"""
        source=(ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text()
        call=source.split("auto growth=encode_growth_state(",1)[1].split(");",1)[0]
        self.assertTrue(call.endswith("w.catalog->growth->innate_learning"))
        codec=(ROOT / "native/world/src/resident/domains/growth/growth_codec.cpp").read_text()
        self.assertIn("decltype(&encode_growth_state)",codec)
        self.assertIn("bool study_teaching_requests,bool innate_learning)",codec)

    def test_naval_navigation_source_order(self):
        """真实航步先交原接敌安装，失败完成仅在clock_commit后读永久来源。"""
        source = (ROOT / "native/world/src/resident/kernel/phase_plan.cpp").read_text(encoding="utf-8")
        self.assertLess(source.index("auto installed=install_naval_contact("), source.index("r=release_disabled_naval_journeys("))
        self.assertLess(source.index("case Phase::clock_commit:"), source.index("query_naval_navigation_termination("))
        self.assertIn("w.naval_navigation_enabled()&&travel_command", source)
        binding = (ROOT / "native/world/src/resident/domains/player_plan/plan_binding.cpp").read_text(encoding="utf-8")
        self.assertIn("if(!w.naval_navigation_enabled())return {true,std::nullopt,{}}", binding)
        self.assertIn("if(source.value||navigation_reason)", binding)

    def test_occupation_need_order(self):
        """新债需求显式marker不挪动旧财政与工资模块的相对顺序。"""
        self._check_group(["fiscal_need_policies", "occupation_fiscal_needs", "cohort_wage_need_policies"])


    def test_contact_inquiry_tail_and_original_plan_structure(self):
        """新增独立尾保原顺序；基础Plan门始终执行，完整绑定在原史之后。"""
        self._check_group(["family_apprenticeship", "political_governance_continuations", "contact_inquiries"])
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        self.assertIn("if(player_plan_enabled()){valid=(growth_tactic_lessons_enabled()||family_voluntary_paths_enabled()||catalog->task_personal_messages||catalog->task_personal_reports)?validate_player_plan_shape(player_plan):validate_player_plan(common,player_plan);",source)
        late=source.index("valid=validate_contact_inquiry_world(*this)")
        self.assertLess(source.index("validate_family_birth_world(*this)"),late)
        self.assertLess(source.index("validate_personal_appointment_world(*this)"),late)
        self.assertGreater(source.index("if(!growth_tactic_lessons_enabled()&&player_plan_enabled()&&catalog->contact_inquiries)"),late)
        self.assertIn("if(!growth_tactic_lessons_enabled()&&private_shops_enabled()&&player_plan_enabled()&&!catalog->contact_inquiries)",source)
        for line in source[:late].splitlines():
            if "validate_player_plan_world(*this)" in line:self.assertIn("!catalog->contact_inquiries",line)

    def test_contact_inquiry_exact_plan_and_player_boundary(self):
        """同原say/Action/actual result才继续；缺源不冒旅行，NPC无本人提交尾。"""
        base=ROOT / "native/world/src/resident"
        binding=(base / "domains/player_plan/plan_binding.cpp").read_text()
        self.assertIn('if(inquiry_action||step.stop_reason.rfind("ContactInquiry",0)==0)',binding)
        self.assertIn('*receipt,*step.completed_tick,actual_result->result_id',binding)
        execution=(base / "domains/player_plan/plan_execution.cpp").read_text()
        self.assertIn('!appointment_action&&!inquiry_action&&',execution)
        self.assertIn('player_plan_contact_inquiry_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id)',execution)
        self.assertIn('player_plan_contact_inquiry_meeting_matches(*inquiry,session)',execution)
        phase=(base / "kernel/phase_plan.cpp").read_text().split('Status try_npc_command(',1)[1]
        guard=phase.index('return common_failure("ContactInquiryPlayerCommandRequired")')
        self.assertLess(guard,phase.index('prepare_command('))
        self.assertLess(guard,phase.index('begin_savepoint('))
        commit=(base / "kernel/resident_world.cpp").read_text().split('Status commit_tail(',1)[1]
        self.assertLess(commit.index('state.history.push_back('),commit.index('bind_contact_inquiry_result_world('))
        self.assertLess(commit.index('bind_contact_inquiry_result_world('),commit.index('finish_player_plan_commit('))
        helper=(base / "domains/player_plan/plan_contact_inquiry.cpp").read_text()
        self.assertIn('source.status!="active"',helper)
        self.assertNotIn('answer',helper)

    def test_governance_intents_tail_and_late_business_sources(self):
        """新NPC意图只末尾注册，全部原治理/受领/赈济先验后再读计划来源。"""
        self._check_group(["contact_inquiries", "political_governance_intents"])
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        late=source.index("valid=validate_political_governance_director_world(*this)")
        for call in ("validate_political_governance(","validate_tasks_with_governance(","validate_public_relief("):
            self.assertLess(source.index(call),late)
        self.assertEqual(source.count("valid=validate_political_governance_director_world(*this)"),1)
        model=(ROOT / "native/world/src/resident/domains/director/director_model.hpp").read_text()
        self.assertIn("personal_appointment_attendance=15, governance_commission=16",model)

    def test_governance_intents_preserve_original_proposal_savepoint(self):
        """新提案借原命令，不遗漏原三域保存点，也不新增第二份账。"""
        source=(ROOT / "native/world/src/resident/kernel/dynamic_integration.cpp").read_text()
        for field in ("politics_governance","governance_tasks","public_relief"):
            self.assertIn(field+"(w."+field+")",source)
            self.assertIn("w."+field+"=std::move("+field+")",source)
        self.assertIn("propose_political_governance_director_world(w,known,policy,base)",source)

    def test_governance_intents_archived_original_observation_boundary(self):
        """归档仍读原计划生成时刻，不拿今日后来的原业务事实填旧计划。"""
        source=(ROOT / "native/world/src/resident/domains/politics/governance_intents/political_governance_intent_validation.cpp").read_text()
        self.assertIn("observed=p.next_review_tick-delay",source)
        self.assertIn("schedule->last_decision_tick!=observed",source)
        self.assertIn("schedule->next_tick!=p.next_review_tick",source)
        self.assertIn("validate_governance_intent_completed_source(*context,*intent,p.subject.id,p.goal_ref,observed)",source)
        self.assertNotIn("const Integer observed=context->common.tick()",source)

    def test_visit_reply_tail_after_existing_inquiry_and_governance(self):
        """共享三方仍同序追加，独立回复段不改变原Interaction访客。"""
        self._check_group(["contact_inquiries", "political_governance_intents", "visit_message_replies"])
        source=(ROOT / "native/world/src/resident/kernel/world_codec.cpp").read_text()
        self.assertIn('if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",static_cast<std::uint32_t>(catalog.visit_message_replies->version)});',source)
        for first,second in [('r=section("contact_inquiries"','r=section("visit_message_replies"'),('if(has("contact_inquiries")){','if(has("visit_message_replies")){')]:
            self.assertLess(source.index(first),source.index(second))

    def test_visit_reply_commit_and_cold_sources(self):
        """实际当次result与原领读全史必先成立，缺本人永久绑定不降级。"""
        base=ROOT / "native/world/src/resident"
        source=(base / "kernel/resident_world.cpp").read_text()
        self.assertLess(source.index('visit_reply_new_result_world(current,*candidate'),source.index('auto completed=commit_tail('))
        body=source.split('Status commit_tail(',1)[1]
        self.assertLess(body.index('state.history.push_back('),body.index('bind_visit_reply_result_world('))
        self.assertLess(body.index('bind_contact_inquiry_result_world('),body.index('bind_visit_reply_result_world('))
        self.assertLess(body.index('bind_visit_reply_result_world('),body.index('finish_player_plan_commit('))
        cold=(base / "kernel/world_bundle.cpp").read_text()
        late=cold.index('valid=validate_visit_reply_world(*this)')
        self.assertLess(cold.index('valid=validate_family_birth_world(*this)'),late)
        self.assertLess(cold.index('valid=validate_contact_inquiry_world(*this)'),late)
        self.assertIn('if(player_plan_enabled()){valid=(growth_tactic_lessons_enabled()||family_voluntary_paths_enabled()||catalog->task_personal_messages||catalog->task_personal_reports)?validate_player_plan_shape(player_plan):validate_player_plan(common,player_plan);',cold)
        receipts=(base / 'domains/interaction/visit_replies/visit_reply_receipts.cpp').read_text()
        self.assertIn('VisitReplyPermanentPlayerBindingRequired',receipts)
        self.assertIn('VisitReplyHistorySourceRequired',receipts)
        self.assertIn('cached->get()!=&result',receipts)

    def test_visit_reply_v2_control_precedes_cold_authorship(self):
        """实际合流只把回复2移到完整控制史后，原1访客不多写空来源。"""
        base=ROOT / "native/world/src/resident"
        source=(base / "kernel/world_bundle.cpp").read_text()
        v1='if(!catalog->visit_message_replies||catalog->visit_message_replies->version==1){valid=validate_visit_reply_world(*this);'
        v2='if(catalog->visit_message_replies&&catalog->visit_message_replies->version==2){valid=validate_visit_reply_world(*this);'
        self.assertEqual(source.count(v1),1)
        self.assertEqual(source.count(v2),1)
        self.assertLess(source.index(v1),source.index('validate_family_disciple_legacy_world(*this)'))
        self.assertLess(source.index('validate_family_birth_world(*this)'),source.index(v2))
        self.assertLess(source.index('validate_family_disciple_legacy_world(*this)'),source.index(v2))
        codec=(base / "domains/interaction/visit_replies/visit_reply_codec.cpp").read_text()
        self.assertIn('if(version==2)return a.optional("authored_origin"',codec)
        self.assertIn('return v.authored_origin?a.reject("VisitReplyAuthoredVersionRequired"):r;',codec)
        shared=(base / "kernel/world_codec.cpp").read_text()
        self.assertIn('has("visit_reply_intents")!=(has("visit_message_replies")&&modules.at("visit_message_replies")=="2")',shared)
        self.assertNotIn('section("visit_reply_intents"',shared)

    def test_visit_reply_npc_guard_precedes_any_write(self):
        """v1只本人，NPC即使借玩家actor也在预备与保存点前拒绝。"""
        source=(ROOT / "native/world/src/resident/kernel/phase_plan.cpp").read_text().split('Status try_npc_command(',1)[1]
        check=source.index('VisitReplyPlayerCommandRequired')
        self.assertLess(check,source.index('prepare_command('))
        self.assertLess(check,source.index('begin_savepoint('))
        self.assertIn('std::holds_alternative<VisitReplyCommand>(command.data)||visit_reply_command_kind(command.kind)',source[:check])

    def test_disciple_control_exact_tail_and_final_plan(self):
        """弟子尾后加且保持旧拜帖/询问顺序；只新profile移完整Plan到所有来源后。"""
        self._check_group(["contact_inquiries", "political_governance_intents", "visit_message_replies", "family_disciple_legacy"])
        source=(ROOT / "native/world/src/resident/kernel/world_bundle.cpp").read_text()
        app=source.index('valid=validate_family_apprenticeship_world(*this)')
        disciple=source.index('valid=validate_family_disciple_legacy_world(*this)')
        late=source.index('if((family_disciple_legacy_enabled()||family_biological_legacy_enabled()||family_collateral_legacy_enabled())&&player_plan_enabled()&&!growth_tactic_lessons_enabled())')
        self.assertLess(app,disciple)
        for call in ('validate_visit_reply_world(*this)','validate_contact_inquiry_world(*this)','validate_political_governance_director_world(*this)'):
            self.assertLess(source.index(call),late)
        self.assertLess(disciple,late)
        self.assertIn('if(player_plan_enabled()){valid=(growth_tactic_lessons_enabled()||family_voluntary_paths_enabled()||catalog->task_personal_messages||catalog->task_personal_reports)?validate_player_plan_shape(player_plan):validate_player_plan(common,player_plan);',source)
        for line in source[:late].splitlines():
            if 'validate_player_plan_world(*this)' in line:self.assertIn('!family_disciple_legacy_enabled()',line)
        codec=(ROOT / 'native/world/src/resident/kernel/world_codec.cpp').read_text()
        self.assertTrue('if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",static_cast<std::uint32_t>(catalog.visit_message_replies->version)});if(catalog.family_disciple_legacy)domains.push_back({"family_disciple_legacy",1});' in codec)
        self.assertLess(codec.index('r=section("visit_message_replies"'),codec.index('r=section("family_disciple_legacy"'))

    def test_daily_rent_after_invitation_identity(self):
        """独立租期最后追加，三生产者不借旧店铺标记推断启用。"""
        self._check_group(["family_disciple_legacy", "visit_reply_intents", "political_governance_invitation_replies", "private_shop_daily_rent"])

    def test_governance_invitation_tail_after_reply_intents(self):
        """谢绝/撤回新尾不得覆盖已合回复2或M14格式及原实际result钩。"""
        self._check_group(["visit_message_replies", "family_disciple_legacy", "visit_reply_intents", "political_governance_invitation_replies"])
        base=ROOT / "native/world/src/resident"
        codec=(base / "kernel/world_codec.cpp").read_text()
        self.assertIn('if(catalog.political_governance_invitation_replies)domains.push_back({governance_invitation_reply_module,1});if(catalog.private_shop_daily_rent)domains.push_back({private_shop_daily_rent_module,1});if(catalog.born_personal_dialogues)domains.push_back({"born_personal_dialogues",1});if(catalog.family_biological_legacy)domains.push_back({"family_biological_legacy",1});',codec)
        self.assertIn('static_cast<std::uint32_t>(catalog.visit_message_replies->version)',codec)
        self.assertIn('military_commanded_damage_domains(std::move(domains))',codec)
        common=(base / "kernel/resident_world.cpp").read_text().split('Status commit_tail(',1)[1]
        self.assertLess(common.index('bind_visit_reply_result_world('),common.index('bind_governance_invitation_player_result('))
        self.assertEqual(common.count('bind_family_disciple_legacy_result_world('),1)
        self.assertLess(common.index('bind_governance_invitation_player_result('),common.index('finish_player_plan_commit('))
        self.assertLess(common.index('bind_family_disciple_legacy_result_world('),common.index('finish_player_plan_commit('))

    def test_invitation_player_control_is_exact_and_late(self):
        """关闭前真实本人由原控制链证明，裁剪cache后也不能冒NPC关闭来源。"""
        base=ROOT / "native/world/src/resident"
        world=(base / "kernel/world_bundle.cpp").read_text()
        self.assertLess(world.index('validate_family_disciple_legacy_world(*this)'),world.index('validate_governance_invitation_world(*this)'))
        integration=(base / "kernel/governance_invitation_world.cpp").read_text()
        self.assertIn('read_governance_invitation_player_before_fact(w.common,w.family,w.family_legacy_enabled()?&w.family_legacy:nullptr,tick,fact)',integration)
        control=(base / "domains/politics/governance_replies/political_governance_invitation_control.cpp").read_text()
        self.assertIn('((*n<*boundary)!=(where<at))',control)
        self.assertIn('family_player_before_fact_with_legacy(common,family,*legacy,tick,fact)',control)
        source=(base / "domains/politics/governance_replies/political_governance_invitation_sources.cpp").read_text()
        self.assertIn('GovernanceInvitationHistoricalPlayerMismatch',source)

    def test_disciple_control_original_commit_and_npc_rejection(self):
        """原Legacy三表是唯一权限账，结果尾原候选前后比较且在Plan之前绑定。"""
        base=ROOT / 'native/world/src/resident'
        source=(base / 'kernel/resident_world.cpp').read_text()
        self.assertIn('current.family_disciple_legacy_enabled()?&current.family_legacy:nullptr',source)
        self.assertIn('current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr',source)
        body=source.split('Status commit_tail(',1)[1]
        self.assertLess(body.index('state.history.push_back('),body.index('bind_family_disciple_legacy_result_world('))
        self.assertLess(body.index('bind_visit_reply_result_world('),body.index('bind_family_disciple_legacy_result_world('))
        self.assertLess(body.index('bind_family_disciple_legacy_result_world('),body.index('finish_player_plan_commit('))
        phase=(base / 'kernel/phase_plan.cpp').read_text().split('Status try_npc_command(',1)[1]
        guard=phase.index('DiscipleControlDirectDecisionRequired')
        self.assertLess(guard,phase.index('prepare_command('))
        self.assertLess(guard,phase.index('begin_savepoint('))

    def test_disciple_control_reader_signature_is_unique(self):
        """同源签名断言阻断默认bool多余尾参形成未定义旧重载。"""
        source=(ROOT / 'native/world/src/resident/domains/family/disciple_control_sources.cpp').read_text()
        self.assertIn('decltype(&read_disciple_control_relation_before_consent)',source)
        self.assertIn('Integer tick,const StableId& before){',source)

    def test_private_shop_exact_local_economy_sources(self):
        """私租/私工资必须先具名证明，旧无来源重载与全局支付守恒不放宽。"""
        base=ROOT / 'native/world/src/resident'
        source=(base / 'domains/local_economy/local_economy_validation.cpp').read_text()
        self.assertIn('employment,nullptr);',source)
        self.assertIn('&shops->common!=&c||&shops->economy!=&s||&shops->economy_catalog!=&catalog||shops->phase',source)
        self.assertIn('auto read=read_private_shop_economy_source_ids(*shops)',source)
        self.assertIn('employed+shop_obligations.size()',source)
        self.assertIn('bool terminal=shop_fractions.count(row.first)',source)
        self.assertIn('ArrearPaymentSumMismatch',source)
        bridge=(base / 'kernel/world_bundle.cpp').read_text()
        self.assertIn('PrivateShopResponsibilitySourceContext{common,local_economy,*catalog->local_economy,private_shops,*catalog->private_shops,nullptr,catalog->private_shop_daily_rent.get(),private_shop_daily_rent_enabled()?&private_shop_daily_rent:nullptr}',bridge)
        self.assertLess(bridge.index('valid=validate_local_economy('),bridge.index('valid=validate_private_shop_world(*this)'))
        proof=(base / 'domains/commerce/private_shop_economy_sources.cpp').read_text()
        for reason in ('PrivateShopEconomyReceiptMismatch','PrivateShopEconomyFractionSourceMismatch','PrivateShopUnprovenEconomyDue','PrivateShopUnprovenEconomyPayment'):
            self.assertIn(reason,proof)

    def test_old_family_acceptance_requires_actual_candidate_speech(self):
        """旧合同本已要求候选本人，不能把玩家choice当控制/财产接受来源。"""
        base=ROOT / 'native/world/src/resident/domains/family'
        legacy=(base / 'legacy_control_consent.cpp').read_text()
        succession=(base / 'succession_consent_source.cpp').read_text()
        self.assertIn('LegacyControlCandidateSayRequired',legacy)
        self.assertIn('SuccessionCandidateSayRequired',succession)
        self.assertIn('node->speaker!=d.successor_actor_id',succession)

    def test_military_combat_condition_exact_original_entry(self):
        """同tick仍核原入口前全局事实序，疲劳不能从今日部队回填。"""
        base=ROOT / 'native/world/src/resident/domains/military'
        history=(base / 'military_rout_history.cpp').read_text()
        self.assertIn('validate_military_combat_condition_entry(c,s,cat,entry,*b.combat_conditions,phase)',history)
        conditions=(base / 'military_conditions.cpp').read_text()
        self.assertIn('seq>=entry_number',conditions)
        self.assertIn('at>=boundary',conditions)
        self.assertIn('return fold_condition(s,cat,id,entry->tick+1,true,verify)',conditions)
        source=(base / 'military_combat_conditions_source.cpp').read_text()
        self.assertIn('contact->siege_id||contact->node_id',source)
        self.assertIn('query_military_condition_before_rout_entry(c,s,cat,stored.unit_id,entry.id,phase)',source)
        self.assertIn('MilitaryCombatConditionNeutralSourceMismatch',source)
        rules=(base / 'battle_rules.cpp').read_text()
        self.assertIn('if(b.combat_conditions){auto condition=military_combat_attack_efficiency(b,id)',rules)
        self.assertIn('ranged&&wall?*wall:10000,fatigue_bp',rules)
        self.assertIn('MilitaryCombatConditionDamageModelRequired',rules)

    def test_collateral_control_dependencies_and_submit(self):
        """完整旁支控制须在所有历史本人消费者之前；before和结果绑定共用原提交。"""
        self._check_group(["known_personal_dialogues","political_office_receptions","task_shop_order_contacts","family_collateral_legacy"])
        source=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        order=["validate_known_personal_dialogue_world", "validate_family_apprenticeship_world", "validate_family_collateral_recognition_world", "validate_family_disciple_legacy_world", "validate_family_biological_world", "validate_family_collateral_world", "validate_task_shop_order_contact_world", "validate_political_office_reception_world"]
        positions=[source.index("valid="+call+"(*this)")for call in order]
        self.assertEqual(positions,sorted(positions))
        self.assertIn("if((family_collateral_legacy_enabled()||family_voluntary_paths_enabled())&&!growth_tactic_lessons_enabled()&&(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns)){auto inheritance=",source)
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        arguments=actual_submit_tail_arguments(submit)
        self.assertEqual(arguments['collateral_before'], 'current.family_collateral_legacy_enabled()?&current.family_collateral_legacy:nullptr')
        self.assertLess(submit.index('bind_family_collateral_result_world'),submit.index('finish_player_plan_commit'))

    def test_shop_contacts_actual_source_and_final_dependencies(self):
        """新接触先核完整控制再入成本；所有新作者最终计划在两域全史之后。"""
        self._check_group(["known_personal_dialogues","political_office_receptions","task_shop_order_contacts"])
        source=(ROOT/"native/world/src/resident/kernel/world_bundle.cpp").read_text()
        contact=source.index("valid=validate_task_shop_order_contact_world(*this)")
        self.assertLess(source.index("valid=validate_family_biological_world(*this)"),contact)
        self.assertLess(source.index("valid=validate_family_collateral_world(*this)"),contact)
        tail=source.index("if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns||catalog->task_personal_messages){auto inheritance=")
        self.assertLess(source.index("valid=validate_private_shop_world(*this)",contact),tail)
        for line in source[:tail].splitlines():
            if "validate_player_plan_world(*this)" in line:self.assertIn("!catalog->task_shop_order_contacts_enabled",line)
        self.assertIn("growth_tactic_lessons_enabled()?&growth_tactic_lessons:nullptr",source[tail:])
        submit=(ROOT/"native/world/src/resident/kernel/resident_world.cpp").read_text()
        self.assertEqual(actual_submit_tail_arguments(submit)["shop_order_contact_source"],"shop_order_contact_source.value")

    def test_siege_occupation_and_recovery_same_strict_phase(self):
        """军事早史先验后才读康复；current显式to，原credit-before不改变。"""
        base=ROOT/"native/world/src/resident"
        world=(base/"kernel/world_bundle.cpp").read_text()
        self.assertLess(world.index("validate_military_siege_engineering_world_structure(*this)"),world.index("validate_growth_recovery_world(*this)"))
        bridge=(base/"kernel/growth_recovery_integration.cpp").read_text()
        self.assertIn("actor,*phase_):query_military_actor_occupation",bridge)
        self.assertIn("service,*phase_):validate_growth_recovery_qualification_now",bridge)
        self.assertIn("tick,fact,*phase_):query_military_actor_occupation_before",bridge)
        occupation=(base/"domains/military/military_actor_occupation.cpp").read_text()
        self.assertIn("phase.to_tick,{},true,phase.to_tick",occupation)
        self.assertIn("phase.from_tick!=c.tick()",occupation)
        qualification=(base/"domains/growth/growth_recovery_qualification.cpp").read_text()
        self.assertIn("phase?phase->to_tick:c.tick()",qualification)
        self.assertIn("phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1",qualification)
        service=(base/"domains/growth/growth_recovery_services.cpp").read_text()
        self.assertIn("validate_growth_recovery_occupation_now(c,occupation_,*id,phase_)",service)

    def test_military_siege_engineering_order(self):
        self._check_group(["military_tactic_learning", "military_siege_engineering", "military_naval"])
        base = ROOT / "native/world/src/resident"
        source = (base / "kernel/world_bundle.cpp").read_text()
        early = source.index("valid=validate_military_siege_engineering_world_structure(*this)")
        late = source.index("valid=validate_military_siege_engineering_world_actor_sources(*this)")
        self.assertLess(source.index("valid=validate_military_state(common,military"), early)
        self.assertLess(source.index("valid=validate_military_tactic_learning_world_sources(*this)"), late)
        self.assertLess(late, source.index("if(growth_tactic_lessons_enabled()&&(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns&&!catalog->task_personal_messages)){auto inheritance="))
        phase = (base / "kernel/phase_plan.cpp").read_text()
        # 每个原preclock消费者各持同栈服务；本轮timeout须在节点战读墙防前结束。
        factory = "make_military_siege_engineering_access(w,&dynamic_input)"
        self.assertEqual(phase.count(factory), 4)
        for name in ("siege_step", "military_battle", "siege_battles_finished", "release_invalid_commands"):
            start = phase.index("case Phase::" + name + ":")
            end = phase.find("case Phase::", start + 1)
            case = phase[start:end if end >= 0 else len(phase)]
            self.assertEqual(case.count(factory), 1, name)
        battle_case = phase[phase.index("case Phase::military_battle:"):phase.index("case Phase::battle_actor_changes:")]
        self.assertLess(battle_case.index(factory), battle_case.index("step_military_battles("))
        self.assertIn("*w.catalog->military,engineering_access,to_tick", battle_case)
        self.assertIn("*w.catalog->military,ma,w.naval", battle_case)
        access = (base / "kernel/military_access.cpp").read_text()
        self.assertIn("make_military_siege_engineering_access(world_)", access)
        codec = (base / "kernel/world_codec.cpp").read_text()
        self.assertIn('s.codec_version==16||s.codec_version==17', codec)
        self.assertIn('section.codec_version==16||section.codec_version==17', codec)


    def test_shop_completion_preserves_actual_phase_through_client_history(self):
        # 原结算候选未推钟时，两种纯读必须借同一输入；开店历史边界仍保持原钟/原事实。
        base = ROOT / "native/world/src/resident"
        bridge = (base / "kernel/private_shop_integration.cpp").read_text()
        client_ctor = re.search(r"clients_\(TaskFulfilledClientContext\{([^}]+)\}\)", bridge)
        self.assertIsNotNone(client_ctor)
        self.assertEqual(client_ctor.group(1).split(",")[-1].strip(), "phase")
        self.assertIn("*w.tasks.shop_orders,phase", bridge)
        clients = (base / "domains/tasks/task_fulfilled_clients.cpp").read_text()
        self.assertIn("actor,before,x.phase)", clients)
        self.assertIn("before->opening_tick>x.common.tick()", clients)
        order = (base / "domains/tasks/shop_orders/task_shop_order_sources.cpp").read_text()
        begin = order.index("read_task_shop_order_fulfilled_clients(")
        self.assertIn("validate_task_shop_order_ledger(c,tasks,state,phase)", order[begin:])

if __name__ == "__main__":
    unittest.main()
