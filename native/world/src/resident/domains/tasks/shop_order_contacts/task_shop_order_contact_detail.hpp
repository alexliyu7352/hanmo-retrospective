#pragma once
#include "task_shop_order_contacts.hpp"
#include "resident/model/procurement_timing.hpp"
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident::shop_contact_detail {
inline std::optional<Integer> serial(const CommonStore& c,const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return {};const auto text=id.substr(lead.size());Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=text)return {};return n;}
inline bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
inline bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline Outcome<Integer> through(const CommonStore& c,const DynamicStepInput* p){if(!p)return {true,c.tick(),{}};if(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||p->from_tick!=c.tick()||p->to_tick!=c.tick()+1)return {false,{},common_failure("TaskShopOrderContactPhaseInvalid").error};return {true,p->to_tick,{}};}
inline const TaskShopOrderBinding* binding(const TaskShopOrderState& s,const StableId& task){const TaskShopOrderBinding* out=nullptr;for(const auto& row:s.bindings.rows())if(row.second.task_id==task){if(out)return nullptr;out=&row.second;}return out;}
inline bool context(const InteractionContext& a,const InteractionContext& b){return !a.kind&&!a.encounter_id&&!a.input_error&&!b.kind&&!b.encounter_id&&!b.input_error&&a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id;}
inline bool work(const TaskShopOrderContactStart& c,const TaskShopOrderAction& a){return c.action_id==a.action_id&&c.task_id==a.task_id&&c.session_id==a.session_id&&c.dialogue_id==a.dialogue_id&&c.node_id==a.node_id&&c.started_tick==a.started_tick&&context(c.context,a.context);}
inline Status presence(const TaskShopOrderContactContext& x,const StableId& actor,Integer tick,const StableId& fact){if(!x.presence_before)return common_failure("TaskShopOrderContactPresenceRequired",fact);auto p=x.presence_before(actor,tick,fact);if(!p.ok)return {false,{},p.error};if(!identity(p.value.source_identity,x.common.state().identity)||p.value.actor_id!=actor||p.value.before_fact_id!=fact||p.value.observed_tick!=tick||!p.value.alive)return common_failure("TaskShopOrderContactPersonNotPresent",fact);return common_success();}
Status source(const TaskShopOrderContactContext&,const TaskShopOrderContactStart&);
}
