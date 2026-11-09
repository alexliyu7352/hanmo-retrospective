#pragma once
#include "propagation_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <type_traits>
#include "propagation_policy.hpp"
#include "social_time2.hpp"
#include "resident/domains/director/director_subject2.hpp"
namespace hanmo::resident {
// 原渠道prepare产生临时强型值；只有该渠道自身writer能消费，不解码任意命令。
class PreparedPropagationChannel {public:virtual ~PreparedPropagationChannel()=default;};
struct PropagationRecipient {SubjectRef subject;SourceRef actual_known_contact_source;};
// The original known-contact page owns its actual ordinary allocations until
// the final reader finishes. Only the real contact producer may fill it.
class NativeFaceToFacePropagationRead;
class PropagationRecipientPage {
    friend class NativeFaceToFacePropagationRead;
    CheckpointMemoryCharge memory_;
    std::vector<PropagationRecipient> values_;
public:
    PropagationRecipientPage()noexcept=default;
    PropagationRecipientPage(const PropagationRecipientPage&)=delete;
    PropagationRecipientPage&operator=(const PropagationRecipientPage&)=delete;
    PropagationRecipientPage(PropagationRecipientPage&&)noexcept=default;
    PropagationRecipientPage&operator=(PropagationRecipientPage&&)=delete;
    std::size_t size()const noexcept{return values_.size();}
    bool empty()const noexcept{return values_.empty();}
    auto begin()const noexcept{return values_.cbegin();}
    auto end()const noexcept{return values_.cend();}
};
static_assert(std::is_nothrow_move_constructible_v<std::vector<PropagationRecipient>>);

struct PropagationChannelProposal {SubjectRef recipient;MessageChannelKind channel=MessageChannelKind::face_to_face;std::shared_ptr<const PreparedPropagationChannel> original_prepared;};
struct PropagationActualDispatch {StableId message_id,report_id;SubjectRef sender,recipient;MessageChannelKind channel=MessageChannelKind::face_to_face;WorldMoment created_at;std::optional<WorldMoment> delivered_at;SourceRef original_business_source;};
class PropagationChannelRead {
public:virtual ~PropagationChannelRead()=default;
 virtual bool supports(MessageChannelKind)const noexcept=0;
 virtual bool supports_report(KnowledgeCategory)const noexcept=0;
 // 仅枚举sender自己的实际已知联系；按稳定subject键及原公平游标给有界一页。
 virtual Outcome<PropagationRecipientPage> known_recipients(const SubjectRef&,Integer fairness_cursor,Integer limit,const ReadStamp&)const=0;
 virtual Outcome<std::optional<PropagationChannelProposal>> prepare(const SubjectRef&,const Report&,const PropagationRecipient&,MessageChannelKind,const ReadStamp&)const=0;
 virtual Outcome<PropagationActualDispatch> read_actual_dispatch(const StableId&message)const=0;
 virtual Status validate_historical_disclosure(const SocialPropagationDecision&,const PropagationActualDispatch&)const=0;
 // The actual channel owner proves its immutable dispatch adoption follows this
 // already-captured Common cut, including channels without their own Fact head.
 virtual Status validate_actual_dispatch_after_cut(const PropagationActualDispatch&,const OriginalSocialRelationCut&)const=0;
 // Even declined candidates must have a real sender-known contact before the
 // adopted cut; current proximity or a contact learned by this dispatch cannot supply it.
 virtual Status validate_known_recipient_at_cut(const SubjectRef& sender,const SubjectRef& recipient,const OriginalSocialRelationCut&)const=0;
};
class PropagationChannelWrite {public:virtual ~PropagationChannelWrite()=default;
 // 原S3 writer同候选完成真实收费/行动/排信；返回值须由read_actual_dispatch反读。
 virtual Outcome<StableId> adopt(const PropagationChannelProposal&,const SubjectRef&,const StableId&report,const ReadStamp&)=0;
};
struct PropagationReadContext {
 const CommonStore& common;const SocialState& social;const DirectorState& director;const DirectorCatalog& director_catalog;
 const PropagationPolicy& policy;PropagationPolicyLimits limits;const CatalogIdentity& catalog_identity;
 const ExactResultTimeRead& exact;const ExactResultReadStamp& exact_stamp;
 const PropagationDispositionRead& disposition;const PropagationChannelRead& channels;
 const CurrentReadStampRead& clock;const ReadStampAuthority& authority;const DomainReadVersionSource& versions;
};
struct PropagationProposal {
 SocialPropagationDecision decision;std::vector<PropagationChannelProposal> original_dispatches;
 std::shared_ptr<const ReadStamp> stamp;
};
// Three original MC1 channel readers call this only after their full actual
// dispatch reader. The one shared check compares that proof and its true Common
// append against the adopted cut; it does not create disclosure authority.
Status validate_propagation_original_dispatch_after_cut(const CommonStore&,const PropagationActualDispatch& requested,const PropagationActualDispatch& original,const OriginalSocialRelationCut&);
StableId propagation_decision_key(const SubjectRef&,Integer epoch,Integer seq);
StableId propagation_cooldown_key(const SubjectRef&,const StableId&report,const SubjectRef&,const FixedLockedRuleInput&);
Status validate_propagation_composition(const PropagationPolicy&,const PropagationPolicyLimits&,const PropagationChannelRead&);
// null是合法无已知候选/未选策略；不分ID、不改知识或复评时间。
Outcome<std::optional<PropagationProposal>> prepare_propagation_review(const PropagationReadContext&,const SubjectRef&,Integer registration_epoch,Integer decision_seq);
// 同一原Director候选中调用；跨任一渠道/最后冷验失败须撤整个World候选。
Outcome<const SocialPropagationDecision*> adopt_propagation_review_in_candidate(SocialState&,DirectorState&,const PropagationReadContext&,PropagationChannelWrite&,const PropagationProposal&);
Status validate_social_propagation(const PropagationReadContext&);
Status validate_social_propagation_shape(const DynamicSocialState&);
Outcome<Integer> social_propagation_decision_source_schema(const DynamicSocialState&,const SourceRef&);
Outcome<const SocialPropagationDecision*> read_social_propagation_decision_source(const PropagationReadContext&,const SourceRef&,Integer schema);
class PropagationAdoptionPrefixPinSink {public:virtual ~PropagationAdoptionPrefixPinSink()=default;
 virtual Status original_prefix_fact(const StableId& decision_key,Integer ordinal,const FactRecord&)=0;
};
StableId propagation_adoption_prefix_pin_key(const StableId& decision_key,Integer ordinal);
// One validated borrowed walk, including every original Common prefix row.
Status visit_propagation_adoption_prefix_pins(const CommonStore&,const DynamicSocialState&,PropagationAdoptionPrefixPinSink&);
}
