// 家庭、收养与永久身份来源的唯一typed域；钱包、物资、人物头仍在共同候选内。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "household_control.hpp"
#include "character_creation_source.hpp"
#include "adult_adoption_source.hpp"
#include "voluntary_succession_model.hpp"
#include <set>
namespace hanmo::resident {
struct FamilyLegacyState;
struct FamilyOriginV2Sources;
struct LifeAgeSources;
struct FamilyPolicy {
  Integer child_age_years = 0, guardian_min_age = 0, heir_min_age = 0,
          retirement_min_age = 0;
  std::optional<bool> enable_travel_plan;
  FieldOrder field_order{};
};
struct FamilyConfig {
  StableId prototype_id, child_name_key, wallet_id;
  Integer fee = 0, adoption_ticks = 0;
  FamilyPolicy policy;
  bool valid = false;
};
struct IdentityPrototype {
  ActorAttributes attributes;
  ActorSkills skills;
  FieldOrder attributes_order, skills_order;
  StableId appearance_kind, attire_style;
  AppearanceFeatures fixed_features;
  OrderedTable<std::vector<StableId>> candidates;
  bool historical = false, has_travel_plan = false;
};
// 旧双人和新版可配名单有明确模式；目录模式不写入家庭持久状态。
enum class FamilyFounderSelection {
  legacy_pair,
  configured_roster,
  created_character
};
// 允许表来自所选情景；空表保持旧固定创始人，新profile的生成来源必须具备完整六艺。
// 仅正规Installed显式25..32配对可选择living_paths；原1..16仍早核全部原许可。
enum class FamilyVoluntarySourceMode { authored_roles = 1, living_paths = 2 };
struct FamilyCatalog {
  bool enabled = false;
  StableId founder_actor_id;
  OrderedTable<FamilyConfig> roles;
  OrderedTable<IdentityPrototype> prototypes;
  std::vector<StableId> allowed_founder_actor_ids;
  FamilyFounderSelection founder_selection =
      FamilyFounderSelection::legacy_pair;
  bool extended_skills = false;
  std::optional<HouseholdControlCatalog> household_control;
  std::optional<CharacterCreationCatalog> character_creation;
  std::optional<AdultAdoptionRequestCatalog> adult_adoption;
  std::optional<SuccessionConsentCatalog> voluntary_succession;
  FamilyVoluntarySourceMode voluntary_source_mode =
      FamilyVoluntarySourceMode::authored_roles;
};
struct AdoptionTerms {
  StableId guardian_actor_id, provider_actor_id, role_id, place_id,
      prototype_id, display_name;
  Integer birth_tick = 0, duration = 0, fee = 0;
  StableId payer_wallet_id, recipient_wallet_id;
  FamilyPolicy policy;
  FieldOrder field_order{};
};
struct AdoptionFact {
  StableId kind = "family_adoption_pending", guardian_actor_id;
  Integer start_tick = 0;
  AdoptionTerms terms;
  InteractionContext context;
  Integer completed_tick = -1;
  StableId child_actor_id;
  bool success = false;
  FieldOrder field_order{};
};
struct IdentityCreationFact {
  StableId prototype_id, actor_id;
  Integer created_tick = 0, birth_tick = 0;
  StableId display_name, source;
  FieldOrder field_order{};
};
struct SuccessionWallet {
  StableId source_id, destination_id;
  Integer amount = 0;
  FieldOrder field_order{};
};
struct SuccessionGoods {
  StableId source_inventory_id, destination_inventory_id, lot_id, item_id;
  Integer quantity = 0;
  FieldOrder field_order{};
};
struct SuccessionEstate {
  StableId estate_id, inventory_id, wallet_id;
  FieldOrder field_order{};
};
struct SuccessionAssets {
  std::vector<SuccessionWallet> wallets;
  std::vector<SuccessionGoods> goods;
  std::vector<SuccessionEstate> estates;
  std::vector<StableId> disabled_roles;
  FieldOrder field_order{};
};
struct FamilyChoice {
  StableId kind, guardian_actor_id, heir_actor_id, adoption_source, place_id;
  Integer tick = 0;
  std::optional<StableId> designation_source;
  std::optional<SuccessionAssets> assets;
  FieldOrder field_order{};
};
using FamilyFact =
    std::variant<AdoptionFact, IdentityCreationFact, FamilyChoice,
                 HouseholdControlFact, CharacterCreationFact, AdultAdoptionFact,
                 SuccessionConsentFact, VoluntarySuccessionFact>;
struct FamilyAction {
  AdoptionTerms training;
  InteractionContext context;
  FieldOrder field_order{};
};
struct FamilyState {
  StableId founder_actor_id;
  OrderedTable<StableId> children, designations;
  std::vector<StableId> successions;
  OrderedTable<FamilyFact> facts;
  OrderedTable<FamilyAction> actions;
  FieldOrder field_order{};
};
// 初始准入只核作者模式与同一人物集合，当前玩家仍须另通过完整继承链。
inline Status validate_family_founder(const CommonState &common,
                                      const FamilyState &family,
                                      const FamilyCatalog &catalog) {
  if (catalog.founder_selection == FamilyFounderSelection::created_character) {
    if (!catalog.character_creation ||
        !catalog.allowed_founder_actor_ids.empty() ||
        !common.actors.find(family.founder_actor_id))
      return common_failure("InvalidFamilyFounder");
    for (const auto &row : family.facts.rows())
      if (const auto *source = std::get_if<CharacterCreationFact>(&row.second);
          source && source->actor_id == family.founder_actor_id &&
          source->prototype_actor_id ==
              catalog.character_creation->prototype_actor_id)
        return common_success();
    return common_failure("MissingCharacterCreationSource");
  }
  if (catalog.founder_selection != FamilyFounderSelection::legacy_pair &&
      catalog.founder_selection != FamilyFounderSelection::configured_roster)
    return common_failure("InvalidFamilyFounder");
  if (catalog.allowed_founder_actor_ids.empty())
    return catalog.founder_selection == FamilyFounderSelection::legacy_pair &&
                   family.founder_actor_id == catalog.founder_actor_id
               ? common_success()
               : common_failure("InvalidFamilyFounder");
  const std::set<StableId> allowed(catalog.allowed_founder_actor_ids.begin(),
                                   catalog.allowed_founder_actor_ids.end());
  const bool count_valid =
      catalog.founder_selection == FamilyFounderSelection::legacy_pair
          ? catalog.allowed_founder_actor_ids.size() == 2
          : catalog.allowed_founder_actor_ids.size() <= 4096;
  if (!count_valid ||
      allowed.size() != catalog.allowed_founder_actor_ids.size() ||
      allowed.count(StableId{}) || !allowed.count(catalog.founder_actor_id) ||
      !allowed.count(family.founder_actor_id))
    return common_failure("InvalidFamilyFounder");
  for (const auto &id : allowed)
    if (!catalog.prototypes.find(id) || !common.actors.find(id))
      return common_failure("InvalidFamilyFounder", id);
  return common_success();
}
// 生成人物只使用与本世界相容的原型；缺失六艺和越界经验在收费、分配ID之前明确拒绝。
inline Status
family_prototype_skill_eligibility(const FamilyCatalog &catalog,
                                   const IdentityPrototype &prototype) {
  if (catalog.extended_skills && !prototype.skills.extended)
    return common_failure("FamilyPrototypeSkillsRequired");
  if (!catalog.extended_skills && prototype.skills.extended)
    return common_failure("FamilyPrototypeSkillsUnsupported");
  if (prototype.skills.extended) {
    const auto &value = *prototype.skills.extended;
    for (const auto xp : {value.naval, value.espionage, value.fortification,
                          value.agriculture, value.medicine, value.forging})
      if (xp < 0 || xp > 1200)
        return common_failure("InvalidPrototypeExtendedSkills");
  }
  return common_success();
}
struct FamilyCommand {
  StableId kind;
  std::optional<StableId> display_name, heir_actor_id;
  InteractionContext context;
  std::optional<StableId> request_id, adoptee_actor_id, plan_id;
};
struct FamilyQuote {
  Integer duration = 0;
  std::optional<Integer> cost;
  std::optional<InteractionContext> context;
  std::optional<AdoptionTerms> training;
  std::optional<FamilyChoice> family;
  std::optional<AdultAdoptionRequestQuote> adult_adoption;
  std::optional<SuccessionConsentQuote> succession_consent;
  std::optional<SuccessionPersonalQuote> succession_personal;
  std::optional<StableId> succession_plan_id;
};
struct FamilyEffect {
  std::optional<bool> success, already_completed;
  std::optional<StableId> code, actor_id, fact_id, heir_actor_id,
      player_actor_id;
  FieldOrder field_order{};
};
// 跨域只借当前真值；动态旅行初始化由原旅行owner执行，不让家事复制旅程规则。
struct FamilyAccess {
  EstateAccess estate_access;
  const InteractionState *interaction = nullptr;
  const InteractionCatalog *interaction_catalog = nullptr;
  const InteractionAccess *interaction_access = nullptr;
  const TaskState *tasks = nullptr;
  std::function<bool(const StableId &, bool)> military_or_office_pending;
  std::function<Status(const StableId &, const StableId &, bool)>
      initialize_schedule;
  const LocalEconomyState *economy = nullptr;
  const FamilyLegacyState *legacy = nullptr;
  std::function<Status(const ActorRecord &)> biological_identity;
  // Borrowed from the same candidate/job; never derive initial consent age from
  // today.
  const FamilyOriginV2Sources *origin_v2 = nullptr;
  const LifeAgeSources *initial_age = nullptr;
};
Outcome<FamilyCatalog> decode_family_catalog(const BoundaryValue &data,
                                             const StableId &scenario_id);
Outcome<FamilyState> decode_family_seed(const BoundaryValue &,
                                        bool household_control = false,
                                        bool character_creation = false,
                                        bool extended_skills = false,
                                        bool adult_adoption = false,
                                        bool voluntary_succession = false,
                                        bool biological_birth = false);
BoundaryValue encode_family_state(const FamilyState &);
Outcome<FamilyCommand> decode_family_command(const StableId &,
                                             const BoundaryValue &);
BoundaryValue encode_family_quote(const FamilyQuote &);
BoundaryValue encode_family_effect(const FamilyEffect &);
Outcome<FamilyEffect> decode_family_effect(const BoundaryValue &);
Status visit_family_checkpoint(CodecVisitor &, FamilyState &,
                               bool household_control = false,
                               bool character_creation = false,
                               bool extended_skills = false,
                               bool adult_adoption = false,
                               bool voluntary_succession = false);
Status visit_family_effect(CodecVisitor &, FamilyEffect &);
Outcome<FamilyQuote> preview_family(const CommonStore &, const FamilyState &,
                                    const FamilyCatalog &,
                                    const CommonCatalog &, const EstateState &,
                                    const EstateCatalog &, const FamilyAccess &,
                                    const StableId &, const FamilyCommand &,
                                    const StableId &own_action = {});
Status start_family_action(CommonStore &, FamilyState &, const FamilyCatalog &,
                           const CommonCatalog &, const EstateState &,
                           const EstateCatalog &, const FamilyAccess &,
                           const ActionCoreView &, const FamilyCommand &,
                           const FamilyQuote &);
Outcome<FamilyEffect> finish_family_action(
    CommonStore &, FamilyState &, InteractionState &, SocialState &,
    const FamilyCatalog &, const CommonCatalog &, const EstateState &,
    const EstateCatalog &, const FamilyAccess &, const ActionCoreView &);
Outcome<FamilyEffect> apply_family_instant(
    CommonStore &, FamilyState &, EstateState &, const FamilyCatalog &,
    const CommonCatalog &, const EstateCatalog &, const FamilyAccess &,
    const StableId &, const FamilyCommand &, const FamilyQuote &);
Status validate_family(const CommonStore &, const FamilyState &,
                       const FamilyCatalog &, const CommonCatalog &,
                       const EstateState &, const EstateCatalog &,
                       const EstateAccess *estate_access = nullptr,
                       const LocalEconomyState *economy = nullptr,
                       const InteractionCatalog *adult_interaction = nullptr,
                       const FamilyLegacyState *legacy = nullptr,
                       const std::function<Status(const ActorRecord &)>
                           *biological_identity = nullptr,
                       const FamilyOriginV2Sources *origin_v2 = nullptr,
                       const LifeAgeSources *initial_age = nullptr);
Status validate_family_identity(const CommonStore &, const FamilyState &,
                                const FamilyCatalog &, const ActorRecord &);
std::vector<StableId> family_children(const FamilyState &,
                                      const StableId &guardian);
bool family_travel_enabled(const CommonStore &, const FamilyState &,
                           const StableId &);
} // namespace hanmo::resident
namespace hanmo::resident {
// 休整账本由services唯一owner投影；家庭页仅嵌入既有行，不能自行算休整结算。
using FamilyRestRows =
    std::function<Outcome<BoundaryValue>(const StableId &actor)>;
Outcome<BoundaryValue> query_family_view(const CommonStore &,
                                         const FamilyState &,
                                         const CommonCatalog &,
                                         const StableId &query,
                                         const BoundaryValue &args,
                                         const FamilyRestRows &rest_rows = {});
} // namespace hanmo::resident
