// 原四义务和三工作来源的同一字段表；不把存读通过当实际授权或已供服务。
#pragma once
#include "care_arrangement_codec_fields.hpp"
#include "care_clearance_origin.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/military/sea_embark_care_ref_codec_fields.hpp"

namespace hanmo::resident::care_clearance_origin_codec {
// 解码前按真正分支计默认深值，不用未计费的临时通用树保存来源。
template <std::size_t Index, class C, class V> Status select(C &c, V &value) {
  using T = std::variant_alternative_t<Index, V>;
  const auto ready = c.template prepare_default_values<T>();
  if (!ready.ok)
    return ready;
  value.template emplace<Index>();
  return c.status();
}

// 四类原责任只接受完整实际引用；这里只核形状，真正 owner 必须再反读接受链。
template <class C> Status obligation(C &c, FacilityClearanceObligation &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 3)
    return c.reject("InvalidClearanceObligationTag");
  if (c.reading()) {
    const auto ready = tag == 0   ? select<0>(c, value)
                       : tag == 1 ? select<1>(c, value)
                       : tag == 2 ? select<2>(c, value)
                                  : select<3>(c, value);
    if (!ready.ok)
      return ready;
  }
  const auto source = [&](const char *name, MomentOwnerSource &ref) {
    return c.object(name, [&](auto &x) {
      return care_requirement_codec::moment_source(x, ref);
    });
  };
  if (tag == 0) {
    auto &v = std::get<ClearanceCareHandoverObligation>(value);
    care_requirement_codec::text(c, "accepted_care_terms_ref",
                                 v.accepted_care_terms_ref);
    return care_requirement_codec::text(c, "care_handover_ref",
                                        v.care_handover_ref);
  }
  if (tag == 1) {
    auto &v = std::get<ClearanceMedicalObligation>(value);
    care_requirement_codec::text(c, "case_id", v.case_id);
    source("referral_or_original_termination_ref",
           v.referral_or_original_termination_ref);
    return source("accepted_patient_or_care_authority_ref",
                  v.accepted_patient_or_care_authority_ref);
  }
  if (tag == 2) {
    auto &v = std::get<ClearanceResidenceObligation>(value);
    care_requirement_codec::text(c, "agreement_id", v.agreement_id);
    care_requirement_codec::text(c, "intake_line_ref", v.intake_line_ref);
    source("original_end_or_change_intent_ref",
           v.original_end_or_change_intent_ref);
    source("accepted_residence_terms_ref", v.accepted_residence_terms_ref);
    source("care_or_departure_authority_ref",
           v.care_or_departure_authority_ref);
    if (c.status().ok &&
        (v.original_end_or_change_intent_ref.source_ref.domain != "estate" ||
         v.original_end_or_change_intent_ref.source_ref.kind !=
             "residence_physical_occupancy" ||
         v.original_end_or_change_intent_ref.source_schema != 1))
      return c.reject("OriginalResidenceDepartureAdmissionRequired");
    return c.status();
  }
  auto &v = std::get<ClearanceLawfulDispositionObligation>(value);
  source("original_placement_source_ref", v.original_placement_source_ref);
  care_arrangement_codec::moment_refs(c, "death_or_population_loss_refs",
                                      v.death_or_population_loss_refs);
  source("original_accepted_disposition_clause_ref",
         v.original_accepted_disposition_clause_ref);
  source("executor_or_care_authority_ref", v.executor_or_care_authority_ref);
  care_requirement_codec::text(c, "authorized_site_ref", v.authorized_site_ref);
  return c.status().ok && v.death_or_population_loss_refs.empty()
             ? c.reject("OriginalClearanceDeathOrLossRequired")
             : c.status();
}

// 七字段 Sea 来源严格保 Terms3/Result3 成对关系；不借其值模拟原 S2 reader。
template <class C>
Status work_origin(C &c, FacilityClearanceWorkOrigin &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidClearanceWorkOriginTag");
  if (c.reading()) {
    const auto ready = tag == 0   ? select<0>(c, value)
                       : tag == 1 ? select<1>(c, value)
                                  : select<2>(c, value);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return c.status();
  if (tag == 1)
    return care_requirement_codec::text(
        c, "handover_id", std::get<ClearanceCareWorkOrigin>(value).handover_id);
  auto &v = std::get<ClearanceSeaWorkOrigin>(value);
  care_requirement_codec::text(c, "action_ref", v.action_ref);
  c.object("care_transport_binding_ref", [&](auto &x) {
    return sea_embark_care_ref_codec::reference(x,
                                                v.care_transport_binding_ref);
  });
  c.object("terms_source_ref", [&](auto &x) {
    return care_requirement_codec::source(x, v.terms_source_ref);
  });
  c.field("terms_source_schema", v.terms_source_schema);
  care_requirement_codec::text(c, "action_elapsed_ref", v.action_elapsed_ref);
  c.optional("result_source_ref", v.result_source_ref, [](auto &x, auto &ref) {
    return care_requirement_codec::source(x, ref);
  });
  c.optional("result_source_schema", v.result_source_schema,
             [](auto &x, auto &schema) {
               return x.field("value", schema);
             });
  if (!c.status().ok)
    return c.status();
  if (v.terms_source_schema != 3 || v.terms_source_ref.domain != "military" ||
      v.terms_source_ref.kind != "sea_transfer_terms" ||
      v.care_transport_binding_ref.transfer_action_id != v.action_ref ||
      bool(v.result_source_ref) != bool(v.result_source_schema))
    return c.reject("OriginalSeaClearanceWorkSourceRequired");
  if (v.result_source_ref &&
      (*v.result_source_schema != 3 ||
       v.result_source_ref->domain != "military" ||
       v.result_source_ref->kind != "sea_transfer_result"))
    return c.reject("OriginalSeaClearanceResult3Required");
  return c.status();
}
} // namespace hanmo::resident::care_clearance_origin_codec
