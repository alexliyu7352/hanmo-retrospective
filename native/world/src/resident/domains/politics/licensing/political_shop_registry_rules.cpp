#include "political_shop_registry_detail.hpp"
namespace hanmo::resident {
Outcome<PoliticalShopRegistryAcceptanceQuote> preview_political_shop_registry_acceptance(const PoliticalShopRegistryContext& x,const PoliticalTakeoverRead& verified,const StableId& actor,const PoliticalAcceptShopRegistry& command){
 const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PoliticalShopRegistryAcceptanceQuote>{return {false,{},common_failure(code,id).error};};const auto& l=x.legal;
 if(l.phase||x.control.phase)return fail("PoliticalShopRegistryCommitClockRequired");auto checked=validate_political_shop_registry_successions(x);if(!checked.ok)return {false,{},checked.error};
 const auto* policy=x.catalog.policies.find(command.policy_id);const auto* court=policy?l.catalog.courts.find(policy->court_id):nullptr;if(!policy||!court||actor!=command.authority.actor_id)return fail("PoliticalShopRegistryPolicyMismatch",command.policy_id);
 auto endpoint=shop_registry_detail::endpoint(x,*policy);if(!endpoint.ok)return {false,{},endpoint.error};auto grant=shop_registry_detail::authority_now(x,endpoint.value,command.authority);if(!grant.ok)return {false,{},grant.error};
 auto control=read_political_shop_registry_control(x.control,l.politics,l.politics_catalog,*court);if(!control.ok)return {false,{},control.error};if(control.value.revision<1||control.value.registration_fact_id!=command.registration_fact_id||control.value.approval_fact_id.empty()||control.value.approval_fact_id!=command.expected_approval_fact_id||control.value.organization_id!=endpoint.value.organization_id)return fail("PoliticalShopRegistryApprovedControlRequired",court->id);
 // 真正办理必须经过原全责任批准读，不能仅把一个本地Common头当作有效公共接管。
 auto approval=verified.political_takeover_source(command.expected_approval_fact_id);if(!approval.ok)return {false,{},approval.error};const auto& source=approval.value.source;
 if(!shop_law_detail::same_identity(source.source_identity,l.common.state().identity)||source.fact_id!=command.expected_approval_fact_id||source.occupation_fact_id!=control.value.occupation_fact_id||source.successor_organization_id!=endpoint.value.organization_id||source.jurisdiction_id!=court->jurisdiction_id||source.site_id!=endpoint.value.site_id)return fail("PoliticalShopRegistryApprovedControlRequired",court->id);
 const auto* previous=shop_registry_detail::latest(x,court->id,l.common.state().next_instance_id);const auto expected=previous?std::optional<StableId>{previous->id}:std::nullopt;
 if(command.expected_previous_acceptance_fact_id!=expected)return fail("PoliticalShopRegistryAcceptanceChanged",court->id);if(previous&&previous->registration_fact_id==command.registration_fact_id)return fail("PoliticalShopRegistryAlreadyAccepted",previous->id);
 auto book=read_political_shop_registry_legal_sources(l.common,l.catalog,l.state,court->id);if(!book.ok)return {false,{},book.error};
 return {true,{l.common.state().identity,actor,grant.value,command,endpoint.value,book.value.initial_registry_fact_id,control.value.occupation_fact_id,book.value.fact_ids},{}};
}
Outcome<StableId> apply_political_shop_registry_acceptance_in_candidate(CommonStore& common,PoliticalShopRegistrySuccessionState& state,const PoliticalShopRegistryContext& x,const PoliticalTakeoverRead& verified,const StableId& actor,const PoliticalAcceptShopRegistry& command,const PoliticalShopRegistryAcceptanceQuote& quote){
 if(&common!=&x.legal.common||&state!=&x.state)return {false,{},common_failure("PoliticalShopWrongCandidate").error};
 auto fresh=preview_political_shop_registry_acceptance(x,verified,actor,command);if(!fresh.ok)return {false,{},fresh.error};const auto& actual=fresh.value;
 const auto endpoint_equal=[](const PoliticalShopRegistryEndpoint& a,const PoliticalShopRegistryEndpoint& b){return a.court_id==b.court_id&&a.jurisdiction_id==b.jurisdiction_id&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.site_id==b.site_id&&a.role_id==b.role_id&&a.dialogue_id==b.dialogue_id&&a.request_node_id==b.request_node_id;};
 if(!shop_law_detail::same_identity(actual.identity,quote.identity)||actual.actor_id!=quote.actor_id||actual.grant_id!=quote.grant_id||!endpoint_equal(actual.endpoint,quote.endpoint)||actual.initial_registry_fact_id!=quote.initial_registry_fact_id||actual.occupation_fact_id!=quote.occupation_fact_id||actual.retained_legal_fact_ids!=quote.retained_legal_fact_ids||!compare_boundary(encode_political_accept_shop_registry(actual.command),encode_political_accept_shop_registry(quote.command)).exact())return {false,{},common_failure("PoliticalShopRegistryQuoteChanged").error};
 // 与原零时命令一样由外层World候选/Common保存点包住；本层不再开嵌套Common保存点。
 auto id=common.next_id(political_shop_registry_accept_prefix);if(!id.ok)return id;
 PoliticalShopRegistryAcceptance record{id.value,command.policy_id,actual.endpoint.court_id,actual.initial_registry_fact_id,command.expected_previous_acceptance_fact_id,command.registration_fact_id,command.expected_approval_fact_id,actual.occupation_fact_id,command.authority,actual.grant_id,common.tick(),actual.retained_legal_fact_ids};
 auto head=common.append_fact({id.value,political_shop_registry_accept_kind,political_shop_registry_succession_module});if(!head.ok)return {false,{},head.error};if(!state.acceptances.insert(id.value,std::move(record)))return {false,{},common_failure("PoliticalShopRegistryDuplicateAcceptance",id.value).error};
 auto checked=validate_political_shop_registry_successions(x);if(!checked.ok)return {false,{},checked.error};return id;
}
}
