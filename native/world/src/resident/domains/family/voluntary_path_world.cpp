// 先核原人物/关系与真实会面，后验本次生前许可；不从今日控制人回填旧声明。
#include "voluntary_path_world.hpp"
#include "voluntary_family_readiness.hpp"
#include "voluntary_succession.hpp"
#include "family_codec_version.hpp"
#include "historical_player.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/native_family_initial_sources.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue.hpp"
#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_catalog.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply.hpp"
#include "resident/domains/tasks/extensions/task_extension_state.hpp"
#include "voluntary_path_control.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_catalog.hpp"
namespace hanmo::resident {
namespace {
// 同一已选目录的明确声明逐种核对，不能把一句死后同意/雇用/收养兼作生前换控。
Outcome<bool> other_declaration(const WorldBundle &w, const StableId &dialogue,
                                const StableId &node) {
  const auto &c = *w.catalog;
  auto is = [&](const StableId &d, const StableId &n) {
    return d == dialogue && n == node;
  };
  if (c.family->adult_adoption)
    for (const auto &r : c.family->adult_adoption->definitions.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  if (c.family_legacy)
    for (const auto &r : c.family_legacy->definitions.rows())
      if (is(r.second.dialogue_id, r.second.accept_node_id) ||
          is(r.second.dialogue_id, r.second.revoke_node_id))
        return {true, true, {}};
  if (c.family_marriage)
    for (const auto &r : c.family_marriage->statements.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  if (c.family_apprenticeship)
    for (const auto &r : c.family_apprenticeship->statements.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  if (c.family_birth)
    for (const auto &r : c.family_birth->arrangements.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  if (c.family_care_relief)
    for (const auto &r : c.family_care_relief->offers.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  if (c.family_biological_legacy)
    for (const auto &r : c.family_biological_legacy->definitions.rows()) {
      auto d = r.second.dialogue_id;
      if (!r.second.template_id.empty()) {
        if (!c.born_personal_dialogues)
          return {
              false,
              {},
              common_failure("VoluntaryDeclarationTemplateRequired", r.first)
                  .error};
        const auto *t = born_personal_dialogue_template(
            *c.born_personal_dialogues, r.second.template_id);
        if (!t)
          return {
              false,
              {},
              common_failure("VoluntaryDeclarationTemplateRequired", r.first)
                  .error};
        d = t->dialogue_id;
      }
      if (is(d, r.second.accept_node_id) || is(d, r.second.revoke_node_id))
        return {true, true, {}};
    }
  if (c.family_collateral_legacy)
    for (const auto &r : c.family_collateral_legacy->definitions.rows()) {
      const auto *t = known_personal_dialogue_template(
          *c.known_personal_dialogues, r.second.template_id);
      if (!t)
        return {false,
                {},
                common_failure("VoluntaryDeclarationTemplateRequired", r.first)
                    .error};
      if (is(t->dialogue_id, r.second.recognize_node_id) ||
          is(t->dialogue_id, r.second.accept_node_id) ||
          is(t->dialogue_id, r.second.revoke_node_id))
        return {true, true, {}};
    }
  if (c.growth) {
    for (const auto &r : c.growth->teaching_requests.definitions.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
    for (const auto &r : c.growth->study_teaching_requests.definitions.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  }
  if (c.growth_tactic_lessons)
    for (const auto &r : c.growth_tactic_lessons->definitions.rows())
      if (is(r.second.dialogue_id, r.second.offer_node_id))
        return {true, true, {}};
  if (c.estate_employment)
    for (const auto &r : c.estate_employment->policies.rows()) {
      const auto &p = r.second;
      if (is(p.offer_dialogue_id, p.offer_node_id) ||
          is(p.worker_dialogue_id, p.worker_node_id) ||
          is(p.provider_offer_dialogue_id, p.provider_offer_node_id) ||
          is(p.provider_reply_dialogue_id, p.provider_reply_node_id))
        return {true, true, {}};
    }
  if (c.private_shop_liquidations)
    for (const auto &r : c.private_shop_liquidations->policies.rows()) {
      const auto &p = r.second;
      if (is(p.proprietor_dialogue_id, p.proprietor_appointment_node_id) ||
          is(p.executor_dialogue_id, p.executor_acceptance_node_id))
        return {true, true, {}};
    }
  if (c.private_shops) {
    for (const auto &r : c.private_shops->premises.rows())
      if (is(r.second.agreement_dialogue_id, r.second.agreement_node_id))
        return {true, true, {}};
    for (const auto &r : c.private_shops->staff.policies.rows())
      if (is(r.second.employer_dialogue_id, r.second.employer_offer_node_id) ||
          is(r.second.worker_dialogue_id, r.second.worker_accept_node_id))
        return {true, true, {}};
    for (const auto &r : c.private_shops->orders.policies.rows())
      if (is(r.second.dialogue_id, r.second.node_id))
        return {true, true, {}};
  }
  // 回书本人声明只授权原信件交付，不能兼作生前换控同意。
  if (c.task_message_returns)
    for (const auto &p : c.task_message_returns->policies)
      if (is(p.dialogue_id, p.node_id))
        return {true, true, {}};
  if (c.task_extensions)
    for (const auto &p : c.task_extensions->policies)
      if (is(p.dialogue_id, p.node_id))
        return {true, true, {}};
  if (c.personal_appointments)
    for (const auto &p : c.personal_appointments->policies)
      if (is(p.dialogue_id, p.accept_node_id) ||
          is(p.dialogue_id, p.cancel_node_id) ||
          is(p.dialogue_id, p.meet_node_id))
        return {true, true, {}};
  if (c.contact_inquiries)
    for (const auto &p : c.contact_inquiries->policies)
      if (is(p.dialogue_id, p.node_id))
        return {true, true, {}};
  if (c.interaction->party_recruitment_consents)
    for (const auto &p :
         c.interaction->party_recruitment_consents->declarations)
      if (is(p.dialogue_id, p.node_id))
        return {true, true, {}};
  if (c.interaction->visit_messages)
    for (const auto &p : c.interaction->visit_messages->policies) {
      if (is(p.dialogue_id, p.deposit_node_id) ||
          is(p.dialogue_id, p.receive_node_id) ||
          is(p.dialogue_id, p.withdraw_node_id))
        return {true, true, {}};
      if (c.visit_message_replies)
        for (const auto &reply : c.visit_message_replies->policies)
          if (reply.message_policy_id == p.id &&
              (is(p.dialogue_id, reply.reply_node_id) ||
               is(p.dialogue_id, reply.collect_node_id)))
            return {true, true, {}};
    }
  return {true, false, {}};
}
} // namespace
Status validate_family_voluntary_paths_profile(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->family || !w.catalog->common ||
      !w.catalog->interaction)
    return common_failure("VoluntaryPathCatalogRequired");
  const bool enabled = w.family_voluntary_paths_enabled();
  auto command = [&](const StableId &kind) -> Status {
    if (kind.rfind("family_voluntary_path_", 0) != 0)
      return common_success();
    if (!enabled)
      return common_failure("VoluntaryPathProfileRequired");
    return kind == "family_voluntary_path_accept" ||
                   kind == "family_voluntary_path_continue" ||
                   kind == "family_voluntary_path_cancel" ||
                   kind == "family_voluntary_path_withdraw"
               ? common_success()
               : common_failure("UnknownVoluntaryPathCommand");
  };
  for (const auto &h : w.common.state().history) {
    auto v = command(h.kind);
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.common.state().results.rows())
    if (r.second) {
      auto v = command(r.second->kind);
      if (!v.ok)
        return v;
    }
  Integer markers = 0, voluntary = 0, known = 0;
  for (const auto &m : w.catalog->identity.module_versions) {
    if (m.first == voluntary_path_module) {
      ++markers;
      if (m.second != "1")
        return common_failure("UnsupportedVoluntaryPathVersion");
    }
    if (m.first == "voluntary_succession") {
      ++voluntary;
      if (m.second != (enabled ? "2" : "1"))
        return common_failure("VoluntaryPathSuccessionPairRequired");
    }
    if (m.first == "known_personal_dialogues") {
      ++known;
      if (m.second != "1")
        return common_failure("VoluntaryPersonalTemplateRequired");
    }
  }
  if (enabled != w.family_voluntary_paths_seed_complete ||
      bool(w.catalog->family_voluntary_paths) != enabled ||
      markers != (enabled ? 1 : 0) ||
      (w.catalog->family->voluntary_source_mode ==
       FamilyVoluntarySourceMode::living_paths) != enabled)
    return common_failure("VoluntaryPathProfileMismatch");
  if (!enabled) {
    if (!w.family_voluntary_paths.consents.empty() ||
        !w.family_voluntary_paths.cancellations.empty() ||
        !w.family_voluntary_paths.completion_revisions.empty() ||
        !w.family_voluntary_paths.commits.empty())
      return common_failure("VoluntaryPathProfileRequired");
    return voluntary_path_fact_payloads(w.common, nullptr).ok
               ? common_success()
               : common_failure("VoluntaryPathProfileRequired");
  }
  if (!w.dynamic_enabled() || !w.family_life_enabled() ||
      !w.catalog->family->enabled || !w.catalog->family->voluntary_succession ||
      voluntary != 1 || known != 1 || !w.catalog->known_personal_dialogues ||
      !w.interaction.known_personal_dialogues ||
      w.catalog->interaction->common.get() != w.catalog->common.get())
    return common_failure("VoluntaryPathDependenciesRequired");
  for (const auto &r : w.catalog->family_voluntary_paths->definitions.rows()) {
    const auto kind = r.second.relation_kind;
    if ((kind == VoluntaryRelationKind::biological_child &&
         !w.catalog->family_lineage) ||
        (kind == VoluntaryRelationKind::accepted_disciple &&
         !w.family_apprenticeship_enabled()) ||
        (kind == VoluntaryRelationKind::recognized_collateral &&
         !w.family_collateral_legacy_enabled()))
      return common_failure("VoluntaryRelationProfileRequired", r.first);
  }
  return validate_voluntary_path_state_shape(w.family_voluntary_paths);
}
VoluntaryPathWorldAccess::VoluntaryPathWorldAccess(
    const WorldBundle &w,
    std::shared_ptr<KnownPersonalDialogueWorldAccess> meetings)
    : world_(w), meetings_(std::move(meetings)),
      context_{
          {w.common, *w.catalog->common, w.family,
           w.catalog->family_lineage.get(),
           w.family_birth_enabled() ? &w.family_birth : nullptr,
           w.family_apprenticeship_enabled() ? &w.family_apprenticeship
                                             : nullptr,
           w.family_collateral_legacy_enabled() ? &w.family_collateral_legacy
                                                : nullptr,
           w.family_death_read()},
          *w.catalog->family_voluntary_paths,
          *w.catalog->family->voluntary_succession,
          *w.catalog->interaction,
          [this](const StableId &id) -> Outcome<KnownPersonalDialogueTemplate> {
            const auto *t =
                known_personal_dialogue_template(meetings_->catalog(), id);
            return t ? Outcome<KnownPersonalDialogueTemplate>{true, *t, {}}
                     : Outcome<KnownPersonalDialogueTemplate>{
                           false,
                           {},
                           common_failure("VoluntaryPersonalTemplateRequired",
                                          id)
                               .error};
          },
          [this](const StableId &dialogue, const StableId &node) {
            return other_declaration(world_, dialogue, node);
          },
          &meetings_->read(),
          [this](Integer tick, const StableId &fact) {
            return read_voluntary_player_before_fact(
                world_.common, world_.family,
                world_.family_legacy_enabled() ? &world_.family_legacy
                                               : nullptr,
                tick, fact);
          },
          [this](const StableId &a, const StableId &b) -> Status {
            auto initial = NativeFamilyInitialSourcesLease::create(world_);
            if (!initial.ok)
              return {false, {}, std::move(initial.error)};
            auto interaction = world_.services.interaction_access();
            auto family = world_.services.family_access(
                interaction,
                initial.value ? &initial.value->origins() : nullptr,
                initial.value ? &initial.value->initial_age() : nullptr);
            // The same-world sources outlive the entire original readiness
            // call.
            return validate_voluntary_family_parties_ready(
                world_.common, world_.family, world_.estate, family, a, b);
          }} {}
Outcome<std::shared_ptr<VoluntaryPathWorldAccess>>
VoluntaryPathWorldAccess::create(const WorldBundle &w) {
  auto v = validate_family_voluntary_paths_profile(w);
  if (!v.ok)
    return {false, {}, v.error};
  if (!w.family_voluntary_paths_enabled())
    return {false, {}, common_failure("VoluntaryPathProfileRequired").error};
  auto meetings = KnownPersonalDialogueWorldAccess::create(w);
  if (!meetings.ok)
    return {false, {}, meetings.error};
  auto out = std::shared_ptr<VoluntaryPathWorldAccess>(
      new VoluntaryPathWorldAccess(w, std::move(meetings.value)));
  v = validate_voluntary_path_catalog(
      out->context_.catalog, out->context_.original_requests,
      out->context_.interactions, out->context_.templates,
      out->context_.other_declarations);
  return v.ok ? Outcome<std::shared_ptr<VoluntaryPathWorldAccess>>{true,
                                                                   std::move(
                                                                       out),
                                                                   {}}
              : Outcome<std::shared_ptr<VoluntaryPathWorldAccess>>{
                    false, {}, v.error};
}
Status validate_family_voluntary_paths_world(const WorldBundle &w) {
  auto v = validate_family_voluntary_paths_profile(w);
  if (!v.ok || !w.family_voluntary_paths_enabled())
    return v;
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  v = validate_voluntary_path_history(a.value->context(),
                                      w.family_voluntary_paths);
  if (!v.ok)
    return v;
  v = validate_voluntary_path_result_bindings(w.common, w.family,
                                              w.family_voluntary_paths);
  if (!v.ok)
    return v;
  auto p = voluntary_path_fact_payloads(w.common, &w.family_voluntary_paths);
  return p.ok ? common_success() : Status{false, {}, p.error};
}
Status bind_family_voluntary_paths_result_world(
    WorldBundle &w, const WorldIdentity &identity, const FamilyState *family,
    const VoluntaryPathState *before, const StableId &actor, Integer request,
    const CommitResult &result) {
  if (!w.family_voluntary_paths_enabled())
    return family || before ? common_failure("VoluntaryPathProfileRequired")
                            : common_success();
  auto v = validate_family_voluntary_paths_profile(w);
  if (!v.ok)
    return v;
  if (!family || !before)
    return common_failure("VoluntaryPathBeforeStateRequired");
  return bind_voluntary_path_result_in_candidate(
      w.common, identity, *family, *before, w.family, w.family_voluntary_paths,
      actor, request, result);
}
Outcome<VoluntaryPathQuote>
preview_family_voluntary_path_world(const WorldBundle &w, const StableId &actor,
                                    const VoluntaryPathCommand &cmd) {
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  return preview_voluntary_path_command(a.value->context(),
                                        w.family_voluntary_paths, actor, cmd);
}
Outcome<FamilyEffect>
apply_family_voluntary_path_world(WorldBundle &w, const StableId &actor,
                                  const VoluntaryPathCommand &cmd,
                                  const VoluntaryPathQuote &quote) {
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  return apply_voluntary_path_command(w.common, w.family,
                                      w.family_voluntary_paths,
                                      a.value->context(), actor, cmd, quote);
}
Outcome<FamilyQuote> preview_family_voluntary_original_world(
    const WorldBundle &w, const StableId &actor, const FamilyCommand &cmd) {
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  auto i = w.services.interaction_access();
  return preview_original_voluntary_path_command(a.value->context(),
                                                 w.family_voluntary_paths,
                                                 w.interaction, i, actor, cmd);
}
Outcome<FamilyEffect>
apply_family_voluntary_original_world(WorldBundle &w, const StableId &actor,
                                      const FamilyCommand &cmd,
                                      const FamilyQuote &quote) {
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  auto i = w.services.interaction_access();
  return apply_original_voluntary_path_command(
      w.common, w.family, w.family_voluntary_paths, a.value->context(),
      w.interaction, i, actor, cmd, quote);
}
Outcome<BoundaryValue> query_family_voluntary_paths_world(const WorldBundle &w,
                                                          Integer page) {
  auto a = VoluntaryPathWorldAccess::create(w);
  if (!a.ok)
    return {false, {}, a.error};
  auto i = w.services.interaction_access();
  return query_voluntary_path_page(a.value->context(), w.family_voluntary_paths,
                                   w.interaction, i, page);
}
} // namespace hanmo::resident
