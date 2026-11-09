// MC1 custody propagation consumes the original witnessed deposit writer.
#pragma once
#include "visit_report_custody.hpp"
#include "resident/domains/interaction/face_to_face_propagation.hpp"
namespace hanmo::resident {
class NativeCustodyPropagationRead final:public PropagationChannelRead {
 const VisitReportCustodyContext& context_;const NativeFaceToFacePropagationRead& contacts_;
public:
 NativeCustodyPropagationRead(const VisitReportCustodyContext&x,const NativeFaceToFacePropagationRead&contacts):context_(x),contacts_(contacts){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &context_.common==&c&&&context_.disclosure.social==&s&&contacts_.bound_to(c,s);}
 bool supports(MessageChannelKind)const noexcept override;
 bool supports_report(KnowledgeCategory)const noexcept override;
 Outcome<PropagationRecipientPage> known_recipients(const SubjectRef&,Integer,Integer,const ReadStamp&)const override;
 Outcome<std::optional<PropagationChannelProposal>> prepare(const SubjectRef&,const Report&,const PropagationRecipient&,MessageChannelKind,const ReadStamp&)const override;
 Outcome<PropagationActualDispatch> read_actual_dispatch(const StableId&)const override;
 Status validate_historical_disclosure(const SocialPropagationDecision&,const PropagationActualDispatch&)const override;
 Status validate_actual_dispatch_after_cut(const PropagationActualDispatch&,const OriginalSocialRelationCut&)const override;
 Status validate_known_recipient_at_cut(const SubjectRef&,const SubjectRef&,const OriginalSocialRelationCut&)const override;
};
class NativeCustodyPropagationWrite final:public PropagationChannelWrite {
 CommonStore& common_;InteractionState& interaction_;SocialState& social_;ReadStampAuthority& authority_;const VisitReportCustodyContext& context_;
public:
 NativeCustodyPropagationWrite(CommonStore&c,InteractionState&i,SocialState&s,ReadStampAuthority&a,const VisitReportCustodyContext&x):common_(c),interaction_(i),social_(s),authority_(a),context_(x){}
 Outcome<StableId> adopt(const PropagationChannelProposal&,const SubjectRef&,const StableId&,const ReadStamp&)override;
};
}
