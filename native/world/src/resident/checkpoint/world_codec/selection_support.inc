// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
using namespace boundary;
#include "resident/domains/politics/world_codec/selection_enablement.inc"
BoundaryValue::Dict &dict(BoundaryValue &v) {
  auto *p = std::get_if<BoundaryValue::Dict>(&v.data);
  if (!p)
    fail("InvalidResidentObject", "");
  return *p;
}
void set(BoundaryValue &v, const StableId &key, BoundaryValue value) {
  if (auto *p = v.find(key))
    *p = std::move(value);
  else
    dict(v).emplace_back(key, std::move(value));
}
void merge_unique(BoundaryValue &to, const BoundaryValue &from) {
  for (const auto &r : object(from, "merge")) {
    if (to.find(r.first))
      fail("DuplicateDomainPayload", r.first);
    dict(to).push_back(r);
  }
}
void required_empty(const BoundaryValue &root, const StableId &key) {
  if (!object(field(root, key, ""), key).empty())
    fail("UnsupportedNativeCapability", key);
}
const std::vector<CheckpointDomain> checkpoint_domains = {
    {"common", 4},   {"travel", 3}, {"military", 3}, {"interaction", 2},
    {"social", 1},   {"estate", 2}, {"tasks", 1},    {"family", 1},
    {"personal", 1}, {"growth", 3}};
const std::vector<CheckpointDomain> dynamic_checkpoint_domains = {
    {"common", 5},        {"travel", 3},        {"military", 4},
    {"interaction", 2},   {"social", 1},        {"estate", 2},
    {"tasks", 1},         {"family", 1},        {"personal", 1},
    {"growth", 3},        {"local_economy", 1}, {"organization", 2},
    {"affairs", 1},       {"director", 1},      {"dynamic_tasks", 2},
    {"dynamic_social", 1}};
#include "resident/domains/military/world_codec/selection_population.inc"
// 地方生活只修改自己的四个版本；同目录未选择本能力的情景仍使用原域表。
template <class DomainList> DomainList regional_domains(DomainList domains) {
  for (auto &d : domains) {
    if (d.name == "director")
      d.codec_version = 2;
    else if (d.name == "growth")
      d.codec_version = 4;
    else if (d.name == "interaction")
      d.codec_version = 3;
    else if (d.name == "dynamic_social")
      d.codec_version = 2;
  }
  return domains;
}
const auto regional_checkpoint_domains =
    regional_domains(dynamic_checkpoint_domains);
const auto regional_population_checkpoint_domains =
    regional_domains(population_checkpoint_domains);
#include "resident/domains/military/world_codec/selection_siege.inc"
#include "resident/domains/local_economy/world_codec/selection_management.inc"
#include "resident/domains/family/world_codec/selection_creation.inc"
#include "resident/domains/local_economy/world_codec/selection_production.inc"
// 新任职只提升真正变化的共同回执、成员、工资和决策域；旧组合逐字节沿用。
template <class DomainList> DomainList named_domains(DomainList domains) {
  for (auto &d : domains) {
    if (d.name == "common")
      d.codec_version = 7;
    else if (d.name == "local_economy" || d.name == "organization" ||
             d.name == "director")
      d.codec_version = 3;
  }
  return domains;
}
const auto named_dynamic_checkpoint_domains =
    named_domains(dynamic_checkpoint_domains);
const auto named_population_checkpoint_domains =
    named_domains(population_checkpoint_domains);
const auto named_regional_checkpoint_domains =
    named_domains(regional_checkpoint_domains);
const auto named_regional_population_checkpoint_domains =
    named_domains(regional_population_checkpoint_domains);
const auto named_siege_checkpoint_domains =
    named_domains(siege_checkpoint_domains);
const auto named_siege_population_checkpoint_domains =
    named_domains(siege_population_checkpoint_domains);
const auto named_siege_regional_checkpoint_domains =
    named_domains(siege_regional_checkpoint_domains);
const auto named_siege_regional_population_checkpoint_domains =
    named_domains(siege_regional_population_checkpoint_domains);
const std::vector<CheckpointDomain> &
legacy_domains_for(const PreparedCatalog &catalog) {
  if (siege_enabled(catalog)) {
    if (catalog.regional_life)
      return population_enabled(catalog)
                 ? siege_regional_population_checkpoint_domains
                 : siege_regional_checkpoint_domains;
    return population_enabled(catalog) ? siege_population_checkpoint_domains
                                       : siege_checkpoint_domains;
  }
  if (catalog.regional_life)
    return population_enabled(catalog) ? regional_population_checkpoint_domains
                                       : regional_checkpoint_domains;
  return population_enabled(catalog) ? population_checkpoint_domains
         : catalog.dynamic_profile == DynamicProfile::local_society_v1
             ? dynamic_checkpoint_domains
             : checkpoint_domains;
}
const std::vector<CheckpointDomain> &
base_domains_for(const PreparedCatalog &catalog) {
  const auto &base = legacy_domains_for(catalog);
  if (!catalog.named_membership)
    return base;
  if (&base == &dynamic_checkpoint_domains)
    return named_dynamic_checkpoint_domains;
  if (&base == &population_checkpoint_domains)
    return named_population_checkpoint_domains;
  if (&base == &regional_checkpoint_domains)
    return named_regional_checkpoint_domains;
  if (&base == &regional_population_checkpoint_domains)
    return named_regional_population_checkpoint_domains;
  if (&base == &siege_checkpoint_domains)
    return named_siege_checkpoint_domains;
  if (&base == &siege_population_checkpoint_domains)
    return named_siege_population_checkpoint_domains;
  if (&base == &siege_regional_checkpoint_domains)
    return named_siege_regional_checkpoint_domains;
  if (&base == &siege_regional_population_checkpoint_domains)
    return named_siege_regional_population_checkpoint_domains;
  return base;
}
#include "resident/domains/growth/world_codec/selection_study.inc"
#include "resident/domains/director/world_codec/selection_registry.inc"
#include "resident/domains/affairs/world_codec/selection_needs.inc"
bool outcome_enabled(const PreparedCatalog &catalog) {
  return catalog.interaction &&
         !catalog.interaction->regional_outcome_topics.empty();
}
// 新共同格式逐一对应旧5/6/7，不能用一个版本混读不同旧回执形状。
template <class DomainList>
DomainList extended_skill_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "common") {
      if (domain.codec_version < 5 || domain.codec_version > 7)
        boundary::fail("ExtendedSkillsNativeProfileRequired",
                       "checkpoint/common");
      domain.codec_version += 3;
    }
  return domains;
}
#include "resident/domains/growth/world_codec/selection_cards_teaching.inc"
#include "resident/domains/politics/world_codec/selection_diplomacy_governance.inc"
#include "resident/domains/military/world_codec/selection_command.inc"
#include "resident/domains/politics/world_codec/selection_takeover.inc"
#include "resident/domains/family/world_codec/selection_lifecycle.inc"
#include "resident/domains/commerce/world_codec/fact_kind_selection.inc"
// 原BoundaryValue兼容出口只适配sink；所有语义/字段选择由同一emit执行。
template <class Emit> Outcome<BoundaryValue> projection_value(Emit emit) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    auto status = writer.object("", emit);
    if (status.ok)
      status = writer.finish();
    if (!status.ok)
      return {false, {}, std::move(status.error)};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
#include "resident/domains/commerce/world_codec/shop_estate_fact_projection.inc"
#include "resident/domains/family/world_codec/apprenticeship_fact_projection.inc"
#include "resident/domains/commerce/world_codec/shop_market_fact_projection.inc"
#include "resident/domains/family/world_codec/selection_birth_succession.inc"
#include "resident/domains/player_plan/world_codec/selection_version.inc"
#include "resident/domains/travel/world_codec/selection_physical_domains.inc"
Status configure_domain_resource_references(CodecVisitor &codec,
                                            const CatalogIdentity &identity) {
  auto selected = select_common_codec(5, identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto &flags = selected.value.features;
  codec.item_instance_refs_enabled = flags.item_instances;
  codec.lot_property_refs_enabled = flags.lot_properties;
  codec.domain_activity_reservation_refs_enabled = flags.domain_activity;
  codec.carrier_location_enabled = flags.carrier_location;
  return common_success();
}
// 全World读、写与标题只读共用实际P06表，不能把高版本号当extended-skills。
Status configure_common_physical_codec(CodecVisitor &codec, Integer version,
                                       const CatalogIdentity &identity) {
  auto actual = decode_common_codec_selection(version);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  auto pair = validate_common_codec_selection(actual.value.base_version,
                                              identity, version);
  if (!pair.ok)
    return pair;
  const auto &f = actual.value.features;
  if (f.appearance || f.equipment_instances)
    return common_failure("CommonSelectedExtensionFieldsRequired", {},
                          ErrorKind::unsupported_capability);
  auto control = selected_native_control_continuation_version(identity);
  if (!control.ok)
    return {false, {}, std::move(control.error)};
  codec.control_continuation_codec_version = control.value;
  auto field = selected_field_battle_version(identity);
  if (!field.ok)
    return {false, {}, std::move(field.error)};
  codec.field_battle_codec_version = field.value;
  auto work = selected_native_task_work_version(identity);
  if (!work.ok)
    return {false, {}, std::move(work.error)};
  codec.task_work_execution_codec_version = work.value;
  auto face = selected_native_face_to_face_version(identity);
  if (!face.ok)
    return {false, {}, std::move(face.error)};
  codec.face_to_face_codec_version = face.value;
  bool household_seen = false;
  codec.household_purchase_codec_version = 0;
  for (const auto &module : identity.module_versions) {
    if (module.first != "household_purchases")
      continue;
    if (household_seen || (module.second != "2" && module.second != "3"))
      return common_failure("InvalidHouseholdPurchaseReceiptSelection");
    household_seen = true;
    codec.household_purchase_codec_version = module.second == "2" ? 2 : 3;
  }
  auto military = selected_native_military_state_versions(identity);
  if (!military.ok)
    return {false, {}, std::move(military.error)};
  codec.dynamic_military_codec_version = military.value.dynamic_military;
  codec.item_instance_refs_enabled = f.item_instances;
  codec.lot_property_refs_enabled = f.lot_properties;
  codec.domain_activity_reservation_refs_enabled = f.domain_activity;
  codec.carrier_location_enabled = f.carrier_location;
  if (actual.value.base_version == 7 || actual.value.base_version == 10)
    codec.schema_features |= common_checkpoint::named_membership_receipts;
  if (common_codec_extended_skills(actual.value))
    codec.schema_features |= common_checkpoint::extended_actor_skills;
  return common_success();
}
