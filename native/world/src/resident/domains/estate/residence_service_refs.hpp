// EC53 原住居地址的轻量共享类型；不带目录、权利或身体能力依赖。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
// 只指本 owner 的实际行，不接受任意 SourceRef 作为居住权。
struct ResidenceAgreementRef {
  StableId agreement_id;
  Integer terms_revision = 0;
};
struct ResidenceIntakeLineRef {
  ResidenceAgreementRef agreement;
  StableId line_id;
};
} // namespace hanmo::resident
