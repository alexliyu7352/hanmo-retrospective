#pragma once
#include "task_report_carriage_command.hpp"
#include "resident/domains/interaction/face_to_face_propagation.hpp"
namespace hanmo::resident {
// MC1 carrier selection consumes an existing accepted Report scope and its
// already-adopted real travel. It never creates a contract or optional trip.
class NativeTaskReportPropagationRead final:public PropagationChannelRead {
 const TaskReportCarriageContext& x_;const NativeFaceToFacePropagationRead& contacts_;
public:
 NativeTaskReportPropagationRead(const TaskReportCarriageContext& x,const NativeFaceToFacePropagationRead& contacts):x_(x),contacts_(contacts){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &x_.common==&c&&&x_.face.disclosure.social==&s&&contacts_.bound_to(c,s);}
 bool supports(MessageChannelKind)const noexcept override;
 bool supports_report(KnowledgeCategory)const noexcept override;
 Outcome<PropagationRecipientPage> known_recipients(const SubjectRef&,Integer,Integer,const ReadStamp&)const override;
 Outcome<std::optional<PropagationChannelProposal>> prepare(const SubjectRef&,const Report&,const PropagationRecipient&,MessageChannelKind,const ReadStamp&)const override;
 Outcome<PropagationActualDispatch> read_actual_dispatch(const StableId&)const override;
 Status validate_historical_disclosure(const SocialPropagationDecision&,const PropagationActualDispatch&)const override;
 Status validate_actual_dispatch_after_cut(const PropagationActualDispatch&,const OriginalSocialRelationCut&)const override;
 Status validate_known_recipient_at_cut(const SubjectRef&,const SubjectRef&,const OriginalSocialRelationCut&)const override;
};
class NativeTaskReportPropagationWrite final:public PropagationChannelWrite {
 CommonStore& common_;TaskState& tasks_;SocialState& social_;CommunicationLinksState& links_;
 ReadStampAuthority& authority_;const TaskReportCarriageContext& x_;
public:
 NativeTaskReportPropagationWrite(CommonStore&c,TaskState&t,SocialState&s,CommunicationLinksState&l,ReadStampAuthority&authority,const TaskReportCarriageContext&x):common_(c),tasks_(t),social_(s),links_(l),authority_(authority),x_(x){}
 Outcome<StableId> adopt(const PropagationChannelProposal&,const SubjectRef&,const StableId&,const ReadStamp&)override;
};
}
