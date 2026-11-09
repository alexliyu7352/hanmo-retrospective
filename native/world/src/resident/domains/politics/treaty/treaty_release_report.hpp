// P10已产生的真实解除请求Report到同版完整正文；不产生收件人的知情/同意。
#pragma once
#include "treaty_release_sources.hpp"
#include "resident/domains/interaction/report_official_content.hpp"
namespace hanmo::resident {
Outcome<OfficialContentRef> read_treaty_release_report_official_content(const TreatyReleaseContext&,
 const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,const StableId& report_id);
class NativeTreatyReleaseReportOfficialContentRead final:public ReportOfficialContentRead {
 const TreatyReleaseContext& x_;const SocialState& social_;
public:
 NativeTreatyReleaseReportOfficialContentRead(const TreatyReleaseContext& x,const SocialState& s):x_(x),social_(s){}
 bool bound_to(const CommonStore&,const SocialState&)const noexcept override;
 Outcome<std::optional<OfficialContentRef>> read_report_official_content(const Report&,
  const ExactResultTimeRead&,const ExactResultReadStamp&)const override;
};
}
