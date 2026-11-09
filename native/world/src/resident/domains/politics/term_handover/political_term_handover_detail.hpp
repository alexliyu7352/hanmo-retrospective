// 生前让职冷热门共用同一专项Grant和本任首职，不能复用死亡提名或先免职后失效的旧许可。
#pragma once
#include "political_term_handover_rules.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident::term_handover_detail {
// 任一真实答复或撤回都关闭原offer；已执行交接不能被当成仍可撤回的意愿。
inline bool closed(const PoliticalTermHandoverState& s,const StableId& offer){for(const auto& row:s.answers.rows())if(row.second.offer_fact_id==offer)return true;for(const auto& row:s.withdrawals.rows())if(row.second.offer_fact_id==offer)return true;return false;}
// 本人必须真实首府空闲可行动；相同地图坐标不能替代原SettlementLocation。
inline Status contact(const PoliticalTermHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor){auto v=politics_detail::available(x.common,x.activity,actor);if(!v.ok)return v;auto place=politics_detail::place(x.common,actor);return place&&place->place_id==p.seat_place_id?common_success():common_failure("PoliticalTermHandoverSeatRequired",actor);}
// 原成员拒绝新职责时不因让职关系被绕过；历史资格另按本业务头重放原回应。
inline Status member_now(const PoliticalTermHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor){OrganizationAccess read(x.common,x.organizations,x.organization_catalog,x.economy);return read.actor_accepts_duty(actor,p.organization_id)?common_success():common_failure("PoliticalTermHandoverMemberUnavailable",actor);}
inline Status member_before(const PoliticalTermHandoverContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor,Integer tick,const StableId& fact){return validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,{p.organization_id,p.leader_office_id,0,actor},tick,fact,PoliticalMemberHistoryUse::political_term_handover);}
// 历史只证明出生/成年与真实死因前界；实际当面行为由该次本业务永久记录见证。
inline Status person_before(const PoliticalTermHandoverContext& x,const StableId& actor,Integer tick,const StableId& fact){auto source=read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,actor,tick,fact);if(!source.ok)return {false,{},source.error};if(!source.value.alive)return common_failure("PoliticalTermHandoverSignerDead",actor);const auto* person=x.common.state().actors.find(actor);constexpr Integer bound=2147483647;if(!person||!person->birth_tick||*person->birth_tick< -bound||tick<0||tick>bound||*person->birth_tick>tick)return common_failure("ActorAgeUnknown",actor);return tick-*person->birth_tick>=18*720?common_success():common_failure("OfficeRequiresAdult",actor);}
// 新独立Grant同时约束签发与接受时点；首片零资金、不引用原任何钱包或库存。
inline Status grant(const PoliticalTermHandoverContext& x,const PoliticalTermHandoverPolicy& p,const PoliticalSuccessionPolicy& original,const StableId& saved,const StableId& kind,Integer tick){const auto* g=x.organization_catalog.grants.find(saved);const auto* office=x.organization_catalog.offices.find(original.leader_office_id);if(saved!=p.handover_grant_id||!g||!office||g->office_id!=office->id||std::find(office->grant_ids.begin(),office->grant_ids.end(),saved)==office->grant_ids.end()||tick<g->valid_from||(g->valid_until&&tick>*g->valid_until)||std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),kind)==g->allowed_command_kinds.end()||std::none_of(g->scopes.begin(),g->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==original.seat_place_id;}))return common_failure("PoliticalTermHandoverSpecificGrantRequired",saved);return common_success();}
// 后任必须来自已完成的原政治就任，且同一Office根/epoch在签发及接受时均未被后来换任取代。
inline Outcome<std::optional<PoliticalHeadSource>> head(const PoliticalTermHandoverContext& x,const PoliticalTermHandoverPolicy& p,Integer tick,const StableId& boundary){return boundary.empty()?read_political_successor_round_head(x.heads,x.round_catalog,x.rounds,p.successor_round_policy_id):read_political_successor_round_head_before_term_handover(x.heads,x.round_catalog,x.rounds,p.successor_round_policy_id,tick,boundary);}
// 此纯读只证明真实在位，不把先前意愿、Family控制或副手身份当公职权源。
inline Status installed_office(const PoliticalTermHandoverContext& x,const PoliticalTermHandoverPolicy& p,const PoliticalSuccessionPolicy& original,const StableId& actor,Integer epoch,Integer tick,const StableId& boundary,const StableId& source,const StableId& change={}){
 auto read=head(x,p,tick,boundary);if(!read.ok)return {false,{},read.error};const auto* initial=x.organization_catalog.offices.find(original.leader_office_id);const auto* current=x.organizations.offices.find(original.leader_office_id);if(!initial||!current||initial->deputy_actor_id||(boundary.empty()&&current->deputy_actor_id))return common_failure("PoliticalTermHandoverDeputyDispositionRequired",p.id);
 if(!read.value||read.value->origin==PoliticalHeadOrigin::authored||read.value->source_fact_id.empty()||read.value->office_change_fact_id.empty()||read.value->actor_id!=actor||read.value->office_epoch!=epoch||read.value->source_fact_id!=source||(!change.empty()&&read.value->office_change_fact_id!=change))return common_failure("PoliticalTermHandoverInstalledHeadRequired",p.id);return common_success();
}
// 政策总是沿明确原新轮政策回到同一组织首职，不允许换库或改组织。
inline const PoliticalSuccessionPolicy* original_policy(const PoliticalTermHandoverContext& x,const PoliticalTermHandoverPolicy& p){const auto* round=x.round_catalog.policies.find(p.successor_round_policy_id);return round?x.heads.succession_catalog.policies.find(round->succession_policy_id):nullptr;}
}
