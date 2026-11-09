// Finite MC1 adapter for the original active face-to-face telling writer.
#pragma once
#include "face_to_face_report.hpp"
#include "resident/domains/social/propagation.hpp"
namespace hanmo::resident {
class NativeFaceToFacePropagationRead final:public PropagationChannelRead {
 const FaceToFaceReportContext& context_;
public:
 explicit NativeFaceToFacePropagationRead(const FaceToFaceReportContext& x):context_(x){}
 bool bound_to(const CommonStore&c)const noexcept{return &context_.common==&c;}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &context_.common==&c&&&context_.disclosure.social==&s;}
 bool supports(MessageChannelKind)const noexcept override;
 bool supports_report(KnowledgeCategory)const noexcept override;
 Outcome<PropagationRecipientPage> known_recipients(const SubjectRef&,Integer,Integer,const ReadStamp&)const override;
 Outcome<std::optional<PropagationChannelProposal>> prepare(const SubjectRef&,const Report&,const PropagationRecipient&,MessageChannelKind,const ReadStamp&)const override;
 Outcome<PropagationActualDispatch> read_actual_dispatch(const StableId&)const override;
 Status validate_historical_disclosure(const SocialPropagationDecision&,const PropagationActualDispatch&)const override;
 Status validate_actual_dispatch_after_cut(const PropagationActualDispatch&,const OriginalSocialRelationCut&)const override;
 Status validate_known_recipient_at_cut(const SubjectRef&,const SubjectRef&,const OriginalSocialRelationCut&)const override;
};
class NativeFaceToFacePropagationWrite final:public PropagationChannelWrite {
 CommonStore& common_;InteractionState& interaction_;SocialState& social_;ReadStampAuthority& authority_;
 const FaceToFaceReportContext& context_;
public:
 NativeFaceToFacePropagationWrite(CommonStore& c,InteractionState&i,SocialState&s,ReadStampAuthority&a,const FaceToFaceReportContext&x):common_(c),interaction_(i),social_(s),authority_(a),context_(x){}
 Outcome<StableId> adopt(const PropagationChannelProposal&,const SubjectRef&,const StableId&,const ReadStamp&)override;
};
}
