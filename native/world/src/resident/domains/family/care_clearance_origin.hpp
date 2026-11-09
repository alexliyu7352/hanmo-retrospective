// FH §5.1/§5.2 的原辅助清场义务与唯一工作来源。
// 这里只保存已定深值，不授搬离权、不创建动作，也不把 Care 或 Sea
// 的工量再记一次。
#pragma once
#include "care_arrangement.hpp"
#include "resident/domains/military/sea_embark_care_ref.hpp"

namespace hanmo::resident {
// 原接受照护条款及同一个真实 handover，不能用亲属身份替代接受。
struct ClearanceCareHandoverObligation {
  StableId accepted_care_terms_ref, care_handover_ref;
};
// 原医疗责任只借自己的合法终止/转诊及患者或照护权限源。
struct ClearanceMedicalObligation {
  StableId case_id;
  MomentOwnerSource referral_or_original_termination_ref;
  MomentOwnerSource accepted_patient_or_care_authority_ref;
};
// 原住居 line 的退出责任，保实际接受源，不以未来迁出结果反证起始权限。
struct ClearanceResidenceObligation {
  StableId agreement_id, intake_line_ref;
  MomentOwnerSource original_end_or_change_intent_ref;
  MomentOwnerSource accepted_residence_terms_ref;
  MomentOwnerSource care_or_departure_authority_ref;
};
// 原死亡/损失与合法已接受善后责任分别保存，身体减少不自动证明实际清空。
struct ClearanceLawfulDispositionObligation {
  MomentOwnerSource original_placement_source_ref;
  std::vector<MomentOwnerSource> death_or_population_loss_refs;
  MomentOwnerSource original_accepted_disposition_clause_ref;
  MomentOwnerSource executor_or_care_authority_ref;
  StableId authorized_site_ref;
};
// 原四种义务依 FH 固定 wire 0..3，非任意自由业务种类。
using FacilityClearanceObligation =
    std::variant<ClearanceCareHandoverObligation, ClearanceMedicalObligation,
                 ClearanceResidenceObligation,
                 ClearanceLawfulDispositionObligation>;

// 仅此支创建自己的本地 F Action，其工量不保存在本来源值中。
struct ClearanceLocalWorkOrigin {};
// 同一次真实 CareHandover 原 F 服务，不能创建第二 clearance Action。
struct ClearanceCareWorkOrigin {
  StableId handover_id;
};
// S2 原七字段支；binding 使用 S2 唯一强型 import，不能复制 Family 同名地址。
// 这份引用不代表原绑定、Terms3 或 Completed Result3 已经存在。
struct ClearanceSeaWorkOrigin {
  StableId action_ref;
  SeaEmbarkCareBindingRef care_transport_binding_ref;
  SourceRef terms_source_ref;
  Integer terms_source_schema = 3;
  StableId action_elapsed_ref;
  std::optional<SourceRef> result_source_ref;
  std::optional<Integer> result_source_schema;
};
// 三支固定顺序；来源在初次采用后不可换支，不能将已经付出的工量挪到另一责任。
using FacilityClearanceWorkOrigin =
    std::variant<ClearanceLocalWorkOrigin, ClearanceCareWorkOrigin,
                 ClearanceSeaWorkOrigin>;
} // namespace hanmo::resident
