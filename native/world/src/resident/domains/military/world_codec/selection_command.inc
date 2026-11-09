// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 动态军事4仅在显式policy下派生5；静态3绝不推测等价。
// 恢复为Military8显式尾段；无完整前置绝不从高版本反推启用。
// 缺粮匿名逃散明确Military9尾段，仍要求原8完整前置。
bool military_siege_engineering_enabled(const PreparedCatalog &c) {
  return c.military && c.military->siege_engineering_policy.has_value();
}
template <class DomainList>
DomainList military_siege_engineering_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 16)
        boundary::fail("MilitarySiegeEngineeringPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 17;
    }
  return domains;
}
bool military_tactic_learning_enabled(const PreparedCatalog &c) {
  return c.military && c.military->battle_catalog.tactic_learning.has_value();
}
template <class DomainList>
DomainList military_tactic_learning_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 15)
        boundary::fail("MilitaryTacticLearningPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 16;
    }
  return domains;
}
bool military_command_resources_enabled(const PreparedCatalog &c) {
  return c.military && c.military->command_resources_policy.has_value();
}
template <class DomainList>
DomainList military_command_resources_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 14)
        boundary::fail("MilitaryCommandResourcesPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 15;
    }
  return domains;
}
bool military_sequential_enabled(const PreparedCatalog &c) {
  return c.military && c.military->sequential_round_policy.has_value();
}
bool military_commanded_damage_enabled(const PreparedCatalog &c) {
  return c.military && c.military->commanded_damage_policy.has_value();
}
template <class DomainList>
DomainList military_commanded_damage_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 13)
        boundary::fail("MilitaryCommandedDamagePrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 14;
    }
  return domains;
}
bool military_combat_conditions_enabled(const PreparedCatalog &c) {
  return c.military && c.military->combat_condition_policy.has_value();
}
bool military_garrison_rest_enabled(const PreparedCatalog &c) {
  return c.military && c.military->garrison_rest_policy.has_value();
}
bool military_command_shock_enabled(const PreparedCatalog &c) {
  return c.military && c.military->command_shock_policy.has_value();
}
template <class DomainList>
DomainList military_combat_conditions_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 12)
        boundary::fail("MilitaryCombatConditionPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 13;
    }
  return domains;
}
template <class DomainList>
DomainList military_garrison_rest_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 11)
        boundary::fail("MilitaryGarrisonRestPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 12;
    }
  return domains;
}
template <class DomainList>
DomainList military_command_shock_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 10)
        boundary::fail("MilitaryCommandShockPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 11;
    }
  return domains;
}
bool military_rout_enabled(const PreparedCatalog &c) {
  return c.military && c.military->rout_retreat_policy.has_value();
}
template <class DomainList>
DomainList military_rout_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 9)
        boundary::fail("MilitaryRoutPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 10;
    }
  return domains;
}
bool military_supply_attrition_enabled(const PreparedCatalog &c) {
  return c.military && c.military->supply_attrition_policy.has_value();
}
template <class DomainList>
DomainList military_supply_attrition_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 8)
        boundary::fail("MilitarySupplyAttritionPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 9;
    }
  return domains;
}
bool military_recovery_enabled(const PreparedCatalog &c) {
  return c.military && c.military->recovery_policy.has_value();
}
template <class DomainList>
DomainList military_recovery_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 7)
        boundary::fail("MilitaryRecoveryPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 8;
    }
  return domains;
}
bool field_commands_enabled(const PreparedCatalog &c) {
  return c.military && c.military->field_command_policy.has_value();
}
template <class DomainList>
DomainList field_command_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 6)
        boundary::fail("MilitaryFieldCommandPrerequisiteRequired",
                       "checkpoint/military");
      d.codec_version = 7;
    }
  return domains;
}
bool officers_enabled(const PreparedCatalog &c) {
  return c.military && c.military->officer_assignment_policy.has_value();
}
template <class DomainList> DomainList officer_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "military") {
      if (d.codec_version != 4 && d.codec_version != 5)
        boundary::fail("MilitaryOfficerAssignmentProfileMismatch",
                       "checkpoint/military");
      d.codec_version = 6;
    }
  return domains;
}
bool requests_enabled(const PreparedCatalog &catalog) {
  return catalog.military && catalog.military->request_policy.has_value();
}
template <class DomainList> DomainList request_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "military") {
      if (domain.codec_version != 4)
        boundary::fail("MilitaryRequestProfileRequired", "checkpoint/military");
      domain.codec_version = 5;
    }
  return domains;
}
