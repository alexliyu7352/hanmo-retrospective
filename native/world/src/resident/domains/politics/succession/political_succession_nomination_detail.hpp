// 提名热/冷公用原来源约束，不能用当前首职去重新解释旧签署。
#pragma once
#include "political_succession_nominations.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident::succession_detail {
// 原记录只有真实唯一接受，后续拒绝/撤回不删旧本人选择。
inline const PoliticalSuccessionAnswer* accepted(const PoliticalSuccessionNominationState& s,const StableId& nomination){for(const auto& row:s.answers.rows())if(row.second.nomination_fact_id==nomination&&row.second.accepted)return &row.second;return nullptr;}
inline bool closed(const PoliticalSuccessionNominationState& s,const StableId& nomination){for(const auto& row:s.answers.rows())if(row.second.nomination_fact_id==nomination&&!row.second.accepted)return true;for(const auto& row:s.withdrawals.rows())if(row.second.nomination_fact_id==nomination)return true;return false;}
// 首版政策只完成作者原首任的一次正式交接；继任后的再提名须后续明确王朝合同。
inline bool acceded(const PoliticalSuccessionNominationContext& x,const StableId& nomination){if(x.accessions)for(const auto& row:x.accessions->accessions.rows()){const auto* a=x.state.answers.find(row.second.acceptance_fact_id);if(a&&a->nomination_fact_id==nomination)return true;}return false;}
// 两类新职责只用原成员回应，不因家族控制、同名角色或今日职位免除拒新职状态。
inline Status member_now(const PoliticalSuccessionNominationContext& x,const StableId& actor,const StableId& org){OrganizationAccess access(x.common,x.organizations,x.organization_catalog,x.economy);return access.actor_accepts_duty(actor,org)?common_success():common_failure("PoliticalSuccessionMemberUnavailable",actor);}
// 生前实际接受需要原两人同首府且空闲；不借表现坐标处理在军或旅途本人。
inline Status contact(const PoliticalSuccessionNominationContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor){auto valid=politics_detail::available(x.common,x.activity,actor);if(!valid.ok)return valid;auto here=politics_detail::place(x.common,actor);return here&&here->place_id==p.seat_place_id?common_success():common_failure("PoliticalSuccessionSeatRequired",actor);}
// 历史成年只取同一原人物出生来源和该次签署tick；存在/生命另由S4原事实证明。
inline Status person_before(const PoliticalSuccessionNominationContext& x,const StableId& actor,Integer tick,const StableId& fact){auto presence=read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,actor,tick,fact);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return common_failure("PoliticalSuccessionSignerDead",actor);const auto* person=x.common.state().actors.find(actor);constexpr Integer maximum=2147483647;if(!person||!person->birth_tick||*person->birth_tick< -maximum||tick<0||tick>maximum||*person->birth_tick>tick)return common_failure("ActorAgeUnknown",actor);return tick-*person->birth_tick>=18*720?common_success():common_failure("OfficeRequiresAdult",actor);}
// 原一份Grant必须完整授权对应命令；不得拼接不同Grant或放宽原期限。
inline Status grant(const PoliticalSuccessionNominationContext& x,const PoliticalSuccessionPolicy& p,const StableId& id,const StableId& kind,Integer tick){const auto* g=x.organization_catalog.grants.find(id);const auto* office=x.organization_catalog.offices.find(p.leader_office_id);if(!g||!office||id!=p.nomination_grant_id||g->office_id!=p.leader_office_id||std::find(office->grant_ids.begin(),office->grant_ids.end(),id)==office->grant_ids.end()||tick<g->valid_from||(g->valid_until&&tick>*g->valid_until)||std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),kind)==g->allowed_command_kinds.end()||std::none_of(g->scopes.begin(),g->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==p.seat_place_id;}))return common_failure("PoliticalSuccessionNominationGrantRequired",id);return common_success();}
// 旧成员函数先识别本业务精确头，再按原唯一episode和回应史核新职资格。
inline Status member_before(const PoliticalSuccessionNominationContext& x,const PoliticalSuccessionPolicy& p,const StableId& actor,Integer tick,const StableId& fact){return validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,{p.organization_id,p.leader_office_id,0,actor},tick,fact,PoliticalMemberHistoryUse::political_succession);}
}
