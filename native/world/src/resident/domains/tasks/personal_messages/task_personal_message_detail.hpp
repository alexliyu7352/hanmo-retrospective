// 双序只核这次真实个人送信来源；Known原会面与实际结果仍各自完整验证。
#pragma once
#include "task_personal_message_contract.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident::personal_message_detail {
inline std::optional<Integer> serial(const CommonStore& c,const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return {};const auto text=id.substr(lead.size());Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=text)return {};return n;}
inline bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto*first=c.state().facts.find(a);const auto*last=c.state().facts.find(b);if(!first||!last)return false;auto x=read_actual_common_fact_position(c,*first);auto y=read_actual_common_fact_position(c,*last);return x.ok&&y.ok&&x.value<y.value;}
inline bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline bool narrative(const InteractionContext& c){return c.kind==std::optional<StableId>{"narrative"}&&c.role_id.empty()&&!c.encounter_id&&!c.input_error&&!c.place_id.empty()&&!c.provider_actor_id.empty();}
inline bool context(const InteractionContext& a,const InteractionContext& b){return narrative(a)&&narrative(b)&&a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id;}
inline Outcome<Integer> through(const CommonStore& c,const DynamicStepInput* p,const std::optional<TaskExactReadClock>& exact={}){if(exact){if(p||!exact->elapsed||!exact->scale||!exact->world)return {false,{},common_failure("TaskExactClockPairRequired").error};return {true,exact->at.half_day_tick,{}};}if(!p)return {true,c.tick(),{}};if(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||p->from_tick!=c.tick()||p->to_tick!=c.tick()+1)return {false,{},common_failure("TaskPersonalMessagePhaseInvalid").error};return {true,p->to_tick,{}};}
inline Status presence(const TaskPersonalMessageSources& x,const StableId& actor,Integer tick,const StableId& fact){if(!x.presence_before)return common_failure("TaskPersonalMessagePresenceRequired",fact);auto p=x.presence_before(actor,tick,fact);if(!p.ok)return {false,{},p.error};if(!identity(p.value.source_identity,x.common.state().identity)||p.value.actor_id!=actor||p.value.before_fact_id!=fact||p.value.observed_tick!=tick||!p.value.alive)return common_failure("TaskPersonalMessagePersonNotPresent",fact);return common_success();}
inline const TaskPersonalMessageStart* start_for_action(const TaskPersonalMessageState& state,const StableId& action){const TaskPersonalMessageStart* out=nullptr;for(const auto& row:state.starts.rows())if(row.second.action_id==action){if(out)return nullptr;out=&row.second;}return out;}
inline const TaskPersonalMessageEnd* end_for_start(const TaskPersonalMessageState& state,const StableId& start){const TaskPersonalMessageEnd* out=nullptr;for(const auto& row:state.endings.rows())if(row.second.start_fact_id==start){if(out)return nullptr;out=&row.second;}return out;}
}
