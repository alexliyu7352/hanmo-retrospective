// 原首职/专用Grant/个人新职责资格分别核证，接待岗位不成为公权的替代来源。
#pragma once
#include "political_office_reception_rules.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident::office_reception_detail {
// 只沿原专用Grant，不能拼接其它权限；目录已限定零金额和唯一柜台范围。
inline Status grant(const PoliticalOfficeReceptionContext& x,const PoliticalOfficeReceptionPolicy& p,Integer tick){const auto* g=x.organization_catalog.grants.find(p.reception_grant_id);if(!g||tick<g->valid_from||(g->valid_until&&tick>*g->valid_until))return common_failure("PoliticalOfficeReceptionGrantExpired",p.id);return common_success();}
// 原OrgDuty即使暂时inactive仍保其固定actor历史，本模块不占用该岗位。
inline Status duty(const PoliticalOfficeReceptionContext& x){for(const auto& p:x.catalog.policies.rows())for(const auto& d:x.organizations.duties.rows())if(d.second.role_id==p.second.role_id)return common_failure("PoliticalOfficeReceptionDutyDispositionRequired",p.second.role_id);return common_success();}
// 历史只重建签字生命/成年前界，柜台办理由永久业务事实见证，不冒重放所有旧位置占用。
inline Status person_before(const PoliticalOfficeReceptionContext& x,const StableId& actor,Integer tick,const StableId& fact){auto person=read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,actor,tick,fact);if(!person.ok)return {false,{},person.error};const auto* record=x.common.state().actors.find(actor);if(!person.value.alive)return common_failure("PoliticalOfficeReceptionSignerDead",actor);constexpr Integer bound=2147483647;if(!record||!record->birth_tick||*record->birth_tick< -bound||tick<0||tick>bound||*record->birth_tick>tick)return common_failure("ActorAgeUnknown",actor);return tick-*record->birth_tick>=18*720?common_success():common_failure("OfficeRequiresAdult",actor);}
// 单个实际Office截面同时核当时本人/epoch，副手不因为原继任人暂缺就获得接待认领资格。
inline Status office(const PoliticalOfficeReceptionContext& x,const PoliticalOfficeReceptionAssignment& a,const PoliticalSuccessionPolicy& p){auto read=read_political_office_reception_office_before(x.common,x.organizations,x.organization_catalog,p,a.tick,a.id);if(!read.ok)return {false,{},read.error};if(read.value.vacant||read.value.holder_actor_id!=std::optional<StableId>{a.actor_id}||read.value.office_epoch!=a.office_epoch||read.value.latest_office_fact_id!=a.office_change_fact_id)return common_failure("PoliticalOfficeReceptionHistoricalOfficeMismatch",a.id);return common_success();}
}
