// 询问是一次真实一期会面；预览不得把对方答案交给未完成行动的玩家。
#pragma once
#include "contact_inquiry_state.hpp"
#include "resident/domains/interaction/known_contacts/known_contact_location.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
struct ContactInquiryTime2Context;struct ActionElapsedStore;class ProjectionWriter;
struct ContactInquiryRequest {StableId policy_id;};
struct ContactInquiryQuote {AuthoredProviderSourceProof consent;StableId policy_id,subject_actor_id;Integer duration_ticks=1;std::optional<WorldMoment> actual_quoted_at;};
struct ContactInquiryCompletion {StableId fact_id;bool success=false;StableId reason;};
const ContactInquiryPolicy* contact_inquiry_policy(const ContactInquiryCatalog&,const StableId&);
Status validate_contact_inquiry_catalog(const ContactInquiryCatalog&,const InteractionCatalog&);
Outcome<ContactInquiryState> make_initial_contact_inquiry_state(const ContactInquiryCatalog&,const InteractionCatalog&);
Outcome<ContactInquiryCatalog> decode_contact_inquiry_catalog(const BoundaryValue&,const InteractionCatalog&);
BoundaryValue encode_contact_inquiry_catalog(const ContactInquiryCatalog&);
Status emit_contact_inquiry_catalog_fields(ProjectionWriter&,const ContactInquiryCatalog&);
// 调用前完整Family/Creation/Birth和原Task/预约史先验；此口只复核本段原事实及实际来源前界。
Status validate_contact_inquiries(const KnownContactLocationAccess&,const InteractionState&,const InteractionCatalog&,const ContactInquiryCatalog&,const ContactInquiryState&,const FamilyDeathOriginRead&,const DynamicStepInput* phase=nullptr,const ContactInquiryTime2Context* exact=nullptr);
Outcome<ContactInquiryQuote> preview_contact_inquiry(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ContactInquiryCatalog&,const ContactInquiryState&,const StableId& actor,const ContactInquiryRequest&,const ContactInquiryTime2Context* exact=nullptr);
// 外层必须同候选保存Common和独立Interaction尾，失败不遗留占用或认领；此口不分配最终请求结果。
Outcome<StableId> start_contact_inquiry_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ContactInquiryCatalog&,ContactInquiryState&,const ContactInquiryQuote&,const ContactInquiryTime2Context* exact=nullptr,ActionElapsedStore* elapsed=nullptr);
// 原phase减到0再调用；答复按开工前原知识截面，不把半日后新增见闻回填，随后由统一release释放。
Outcome<ContactInquiryCompletion> finish_contact_inquiry_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ContactInquiryCatalog&,ContactInquiryState&,const KnownContactLocationAccess&,const ActionCoreView&,const DynamicStepInput&);
}
