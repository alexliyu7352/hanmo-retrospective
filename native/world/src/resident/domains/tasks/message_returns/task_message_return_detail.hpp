// 原实例号只作具名事实的双序交叉核；不把任意数字区间当会面或控制授权。
#pragma once
#include "task_message_return.hpp"
#include "resident/model/procurement_timing.hpp"
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident::message_return_detail {
inline std::optional<Integer> serial(const CommonStore& c,const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return {};const auto text=id.substr(lead.size());Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=text)return {};return n;}
inline bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
inline bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline bool context(const InteractionContext& a,const InteractionContext& b){return !a.kind&&!a.encounter_id&&!a.input_error&&!b.kind&&!b.encounter_id&&!b.input_error&&a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id;}
inline Outcome<Integer> through(const CommonStore& c,const DynamicStepInput* p){if(!p)return {true,c.tick(),{}};if(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||p->from_tick!=c.tick()||p->to_tick!=c.tick()+1)return {false,{},common_failure("TaskMessageReturnPhaseInvalid").error};return {true,p->to_tick,{}};}
inline Status presence(const TaskMessageReturnSources& x,const StableId& actor,Integer tick,const StableId& fact){if(!x.presence_before)return common_failure("TaskMessageReturnPresenceRequired",fact);auto p=x.presence_before(actor,tick,fact);if(!p.ok)return {false,{},p.error};if(!identity(p.value.source_identity,x.common.state().identity)||p.value.actor_id!=actor||p.value.before_fact_id!=fact||p.value.observed_tick!=tick||!p.value.alive)return common_failure("TaskMessageReturnPersonNotPresent",fact);return common_success();}
inline bool same_lot(const LotView& a,const LotView& b){return a.id==b.id&&a.item_id==b.item_id&&a.quantity==b.quantity&&a.reserved_by==b.reserved_by&&owner_equal(a.owner,b.owner)&&a.extra==b.extra;}
inline const TaskMessageReturnStart* start_for_action(const TaskMessageReturnState& state,const StableId& action){const TaskMessageReturnStart* found=nullptr;for(const auto& row:state.starts.rows())if(row.second.action_id==action){if(found)return nullptr;found=&row.second;}return found;}
}
