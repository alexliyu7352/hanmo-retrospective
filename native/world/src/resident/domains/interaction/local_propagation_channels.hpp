#pragma once
#include "visit_messages/visit_report_propagation.hpp"
#include "resident/domains/tasks/task_report_propagation.hpp"
namespace hanmo::resident {
class NativeLocalPropagationRead final:public PropagationChannelRead {
 const CommonStore& common_;const SocialState& social_;const NativeFaceToFacePropagationRead& face_;const NativeCustodyPropagationRead& custody_;
 const NativeTaskReportPropagationRead* carrier_=nullptr;
 const PropagationChannelRead* owner(MessageChannelKind)const noexcept;
public:
 NativeLocalPropagationRead(const CommonStore&common,const SocialState&s,const NativeFaceToFacePropagationRead&f,const NativeCustodyPropagationRead&c):common_(common),social_(s),face_(f),custody_(c){}
 NativeLocalPropagationRead(const CommonStore&common,const SocialState&s,const NativeFaceToFacePropagationRead&f,const NativeCustodyPropagationRead&c,const NativeTaskReportPropagationRead&carrier):common_(common),social_(s),face_(f),custody_(c),carrier_(&carrier){}
 bool supports(MessageChannelKind)const noexcept override;
 bool supports_report(KnowledgeCategory)const noexcept override;
 Outcome<PropagationRecipientPage> known_recipients(const SubjectRef&,Integer,Integer,const ReadStamp&)const override;
 Outcome<std::optional<PropagationChannelProposal>> prepare(const SubjectRef&,const Report&,const PropagationRecipient&,MessageChannelKind,const ReadStamp&)const override;
 Outcome<PropagationActualDispatch> read_actual_dispatch(const StableId&)const override;
 Status validate_historical_disclosure(const SocialPropagationDecision&,const PropagationActualDispatch&)const override;
 Status validate_actual_dispatch_after_cut(const PropagationActualDispatch&,const OriginalSocialRelationCut&)const override;
 Status validate_known_recipient_at_cut(const SubjectRef&,const SubjectRef&,const OriginalSocialRelationCut&)const override;
};
class NativeLocalPropagationWrite final:public PropagationChannelWrite {
 NativeFaceToFacePropagationWrite& face_;NativeCustodyPropagationWrite& custody_;
 NativeTaskReportPropagationWrite* carrier_=nullptr;
public:
 NativeLocalPropagationWrite(NativeFaceToFacePropagationWrite&f,NativeCustodyPropagationWrite&c):face_(f),custody_(c){}
 NativeLocalPropagationWrite(NativeFaceToFacePropagationWrite&f,NativeCustodyPropagationWrite&c,NativeTaskReportPropagationWrite&carrier):face_(f),custody_(c),carrier_(&carrier){}
 Outcome<StableId> adopt(const PropagationChannelProposal&,const SubjectRef&,const StableId&,const ReadStamp&)override;
};
}
