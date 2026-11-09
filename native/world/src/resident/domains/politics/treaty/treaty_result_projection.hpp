// Original offer/counteroffer results only. P10 owns Report creation and actual
// personal observation; this projection never writes knowledge or a message.
#pragma once
#include "treaty_official_content.hpp"
#include "resident/domains/interaction/report_official_content.hpp"
namespace hanmo::resident {
bool native_treaty_revision_result_kind(const FactRecord&);
class NativeTreatyRevisionResultRead final:public ExactResultOwnerRead {
 const TreatyRevisionContext& context_;
public:explicit NativeTreatyRevisionResultRead(const TreatyRevisionContext& x):context_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
// This binds an already-created, fully validated P10 Report to the immutable
// complete body. It grants neither disclosure nor knowledge to any receiver.
Outcome<OfficialContentRef> read_treaty_report_official_content(
 const TreatyRevisionContext&,const SocialState&,const ExactResultTimeRead&,
 const ExactResultReadStamp&,const StableId& actual_report_id);
class NativeTreatyReportOfficialContentRead final:public ReportOfficialContentRead {
 const TreatyRevisionContext& context_;const SocialState& social_;
public:NativeTreatyReportOfficialContentRead(const TreatyRevisionContext& x,const SocialState& s):context_(x),social_(s){}
 bool bound_to(const CommonStore&,const SocialState&)const noexcept override;
 Outcome<std::optional<OfficialContentRef>> read_report_official_content(const Report&,
  const ExactResultTimeRead&,const ExactResultReadStamp&)const override;
};

}
