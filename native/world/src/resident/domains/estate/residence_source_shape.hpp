// 首次 /2 来源结构门；通过仅证明字段/有限状态相容，绝不证明业务授权或床位可用。
#pragma once
#include "residence_service_contract.hpp"
namespace hanmo::resident {
Status validate_residence_agreement_ref(const ResidenceAgreementRef &);
Status validate_resident_source_shape(const ResidentSource &);
Status
validate_residence_line_transition_shape(const ResidenceLineTransition &);
// 同原参数做来源精确比较，不按当前 cohort/家庭口数扩接受范围。
bool same_resident_source(const ResidentSource &, const ResidentSource &);
} // namespace hanmo::resident
