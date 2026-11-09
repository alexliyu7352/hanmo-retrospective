// MC1: authorization to disclose one already-known Report. Contact is a separate proof.
#pragma once
#include "report_disclosure_types.hpp"
#include "report_official_content.hpp"
#include "resident/domains/social/social_time2.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// Only the original business owner may widen a non-public report's recipients.
// A successful read names its real immutable permission/accepted reporting scope.
class ReportDisclosureOriginalRead {
public:virtual ~ReportDisclosureOriginalRead()=default;
 virtual Outcome<std::optional<MomentOwnerSource>> read_report_disclosure_permission(const Report&,const SubjectRef&sender,const SubjectRef&receiver,const WorldMoment&,const ReadStamp&)const=0;
 virtual Status validate_report_disclosure_permission(const Report&,const SubjectRef&sender,const SubjectRef&receiver,const MomentOwnerSource&,const WorldMoment&,const SourceRef&actual_disclosure)const=0;
};
struct ReportDisclosureReadContext {
 const CommonStore& common;const SocialState& social;const ExactResultTimeRead& results;
 const ExactResultReadStamp& result_stamp;const CurrentReadStampRead& clock;
 const ReadStampAuthority& authority;const DomainReadVersionSource& versions;
 const ReportDisclosureOriginalRead* originals=nullptr;
 const ReportOfficialContentRead* official_contents=nullptr;
};
Outcome<ReportDisclosurePermission> prepare_report_disclosure_permission(const ReportDisclosureReadContext&,const StableId&report,const SubjectRef&sender,const SubjectRef&receiver,const ReadStamp&);
// 返回字段的真实预留交调用者持有；原business provider须独立在自身返回分配前计费。
Outcome<ReportDisclosurePermission> prepare_report_disclosure_permission(const ReportDisclosureReadContext&,const StableId&report,const SubjectRef&sender,const SubjectRef&receiver,const ReadStamp&,CheckpointMemoryCharge&);
Status validate_report_disclosure_permission_history(const ReportDisclosureReadContext&,const StableId&report,const SubjectRef&sender,const SubjectRef&receiver,const ReportDisclosurePermission&,const WorldMoment&,const SourceRef&actual_disclosure);
}
