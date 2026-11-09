#include "native_organization_runtime.hpp"
// 新领域的跨域只读接线始终借当前候选；不把派生报告或合同余额保存为第二权威。
#include "world_bundle.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_sources.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/domains/organization/membership_offer.hpp"
namespace hanmo::resident {
// 私人动态来源仅在实际选定能力时参与同一社会读取，旧调用者默认拒绝。
bool BundleServices::permits_named_membership_facts()const{return world_.named_membership_enabled();}
bool BundleServices::organization_exists(const StableId& id)const{return world_.dynamic_enabled()&&world_.organization.organizations.find(id);}
bool BundleServices::household_exists(const StableId& id)const{return world_.dynamic_enabled()&&world_.local_economy.households.find(id);}
bool BundleServices::reservation_exists(const ReservationRef& ref)const{
    if(world_.family_care_relief_enabled()&&child_care_relief_reservation_exists(world_.common,world_.child_care_relief,ref))return true;
    if(world_.family_birth_enabled()&&family_birth_reservation_exists(world_.common,world_.family_birth,ref))return true;
    if(!world_.dynamic_enabled())return false;
    if(ref.kind==ReservationRef::Kind::contract&&ref.id.rfind("estate_employment:",0)==0)return world_.estate_employment_enabled()&&world_.estate_employment.contracts.find(ref.id);
    // 店单使用原独立Task合同预约；只由已启用目录和当前真实active绑定认领，不能凭前缀放行。
    if(ref.kind==ReservationRef::Kind::contract)return task_contract_reservation_exists(world_.tasks,ref.id)||(world_.governance_tasks_enabled()&&governance_reservation_exists(world_.tasks,world_.governance_tasks,ref.id))||(world_.task_shop_orders_enabled()&&world_.tasks.shop_orders&&task_shop_order_reservation_exists(world_.tasks,*world_.tasks.shop_orders,ref.id));
    if(ref.kind==ReservationRef::Kind::org_activity){const auto* activity=world_.organization.activities.find(ref.id);return activity&&(activity->status==OrgActivityStatus::approved||activity->status==OrgActivityStatus::active||activity->status==OrgActivityStatus::waiting);}
    return ref.kind==ReservationRef::Kind::action&&world_.common.action(ref.id);
}
OrganizationAccess BundleServices::organization_access() const {
  return {world_.common, world_.organization, *world_.catalog->organization,
          world_.local_economy,
          std::make_shared<NativeOrganizationAuthorityClock>(world_)};
}
// 只承认原领域已经具备永久完成事实的行动种类；普通等待/交易不伪造职责结果。
bool BundleServices::supports_action_result(const StableId& kind)const{return world_.dynamic_enabled()&&(kind=="study_skill"||kind=="learn_skill"||kind=="practice_skill"||kind=="teach_skill"||kind=="long_rest"||kind=="military_train"||(world_.regional_life_enabled()&&(kind=="service"||kind=="trade")));}
// 完成结果沿原事实ID读取；取消和中断保持失败，进行中与未知种类返回无终态。
std::optional<DynamicActionResult> BundleServices::action_result(const StableId& id)const{
    if(!world_.dynamic_enabled())return {};
    const auto* header=world_.common.state().facts.find(id);if(!header)return {};
    if(const auto* value=world_.growth.facts.find(id)){
        if(const auto* fact=std::get_if<TrainingFact>(value)){if(fact->phase!="completed"||header->kind!="training")return {};DynamicActionResult result{id,fact->action_kind,id,StableId{},fact->completed_tick,fact->success,{fact->training.student_id}};if(!fact->training.teacher_id.empty()&&fact->training.teacher_id!=fact->training.student_id)result.participant_actor_ids.push_back(fact->training.teacher_id);return result;}
        if(const auto* fact=std::get_if<StudyFact>(value)){if(fact->phase!="completed"||header->kind!="skill_study")return {};DynamicActionResult result{id,"study_skill",id,fact->code,fact->completed_tick,fact->success,{fact->terms.student_id}};if(!fact->terms.teacher_id.empty()&&fact->terms.teacher_id!=fact->terms.student_id)result.participant_actor_ids.push_back(fact->terms.teacher_id);return result;}
        if(const auto* fact=std::get_if<RestFact>(value)){if(fact->phase!="completed"||header->kind!="long_rest")return {};return DynamicActionResult{id,"long_rest",id,fact->stop_reason=="duration"?StableId{}:fact->stop_reason,fact->completed_tick,fact->stop_reason=="duration",{fact->actor_id}};}
    }
    for(const auto& fact:world_.military.training_facts)if(fact.id==id&&fact.phase=="completed"&&header->kind=="military_training")return DynamicActionResult{id,"military_train",id,fact.code,fact.completed_tick,fact.success,fact.terms.participants};
    if(auto life=life_result(id);life&&life->phase!=LifeActivityPhase::active)return DynamicActionResult{id,life->kind==LifeActivityKind::trade?"trade":"service",life->fact_id,life->code,life->completed_tick,life->success,life->participant_actor_ids};
    return {};
}
// 只读取本候选真实生活记录，旧profile没有这些完成来源。
std::optional<LifeActivityView> BundleServices::life_result(const StableId& id)const{return world_.regional_life_enabled()?growth_life_result(world_.growth,id):std::optional<LifeActivityView>{};}
std::vector<StableId> BundleServices::life_fact_ids(Integer after,Integer through)const{return world_.regional_life_enabled()?growth_life_fact_ids(world_.growth,after,through):std::vector<StableId>{};}
const DeliveryFact* BundleServices::delivery(const StableId& id)const{return world_.dynamic_enabled()?world_.tasks.deliveries.find(id):nullptr;}
std::optional<DynamicFactView> BundleServices::fact(const StableId& id)const{
    if(!world_.dynamic_enabled()||!world_.common.state().facts.find(id))return std::nullopt;
    if(world_.regional_life_enabled())if(auto result=growth_life_dynamic_fact(world_.growth,id))return result;
    if(world_.diplomacy_enabled())if(auto result=diplomacy_dynamic_fact(world_.diplomacy,id))return result;
    if(world_.politics_enabled())if(auto result=politics_dynamic_fact(world_.politics,id))return result;
    if(world_.political_fiscal_enabled())if(auto result=political_fiscal_dynamic_fact(world_.political_fiscal,id))return result;
    if(world_.economic_supply_policies_enabled()&&world_.common.state().facts.find(id)->domain==economic_supply_policy_module){auto source=EconomicSupplyWorldAccess::create(world_);if(!source.ok)return std::nullopt;auto value=economic_supply_policy_dynamic_fact(source.value->context(),id);return value.ok?value.value:std::nullopt;}
    if(auto result=local_economy_dynamic_fact(world_.local_economy,id))return result;
    if(world_.production_disruptions_enabled())if(auto result=production_disruption_dynamic_fact(world_.production_disruptions,id))return result;
    if(world_.fiscal_settlement_enabled())if(auto result=fiscal_settlement_dynamic_fact(world_.fiscal_settlement,*world_.catalog->fiscal_settlement,id))return result;
    if(world_.occupation_fiscal_enabled())if(auto result=occupation_fiscal_dynamic_fact(world_.occupation_fiscal_settlement,*world_.catalog->fiscal_settlement,id))return result;
    if(world_.named_membership_enabled())if(auto result=membership_offer_dynamic_fact(world_.organization,id))return result;
    if(auto result=organization_dynamic_fact(world_.organization,id))return result;
    DynamicFactView result;result.id=id;result.rule_version="local_society_v1";
    if(const auto* delivered=world_.tasks.deliveries.find(id)){
        result.kind="task_delivery";result.tick=delivered->completed_tick;result.cause_fact_ids={delivered->transfer_fact_id};result.subject_id=delivered->task_id;result.outcome="delivered";
        result.participants.push_back({delivered->beneficiary,"beneficiary"});
        if(const auto* task=world_.tasks.tasks.find(delivered->task_id))result.participants.push_back({{SubjectKind::actor,task->assignee},"executor"});
        return result;
    }
    if(const auto* transfer=world_.tasks.transfers.find(id)){
        result.kind="task_transfer";result.tick=transfer->completed_tick;result.subject_id=transfer->task_id;result.outcome=transfer->purpose;
        if(const auto* task=world_.tasks.tasks.find(transfer->task_id))result.participants.push_back({{SubjectKind::actor,task->assignee},"executor"});
        return result;
    }
    if(auto terminal=action_result(id)){
        result.kind=world_.common.state().facts.find(id)->kind;result.tick=terminal->completed_tick;result.subject_id=id;result.outcome=terminal->success?"completed":"failed";
        if(const auto* value=world_.growth.facts.find(id))std::visit([&](const auto& fact){using T=std::decay_t<decltype(fact)>;if constexpr(!std::is_same_v<T,BargainFact>){result.participants.push_back({{SubjectKind::actor,fact.actor_id},"executor"});if constexpr(std::is_same_v<T,TrainingFact>){if(!fact.training.teacher_id.empty())result.participants.push_back({{SubjectKind::actor,fact.training.teacher_id},"teacher"});}else if constexpr(std::is_same_v<T,StudyFact>){if(!fact.terms.teacher_id.empty())result.participants.push_back({{SubjectKind::actor,fact.terms.teacher_id},"teacher"});}}},*value);
        for(const auto& fact:world_.military.training_facts)if(fact.id==id)for(const auto& actor:fact.terms.participants)result.participants.push_back({{SubjectKind::actor,actor},"participant"});
        return result;
    }
    return std::nullopt;
}
std::vector<StableId> BundleServices::fact_ids(Integer after_tick,Integer through_tick)const{
    std::vector<StableId> result;
    for(const auto& row:world_.common.state().facts.rows())if(auto observed=fact(row.first))if(observed->tick>after_tick&&observed->tick<=through_tick)result.push_back(row.first);
    return result;
}
} // 借用对象随WorldBundle复制重新绑定；领域不得跨命令保留这些指针。
