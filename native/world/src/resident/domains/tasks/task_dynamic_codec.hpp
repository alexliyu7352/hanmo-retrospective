// 动态领域冷JSON与二进制检查点共用显式字段访问；转换器不会保存在运行世界里。
#pragma once
#include "task_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <optional>
#include <string_view>
namespace hanmo::resident::dynamic_codec {
// 冷边界访问器严格核对字段、类型和集合形状；任何未知字段都不能静默遗失。
class JsonArchive {
    // Only a root owns a visitor/output. Child adapters borrow the exact
    // structural visitor supplied by the one original JSON engine.
    BoundaryValue value_;
    std::optional<ActorFoundationJsonVisitor> owner_;
    ActorFoundationJsonVisitor* original_=nullptr;
    explicit JsonArchive(ActorFoundationJsonVisitor& original):original_(&original){}
public:
    JsonArchive():owner_(std::in_place,value_),original_(&*owner_){}
    explicit JsonArchive(const BoundaryValue& input):owner_(std::in_place,input),original_(&*owner_){}
    bool reading()const{return original_->reading();}
    Status status()const{return original_->status();}
    const BoundaryValue& value()const{return value_;}
    Status reject(std::string_view code,std::string_view path={}){return original_->reject(code,path);}
    template<class T>Status field(std::string_view name,T& value){return original_->field(name,value);}
    template<class T>Status prepare_default_values(std::uint64_t count=1){return original_->template prepare_default_values<T>(count);}
    Status unsigned_field(std::string_view name,std::uint64_t& value){return original_->unsigned_field(name,value);}
    template<class F>Status object(std::string_view name,F visit){
        return original_->object(name,[&](ActorFoundationJsonVisitor& child){JsonArchive adapter(child);return visit(adapter);});
    }
    template<class T,class Allocator,class F>Status sequence(std::string_view name,std::vector<T,Allocator>& values,F visit){
        // Dynamic rows are objects even for string/integer T. Their original
        // {value:...} wire must not become the foundation scalar-array wire.
        return original_->sequence(name,values,[&](ActorFoundationJsonVisitor& child,T& value){JsonArchive adapter(child);return visit(adapter,value);},false);
    }
    template<class Range,class F>Status write_sequence(std::string_view name,const Range& values,F visit){
        return original_->write_sequence(name,values,[&](ActorFoundationJsonVisitor& child,const auto& value){JsonArchive adapter(child);return visit(adapter,value);},false);
    }
    template<class T,class F>Status optional(std::string_view name,std::optional<T>& value,F visit){
        // Preserve the original dynamic {present,...} object, not foundation's
        // null-or-value encoding. Structural field/object ownership is shared.
        return object(name,[&](JsonArchive& child){
            bool present=value.has_value();auto valid=child.field("present",present);if(!valid.ok)return valid;
            if(child.reading()){
                if(present){valid=child.template prepare_default_values<T>();if(!valid.ok)return valid;value.emplace();}
                else value.reset();
            }
            return present?visit(child,*value):child.status();
        });
    }
    Status finish(){return original_->finish();}
};
// 两种冷编码共享同一声明顺序，未知枚举及重复ID在构造引用前拒绝。
template<class A,class E>Status enumeration(A& a,std::string_view key,E& value,Integer maximum){Integer raw=static_cast<Integer>(value);a.field(key,raw);if(raw<0||raw>maximum)return a.reject("UnknownDynamicEnum",key);if(a.reading())value=static_cast<E>(raw);return a.status();}
template<class A>Status strings(A& a,std::string_view key,std::vector<StableId>& values){return a.sequence(key,values,[](auto& row,auto& value){return row.field("value",value);});}
template<class A,class T,class F>Status table(A& a,std::string_view key,OrderedTable<T>& value,F visit,std::string_view duplicate_code="DuplicateDynamicId"){
 auto record=[&](auto& row,auto& entry){row.field("key",entry.first);return row.object("record",[&](auto& nested){
  if constexpr(std::is_invocable_r_v<Status,F&,decltype(nested),const StableId&,T&>)return visit(nested,static_cast<const StableId&>(entry.first),entry.second);
  else return visit(nested,entry.second);
 });};
 // Typed retention visits this exact original payload once under the actual
 // table row credits, never a second copied sequence or arbitrary root skip.
 if constexpr(std::is_same_v<std::remove_cv_t<std::remove_reference_t<A>>,CodecVisitor>){}
 // The sole Common collection/adoption mechanism preserves the existing
 // dynamic {key,record} callback and whole ordinary decoded-vector ownership.
 return common_checkpoint::table_rows(a,key,value,duplicate_code,record);
}
template<class A>Status owner(A& a,Owner& value){enumeration(a,"kind",value.kind,3);a.field("id",value.id);return a.status();}
template<class A>Status subject(A& a,SubjectRef& value){enumeration(a,"kind",value.kind,2);a.field("id",value.id);return a.status();}
template<class A>Status scope(A& a,ScopeRef& value){enumeration(a,"kind",value.kind,4);a.field("id",value.id);return a.status();}
template<class A>Status authority(A& a,ActingAuthority& value){a.field("organization_id",value.organization_id);a.field("office_id",value.office_id);a.field("authority_epoch",value.authority_epoch);a.field("actor_id",value.actor_id);return a.status();}
template<class A>Status offer_ref(A& a,OfferRef& value){enumeration(a,"kind",value.kind,1);a.field("id",value.id);return a.status();}
template<class A>Status definition(A& a,TaskDefinition& v){a.field("id",v.id);a.field("template_id",v.template_id);a.field("accept_role",v.accept_role);a.field("report_role",v.report_role);a.field("funding_wallet",v.funding_wallet);a.field("on_unavailable",v.on_unavailable);a.field("display_name",v.display_name);a.object("issuer",[&](auto& c){return owner(c,v.issuer);});a.field("budget",v.budget);a.field("reward",v.reward);a.field("accept_ticks",v.accept_ticks);a.field("deadline_ticks",v.deadline_ticks);return a.object("goal",[&](auto& c){auto& g=v.goal;c.field("item_id",g.item_id);c.field("message_id",g.message_id);c.field("receiver_role",g.receiver_role);c.field("receiver_actor",g.receiver_actor);c.field("actor_id",g.actor_id);c.field("destination_place_id",g.destination_place_id);c.field("objective_id",g.objective_id);c.field("quantity",g.quantity);return c.status();});}
template<class A>Status binding(A& a,SourceBinding& v){a.field("affair_id",v.affair_id);a.field("need_id",v.need_id);a.field("claim_id",v.claim_id);a.field("contract_id",v.contract_id);a.field("source_revision",v.source_revision);return a.status();}
template<class A>Status terms(A& a,FrozenTaskTerms& v){a.object("definition",[&](auto& c){return definition(c,v.definition);});a.field("agreed_quantity",v.agreed_quantity);a.field("purchase_budget_cap",v.purchase_budget_cap);a.field("reward_total",v.reward_total);a.field("deadline_tick",v.deadline_tick);a.field("target_inventory_id",v.target_inventory_id);a.field("item_id",v.item_id);a.field("quantity_unit",v.quantity_unit);a.field("source_wallet_id",v.source_wallet_id);a.field("reward_wallet_id",v.reward_wallet_id);a.field("payee_wallet_id",v.payee_wallet_id);a.object("beneficiary",[&](auto& c){return subject(c,v.beneficiary);});a.field("terms_version",v.terms_version);a.field("payment_policy",v.payment_policy);a.field("unused_budget_policy",v.unused_budget_policy);a.field("late_reward_policy",v.late_reward_policy);a.field("cancellation_policy_id",v.cancellation_policy_id);a.field("personal_advance_allowed",v.personal_advance_allowed);return a.status();}
template<class A>Status offer(A& a,OfferInstance& v){a.field("id",v.id);a.field("template_id",v.template_id);a.field("source_need_id",v.source_need_id);a.field("need_revision_at_creation",v.need_revision_at_creation);a.object("issuer",[&](auto& c){return owner(c,v.issuer);});a.object("accept_scope",[&](auto& c){return scope(c,v.accept_scope);});a.object("report_scope",[&](auto& c){return scope(c,v.report_scope);});a.field("funding_wallet_id",v.funding_wallet_id);a.field("reward_wallet_id",v.reward_wallet_id);a.field("terms_version",v.terms_version);a.field("available_from_tick",v.available_from_tick);a.optional("expires_tick",v.expires_tick,[](auto& c,auto& x){return c.field("value",x);});enumeration(a,"status",v.status,2);a.object("definition",[&](auto& c){return definition(c,v.definition);});return a.status();}
template<class A>Status contract_prefix(A& a,ContractRecord& v){a.field("id",v.id);a.object("source_binding",[&](auto& c){return binding(c,v.source_binding);});a.object("issuer",[&](auto& c){return owner(c,v.issuer);});a.object("assignee",[&](auto& c){return subject(c,v.assignee);});a.optional("executor_actor_id",v.executor_actor_id,[](auto& c,auto& x){return c.field("value",x);});a.optional("org_activity_id",v.org_activity_id,[](auto& c,auto& x){return c.field("value",x);});a.optional("current_task_id",v.current_task_id,[](auto& c,auto& x){return c.field("value",x);});strings(a,"historical_task_ids",v.historical_task_ids);a.object("frozen_terms",[&](auto& c){return terms(c,v.frozen_terms);});enumeration(a,"status",v.status,3);a.optional("settlement_id",v.settlement_id,[](auto& c,auto& x){return c.field("value",x);});strings(a,"restitution_refs",v.restitution_refs);a.field("delivered_quantity",v.delivered_quantity);a.field("eligible_delivered_quantity",v.eligible_delivered_quantity);a.field("paid_reward",v.paid_reward);a.field("refunded_budget",v.refunded_budget);a.field("refunded_reward",v.refunded_reward);a.field("execution_stopped",v.execution_stopped);a.field("stop_reason",v.stop_reason);a.optional("accepted_authority",v.accepted_authority,[](auto& c,auto& x){return authority(c,x);});strings(a,"cargo_disposition_ids",v.cargo_disposition_ids);return a.status();}
template<class A>Status contract(A&a,ContractRecord&v){if(v.deadline_closure)return a.reject("QuantityDeadlineRequiresDynamicTasks4");return contract_prefix(a,v);}
template<class A>Status exact_moment(A& a,WorldMoment& v){if(!a.status().ok)return a.status();if constexpr(std::is_same_v<std::remove_cv_t<std::remove_reference_t<A>>,CodecVisitor>){}if(!prepare_checkpoint_default_values<EncodedWorldMoment>(a).ok)return a.status();EncodedWorldMoment e;if(!a.reading()){auto r=encode_world_moment(v);if(!r.ok)return a.reject(r.error.original.code);e=std::move(r.value);}a.field("half_day_tick",e.half_day_tick);a.object("sub_half_day",[&](auto& x){x.field("numerator",e.sub_half_day.numerator);return x.field("denominator",e.sub_half_day.denominator);});if(a.reading()&&a.status().ok){auto r=decode_world_moment(e);if(!r.ok)return a.reject(r.error.original.code);v=std::move(r.value);}return a.status();}
template<class A>Status completion_time(A& a,Integer& tick,std::optional<WorldMoment>& at,bool exact){if(!exact){if(at)return a.reject("UnexpectedExactTaskTime");return a.field("completed_tick",tick);}if(a.reading()){if(!prepare_checkpoint_default_values<WorldMoment>(a).ok)return a.status();at.emplace();}if(!at)return a.reject("MissingExactTaskTime");a.object("completed_at",[&](auto& x){return exact_moment(x,*at);});if(a.reading()&&a.status().ok)tick=at->half_day_tick;return a.status();}
template<class A>Status exact_quantity_completion_result(A&a,std::optional<SourceRef>&ref,bool exact){
 if(!exact)return ref?a.reject("ExactQuantityCompletionResultRequiresSource2"):a.status();
 if(a.reading()){if(!prepare_checkpoint_default_values<SourceRef>(a).ok)return a.status();ref.emplace();}if(!ref)return a.reject("ExactQuantityCompletionResultRequired");
 return a.object("original_completion_result_ref",[&](auto&c){c.field("domain",ref->domain);c.field("kind",ref->kind);c.field("id",ref->id);if(ref->domain!="common"||ref->kind!="command_result"||ref->id.empty())return c.reject("ExactQuantityActualCommonResultRequired");return c.status();});
}
template<class A>Status delivery(A& a,DeliveryFact& v,bool exact=false){if(a.reading())v.quantity_source_kind=exact?TaskQuantitySourceKind::procurement_exact:TaskQuantitySourceKind::legacy_procurement;else if(v.quantity_source_kind!=(exact?TaskQuantitySourceKind::procurement_exact:TaskQuantitySourceKind::legacy_procurement))return a.reject("TaskQuantitySourceKindMismatch");a.field("id",v.id);a.field("task_id",v.task_id);a.field("activity_id",v.activity_id);a.field("source_action_id",v.source_action_id);a.field("from_inventory_id",v.from_inventory_id);a.field("to_inventory_id",v.to_inventory_id);a.object("beneficiary",[&](auto& c){return subject(c,v.beneficiary);});a.field("item_id",v.item_id);a.field("quantity",v.quantity);completion_time(a,v.completed_tick,v.completed_at,exact);a.field("transfer_fact_id",v.transfer_fact_id);return exact_quantity_completion_result(a,v.original_completion_result_ref,exact);}
template<class A>Status transfer(A& a,TaskTransferFact& v,bool exact=false){if(a.reading())v.quantity_source_kind=exact?TaskQuantitySourceKind::procurement_exact:TaskQuantitySourceKind::legacy_procurement;else if(v.quantity_source_kind!=(exact?TaskQuantitySourceKind::procurement_exact:TaskQuantitySourceKind::legacy_procurement))return a.reject("TaskQuantitySourceKindMismatch");a.field("id",v.id);a.field("contract_id",v.contract_id);a.field("task_id",v.task_id);a.field("source_action_id",v.source_action_id);a.field("from_inventory_id",v.from_inventory_id);a.field("to_inventory_id",v.to_inventory_id);a.field("item_id",v.item_id);a.field("quantity",v.quantity);completion_time(a,v.completed_tick,v.completed_at,exact);a.object("from_owner",[&](auto& c){return owner(c,v.from_owner);});a.object("to_owner",[&](auto& c){return owner(c,v.to_owner);});a.field("purpose",v.purpose);return exact_quantity_completion_result(a,v.original_completion_result_ref,exact);}
template<class A>Status shipment(A& a,Shipment& v){a.field("id",v.id);a.field("activity_id",v.activity_id);a.field("source_inventory_id",v.source_inventory_id);a.field("cargo_inventory_id",v.cargo_inventory_id);a.field("destination_inventory_id",v.destination_inventory_id);a.object("cargo_owner",[&](auto& c){return owner(c,v.cargo_owner);});a.object("carrier_ref",[&](auto& c){return subject(c,v.carrier_ref);});a.field("route_or_action_id",v.route_or_action_id);a.field("status",v.status);return strings(a,"receipt_ids",v.receipt_ids);}
// 货损锚点只保位置的闭合原生分支；未知标签不能退化成默认场所。
template<class A>Status cargo_location(A& a,Location& v){Integer kind=static_cast<Integer>(v.index());a.field("kind",kind);if(!a.status().ok)return a.status();if(kind<0||kind>4)return a.reject("UnknownCargoLocation");if(a.reading()){switch(kind){case 0:{if(!prepare_checkpoint_default_values<SettlementLocation>(a).ok)return a.status();v=SettlementLocation{};}break;case 1:{if(!prepare_checkpoint_default_values<WildernessLocation>(a).ok)return a.status();v=WildernessLocation{};}break;case 2:{if(!prepare_checkpoint_default_values<FollowingLocation>(a).ok)return a.status();v=FollowingLocation{};}break;case 3:{if(!prepare_checkpoint_default_values<InArmyLocation>(a).ok)return a.status();v=InArmyLocation{};}break;default:{if(!prepare_checkpoint_default_values<OnRouteLocation>(a).ok)return a.status();v=OnRouteLocation{};}break;}}if(auto* x=std::get_if<SettlementLocation>(&v)){a.field("settlement_id",x->settlement_id);a.field("place_id",x->place_id);}else if(auto* x=std::get_if<WildernessLocation>(&v)){a.field("map_id",x->map_id);a.field("origin",x->origin);a.field("x",x->point.x);a.field("y",x->point.y);}else if(auto* x=std::get_if<FollowingLocation>(&v))a.field("party_id",x->party_id);else if(auto* x=std::get_if<InArmyLocation>(&v))a.field("army_id",x->army_id);else{auto& route=std::get<OnRouteLocation>(v);a.field("edge_id",route.edge_id);a.field("origin",route.origin);a.field("progress",route.progress);}return a.status();}
template<class A>Status cargo_policy(A& a,TaskCargoPolicy& v){a.field("id",v.id);a.field("version",v.version);a.field("task_template_id",v.task_template_id);a.field("item_id",v.item_id);a.field("outcome",v.outcome);a.field("recipient",v.recipient);a.field("allocation",v.allocation);a.field("unavailable_recipient",v.unavailable_recipient);a.field("transfer_cap",v.transfer_cap);a.field("destroy_cap",v.destroy_cap);return a.status();}
template<class A>Status cargo_exposure(A& a,TaskCargoExposure& v){a.field("battle_id",v.battle_id);a.field("encounter_id",v.encounter_id);a.field("contract_id",v.contract_id);a.field("task_id",v.task_id);a.field("carrier_actor_id",v.carrier_actor_id);a.field("recipient_actor_id",v.recipient_actor_id);a.field("recipient_prototype_id",v.recipient_prototype_id);a.field("source_inventory_id",v.source_inventory_id);a.field("destination_inventory_id",v.destination_inventory_id);a.object("policy",[&](auto& c){return cargo_policy(c,v.policy);});a.object("carrier_anchor",[&](auto& c){return cargo_location(c,v.carrier_anchor);});a.object("recipient_anchor",[&](auto& c){return cargo_location(c,v.recipient_anchor);});a.field("created_tick",v.created_tick);a.field("source_inventory_revision",v.source_inventory_revision);a.field("destination_inventory_revision",v.destination_inventory_revision);a.field("destination_capacity",v.destination_capacity);a.field("destination_weight",v.destination_weight);a.field("frozen_quantity",v.frozen_quantity);a.field("transfer_quantity",v.transfer_quantity);a.field("destroy_quantity",v.destroy_quantity);a.sequence("lots",v.lots,[](auto& c,auto& lot){c.field("lot_id",lot.lot_id);return c.field("quantity",lot.quantity);});enumeration(a,"resolution",v.resolution,2);a.field("source_result_id",v.source_result_id);a.field("disposition_fact_id",v.disposition_fact_id);a.field("resolution_reason",v.resolution_reason);a.field("resolved_tick",v.resolved_tick);return a.status();}
template<class A>Status cargo_disposition(A& a,TaskCargoDispositionFact& v){a.field("id",v.id);a.field("battle_id",v.battle_id);a.field("encounter_id",v.encounter_id);a.field("source_result_id",v.source_result_id);a.field("contract_id",v.contract_id);a.field("task_id",v.task_id);a.field("carrier_actor_id",v.carrier_actor_id);a.field("recipient_actor_id",v.recipient_actor_id);a.field("from_inventory_id",v.from_inventory_id);a.field("to_inventory_id",v.to_inventory_id);a.field("item_id",v.item_id);a.object("from_owner",[&](auto& c){return owner(c,v.from_owner);});a.object("to_owner",[&](auto& c){return owner(c,v.to_owner);});a.field("tick",v.tick);a.field("source_quantity_before",v.source_quantity_before);a.field("source_quantity_after",v.source_quantity_after);a.field("transferred_quantity",v.transferred_quantity);a.field("destroyed_quantity",v.destroyed_quantity);return a.status();}
// 旧TaskRecord和TaskAction字节保持原样，新字段由独立动态section按相同ID绑定。
// Finite borrowed view: no temporary ID vector or set on the writer path.
template<class Range,class Predicate>class FilteredRows {
 const Range& rows_;Predicate predicate_;
 using BaseIterator=decltype(std::declval<const Range&>().begin());
public:
 struct Iterator{BaseIterator current,end;const Predicate* predicate;void skip(){while(current!=end&&!(*predicate)(*current))++current;}const auto&operator*()const{return *current;}Iterator&operator++(){++current;skip();return *this;}bool operator!=(const Iterator&other)const{return current!=other.current;}};
 FilteredRows(const Range&rows,Predicate predicate):rows_(rows),predicate_(std::move(predicate)){}
 Iterator begin()const{Iterator value{rows_.begin(),rows_.end(),&predicate_};value.skip();return value;}Iterator end()const{return {rows_.end(),rows_.end(),&predicate_};}
 std::size_t size()const{std::size_t count=0;for(const auto&row:rows_)if(predicate_(row))++count;return count;}
};
template<class A>Status task_extensions(A& a,TaskState& state){
 auto task_fields=[&](auto& c,StableId& id,TaskRecord& task){c.field("task_id",id);c.optional("offer_ref",task.offer_ref,[](auto& nested,auto& value){return offer_ref(nested,value);});c.optional("contract_id",task.contract_id,[](auto& nested,auto& value){return nested.field("value",value);});if(state.exact_quantities)c.optional("accepted_at",task.accepted_at,[](auto& nested,auto& value){return exact_moment(nested,value);});return c.status();};
 auto action_fields=[&](auto& c,StableId& id,TaskAction& action){c.field("action_id",id);c.optional("delivery_quantity",action.delivery_quantity,[](auto& nested,auto& value){return nested.field("value",value);});return c.field("restitution",action.restitution);};
 if(!a.reading()){
  auto task_selected=[&](const auto&row){return row.second.contract_id||row.second.offer_ref||(state.exact_quantities&&row.second.accepted_at);};
  FilteredRows task_rows{state.tasks.rows(),task_selected};
  auto result=a.write_sequence("task_bindings",task_rows,[&](auto&c,const auto&row){return task_fields(c,const_cast<StableId&>(row.first),const_cast<TaskRecord&>(row.second));});if(!result.ok)return result;
  auto action_selected=[](const auto&row){return row.second.delivery_quantity.has_value();};FilteredRows action_rows{state.actions.rows(),action_selected};
  return a.write_sequence("action_bindings",action_rows,[&](auto&c,const auto&row){return action_fields(c,const_cast<StableId&>(row.first),const_cast<TaskAction&>(row.second));});
 }
 for(auto&row:state.tasks.rows()){row.second.offer_ref.reset();row.second.contract_id.reset();if(state.exact_quantities)row.second.accepted_at.reset();}
 for(auto&row:state.actions.rows()){row.second.delivery_quantity.reset();row.second.restitution=false;}
 std::vector<StableId>ids;
 auto result=a.sequence("task_bindings",ids,[&](auto&c,auto&id){c.field("task_id",id);for(const auto&prior:ids)if(&prior!=&id&&prior==id)return c.reject("DuplicateDynamicTaskBinding",id);auto*task=state.tasks.find(id);if(!task)return c.reject("MissingDynamicTask",id);c.optional("offer_ref",task->offer_ref,[](auto&nested,auto&value){return offer_ref(nested,value);});c.optional("contract_id",task->contract_id,[](auto&nested,auto&value){return nested.field("value",value);});if(state.exact_quantities)c.optional("accepted_at",task->accepted_at,[](auto&nested,auto&value){return exact_moment(nested,value);});return c.status();});if(!result.ok)return result;
 ids.clear();return a.sequence("action_bindings",ids,[&](auto&c,auto&id){c.field("action_id",id);for(const auto&prior:ids)if(&prior!=&id&&prior==id)return c.reject("DuplicateDynamicActionBinding",id);auto*action=state.actions.find(id);if(!action)return c.reject("MissingDynamicTaskAction",id);c.optional("delivery_quantity",action->delivery_quantity,[](auto&nested,auto&value){return nested.field("value",value);});return c.field("restitution",action->restitution);});
}

template<class A>Status tasks(A& a,TaskState& state){if(state.dynamic_tasks_version!=2)return a.reject("CompleteDynamicTaskSuccessorRootsRequired");table(a,"offer_instances",state.offer_instances,[](auto& c,auto& v){return offer(c,v);});table(a,"contracts",state.contracts,[](auto& c,auto& v){return contract(c,v);});table(a,"deliveries",state.deliveries,[&](auto& c,auto& v){return delivery(c,v,state.exact_quantities);});table(a,"transfers",state.transfers,[&](auto& c,auto& v){return transfer(c,v,state.exact_quantities);});table(a,"shipments",state.shipments,[](auto& c,auto& v){return shipment(c,v);});task_extensions(a,state);table(a,"cargo_exposures",state.cargo_exposures,[](auto& c,auto& v){return cargo_exposure(c,v);});table(a,"cargo_dispositions",state.cargo_dispositions,[](auto& c,auto& v){return cargo_disposition(c,v);});return a.status();}
} // serializer只暂借typed字段；目录身份和跨域引用仍由组合层严格核验。
