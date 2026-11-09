#include "private_shop_codec.hpp"
#include "private_shop_opening_source3.hpp"
#include "private_shop_operation_sources.hpp"
#include "private_shop_operation_sources_fields.hpp"
#include "private_shop_payload.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
Outcome<PrivateShopOperationSourcesState>
decode_private_shop_operation_sources(const BoundaryValue &v) {
  return exact_fields::decode_json<PrivateShopOperationSourcesState>(
      v, [](auto &c, auto &row) {
        return private_shop_operation_source_fields::state(c, row);
      });
}
Status emit_private_shop_operation_sources_fields(
    ProjectionWriter &w, const PrivateShopOperationSourcesState &v) {
  try {
    ExactStructureProjectionArchive c(w);
    return private_shop_operation_source_fields::state(
        c, const_cast<PrivateShopOperationSourcesState &>(v));
  } catch (const exact_fields::CodecFailure &e) {
    return {false, {}, e.error};
  }
}
Status
emit_private_shop_operation_sources(ProjectionWriter &w, const StableId &name,
                                    const PrivateShopOperationSourcesState &v) {
  return w.object(name, [&](auto &out) {
    return emit_private_shop_operation_sources_fields(out, v);
  });
}
Outcome<BoundaryValue> encode_private_shop_operation_sources(
    const PrivateShopOperationSourcesState &v) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto status = emit_private_shop_operation_sources(w, "", v);
  if (status.ok)
    status = w.finish();
  return status.ok ? Outcome<BoundaryValue>{true, std::move(out), {}}
                   : Outcome<BoundaryValue>{false, {}, status.error};
}
namespace {
struct ImmutableShopStartProjector {
  dynamic_economy_codec::Projector fields;
  template <class T> void operator()(const char *name, T &value) {
    if constexpr (std::is_same_v<T, StableId>) {
      if (std::string_view(name) == "result_fact_id") {
        StableId empty;
        fields(name, empty);
        return;
      }
    }
    if constexpr (std::is_same_v<T, std::vector<Integer>>) {
      if (std::string_view(name) == "attendance_ticks") {
        std::vector<Integer> empty;
        fields(name, empty);
        return;
      }
    }
    fields(name, value);
  }
};
} // namespace
Outcome<Integer> read_private_shop_bootstrap_payload_schema(
    const CommonStore &c, const PrivateShopState &shops,
    const PrivateShopOperationSourcesState &sources,
    const PrivateShopOperationCapabilitiesState &capabilities,
    const FactRecord &h) {
  auto bad = [&](const char *code) -> Outcome<Integer> {
    return {false, 0, common_failure(code, h.id).error};
  };
  const auto *actual = c.state().facts.find(h.id);
  if ((shops.version != 2 && shops.version != 3) ||
      sources.schema_version != 1 || capabilities.schema_version != 1 ||
      !actual || actual->id != h.id || actual->domain != private_shop_module ||
      h.domain != actual->domain || actual->kind != h.kind ||
      std::count(c.state().facts.order().begin(), c.state().facts.order().end(),
                 h.id) != 1)
    return bad("PrivateShopTypedPayloadHeaderMismatch");
  const PrivateShopLaborAppliedSource *labor = nullptr;
  for (const auto &row : sources.labor_applied_sources)
    if (row.id == h.id) {
      if (labor)
        return bad("DuplicatePrivateShopLaborSource");
      labor = &row;
    }
  const PrivateShopOperationTerminalSource *terminal = nullptr;
  for (const auto &row : sources.terminal_sources)
    if (row.original_result_ref.id == h.id) {
      if (terminal ||
          !same_source_ref(row.original_result_ref, {h.domain, h.kind, h.id}))
        return bad("PrivateShopTerminalPayloadPairMismatch");
      terminal = &row;
    }
  const bool start = h.kind == "private_shop_operating_started",
             end = h.kind == "private_shop_operating_completed" ||
                   h.kind == "private_shop_operating_interrupted",
             applied = h.kind == "private_shop_labor_applied";
  if (bool(labor) != applied || (!end && terminal) || (labor && terminal))
    return bad("PrivateShopOperationPayloadPairMismatch");
  const PrivateShopOperation *operation = nullptr;
  if (start)
    operation = shops.operations.find(h.id);
  else if (end) {
    const auto *result = shops.operating_results.find(h.id);
    if (!result || result->id != h.id)
      return bad("PrivateShopTypedTerminalPayloadMismatch");
    operation = shops.operations.find(result->operation_id);
    if (!operation || operation->result_fact_id != h.id ||
        (terminal && (terminal->operation_ref != operation->id ||
                      terminal->occurred_at.half_day_tick != result->tick)))
      return bad("PrivateShopTypedTerminalPayloadMismatch");
  } else if (applied) {
    const auto serial =
        organization_instance_sequence(h.id, "private_shop_labor");
    if (!serial || *serial >= c.state().next_instance_id || !labor)
      return bad("PrivateShopTypedLaborPayloadMismatch");
    operation = shops.operations.find(labor->operation_ref);
  }
  if (!applied) {
    auto valid = validate_private_shop_fact_identity(c, shops, h);
    if (!valid.ok)
      return {false, 0, valid.error};
  }
  if (!start && !end && !applied) {
    if (shops.version == 3 && h.kind == private_shop_open_kind) {
      const auto *opening = shops.shops.find(h.id);
      if (!opening || !opening->opened_at ||
          opening->opened_at->half_day_tick != opening->opened_tick)
        return bad("PrivateShopOpeningMomentRequired");
      auto valid = encode_world_moment(*opening->opened_at);
      if (!valid.ok)
        return {false, 0, valid.error};
      return {true, 3, {}};
    }
    return {true, 1, {}};
  }
  if (!operation)
    return bad("PrivateShopOperationPayloadOwnerMissing");
  const PrivateShopOperationProfile *profile = nullptr;
  for (const auto &row : capabilities.operation_profiles)
    if (row.operation_ref == operation->id) {
      if (profile)
        return bad("DuplicatePrivateShopOperationProfile");
      profile = &row;
    }
  const PrivateShopOperationAcceptance *accepted = nullptr;
  for (const auto &row : sources.accepted_operations)
    if (row.operation_ref == operation->id) {
      if (accepted)
        return bad("DuplicatePrivateShopOperationAcceptance");
      accepted = &row;
    }
  if (!profile)
    return bad("PrivateShopActualOperationProfileRequired");
  if (profile->operation_profile ==
      PrivateShopOperationProfileKind::legacy_attendance) {
    if (accepted || terminal || labor || profile->migration_source ||
        profile->accepted_purpose_ref ||
        !profile->acceptance_condition_source_refs.empty())
      return bad("PrivateShopLegacyOperationRetagged");
    return {true, 1, {}};
  }
  if (profile->operation_profile !=
          PrivateShopOperationProfileKind::purpose_capability1 ||
      !accepted || profile->migration_source ||
      !profile->accepted_purpose_ref ||
      profile->accepted_purpose_ref->id != "private_shop_operation" ||
      profile->accepted_purpose_ref->version != 1 ||
      profile->accepted_purpose_ref->catalog_digest.empty() ||
      profile->acceptance_condition_source_refs.empty() ||
      !operation->attendance_ticks.empty() ||
      operation->start_tick != accepted->accepted_at.half_day_tick)
    return bad("PrivateShopPurposePayloadPairMismatch");
  if (end && !terminal)
    return bad("PrivateShopOriginalTerminalSourceMissing");
  if (applied) {
    const auto *head = c.state().facts.find(operation->id);
    if (!head || head->domain != private_shop_module ||
        head->kind != "private_shop_operating_started")
      return bad("PrivateShopLaborOriginalStartMissing");
  }
  return {true, applied ? 1 : 2, {}};
}
Status emit_private_shop_operation_source_payload_fields(
    ProjectionWriter &w, const CommonStore &c, const PrivateShopState &shops,
    const PrivateShopOperationSourcesState &sources, const FactRecord &h,
    Integer schema) {
  try {
    const auto *actual = c.state().facts.find(h.id);
    if ((shops.version != 2 && shops.version != 3) ||
        sources.schema_version != 1 || !actual || actual->id != h.id ||
        actual->domain != private_shop_module || h.domain != actual->domain ||
        actual->kind != h.kind ||
        std::count(c.state().facts.order().begin(),
                   c.state().facts.order().end(), h.id) != 1)
      return common_failure("PrivateShopTypedPayloadHeaderMismatch", h.id);
    if (schema == 3 && shops.version == 3 && h.kind == private_shop_open_kind)
      return emit_private_shop_fact_payload_fields(w, c, shops, h);
    if (schema == 2 && h.kind == "private_shop_operating_started") {
      const auto *operation = shops.operations.find(h.id);
      const PrivateShopOperationAcceptance *accepted = nullptr;
      for (const auto &a : sources.accepted_operations)
        if (a.operation_ref == h.id) {
          if (accepted)
            return common_failure("DuplicatePrivateShopOperationAcceptance",
                                  h.id);
          accepted = &a;
        }
      if (!operation || operation->id != h.id || !accepted ||
          !operation->attendance_ticks.empty() ||
          operation->start_tick != accepted->accepted_at.half_day_tick)
        return common_failure("PrivateShopTypedStartPayloadMismatch", h.id);
      auto original = validate_private_shop_fact_header(c, shops, h, w);
      if (!original.ok)
        return original;
      auto status = w.object("original_operation", [&](auto &out) {
        ImmutableShopStartProjector visitor{{out, false, false, false, false}};
        dynamic_economy_codec::record(
            visitor, const_cast<PrivateShopOperation &>(*operation));
        return out.status();
      });
      if (!status.ok)
        return status;
      return w.object("accepted_service", [&](auto &out) {
        ExactStructureProjectionArchive visitor(out);
        return private_shop_operation_source_fields::acceptance(
            visitor, const_cast<PrivateShopOperationAcceptance &>(*accepted));
      });
    }
    if (schema == 1 && h.kind == "private_shop_labor_applied") {
      const PrivateShopLaborAppliedSource *labor = nullptr;
      for (const auto &row : sources.labor_applied_sources)
        if (row.id == h.id) {
          if (labor)
            return common_failure("DuplicatePrivateShopLaborSource", h.id);
          labor = &row;
        }
      const auto serial =
          organization_instance_sequence(h.id, "private_shop_labor");
      if (!labor || !serial || *serial >= c.state().next_instance_id ||
          !shops.operations.find(labor->operation_ref))
        return common_failure("PrivateShopTypedLaborPayloadMismatch", h.id);
      ExactStructureProjectionArchive visitor(w);
      return private_shop_operation_source_fields::labor(
          visitor, const_cast<PrivateShopLaborAppliedSource &>(*labor));
    }
    if (schema == 2 && (h.kind == "private_shop_operating_completed" ||
                        h.kind == "private_shop_operating_interrupted")) {
      const PrivateShopOperationTerminalSource *terminal = nullptr;
      for (const auto &row : sources.terminal_sources)
        if (same_source_ref(row.original_result_ref,
                            {h.domain, h.kind, h.id})) {
          if (terminal)
            return common_failure("DuplicatePrivateShopTerminalSource", h.id);
          terminal = &row;
        }
      const auto *result = shops.operating_results.find(h.id);
      const auto *operation =
          terminal ? shops.operations.find(terminal->operation_ref) : nullptr;
      if (!terminal || !result || !operation ||
          operation->result_fact_id != h.id ||
          result->operation_id != operation->id ||
          result->tick != terminal->occurred_at.half_day_tick)
        return common_failure("PrivateShopTypedTerminalPayloadMismatch", h.id);
      auto original = validate_private_shop_fact_header(c, shops, h, w);
      if (!original.ok)
        return original;
      auto status = w.object("original_result", [&](auto &out) {
        return dynamic_economy_codec::emit_fields(out, *result);
      });
      if (!status.ok)
        return status;
      return w.object("terminal_source", [&](auto &out) {
        ExactStructureProjectionArchive visitor(out);
        return private_shop_operation_source_fields::terminal(
            visitor,
            const_cast<PrivateShopOperationTerminalSource &>(*terminal));
      });
    }
    return common_failure("UnsupportedPrivateShopOperationSourcePayload", h.id,
                          ErrorKind::unsupported_capability);
  } catch (const exact_fields::CodecFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_private_shop_operation_source_payload_fields(
    ProjectionWriter &w, const PrivateShopOperationSourceContext &x,
    const MomentOwnerSource &ref) {
  if (ref.source_schema == 3 && ref.source_ref.kind == private_shop_open_kind) {
    auto original = read_original_private_shop_opening(x.shops, ref.source_ref);
    if (!original.ok)
      return {false, {}, original.error};
  } else if (ref.source_schema == 2 &&
             ref.source_ref.kind == "private_shop_operating_started") {
    auto original = read_original_private_shop_operation_acceptance(x, ref);
    if (!original.ok)
      return {false, {}, original.error};
  } else if (ref.source_schema == 1 &&
             ref.source_ref.kind == "private_shop_labor_applied") {
    auto original = read_original_private_shop_labor(x, ref);
    if (!original.ok)
      return {false, {}, original.error};
  } else {
    auto original = read_original_private_shop_operation_terminal(x, ref);
    if (!original.ok)
      return {false, {}, original.error};
  }
  const auto *header = x.shops.common.state().facts.find(ref.source_ref.id);
  if (!header || header->kind != ref.source_ref.kind ||
      header->domain != ref.source_ref.domain)
    return common_failure("PrivateShopTypedPayloadHeaderMismatch",
                          ref.source_ref.id);
  return emit_private_shop_operation_source_payload_fields(
      w, x.shops.common, x.shops.state, x.sources, *header, ref.source_schema);
}
Status
visit_private_shop_operation_sources(CodecVisitor &c,
                                     PrivateShopOperationSourcesState &v) {
  return exact_fields::visit_binary(c, v, [](auto &out, auto &row) {
    return private_shop_operation_source_fields::state(out, row);
  });
}
} // namespace hanmo::resident
