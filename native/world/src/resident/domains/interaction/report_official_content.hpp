// Original immutable body identity of an already-produced, validated Report.
// This reader neither discloses nor grants consent, and never copies the body.
#pragma once
#include "communication_delivery_read.hpp"
#include "resident/domains/social/social_time2.hpp"
namespace hanmo::resident {
class ReportOfficialContentRead {
public:virtual ~ReportOfficialContentRead()=default;
 virtual bool bound_to(const CommonStore&,const SocialState&)const noexcept=0;
 virtual Outcome<std::optional<OfficialContentRef>> read_report_official_content(const Report&,const ExactResultTimeRead&,const ExactResultReadStamp&)const=0;
};
struct ReportDisclosureReadContext;
// The selected original body owner is mandatory, including when it verifies
// that a real report is ordinary non-official content. Absence is not a proof.
Outcome<std::optional<OfficialContentRef>> read_report_disclosure_official_content(const ReportDisclosureReadContext&,const StableId&actual_report_id);
}
