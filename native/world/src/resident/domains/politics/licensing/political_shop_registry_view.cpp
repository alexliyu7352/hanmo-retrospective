#include "political_shop_registry_view.hpp"
#include "political_shop_registry_detail.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
}
BoundaryValue encode_political_shop_registry_acceptance_quote(const PoliticalShopRegistryAcceptanceQuote& q){return boundary::obj({{"identity",identity(q.identity)},{"actor_id",V{q.actor_id}},{"duration",V{Integer{0}}},{"command_kind",V{StableId{political_shop_registry_accept_command}}},{"command",encode_political_accept_shop_registry(q.command)},{"grant_id",V{q.grant_id}},{"court_id",V{q.endpoint.court_id}},{"organization_id",V{q.endpoint.organization_id}},{"initial_registry_fact_id",V{q.initial_registry_fact_id}},{"occupation_fact_id",V{q.occupation_fact_id}},{"retained_source_count",V{static_cast<Integer>(q.retained_legal_fact_ids.size())}}});}
Outcome<BoundaryValue> query_political_shop_registry_successions(const PoliticalShopRegistryContext& x,const PoliticalTakeoverRead& verified,const StableId& place,Integer cursor){
 if(cursor<0||cursor>std::numeric_limits<Integer>::max()-16)return {false,{},common_failure("InvalidPoliticalShopRegistryCursor").error};auto checked=validate_political_shop_registry_successions(x);if(!checked.ok)return {false,{},checked.error};const auto& l=x.legal;const auto actor=l.common.state().metadata.player_actor_id;auto at=politics_detail::place(l.common,actor);if(!at||at->place_id!=place)return {false,{},common_failure("PoliticalShopRegistryLocalQueryRequired",place).error};
 V::Array rows;Integer count=0;
 for(const auto& row:x.catalog.policies.rows()){
  auto endpoint=shop_registry_detail::endpoint(x,row.second);if(!endpoint.ok)return {false,{},endpoint.error};if(endpoint.value.site_id!=place)continue;const auto* office=l.organizations.offices.find(endpoint.value.office_id);if(!office||office->holder_actor_id!=std::optional<StableId>{actor})continue;
  ActingAuthority authority{office->organization_id,office->id,office->epoch,actor};auto grant=shop_registry_detail::authority_now(x,endpoint.value,authority);if(!grant.ok)continue;const auto* court=l.catalog.courts.find(row.second.court_id);if(!court)return {false,{},common_failure("PoliticalShopRegistryCourtUnknown",row.first).error};
  auto control=read_political_shop_registry_control(x.control,l.politics,l.politics_catalog,*court);if(!control.ok)return {false,{},control.error};if(control.value.revision<1||control.value.organization_id!=endpoint.value.organization_id)continue;
  const auto index=count++;if(index<cursor||index>=cursor+16)continue;const auto* previous=shop_registry_detail::latest(x,court->id,l.common.state().next_instance_id);
  PoliticalAcceptShopRegistry command{authority,row.first,control.value.registration_fact_id,control.value.approval_fact_id,previous?std::optional<StableId>{previous->id}:std::nullopt};auto quote=preview_political_shop_registry_acceptance(x,verified,actor,command);
  V::Dict item{{"policy_id",V{row.first}},{"court_id",V{court->id}},{"label",V{StableId{"接收原完整司法册"}}},{"command",V{StableId{political_shop_registry_accept_command}}},{"params",encode_political_accept_shop_registry(command)},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}};if(quote.ok)item.emplace_back("retained_source_count",V{static_cast<Integer>(quote.value.retained_legal_fact_ids.size())});if(previous)item.emplace_back("previous_acceptance_fact_id",V{previous->id});rows.emplace_back(std::move(item));
 }
 if(cursor>count)return {false,{},common_failure("InvalidPoliticalShopRegistryCursor").error};V::Dict page{{"ok",V{true}},{"enabled",V{count>0}},{"source_identity",identity(l.common.state().identity)},{"observer_actor_id",V{actor}},{"place_id",V{place}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(count>cursor+16)page.emplace_back("next_cursor",V{cursor+16});return {true,V{std::move(page)},{}};
}
}
