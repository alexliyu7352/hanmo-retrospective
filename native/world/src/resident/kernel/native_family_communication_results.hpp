// P10 source-local capture of actual CommunicationExecutionEvidence.
// The complete stable World exit still validates A/route/contact/Knowledge.
#pragma once
#include "dynamic_result_time2.hpp"
namespace hanmo::resident {
struct FamilyCommunicationLinksState2;
class NativeFamilyCommunicationResultRead final : public ExactResultOwnerRead {
  const CommonStore& common_;
  const CatalogIdentity& catalog_;
  const FamilyCommunicationLinksState2& links_;
public:
  NativeFamilyCommunicationResultRead(const CommonStore& common,
      const CatalogIdentity& catalog,const FamilyCommunicationLinksState2& links)
      :common_(common),catalog_(catalog),links_(links){}
  Status validate_bindings() const;
  Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&) const override;
  Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&) const override;
};
}
