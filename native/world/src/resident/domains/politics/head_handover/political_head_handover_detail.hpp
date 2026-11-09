// 生前让职冷热门共用同一专项Grant和原首任，不能复用死亡提名或先免职后失效的旧许可。
#pragma once
#include "political_head_handover_rules.hpp"
#include "political_head_handover_time2.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident::head_handover_detail {
// /2 never routes historical control through the legacy tick-only callback.
inline Status seat_source(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& p,Integer tick,const StableId& boundary){
 if(x.state.version==2){if(!x.exact)return common_failure("HeadHandoverTime2OriginalContextRequired");return x.exact->territory.validate_head_handover_seat2(x,p,tick,boundary);}
 return x.seat?x.seat(p,tick,boundary):common_failure("PoliticalHeadHandoverHistoricalReadersRequired");
}
// 任一真实答复或撤回都关闭原offer；已执行交接不能被当成仍可撤回的意愿。
inline bool closed(const PoliticalHeadHandoverState& s,const StableId& offer){for(const auto& row:s.answers.rows())if(row.second.offer_fact_id==offer)return true;for(const auto& row:s.withdrawals.rows())if(row.second.offer_fact_id==offer)return true;return false;}
// 本人必须真实首府空闲可行动；相同地图坐标不能替代原SettlementLocation。
inline Status contact(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor){auto v=politics_detail::available(x.common,x.activity,actor);if(!v.ok)return v;auto place=politics_detail::place(x.common,actor);return place&&place->place_id==p.seat_place_id?common_success():common_failure("PoliticalHeadHandoverSeatRequired",actor);}
// 原成员拒绝新职责时不因让职关系被绕过；历史资格另按本业务头重放原回应。
inline Status member_now(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor){OrganizationAccess read(x.common,x.organizations,x.organization_catalog,x.economy);return read.actor_accepts_duty(actor,p.organization_id)?common_success():common_failure("PoliticalHeadHandoverMemberUnavailable",actor);}
inline Status member_before(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor,Integer tick,const StableId& fact){
 if(x.state.version==2){const auto* h=x.common.state().facts.find(fact);if(!h||!x.exact)return common_failure("HeadHandoverTime2SourceRequired",fact);
  auto at=read_political_head_handover_moment2(x,{h->domain,h->kind,fact});if(!at.ok)return {false,{},at.error};
  return validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,
   {p.organization_id,p.leader_office_id,0,actor},tick,fact,PoliticalMemberHistoryUse::political_head_handover,nullptr,&x.exact->clock,&at.value);
 }
 return validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,{p.organization_id,p.leader_office_id,0,actor},tick,fact,PoliticalMemberHistoryUse::political_head_handover);
}
// 历史只证明出生/成年与真实死因前界；实际当面行为由该次本业务永久记录见证。
inline Status person_before(const PoliticalHeadHandoverContext& x,const StableId& actor,Integer tick,const StableId& fact){if(x.state.version==2)return validate_head_handover_person2(x,actor,fact);auto source=read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,actor,tick,fact);if(!source.ok)return {false,{},source.error};if(!source.value.alive)return common_failure("PoliticalHeadHandoverSignerDead",actor);const auto* person=x.common.state().actors.find(actor);constexpr Integer bound=2147483647;if(!person||!person->birth_tick||*person->birth_tick< -bound||tick<0||tick>bound||*person->birth_tick>tick)return common_failure("ActorAgeUnknown",actor);return tick-*person->birth_tick>=18*720?common_success():common_failure("OfficeRequiresAdult",actor);}
// 新独立Grant同时约束签发与接受时点；首片零资金、不引用原任何钱包或库存。
inline Status grant(const PoliticalHeadHandoverContext& x,const PoliticalHeadHandoverPolicy& p,const PoliticalSuccessionPolicy& original,const StableId& saved,const StableId& kind,Integer tick,const StableId& fact={}){if(x.state.version==2)return validate_head_handover_authority2(x,p,original,fact);const auto* g=x.organization_catalog.grants.find(saved);const auto* office=x.organization_catalog.offices.find(original.leader_office_id);if(saved!=p.handover_grant_id||!g||!office||g->office_id!=office->id||std::find(office->grant_ids.begin(),office->grant_ids.end(),saved)==office->grant_ids.end()||tick<g->valid_from||(g->valid_until&&tick>*g->valid_until)||std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),kind)==g->allowed_command_kinds.end()||std::none_of(g->scopes.begin(),g->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==original.seat_place_id;}))return common_failure("PoliticalHeadHandoverSpecificGrantRequired",saved);return common_success();}
// 首版只交原首任尚未发生任何换任的职位；已有副手须另有明确处置，不能默取其位置。
inline Status original_office(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor,Integer epoch,Integer tick,const StableId& boundary){if(x.state.version==2)return validate_head_handover_office2(x,p,actor,epoch,boundary);auto read=boundary.empty()?read_political_succession_office(x.common,x.organizations,x.organization_catalog,p):read_political_head_handover_office_before(x.common,x.organizations,x.organization_catalog,p,tick,boundary);if(!read.ok)return {false,{},read.error};const auto* initial=x.organization_catalog.offices.find(p.leader_office_id);const auto* current=x.organizations.offices.find(p.leader_office_id);if(!initial||!current||initial->deputy_actor_id||(boundary.empty()&&current->deputy_actor_id))return common_failure("PoliticalHeadHandoverDeputyDispositionRequired",p.id);return read.value.original_leader_actor_id==actor&&read.value.holder_actor_id==std::optional<StableId>{actor}&&read.value.office_epoch==epoch&&epoch==initial->epoch?common_success():common_failure("PoliticalHeadHandoverOriginalLeaderRequired",p.id);}
}
