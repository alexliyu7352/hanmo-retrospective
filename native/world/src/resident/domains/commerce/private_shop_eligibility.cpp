// 成功必须同时有本人年龄、可证商术、三个原客户和原司法许可；后来成功不能倒填开店时刻。
#include "private_shop_eligibility.hpp"
#include "private_shop.hpp"
#include "private_shop_age3.hpp"
#include "resident/domains/family/actor_age_birth_read.hpp"
#include "resident/domains/growth/growth_skill_history.hpp"
#include "resident/domains/growth/growth_skill_origin.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 身份账借口先核原目录身份；只有实际出生来源要求Birth账，旧作者夹具不因缺可选借口改变行为。
Status age_source(const CommonStore &common, const CommonCatalog &catalog,
                  const FamilyCatalog &family,
                  const GrowthSkillOriginContext *origins,
                  const StableId &actor) {
  if (origins &&
      (&origins->common != &catalog || &origins->family_catalog != &family))
    return common_failure("PrivateShopSkillOriginCandidateMismatch", actor);
  const auto *created = common.state().facts.find(actor);
  if (created && created->kind == "actor_creation" &&
      created->domain == "family_birth" && (!origins || !origins->births))
    return common_failure("PrivateShopBirthAgeHistoryUnavailable", actor);
  return common_success();
}
} // namespace
Status validate_private_shop_client_completion_source(
    const CommonStore &common, const TaskFulfilledClientSource &source,
    Integer tick, const PrivateShopOpeningBoundary *before) {
  auto bad = [&](const char *code) {
    return common_failure(code, source.task_id, ErrorKind::internal);
  };
  if (tick < 0 || source.completed_tick < 0 || source.completed_tick > tick ||
      source.completed_at.has_value() != source.completion_ref.has_value())
    return bad("PrivateShopClientCompletionShapeMismatch");
  if (!source.completed_at)
    return common_success();
  const auto &actual = *source.completed_at;
  const auto &ref = *source.completion_ref;
  auto valid = compare_world_moments(actual, actual);
  if (!valid.ok)
    return {false, {}, valid.error};
  // The actual Task reader selects its terminal schema from its explicit
  // execution root. Preserve that finite original1/2 proof, never infer a
  // version from this row or relabel legacy integer completion.
  if (actual.half_day_tick != source.completed_tick ||
      (ref.source_schema != 1 && ref.source_schema != 2) ||
      ref.source_ref.domain != "tasks" ||
      ref.source_ref.kind != "task_action_finished" ||
      ref.source_ref.id.empty())
    return bad("PrivateShopExactClientCompletionMismatch");
  const auto *head = common.state().facts.find(ref.source_ref.id);
  const auto &order = common.state().facts.order();
  const auto at = std::find(order.begin(), order.end(), ref.source_ref.id);
  if (!head || head->id != ref.source_ref.id ||
      head->domain != ref.source_ref.domain ||
      head->kind != ref.source_ref.kind || at == order.end())
    return bad("PrivateShopExactClientCompletionMissing");
  if (before) {
    const auto *opening = common.state().facts.find(before->opening_fact_id);
    const auto cut =
        std::find(order.begin(), order.end(), before->opening_fact_id);
    if (before->opening_tick != tick || !opening ||
        opening->id != before->opening_fact_id ||
        opening->domain != private_shop_module ||
        opening->kind != private_shop_open_kind || cut == order.end() ||
        at >= cut)
      return bad("PrivateShopExactClientCompletionOrder");
  }
  // These are the Task owner's actual transfer/delivery sources, not a second
  // completion proof. All must already exist before its sealed report result.
  for (const auto &id : source.objective_fact_ids) {
    const auto objective = std::find(order.begin(), order.end(), id);
    if (!common.state().facts.find(id) || objective == order.end() ||
        objective >= at)
      return bad("PrivateShopExactClientObjectiveOrder");
  }
  return common_success();
}
Outcome<ActorAgeRead> read_private_shop_party_age(
    const CommonStore &common, const CommonCatalog &catalog,
    const FamilyCatalog &family, const GrowthSkillOriginContext *origins,
    const StableId &actor) {
  auto source = age_source(common, catalog, family, origins, actor);
  if (!source.ok)
    return {false, {}, source.error};
  return origins && origins->births
             ? read_actor_age(common, catalog, *origins->births, actor)
             : read_actor_age(common, catalog, actor);
}
Outcome<ActorAgeRead> read_private_shop_party_age_before_fact(
    const CommonStore &common, const CommonCatalog &catalog,
    const FamilyCatalog &family, const GrowthSkillOriginContext *origins,
    const StableId &actor, Integer tick, const StableId &actual_fact) {
  auto source = age_source(common, catalog, family, origins, actor);
  if (!source.ok)
    return {false, {}, source.error};
  const auto *head = common.state().facts.find(actual_fact);
  StableId prefix;
  if (head) {
    if (head->kind == private_shop_open_kind)
      prefix = private_shop_open_prefix;
    else if (head->kind == "private_shop_staff_accepted")
      prefix = "private_shop_staff";
    else if (head->kind == "private_shop_order_issued")
      prefix = "private_shop_order";
  }
  const auto number = prefix.empty()
                          ? std::nullopt
                          : organization_instance_sequence(actual_fact, prefix);
  if (!head || head->id != actual_fact || head->domain != private_shop_module ||
      !number || *number >= common.state().next_instance_id)
    return fail<ActorAgeRead>("PrivateShopAgeFactBoundaryRequired");
  return origins && origins->births
             ? read_actor_age_before_fact(common, catalog, *origins->births,
                                          actor, tick, actual_fact)
             : read_actor_age_before_fact(common, catalog, actor, tick,
                                          actual_fact);
}
Outcome<PrivateShopEligibility> read_private_shop_opening_eligibility(
    const CommonStore &common, const CommonCatalog &catalog,
    const FamilyCatalog &family, const TaskFulfilledClientRead &clients,
    const PrivateShopPermissionRead *law, const PrivateShopPremises &premises,
    const StableId &actor, const PrivateShopOpeningBoundary *before,
    const GrowthState *study_history,
    const GrowthSkillOriginContext *skill_origins,
    const PrivateShopContext *selected_context) {
  const auto tick = before ? before->opening_tick : common.tick();
  const auto *person = common.actor(actor);
  if (!person || premises.id.empty() || premises.place_id.empty() ||
      actor.empty() || tick < 0 || tick > common.tick())
    return fail<PrivateShopEligibility>("PrivateShopOpeningBoundaryMismatch");
  if (before) {
    const auto *head = common.state().facts.find(before->opening_fact_id);
    const auto sequence = organization_instance_sequence(
        before->opening_fact_id, private_shop_open_prefix);
    if (!head || head->kind != private_shop_open_kind ||
        head->domain != private_shop_module || !sequence ||
        *sequence >= common.state().next_instance_id)
      return fail<PrivateShopEligibility>("PrivateShopOpeningFactRequired");
  } else if (!person->alive || !person->available)
    return fail<PrivateShopEligibility>("PrivateShopProprietorUnavailable");
  if (selected_context) {
    if (&selected_context->common != &common ||
        &selected_context->common_catalog != &catalog ||
        &selected_context->family_catalog != &family ||
        selected_context->skill_origins != skill_origins)
      return fail<PrivateShopEligibility>("PrivateShopAgeContextMismatch");
    auto age =
        before ? read_private_shop_party_adult_before_fact(
                     *selected_context, actor, tick, before->opening_fact_id)
               : read_private_shop_party_adult(*selected_context, actor);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return fail<PrivateShopEligibility>("PrivateShopAdultRequired");
  } else {
    auto age = before ? read_private_shop_party_age_before_fact(
                            common, catalog, family, skill_origins, actor, tick,
                            before->opening_fact_id)
                      : read_private_shop_party_age(common, catalog, family,
                                                    skill_origins, actor);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return fail<PrivateShopEligibility>("PrivateShopAdultRequired");
  }
  if (!study_history)
    return fail<PrivateShopEligibility>(
        "PrivateShopCommerceHistoryUnavailable");
  if (skill_origins && (&skill_origins->common != &catalog ||
                        &skill_origins->family_catalog != &family))
    return fail<PrivateShopEligibility>(
        "PrivateShopSkillOriginCandidateMismatch");
  auto skill =
      skill_origins
          ? (before ? read_commerce_study_qualification_before_opening(
                          common, *skill_origins, *study_history, actor, tick,
                          before->opening_fact_id)
                    : read_commerce_study_qualification(common, *skill_origins,
                                                        *study_history, actor))
          : (before ? read_commerce_study_qualification_before_opening(
                          common, catalog, family, *study_history, actor, tick,
                          before->opening_fact_id)
                    : read_commerce_study_qualification(common, catalog, family,
                                                        *study_history, actor));
  if (!skill.ok)
    return {false, {}, skill.error};
  if (!identity(skill.value.source_identity, common.state().identity) ||
      skill.value.actor_id != actor || skill.value.observed_tick != tick ||
      skill.value.skill_id != "commerce" ||
      skill.value.source_kind != "effective_initial_and_permanent_study" ||
      skill.value.boundary_fact_id !=
          (before ? before->opening_fact_id : StableId{}))
    return fail<PrivateShopEligibility>("PrivateShopCommerceSourceMismatch");
  if (skill.value.minimum_level < 2)
    return fail<PrivateShopEligibility>("PrivateShopCommerceLevelRequired");
  auto history = before ? clients.task_fulfilled_clients_before_shop(
                              actor, before->opening_fact_id, tick)
                        : clients.task_fulfilled_clients(actor);
  if (!history.ok)
    return {false, {}, history.error};
  if (!identity(history.value.source_identity, common.state().identity) ||
      history.value.actor_id != actor)
    return fail<PrivateShopEligibility>("PrivateShopClientSourceMismatch");
  std::set<std::pair<Owner::Kind, StableId>> distinct;
  std::vector<TaskFulfilledClientSource> qualified;
  for (const auto &source : history.value.clients) {
    auto completion = validate_private_shop_client_completion_source(
        common, source, tick, before);
    if (!completion.ok)
      return {false, {}, completion.error};
    const auto &client = source.original_client;
    if ((client.kind != Owner::Kind::actor &&
         client.kind != Owner::Kind::organization) ||
        client.id.empty() ||
        (client.kind == Owner::Kind::actor && client.id == actor) ||
        source.task_id.empty() || source.settlement_id.empty() ||
        source.objective_fact_ids.empty() || source.completed_tick < 0 ||
        source.completed_tick > tick)
      return fail<PrivateShopEligibility>("PrivateShopClientSourceMismatch");
    if (!distinct.emplace(client.kind, client.id).second)
      return fail<PrivateShopEligibility>("PrivateShopDuplicateClient");
    if (qualified.size() < 3)
      qualified.push_back(source);
  }
  if (qualified.size() != 3)
    return fail<PrivateShopEligibility>("PrivateShopThreeClientsRequired");
  if (!law)
    return fail<PrivateShopEligibility>(
        "PrivateShopLegalPermissionUnavailable");


  auto request_memory = CheckpointMemoryCharge(0);
  PrivateShopPermissionRequest request{
      checkpoint_fresh_string_copy(actor),
      checkpoint_fresh_string_copy(premises.id),
      checkpoint_fresh_string_copy(premises.place_id)};
  auto allowed = [&]() -> Outcome<OwnedPrivateShopPermissionSource> {
    if (selected_context && selected_context->state.version == 3)
      return before ? law->private_shop_opening_permission_before_fact_owned(
                          request, before->opening_fact_id, tick)
                    : law->private_shop_opening_permission_owned(request);
    // 旧 provider 裸ABI保持原行为；此处不把其返回前沿冒充迁完。
    auto legacy = before ? law->private_shop_opening_permission_before_fact(
                               request, before->opening_fact_id, tick)
                         : law->private_shop_opening_permission(request);
    if (!legacy.ok)
      return {false, {}, legacy.error};
    const auto &p = legacy.value;
    return {true,
            own_private_shop_permission_source(
                p.source_identity, p.request, p.permit_fact_id,
                p.clearance_fact_id, p.issued_tick, p.observed_tick,
                p.valid_through_tick),
            {}};
  }();
  if (!allowed.ok)
    return {false, {}, allowed.error};
  const auto &p = allowed.value.source();
  if (!identity(p.source_identity, common.state().identity) ||
      p.request.actor_id != actor || p.request.premises_id != premises.id ||
      p.request.place_id != premises.place_id || p.permit_fact_id.empty() ||
      p.clearance_fact_id.empty() || p.issued_tick < 0 ||
      p.issued_tick > tick || p.observed_tick != tick ||
      p.valid_through_tick < tick)
    return fail<PrivateShopEligibility>("PrivateShopLegalSourceMismatch");
  const auto &facts = common.state().facts.order();
  const auto boundary =
      before ? std::find(facts.begin(), facts.end(), before->opening_fact_id)
             : facts.end();
  struct PermissionHead {
    const StableId *id;
    const char *prefix;
    const char *kind;
  };
  for (const auto &proof :
       {PermissionHead{&p.permit_fact_id, "political_shop_permit",
                       "political_shop_permit_issued"},
        PermissionHead{&p.clearance_fact_id, "political_shop_clearance",
                       "political_shop_clearance_recorded"}}) {
    const auto &id = *proof.id;
    const auto *head = common.state().facts.find(id);
    const auto sequence = organization_instance_sequence(id, proof.prefix);
    const auto at = std::find(facts.begin(), facts.end(), id);
    if (!head || head->kind != proof.kind ||
        head->domain != "political_shop_licensing" || !sequence ||
        *sequence >= common.state().next_instance_id || at == facts.end() ||
        (before && at >= boundary))
      return fail<PrivateShopEligibility>("PrivateShopLegalSourceOrder");
  }
  return {true,
          {common.state().identity, actor, premises.id, tick,
           skill.value.minimum_xp, skill.value.minimum_level,
           skill.value.study_source_ids, std::move(qualified),
           PrivateShopPermissionValue(std::move(allowed.value))},
          {}};
}
} // namespace hanmo::resident
