// 原明确接纳许可只读源；初始许可不是 Common 业务事实，也不是产权推论。
#pragma once
#include "residence_service_contract.hpp"
namespace hanmo::resident {
struct EstateRestHistoryContext;
struct EstateState;
struct ResidenceAdmissionReadContext {
  const EstateRestHistoryContext &estate_history;
  const EstateState &estates;
  const ResidenceServiceCatalog &catalog;
};
struct ResidenceHostAdmissionRead {
  const ResidenceTerms *terms = nullptr;
  const ResidenceHostAdmissionDefinition *permission = nullptr;
};
// 仅初态目录中的明确许可分支；发生产权处置后须真实许可承接历史，绝不复活。
Outcome<ResidenceHostAdmissionRead>
read_current_residence_host_admission(const ResidenceAdmissionReadContext &,
                                      const ActorFoundationRuleRef &terms,
                                      const StableId &actual_host);
Outcome<ResidenceHostAdmissionRead> read_residence_host_admission_before_source(
    const ResidenceAdmissionReadContext &, const ActorFoundationRuleRef &terms,
    const StableId &actual_host, const SourceRef &actual_before,
    const WorldMoment &actual_before_moment);
} // namespace hanmo::resident
