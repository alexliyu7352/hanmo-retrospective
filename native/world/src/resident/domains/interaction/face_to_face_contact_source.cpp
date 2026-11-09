#include "face_to_face_contact_source.hpp"
#include "face_to_face_report.hpp"
#include "household_purchase_report.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const char *code, const StableId &id = {},
                ErrorKind kind = ErrorKind::rule) {
  return {false, {}, common_failure(code, id, kind).error};
}
bool same_context(const InteractionContext &a, const InteractionContext &b) {
  return a.place_id == b.place_id &&
         a.provider_actor_id == b.provider_actor_id && a.role_id == b.role_id &&
         a.kind == b.kind && a.encounter_id == b.encounter_id &&
         a.input_error == b.input_error;
}
Status within(const WorldMoment &at, const WorldMoment &now) {
  auto cmp = compare_world_moments(at, now);
  if (!cmp.ok)
    return {false, {}, cmp.error};
  return cmp.value <= 0 ? common_success()
                        : common_failure("ContactSourceFromFuture");
}
Status shape(const FaceToFaceContactResult &r,
             const InteractionCatalog &catalog) {
  if (r.session_id.empty() || r.session_actor_id.empty() ||
      r.provider_actor_id.empty() ||
      r.session_actor_id == r.provider_actor_id || r.dialogue_id.empty() ||
      r.first_contact_node_id.empty() ||
      r.context.provider_actor_id != r.provider_actor_id ||
      !face_to_face_contact_context_valid(r.context) ||
      (r.first_contact_phase != "say" && r.first_contact_phase != "choice"))
    return common_failure("InvalidFaceToFaceContactSource", r.session_id);
  const DialogueDefinition *d = nullptr;
  for (const auto &item : catalog.dialogues)
    if (item.id == r.dialogue_id) {
      if (d)
        return common_failure("AmbiguousContactDialogue", r.session_id);
      d = &item;
    }
  const DialogueNode *n = nullptr;
  if (d)
    for (const auto &item : d->nodes)
      if (item.id == r.first_contact_node_id) {
        if (n)
          return common_failure("AmbiguousContactNode", r.session_id);
        n = &item;
      }
  if (!n || n->kind != r.first_contact_phase)
    return common_failure("ContactDialogueSourceMismatch", r.session_id);
  // Current role occupants, availability, location, and node are deliberately
  // not historical evidence.
  auto at = compare_world_moments(r.occurred_at, r.occurred_at);
  return at.ok ? common_success() : Status{false, {}, at.error};
}
} // namespace
Outcome<const FaceToFaceContactResult *> read_face_to_face_contact_source(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const SourceRef &ref, Integer schema) {
  if (schema != 1 || ref.domain != "interaction" ||
      ref.kind != face_to_face_contact_source_kind || ref.id.empty())
    return fail<const FaceToFaceContactResult *>(
        "UnsupportedFaceToFaceContactSource", ref.id,
        ErrorKind::unsupported_capability);
  const auto *r = state.face_to_face_contacts
                      ? state.face_to_face_contacts->results.find(ref.id)
                      : nullptr;
  const auto *h = common.state().facts.find(ref.id);
  if (!r || !h || r->session_id != ref.id || h->domain != ref.domain ||
      h->kind != ref.kind)
    return fail<const FaceToFaceContactResult *>(
        "FaceToFaceContactSourceMissing", ref.id, ErrorKind::internal);
  auto valid = shape(*r, catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, r, {}};
}
Outcome<std::optional<SourceRef>> publish_face_to_face_contact_in_candidate(
    CommonStore &common, InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const WorldMoment &actual) {
  if (!state.face_to_face_contacts)
    return fail<std::optional<SourceRef>>("FaceToFaceContactSourcesNotSelected",
                                          {},
                                          ErrorKind::unsupported_capability);
  const InteractionSession *session = nullptr;
  for (const auto &s : state.interactions)
    if (s.actor_id == actor) {
      if (session)
        return fail<std::optional<SourceRef>>("AmbiguousInteractionContact",
                                              actor, ErrorKind::internal);
      session = &s;
    }
  if (!session || (session->phase != "say" && session->phase != "choice") ||
      !face_to_face_contact_context_valid(session->context))
    return {true, std::nullopt, {}};
  auto proof =
      preview_face_to_face_contact(common, state, catalog, access, actor,
                                   session->context.provider_actor_id);
  if (!proof.ok)
    return {false, {}, proof.error};
  const SourceRef ref{"interaction", face_to_face_contact_source_kind,
                      session->id};
  if (state.face_to_face_contacts->results.find(session->id)) {
    auto original =
        read_face_to_face_contact_source(common, state, catalog, ref);
    if (!original.ok)
      return {false, {}, original.error};
    const auto &r = *original.value;
    if (r.session_actor_id != session->actor_id ||
        r.provider_actor_id != session->context.provider_actor_id ||
        r.dialogue_id != session->dialogue_id ||
        !same_context(r.context, session->context))
      return fail<std::optional<SourceRef>>("ContactSessionBindingChanged",
                                            session->id, ErrorKind::internal);
    auto valid = within(r.occurred_at, actual);
    if (!valid.ok)
      return {false, {}, valid.error};
    return {true, ref, {}};
  }
  FaceToFaceContactResult result{session->id,
                                 session->actor_id,
                                 session->context.provider_actor_id,
                                 session->dialogue_id,
                                 session->node_id,
                                 session->phase,
                                 proof.value.context,
                                 actual};
  auto valid = shape(result, catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = revalidate_face_to_face_contact(common, state, catalog, access,
                                          proof.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto wrote = common.append_fact({ref.id, ref.kind, ref.domain});
  if (!wrote.ok)
    return {false, {}, wrote.error};
  if (!state.face_to_face_contacts->results.insert(ref.id, std::move(result)))
    return fail<std::optional<SourceRef>>("DuplicateFaceToFaceContactSource",
                                          ref.id, ErrorKind::internal);
  return {true, ref, {}};
}
static Outcome<SourceRef> read_current_contact_owned(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &target, const WorldMoment &actual,
    CheckpointMemoryCharge *output_memory) {
  // proof和其context在内部活期单独保留；返回引用在真正fresh构造前交外层预留。
  CheckpointMemoryCharge proof_memory;
  auto proof = preview_face_to_face_contact(common, state, catalog, access,
                                            actor, target, proof_memory);
  if (!proof.ok)
    return {false, {}, proof.error};
  if (output_memory) {


    (*output_memory).grow(0);
  }
  SourceRef ref{"interaction", face_to_face_contact_source_kind,
                proof.value.session_id};
  auto original = read_face_to_face_contact_source(common, state, catalog, ref);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &r = *original.value;
  if (r.session_actor_id != proof.value.session_actor_id ||
      !same_context(r.context, proof.value.context))
    return fail<SourceRef>("ContactSessionBindingChanged", ref.id,
                           ErrorKind::internal);
  auto valid = within(r.occurred_at, actual);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = revalidate_face_to_face_contact(common, state, catalog, access,
                                          proof.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(ref), {}};
}
// 新旧入口保同一来源与当前会话反验顺序，显式owner只改变实际分配的保留寿命。
Outcome<SourceRef> read_current_face_to_face_contact_source(
    const CommonStore &c, const InteractionState &s,
    const InteractionCatalog &cat, const InteractionAccess &a,
    const StableId &actor, const StableId &target, const WorldMoment &actual) {
  return read_current_contact_owned(c, s, cat, a, actor, target, actual,
                                    nullptr);
}
Outcome<SourceRef> read_current_face_to_face_contact_source(
    const CommonStore &c, const InteractionState &s,
    const InteractionCatalog &cat, const InteractionAccess &a,
    const StableId &actor, const StableId &target, const WorldMoment &actual,
    CheckpointMemoryCharge &memory) {
  return read_current_contact_owned(c, s, cat, a, actor, target, actual,
                                    &memory);
}
Status validate_face_to_face_contact_sources(const CommonStore &common,
                                             const InteractionState &state,
                                             const InteractionCatalog &catalog,
                                             const WorldMoment &actual) {
  if (!state.face_to_face_contacts)
    return common_success();
  if (state.face_to_face_contacts->version != 1 &&
      state.face_to_face_contacts->version != 2 &&
      state.face_to_face_contacts->version != 3)
    return common_failure("UnsupportedFaceToFaceContactSourceSchema");
  if (state.face_to_face_contacts->version == 1 &&
      !state.face_to_face_contacts->report_disclosures.empty())
    return common_failure("UnselectedFaceToFaceReportSources");
  if (state.face_to_face_contacts->version < 3 &&
      !state.face_to_face_contacts->purchase_plan_disclosures.empty())
    return common_failure("UnselectedHouseholdPurchaseReportSources");
  for (const auto &row : state.face_to_face_contacts->results.rows()) {
    auto r = read_face_to_face_contact_source(
        common, state, catalog,
        {"interaction", face_to_face_contact_source_kind, row.first});
    if (!r.ok)
      return {false, {}, r.error};
    auto valid = within(r.value->occurred_at, actual);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row :
       state.face_to_face_contacts->report_disclosures.rows()) {
    auto r = read_original_face_to_face_report_source(
        common, state, catalog,
        {"interaction", face_to_face_report_source_kind, row.first}, 1);
    if (!r.ok)
      return {false, {}, r.error};
    auto valid = within(r.value->disclosed_at, actual);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row :
       state.face_to_face_contacts->purchase_plan_disclosures.rows()) {
    auto r = read_original_household_purchase_report_source(
        common, state,
        {"interaction", household_purchase_report_source_kind, row.first}, 1);
    if (!r.ok)
      return {false, {}, r.error};
    auto valid = within(r.value->disclosed_at, actual);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "interaction" &&
        row.second.kind == household_purchase_report_source_kind &&
        !state.face_to_face_contacts->purchase_plan_disclosures.find(row.first))
      return common_failure("OrphanHouseholdPurchaseDisclosureSource",
                            row.first);
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "interaction" &&
        row.second.kind == face_to_face_report_source_kind &&
        !state.face_to_face_contacts->report_disclosures.find(row.first))
      return common_failure("OrphanFaceToFaceReportSource", row.first);
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "interaction" &&
        row.second.kind == face_to_face_contact_source_kind &&
        !state.face_to_face_contacts->results.find(row.first))
      return common_failure("OrphanFaceToFaceContactSource", row.first);
  return common_success();
}
} // namespace hanmo::resident
