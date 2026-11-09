#include "population_count_revision.hpp"
#include "production/production_population_change.hpp"
// 人口只消费经过原军籍和行动来源证明的逐成员转移；家户与钱货容器始终复用。
#include "local_economy_military.hpp"
#include "local_economy_internal.hpp"
#include "resident/domains/military/population_military_recovery.hpp"
#include "resident/domains/military/population_military_attrition.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using namespace local_economy_detail;
// The military owner selects the old Calendar gate or the trusted /3 phase clock.
Outcome<Integer> population_phase_tick(const PopulationMilitaryReadContext& context){
    return read_population_military_boundary_tick(context);
}
// 同一来源的状态只映射到作者声明的群体，死亡没有活人口目的地。
StableId cohort_for(const PopulationMilitarySource& source,PopulationSoldierStatus status){
    switch(status){case PopulationSoldierStatus::fit:return source.serving_cohort_id;case PopulationSoldierStatus::wounded:return source.wounded_cohort_id;case PopulationSoldierStatus::captured:return source.captive_cohort_id;case PopulationSoldierStatus::deserted:return source.displaced_cohort_id;case PopulationSoldierStatus::released:return source.reserve_cohort_id;case PopulationSoldierStatus::dead:return {};}
    return {};
}
// 人口状态不由军事文本推断；唯一映射保留已完成训练群体的 trained_reserve。
PopulationStatus population_status(PopulationSoldierStatus status){
    switch(status){case PopulationSoldierStatus::fit:return PopulationStatus::serving;case PopulationSoldierStatus::wounded:return PopulationStatus::wounded;case PopulationSoldierStatus::captured:return PopulationStatus::captive;case PopulationSoldierStatus::deserted:return PopulationStatus::displaced;case PopulationSoldierStatus::released:return PopulationStatus::trained_reserve;case PopulationSoldierStatus::dead:return PopulationStatus::serving;}
    return PopulationStatus::serving;
}
// 军籍来源只接受唯一成员，不允许把真实人物重复算作匿名人口。
const PopulationSoldierOrigin* origin_for(const PopulationEnrollment& enrollment,const StableId& member){
    const PopulationSoldierOrigin* found=nullptr;for(const auto& value:enrollment.members)if(value.member_id==member){if(found)return nullptr;found=&value;}return found;
}
// 原军事事实逐成员分类是唯一伤亡证据，普通事实头不能授权死亡。
Outcome<const MilitaryRosterReceipt*> casualty_roster(const PopulationMilitaryReadContext& context,const PopulationMilitaryFact& event,const StableId& unit){
    auto casualty=read_population_military_casualty(context,event);if(!casualty.ok)return {false,nullptr,casualty.error};
    if(!casualty.value.units)return {false,nullptr,failure("PopulationMilitaryCasualtySource",event.id).error};
    const MilitaryRosterReceipt* found=nullptr;for(const auto& value:*casualty.value.units)if(value.unit_id==unit){if(found)return {false,nullptr,failure("DuplicatePopulationMilitaryCasualtyUnit",unit).error};found=&value;}
    if(!found)return {false,nullptr,failure("PopulationMilitaryCasualtySource",event.id).error};return {true,found,{}};
}
// 验证具体成员在军事收据中恰有一个分类，俘虏必须绑定该收据真实羁押方。
Status verify_casualty_member(const MilitaryRosterReceipt& roster,const PopulationMilitaryMovement& movement){
    if(std::count(roster.member_ids.begin(),roster.member_ids.end(),movement.member_id)!=1)return failure("PopulationMilitaryCasualtyMember",movement.member_id);
    const std::vector<StableId>* selected=nullptr;int found=0;
    for(const auto* ids:{&roster.fit,&roster.wounded,&roster.dead,&roster.captured,&roster.deserted})found+=std::count(ids->begin(),ids->end(),movement.member_id);
    switch(movement.after){case PopulationSoldierStatus::fit:selected=&roster.fit;break;case PopulationSoldierStatus::wounded:selected=&roster.wounded;break;case PopulationSoldierStatus::dead:selected=&roster.dead;break;case PopulationSoldierStatus::captured:selected=&roster.captured;break;case PopulationSoldierStatus::deserted:selected=&roster.deserted;break;case PopulationSoldierStatus::released:break;}
    if(found!=1||!selected||!has(*selected,movement.member_id))return failure("PopulationMilitaryCasualtyClassification",movement.member_id);
    if(movement.after==PopulationSoldierStatus::captured){Integer count=0;for(const auto& pair:roster.captors)if(pair.first==movement.member_id){++count;if(pair.second.empty()||pair.second!=movement.captor_army_id)return failure("PopulationMilitaryCustodyMismatch",movement.member_id);}if(count!=1)return failure("PopulationMilitaryCustodyMismatch",movement.member_id);}
    else if(!movement.captor_army_id.empty())return failure("PopulationMilitaryCustodyMismatch",movement.member_id);
    return common_success();
}
// 同边界宏养豁免必须反查真实军粮处理，即使缺粮也保留原短缺而不补吃第二份。
Status verify_prior_military_supply(const PopulationMilitaryReadContext& context,const PopulationMilitaryFact& event,const StableId& member){
    // A fractional casualty cannot borrow an integer supply receipt as proof of
    // a whole ration period. The original exact desertion/supply handoff must
    // provide that receipt before this branch can be selected.
    if(event.military_source_schema==2)return failure("ExactMilitaryRationHandoffSourceRequired",event.id);
    std::set<StableId> earlier;for(const auto& row:context.common.state().facts.rows()){if(row.first==event.military_receipt_id)break;earlier.insert(row.first);}Integer found=0;
    for(const auto& receipt:context.military.supply_facts)if(receipt.tick==event.tick&&has(receipt.mouth_ids,member)){const auto* header=context.common.state().facts.find(receipt.id);Integer total=0;if(!header||header->domain!="military"||header->kind!="military_supply"||!earlier.count(receipt.id)||std::count(receipt.mouth_ids.begin(),receipt.mouth_ids.end(),member)!=1||receipt.required<0||receipt.consumed<0||receipt.shortage<0||!add(receipt.consumed,receipt.shortage,total)||total!=receipt.required)return failure("PopulationMilitaryRationHandoffSource",member);++found;}
    return found==1?common_success():failure("PopulationMilitaryRationHandoffSource",member);
}
// 已完成动作的冻结条款证明登记或退籍数量；当前任职复核由军事写入口负责。
Status verify_action_source(const PopulationMilitaryReadContext& context,const PopulationMilitaryFact& event){
    const auto* action=context.state.action_facts.find(event.action_id);const auto* header=context.common.state().facts.find(event.action_id);
    const bool enroll=event.kind=="population_enrolled";
    if(!action||!header||header->domain!="population_military"||action->id!=event.action_id||action->kind!=(enroll?"population_enroll":"population_release")||action->phase!="completed"||!action->success||action->receipt_id!=event.id||action->completed_tick!=event.tick||action->terms.source_id!=event.source_id||action->terms.count!=static_cast<Integer>(event.movements.size()))return failure("PopulationMilitaryActionSource",event.id);
    std::set<StableId> members;Integer labor=0;
    for(const auto& movement:event.movements){const auto* enrollment=context.state.enrollments.find(movement.enrollment_id);const auto* origin=enrollment?origin_for(*enrollment,movement.member_id):nullptr;if(!origin||!members.insert(movement.member_id).second||!has(action->terms.member_ids,movement.member_id))return failure("PopulationMilitaryActionMembers",event.id);if(origin->labor_eligible)++labor;}
    if(members.size()!=action->terms.member_ids.size()||labor!=action->terms.labor_count)return failure("PopulationMilitaryActionMembers",event.id);
    return common_success();
}
// 验证整份待消费来源后才写人口；从永久军籍历史取前态，避免凭剩余数量换人。
Status verify_pending_event(const PopulationMilitaryReadContext& context,const PopulationMilitaryFact& event){
    const auto* header=context.common.state().facts.find(event.id);auto latest=population_phase_tick(context);if(!latest.ok)return {false,{},latest.error};
    if(!context.catalog.enabled||!context.economy_catalog.enabled||!header||header->domain!="population_military"||header->kind!=event.kind||event.tick<0||event.tick>latest.value)return failure("PopulationMilitaryFactSource",event.id);
    const bool casualty=event.kind=="population_casualty";const bool recovered=event.kind=="population_recovered";const bool deserted=event.kind=="population_deserted";const bool military_source=casualty||recovered||deserted;const bool enroll=event.kind=="population_enrolled";const bool release=event.kind=="population_released";
    if(!military_source&&!enroll&&!release)return failure("PopulationMilitaryFactKind",event.id);
    auto tail=validate_population_military_fact_source_tail(context,event);if(!tail.ok)return tail;
    if(casualty){auto source=read_population_military_casualty(context,event);if(!source.ok)return {false,{},source.error};}
    // 回队须完整原恢复批次/军粮/额度，不能借普通伤亡字符串逆转 wounded。
    if(recovered){auto source=validate_population_military_recovered_event(context,event);if(!source.ok)return source;}
    // 只消费新军域连续缺粮的原匿名token逃散源，不能把任意人数短缺写成历史转移。
    if(deserted){auto source=validate_population_military_deserted_event(context,event);if(!source.ok)return source;}
    if(!military_source){if(event.movements.empty()||!event.military_receipt_id.empty())return failure("PopulationMilitaryFactSource",event.id);auto action=verify_action_source(context,event);if(!action.ok)return action;}
    std::map<StableId,PopulationSoldierStatus> previous;bool seen=false,casualty_seen=false;
    for(const auto& row:context.common.state().facts.rows()){
        if(row.first==event.id){seen=true;break;}
        if(row.first==event.military_receipt_id)casualty_seen=true;
        if(const auto* earlier=context.state.facts.find(row.first)){if(military_source&&earlier->military_receipt_id==event.military_receipt_id)return failure("PopulationMilitaryCasualtyConsumed",event.military_receipt_id);for(const auto& movement:earlier->movements)previous[movement.member_id]=movement.after;}
    }
    if(!seen||(military_source&&(!casualty_seen||event.military_receipt_id.empty())))return failure("PopulationMilitaryFactOrder",event.id);
    std::set<StableId> members;
    for(const auto& movement:event.movements){
        const auto* enrollment=context.state.enrollments.find(movement.enrollment_id);const auto* source=enrollment?context.catalog.sources.find(enrollment->source_id):nullptr;const auto* origin=enrollment?origin_for(*enrollment,movement.member_id):nullptr;
        if(!source||!origin||context.common.actor(movement.member_id)||!members.insert(movement.member_id).second||enrollment->id!=movement.enrollment_id||enrollment->unit_id!=source->unit_id||origin->source_cohort_id!=source->reserve_cohort_id||origin->household_group_id!=source->household_group_id||movement.before==PopulationSoldierStatus::dead||movement.after==movement.before||movement.from_cohort_id!=cohort_for(*source,movement.before)||movement.to_cohort_id!=cohort_for(*source,movement.after))return failure("PopulationMilitaryMovementSource",movement.member_id);
        const auto prior=previous.find(movement.member_id);if(enroll){if(event.id!=enrollment->id||enrollment->action_id!=event.action_id||enrollment->completed_tick!=event.tick||movement.before!=PopulationSoldierStatus::released||movement.after!=PopulationSoldierStatus::fit||prior!=previous.end()||event.source_id!=source->id)return failure("PopulationMilitaryEnrollmentSource",movement.member_id);}
        else if(prior==previous.end()||prior->second!=movement.before)return failure("PopulationMilitaryMovementHistory",movement.member_id);
        if(release&&(movement.before!=PopulationSoldierStatus::fit||movement.after!=PopulationSoldierStatus::released||event.source_id!=source->id))return failure("PopulationMilitaryReleaseSource",movement.member_id);
        const auto* from=context.economy.cohorts.find(movement.from_cohort_id);const auto* to=context.economy.cohorts.find(movement.to_cohort_id);
        if(!from||from->household_group_id!=source->household_group_id||from->status!=population_status(movement.before)||(movement.after!=PopulationSoldierStatus::dead&&(!to||to->household_group_id!=source->household_group_id||to->status!=population_status(movement.after))))return failure("PopulationMilitaryCohortSource",movement.member_id);
        if(deserted){if(movement.before!=PopulationSoldierStatus::fit||movement.after!=PopulationSoldierStatus::deserted||!movement.captor_army_id.empty())return failure("PopulationMilitaryAttritionMovement",movement.member_id);auto supplied=verify_prior_military_supply(context,event,movement.member_id);if(!supplied.ok)return supplied;}
        if(casualty){if(movement.before==PopulationSoldierStatus::released||movement.before==PopulationSoldierStatus::captured||movement.before==PopulationSoldierStatus::deserted||(movement.before==PopulationSoldierStatus::wounded&&movement.after==PopulationSoldierStatus::fit))return failure("PopulationMilitaryCasualtyReversal",movement.member_id);auto roster=casualty_roster(context,event,source->unit_id);if(!roster.ok)return {false,{},roster.error};auto actual=verify_casualty_member(*roster.value,movement);if(!actual.ok)return actual;if(movement.after==PopulationSoldierStatus::deserted){auto supplied=verify_prior_military_supply(context,event,movement.member_id);if(!supplied.ok)return supplied;}}
    }
    return common_success();
}
// 每个成员使用一个原生转移或死亡收据；校验所有引用和劳力身份，拒绝重填计数。
Status verify_receipts(const CommonStore& common,const LocalEconomyState& economy,const PopulationMilitaryFact& event,const std::map<StableId,const PopulationSoldierOrigin*>& origins,std::set<StableId>& used){
    if(event.population_receipt_ids.size()!=event.movements.size())return failure("PopulationMilitaryReceiptCount",event.id);
    for(std::size_t i=0;i<event.movements.size();++i){const auto& movement=event.movements[i];const auto& id=event.population_receipt_ids[i];const auto found=origins.find(movement.member_id);const auto* header=common.state().facts.find(id);if(found==origins.end()||!header||header->domain!="local_economy"||!used.insert(id).second)return failure("PopulationMilitaryReceiptSource",id);Integer labor=found->second->labor_eligible?1:0;
        if(movement.after==PopulationSoldierStatus::dead){const auto* loss=economy.population_losses.find(id);if(!loss||header->kind!="population_loss"||loss->id!=id||loss->cohort_id!=movement.from_cohort_id||loss->source_fact_id!=event.id||loss->casualty_fact_id!=event.military_receipt_id||loss->effective_tick!=event.tick||loss->count!=1||loss->labor_eligible_count!=labor||loss->member_ids!=std::vector<StableId>{movement.member_id})return failure("PopulationMilitaryLossMismatch",id);}
        else{const auto* transfer=economy.population_transfers.find(id);if(!transfer||header->kind!="population_transfer"||transfer->id!=id||transfer->from_cohort_id!=movement.from_cohort_id||transfer->to_cohort_id!=movement.to_cohort_id||transfer->source_fact_id!=event.id||transfer->reason!=event.kind||transfer->effective_tick!=event.tick||transfer->count!=1||transfer->labor_eligible_count!=labor)return failure("PopulationMilitaryTransferMismatch",id);}
    }
    return common_success();
}
}
Outcome<OriginalMilitaryPopulationDestination> read_original_military_population_destination(const PopulationMilitaryReadContext& context,const SourceRef& source){
    const auto* transfer=context.economy.population_transfers.find(source.id);
    if(source.domain!="local_economy"||source.kind!="population_transfer"||!transfer||transfer->id!=source.id)return {false,{},common_failure("PopulationMilitaryOriginalTransferRequired",source.id).error};
    const auto* event=context.state.facts.find(transfer->source_fact_id);
    if(!event)return {false,{},common_failure("PopulationMilitaryOriginalEventRequired",source.id).error};
    const SourceRef original{"population_military",event->kind,event->id};
    auto schema=read_population_military_event_source_schema(context,original);if(!schema.ok)return {false,{},schema.error};
    auto at=read_exact_population_military_event_moment(context,original,schema.value);if(!at.ok)return {false,{},at.error};
    auto continuity=population_military_continuity(context);if(!continuity.ok)return {false,{},continuity.error};
    auto verified=validate_population_continuity(context.common,context.economy,context.economy_catalog,continuity.value);if(!verified.ok)return {false,{},verified.error};
    const PopulationMilitaryMovement* movement=nullptr;
    for(std::size_t n=0;n<event->population_receipt_ids.size();++n)if(event->population_receipt_ids[n]==source.id){if(movement||n>=event->movements.size())return {false,{},common_failure("PopulationMilitaryTransferMovementMismatch",source.id).error};movement=&event->movements[n];}
    if(!movement||movement->after==PopulationSoldierStatus::dead||movement->to_cohort_id!=transfer->to_cohort_id)return {false,{},common_failure("PopulationMilitaryTransferMovementMismatch",source.id).error};
    return {true,{source,{original,schema.value},at.value,movement->to_cohort_id,population_status(movement->after)},{}};
}
// 此入口不建立跨域提交点；全部来源先验证，本域副本仅在整批成功后替换。
Outcome<LocalEconomyEffects> apply_population_military_fact(PopulationMilitaryWriteContext& context,const StableId& fact_id){
    const auto* event=context.state.facts.find(fact_id);if(!event||event->id!=fact_id)return {false,{},common_failure("PopulationMilitaryFactSource",fact_id).error};
    auto valid=verify_pending_event(context.read(),*event);if(!valid.ok)return {false,{},valid.error};
    std::map<StableId,const PopulationSoldierOrigin*> origins;for(const auto& row:context.state.enrollments.rows())for(const auto& origin:row.second.members)if(!origins.emplace(origin.member_id,&origin).second)return {false,{},common_failure("DuplicatePopulationMilitaryOrigin",origin.member_id).error};
    if(!event->population_receipt_ids.empty()){std::set<StableId> used;auto same=verify_receipts(context.common,context.economy,*event,origins,used);if(!same.ok)return {false,{},same.error};if(context.population_transport){auto revisions=validate_population_count_revisions(context.common,context.economy,*context.population_transport);if(!revisions.ok)return {false,{},revisions.error};}return {true,{event->population_receipt_ids,event->population_receipt_ids},{}};}
    for(const auto& row:context.economy.population_transfers.rows())if(row.second.source_fact_id==fact_id)return {false,{},common_failure("PopulationMilitaryReceiptUnlinked",fact_id).error};
    for(const auto& row:context.economy.population_losses.rows())if(row.second.source_fact_id==fact_id)return {false,{},common_failure("PopulationMilitaryReceiptUnlinked",fact_id).error};
    std::optional<PreparedProductionPopulationChange> production_change;
    if(context.production_inputs&&!event->movements.empty()){
        if(!context.population_transport||context.population_transport->version!=3||!event->occurred_at||event->military_source_schema!=2)return {false,{},common_failure("PopulationProductionPreciseSourceRequired",fact_id).error};
        if(&context.production_inputs->common!=&context.common||&context.production_inputs->economy!=&context.economy||&context.production_inputs->catalog!=&context.economy_catalog||&context.production_inputs->original.population.population!=context.population_transport)return {false,{},common_failure("PopulationProductionCandidateMismatch",fact_id).error};
        std::vector<StableId> cohorts;for(const auto&movement:event->movements)for(const auto&id:{movement.from_cohort_id,movement.to_cohort_id})if(!id.empty()&&std::find(cohorts.begin(),cohorts.end(),id)==cohorts.end())cohorts.push_back(id);
        auto prepared=prepare_production_population_change(*context.production_inputs,cohorts);if(!prepared.ok)return {false,{},prepared.error};production_change.emplace(std::move(prepared.value));
    }
    auto candidate=context.economy;LocalEconomyEffects effects;std::vector<PopulationTransferLaborCapture> captures;
    for(const auto& movement:event->movements){const auto* origin=origins.at(movement.member_id);Integer labor=origin->labor_eligible?1:0;
        if(movement.after==PopulationSoldierStatus::dead){auto* cohort=candidate.cohorts.find(movement.from_cohort_id);if(!cohort||cohort->headcount<1||cohort->labor_capacity<labor||cohort->labor_capacity-labor>cohort->headcount-1)return {false,{},common_failure("PopulationLossConservation",movement.member_id).error};auto id=context.common.next_id("population_loss");if(!id.ok)return {false,{},id.error};auto appended=context.common.append_fact({id.value,"population_loss","local_economy"});if(!appended.ok)return {false,{},appended.error};cohort->headcount-=1;cohort->labor_capacity-=labor;candidate.population_losses.insert(id.value,{id.value,movement.from_cohort_id,event->id,event->military_receipt_id,1,labor,event->tick,{movement.member_id}});effects.receipt_ids.push_back(id.value);effects.fact_ids.push_back(id.value);}
        else{PopulationTransferRequest request;request.from_cohort_id=movement.from_cohort_id;request.to_cohort_id=movement.to_cohort_id;request.reason=event->kind;request.source_fact_id=event->id;request.count=1;request.labor_eligible_count=labor;request.effective_tick=event->tick;request.destination_status=population_status(movement.after);request.assignment_id=candidate.cohorts.find(movement.to_cohort_id)->assignment_id;if(event->kind=="population_enrolled")request.release_allocation_ids=context.state.enrollments.find(movement.enrollment_id)->terms.release_allocation_ids;PopulationTransferLaborCapture capture;auto moved=transfer_population_counts(context.common,candidate,request,context.population_transport&&context.population_transport->version==3?&capture:nullptr);if(!moved.ok)return {false,{},moved.error};if(!capture.transfer_id().empty())captures.push_back(std::move(capture));effects.receipt_ids.push_back(moved.value);effects.fact_ids.push_back(moved.value);}
    }
    context.economy=std::move(candidate);context.state.facts.find(fact_id)->population_receipt_ids=effects.receipt_ids;if(context.population_transport){auto revisions=record_population_count_receipt_revisions_in_candidate(context.common,context.economy,*context.population_transport,effects.receipt_ids,&captures);if(!revisions.ok)return {false,{},revisions.error};}
    if(production_change){
        std::vector<MomentOwnerSource> changes;for(const auto&id:effects.receipt_ids){const auto*head=context.common.state().facts.find(id);if(!head||head->domain!="local_economy"||(head->kind!="population_transfer"&&head->kind!="population_loss"))return {false,{},common_failure("PopulationProductionActualReceiptRequired",id).error};changes.push_back({{head->domain,head->kind,id},1});}
        auto applied=apply_production_population_change_in_candidate(*context.production_inputs,std::move(*production_change),changes);if(!applied.ok)return {false,{},applied.error};for(const auto&source:applied.value)effects.fact_ids.push_back(source.id);
    }
    return {true,std::move(effects),{}};
}
// 冷验证必须一一对应适配器已证事件，任何多余或伪造的死亡/军籍转移都会拒绝。
Status validate_population_continuity(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog&,const PopulationContinuity& continuity){
    std::map<StableId,const PopulationSoldierOrigin*> origins;for(const auto& origin:continuity.origins)if(!origins.emplace(origin.member_id,&origin).second)return common_failure("DuplicatePopulationMilitaryOrigin",origin.member_id);
    std::set<StableId> used,event_ids;for(const auto& event:continuity.events){if(!event_ids.insert(event.id).second)return common_failure("DuplicatePopulationMilitaryEvent",event.id);auto valid=verify_receipts(common,economy,event,origins,used);if(!valid.ok)return valid;}
    for(const auto& row:economy.population_transfers.rows())if(event_ids.count(row.second.source_fact_id)&&!used.count(row.first))return common_failure("PopulationMilitaryReceiptUnlinked",row.first);
    std::set<StableId> lost;for(const auto& row:economy.population_losses.rows()){if(!used.count(row.first))return common_failure("PopulationMilitaryLossUnproven",row.first);for(const auto& member:row.second.member_ids)if(!lost.insert(member).second)return common_failure("DuplicatePopulationMilitaryDeath",member);}
    if(lost.size()!=continuity.losses.size())return common_failure("PopulationMilitaryDeathCount");
    for(const auto& loss:continuity.losses){if(!lost.count(loss.member_id))return common_failure("PopulationMilitaryDeathUnlinked",loss.member_id);const PopulationLoss* receipt=nullptr;for(const auto& row:economy.population_losses.rows())if(has(row.second.member_ids,loss.member_id))receipt=&row.second;if(!receipt||receipt->source_fact_id!=loss.fact_id||receipt->casualty_fact_id!=loss.military_receipt_id||receipt->cohort_id!=loss.cohort_id||receipt->effective_tick!=loss.tick||receipt->labor_eligible_count!=(loss.labor_eligible?1:0))return common_failure("PopulationMilitaryDeathMismatch",loss.member_id);}
    return common_success();
}
// 军粮责任绑定来源而非静态付款库存；逐口覆盖由原军事名册、野战军和俘虏表证明。
Status validate_local_military_bindings(const PopulationMilitaryReadContext& context,const PopulationContinuity& continuity){
    auto effective=population_phase_tick(context);if(!effective.ok)return {false,{},effective.error};const Integer tick=effective.value;
    const auto& economy=context.economy;std::set<StableId> protected_cohorts,event_ids,mouths;std::map<StableId,std::pair<Integer,Integer>> covered;
    for(const auto& event:continuity.events){event_ids.insert(event.id);for(const auto& movement:event.movements)if(event.kind=="population_casualty"&&movement.after==PopulationSoldierStatus::deserted){auto supplied=verify_prior_military_supply(context,event,movement.member_id);if(!supplied.ok)return supplied;}}
    for(const auto& row:context.catalog.sources.rows()){const auto& source=row.second;const auto* reserve=economy.cohorts.find(source.reserve_cohort_id);if(!reserve||(reserve->status!=PopulationStatus::trained_reserve&&reserve->status!=PopulationStatus::training)||reserve->household_group_id!=source.household_group_id)return common_failure("PopulationMilitaryReserveIdentity",row.first);
        for(const auto& pair:std::vector<std::pair<StableId,PopulationStatus>>{{source.serving_cohort_id,PopulationStatus::serving},{source.wounded_cohort_id,PopulationStatus::wounded},{source.captive_cohort_id,PopulationStatus::captive},{source.displaced_cohort_id,PopulationStatus::displaced}}){const auto* cohort=economy.cohorts.find(pair.first);if(!cohort||!protected_cohorts.insert(pair.first).second||cohort->status!=pair.second||cohort->household_group_id!=reserve->household_group_id||cohort->private_wallet_id!=reserve->private_wallet_id||cohort->private_inventory_id!=reserve->private_inventory_id||cohort->site_id!=source.home_site_id)return common_failure("PopulationMilitaryCohortIdentity",pair.first);const ProvisionBinding* provision=nullptr;for(const auto& binding:economy.provision_bindings.rows())if(binding.second.subject.kind==SubjectKind::cohort&&binding.second.subject.id==pair.first&&binding.second.from_tick<=tick&&tick<=binding.second.through_tick){if(provision)return common_failure("DuplicateProvisionBinding",pair.first);provision=&binding.second;}if(!provision)return common_failure("PopulationProvisionRequired",pair.first);if(pair.second==PopulationStatus::displaced){if(provision->policy_owner!=ProvisionOwner::macro)return common_failure("DisplacedMilitaryProvision",pair.first);}else if(provision->policy_owner!=ProvisionOwner::military||provision->military_source_id!=source.id||!provision->payer_inventory.empty()||!provision->obligation_id.empty())return common_failure("InvalidMilitaryProvisionBinding",pair.first);}
    }
    for(const auto& row:economy.cohorts.rows())if(row.second.status==PopulationStatus::serving&&!protected_cohorts.count(row.first))return common_failure("PopulationMilitaryServingSourceMissing",row.first);
    for(const auto& row:economy.population_transfers.rows())if((protected_cohorts.count(row.second.from_cohort_id)||protected_cohorts.count(row.second.to_cohort_id))&&!event_ids.count(row.second.source_fact_id))return common_failure("PopulationMilitaryTransferUnproven",row.first);
    for(const auto& provision:continuity.provisions){const auto* source=context.catalog.sources.find(provision.source_id);const auto* inventory=context.common.inventory(provision.inventory_id);if(!source||!inventory||!mouths.insert(provision.member_id).second||provision.cohort_id!=cohort_for(*source,provision.status)||(provision.status!=PopulationSoldierStatus::fit&&provision.status!=PopulationSoldierStatus::wounded&&provision.status!=PopulationSoldierStatus::captured)||(provision.army_id.empty()==provision.garrison_id.empty()))return common_failure("PopulationMilitaryProvisionCoverage",provision.member_id);auto& count=covered[provision.cohort_id];if(!add(count.first,1,count.first)||!add(count.second,provision.labor_eligible?1:0,count.second))return common_failure("EconomyQuantityOverflow",provision.cohort_id);}
    for(const auto& row:economy.provision_bindings.rows())if(row.second.policy_owner==ProvisionOwner::military){const auto& binding=row.second;const auto* source=context.catalog.sources.find(binding.military_source_id);const auto* cohort=economy.cohorts.find(binding.subject.id);if(!source||binding.subject.kind!=SubjectKind::cohort||!cohort||(cohort->id!=source->serving_cohort_id&&cohort->id!=source->wounded_cohort_id&&cohort->id!=source->captive_cohort_id)||!binding.payer_inventory.empty()||!binding.obligation_id.empty())return common_failure("InvalidMilitaryProvisionBinding",row.first);if(binding.from_tick<=tick&&tick<=binding.through_tick&&covered[cohort->id]!=std::make_pair(cohort->headcount,cohort->labor_capacity))return common_failure("PopulationMilitaryProvisionCount",cohort->id);for(const auto& obligation:economy.obligations.rows())if(obligation.second.active&&obligation.second.kind==ObligationKind::ration&&same_subject(obligation.second.beneficiary,binding.subject))return common_failure("DuplicateMilitaryRationObligation",obligation.first);}
    return common_success();
}
// 未来接收义务必须尚未活化且精确匹配原营地口粮；旧义务只能由真实整编加占领双源解释。
bool population_military_ration_owner_proven(const PopulationMilitaryReadContext& c,const RecurringObligation& obligation){
    if(!c.military_catalog.siege_integration.enabled||obligation.kind!=ObligationKind::ration||obligation.active)return false;
    const auto* inventory=c.common.inventory(obligation.source_wallet_or_inventory);if(!inventory||inventory->owner.kind!=Owner::Kind::organization)return false;
    for(const auto& row:c.catalog.integration_policies.rows()){const auto& policy=row.second;const auto* source=c.catalog.sources.find(policy.source_id);if(!source||!has(policy.ration_obligation_ids,obligation.id)||policy.receiving_organization_id!=obligation.principal.id||inventory->owner.id!=source->organization_id)continue;const StrongholdDefinition* hold=nullptr;for(const auto& stronghold:c.military_catalog.strongholds)if(stronghold.id==policy.target_stronghold_id&&stronghold.garrison_id==source->garrison_id&&has(stronghold.public_inventory_ids,inventory->id))hold=&stronghold;if(!hold)continue;const auto* seed=c.economy_catalog.obligations.find(obligation.id);if(!seed||seed->active||!obligation.last_receipt_id.empty())continue;bool used=false;for(const auto& receipt:c.economy.rations.rows())if(receipt.second.obligation_id==obligation.id)used=true;if(used)continue;
        Integer matches=0;for(const auto& previous:c.economy.obligations.rows()){const auto& old=previous.second;if(old.id!=obligation.id&&old.active&&old.kind==ObligationKind::ration&&old.principal.kind==SubjectKind::organization&&old.principal.id==source->organization_id&&old.source_wallet_or_inventory==inventory->id&&same_subject(old.beneficiary,obligation.beneficiary)&&old.rate_profile==obligation.rate_profile&&old.period_ticks==obligation.period_ticks&&old.item_id==obligation.item_id)++matches;}if(matches==1)return true;
    }
    for(const auto& row:c.state.integration_facts.rows()){const auto& continued=row.second;if(!has(continued.old_ration_obligation_ids,obligation.id))continue;const MilitaryIntegrationFact* integrated=nullptr;for(const auto& fact:c.military.integration_facts)if(fact.id==continued.military_receipt_id)integrated=&fact;const auto* header=integrated?c.common.state().facts.find(integrated->id):nullptr;const auto* population_header=c.common.state().facts.find(continued.id);if(!integrated||!header||header->domain!="military"||header->kind!="military_integration"||!population_header||population_header->domain!="population_military"||population_header->kind!="population_military_integration"||!has(integrated->population_receipt_ids,continued.id))continue;
        for(const auto& occupied:c.military.occupation_facts){const auto* occupation_header=c.common.state().facts.find(occupied.id);if(occupied.siege_id!=integrated->siege_id||occupied.stronghold_id!=integrated->stronghold_id||occupied.army_id!=integrated->recipient_army_id||occupied.tick!=integrated->tick||occupied.phase!="completed"||!occupied.assets_applied.value_or(false)||!occupied.asset_manifest||!occupation_header||occupation_header->domain!="military"||occupation_header->kind!="military_occupation"||occupied.from_organization_id!=obligation.principal.id||occupied.to_organization_id!=inventory->owner.id||!has(occupied.public_inventory_ids,inventory->id))continue;for(const auto& manifest:occupied.asset_manifest->inventories)if(manifest.first==inventory->id&&manifest.second.from_owner.kind==Owner::Kind::organization&&manifest.second.from_owner.id==obligation.principal.id&&manifest.second.to_owner.kind==Owner::Kind::organization&&manifest.second.to_owner.id==inventory->owner.id)return true;}
    }return false;
}

} // 家户资产与工资欠额不受名册身份切换改写。
