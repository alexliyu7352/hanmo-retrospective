#include "military_supply_lookup.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "military_formation_reservations.hpp"
#include "military_action_resources.hpp"
#include "resident/storage/common_store.hpp"
// 首profile军政真实规则：正式预约、半日完工、实物转移与永久事实均在唯一候选内执行。
#include "resident/storage/money_transfer_recipe.hpp"
#include "military_supply_exact.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "military_exact_validation.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "battle_request_world.hpp"
#include "military_officer_rules.hpp"
#include "military_field_command_rules.hpp"
#include "military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "military_authority_time.hpp"
#include "military_action_original_rules.hpp"
#include "military_action_conditions.hpp"
#include "military_actor_condition_write.hpp"
#include "military_equal.hpp"
#include "battle_world.hpp"
#include "military_lifecycle.hpp"
#include "siege_world.hpp"
#include "training_rules.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <set>
#include <string_view>
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
namespace {
template<class T> T* find_id(std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T> const T* find_id(const std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T> bool has(const std::vector<T>& a,const T& b){return std::find(a.begin(),a.end(),b)!=a.end();}
// 保留原规则失败码及可选对象路径；未给路径时维持既有空路径。
Status fail(const StableId &code, const StableId &path = {}) {
  return common_failure(code, path);
}
template<class T> Outcome<T> error(const Status& s){return {false,{},s.error};}
Owner owner(const StableId& id){return {Owner::Kind::organization,id,{}};}
// Keep the original absent-container projection convention. Actual quantity
// selection and checked summation are owned solely by Common.
Outcome<Integer> count(const CommonRead& c,const StableId& inventory,const StableId& item,const Owner& own,const StableId& reservation={}){if(!c.inventory(inventory))return {true,0,{}};return count_inventory_items(c,inventory,item,{own},ReservationRef{reservation});}
bool at(const CommonRead& c,const StableId& actor,const StableId& place){auto p=c.actor(actor);auto l=p?std::get_if<SettlementLocation>(&p->location):nullptr;return l&&l->place_id==place;}
bool ready(const CommonRead& c,const MilitaryAccess& a,const StableId& actor,const StableId& own){auto p=c.actor(actor);return p&&original_military_actor_life_ready(p->alive,p->available,p->hp)&&(!a.actor_injury||original_military_actor_injury_ready(a.actor_injury(actor)))&&(p->action_id.empty()||p->action_id==own)&&!(a.interaction_access&&a.interaction_access->battle_occupied&&a.interaction_access->battle_occupied(actor));}
bool unique(const std::vector<StableId>& ids){std::set<StableId> s;for(auto& id:ids)if(id.empty()||!s.insert(id).second)return false;return true;}
const StableId* organization_source(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& camp){for(auto& hold:cat.strongholds)if(hold.garrison_id==camp){auto h=find_id(s.strongholds,hold.id);if(h)return &h->owner_organization_id;}auto d=find_id(cat.garrisons,camp);return d?&d->organization_id:nullptr;}
const StableId* ownership_source(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& camp){for(auto& hold:cat.strongholds)if(hold.garrison_id==camp){auto h=find_id(s.strongholds,hold.id);if(h)return &h->occupation_id;}return nullptr;}
StableId organization(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& camp){const auto* source=organization_source(s,cat,camp);return source?*source:StableId{};}
StableId ownership_id(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& camp){const auto* source=ownership_source(s,cat,camp);return source?*source:StableId{};}
Status office_ready(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& actor,const StableId& office){auto o=find_id(s.offices,office);auto d=find_id(cat.offices,office);auto p=c.actor(actor);if(!o||!d||!p||!p->alive||!p->available||!o->enabled||o->holder_actor_id!=actor)return fail("OfficeAuthorityRequired");if(organization(s,cat,d->garrison_id)!=d->organization_id)return fail("OfficeAuthorityLost");return common_success();}
Outcome<InteractionContext> contact(const CommonRead& c,const MilitaryAccess& a,const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own={}){if(!a.interaction||!a.interaction_catalog||!a.interaction_access)return error<InteractionContext>(military_capability_failure("preview",service,actor));return validate_interaction(c,*a.interaction,*a.interaction_catalog,*a.interaction_access,actor,context,service,own);}
const MilitaryUnitDefinition* unit_definition(const MilitaryCatalog& cat,const MilitaryUnitRecord& u){auto camp=find_id(cat.garrisons,u.source_garrison_id);if(!camp)return nullptr;return find_id(camp->units,u.id);}
// 活军名册只在此归并一次；视图报价和真实发粮共同复用原真人/伤兵/俘虏去重顺序。
template<class Insert> Status collect_army_mouths(const CommonRead& c,const MilitaryState& s,const ArmyRecord& army,Insert&& insert){for(const auto& actor:army.member_actor_ids){auto person=c.actor(actor);if(person&&person->alive)insert(actor);}for(const auto& id:army.unit_ids){auto unit=find_id(s.military_units,id);if(!unit)return fail("ArmyRosterMismatch");for(const auto& actor:unit->fit)insert(actor);for(const auto& actor:unit->wounded)insert(actor);}for(const auto& actor:army.prisoner_ids)insert(actor);return common_success();}
Outcome<std::set<StableId>> army_mouths(const CommonRead& c,const MilitaryState& s,const ArmyRecord& army){std::set<StableId> mouths;auto status=collect_army_mouths(c,s,army,[&](const StableId& id){mouths.insert(id);});if(!status.ok)return {false,{},status.error};return {true,std::move(mouths),{}};}
// 原定额和整数向下取整只作当前报价，不预测未来损耗或收粮。
MilitaryProvisions provisions(const GarrisonDefinition& camp,Integer mouths,Integer available){MilitaryProvisions quote;quote.food_item_id=camp.food_item_id;quote.mouth_count=mouths;quote.required=mouths*camp.ration_per_person;quote.available=available;quote.full_ticks=quote.required>0?available/quote.required:0;quote.shortage=std::max<Integer>(0,quote.required-available);return quote;}
struct ExactSupplyAdoption {
 const CommonStore& common;MilitaryExactState& exact;const ExactTrainingPeriodView& period;
 const ExactMilitaryStateValidationContext& validation;MilitarySupplyExactConditionWrite& conditions;
};
// 军令/军事供给事实只向A登记头一次；业务payload留本域并保全原字段序。
Status record_supply(CommonWrite& c,MilitaryState& s,const MilitaryAccess& access,MilitarySupplyFact fact,const ExactSupplyAdoption* exact=nullptr){
 auto id=c.next_id("military");if(!id.ok)return {false,{},id.error};fact.id=id.value;auto status=c.append_fact({fact.id,"military_supply","military"});if(!status.ok)return status;invalidate_military_supply_lookup(s);s.supply_facts.push_back(std::move(fact));
 if(exact){const auto& actual=s.supply_facts.back();MilitarySupplyEffectSource2 source;source.original_supply_fact_ref={"military","military_supply",actual.id};source.period_ref=exact->period.period_ref;source.opening_source_ref=exact->period.original_opening.source_ref;source.occurred_at=exact->period.opening_at;source.shortage_value=actual.shortage;source.original_rule_ref=original_military_supply_rule_ref;
  if(actual.shortage>0)for(const auto& actor:actual.mouth_ids)if(c.actor(actor))source.selected_named_actor_ids.push_back(actor);
  if(!exact->exact.supply_effect_sources.insert(actual.id,std::move(source)))return common_failure("DuplicateMilitarySupplyExactSource",actual.id);
  auto now=exact->validation.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto verified=read_exact_military_supply_source(exact->common,s,exact->exact,exact->validation.periods,{"military","military_supply",actual.id},2,now.value,exact->validation.budget);if(!verified.ok)return {false,{},verified.error};
  return exact->conditions.apply_actual_supply_conditions_in_candidate({"military","military_supply",actual.id});
 }
 return finish_military_actor_condition_sources(c,s,access,{id.value});
}
template<class Units,class Mouths> Status feed(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const GarrisonDefinition& definition,const StableId& organization_id,const StableId& ownership,const StableId& inventory,const Units& units,const Mouths& mouths,Integer tick,const StableId& army,const StableId& camp,const ExactSupplyAdoption* exact=nullptr,const std::vector<StableId>* cause=nullptr){
    auto available=count(c,inventory,definition.food_item_id,owner(organization_id));if(!available.ok)return {false,{},available.error};auto quote=provisions(definition,static_cast<Integer>(mouths.size()),available.value);Integer required=quote.required;Integer quantity=std::min(required,quote.available);Integer shortage=quote.shortage;
    if(quantity>0){ResourceRecipe recipe;recipe.operations.push_back(ConsumeLots{inventory,definition.food_item_id,quantity,{owner(organization_id)},""});auto r=c.apply_resources(recipe);if(!r.ok)return r;}
    for(auto& id:units){auto u=find_id(s.military_units,id);if(!u)return fail("ArmyRosterMismatch");if(shortage>0&&!u->fit.empty())u->morale=cat.rout_retreat_policy&&u->morale==0?0:std::max<Integer>(1,u->morale-8);}
    if(shortage>0&&!exact)for(auto& id:mouths)if(auto p=c.actor(id)){ActorMutation mutation;mutation.actor_id=id;mutation.fatigue=std::min<Integer>(100,p->fatigue+4);auto r=c.apply_actor(mutation);if(!r.ok)return r;}
    MilitarySupplyFact fact;fact.organization_id=organization_id;fact.ownership_id=ownership;fact.army_id=army;fact.garrison_id=camp;fact.tick=tick;fact.item_id=definition.food_item_id;fact.mouth_ids.assign(mouths.begin(),mouths.end());fact.required=required;fact.consumed=quantity;fact.shortage=shortage;if(cause)fact.cause_fact_ids=*cause;else if(exact)fact.cause_fact_ids={exact->period.original_opening.source_ref.id};return record_supply(c,s,access,std::move(fact),exact);
}
}
CheckpointMemoryCharge OriginalOfficeActionCompletion::reserve(const OriginalOfficeActionCompletionBuildView& view){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge OriginalOfficeActionCompletion::reserve(const OriginalOfficeActionCompletion& value){return CheckpointMemoryCharge(0);}
OriginalOfficeActionCompletion::OriginalOfficeActionCompletion()
 :OriginalOfficeActionCompletion(OriginalOfficeActionCompletionBuildView{true,{},{}}){}
OriginalOfficeActionCompletion::OriginalOfficeActionCompletion(const OriginalOfficeActionCompletionBuildView& value)
 :copied_fields(reserve(value)),effect{}{
 if(!value.office_id.empty())effect.office_id.assign(value.office_id.data(),value.office_id.size());
 effect.success=value.success;
 if(!value.code.empty())effect.code.assign(value.code.data(),value.code.size());
}
OriginalOfficeActionCompletion::OriginalOfficeActionCompletion(const OriginalOfficeActionCompletion& value)
 :copied_fields(reserve(value)),effect(value.effect){}

// Both legacy and exact-time completion use this one original Office writer.
// Time/elapsed adoption is the caller's owner; this leaf never invents a due.
Outcome<OriginalOfficeActionCompletion> complete_original_office_action_in_candidate(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action,const OfficeTerms& terms,const std::optional<InteractionContext>& context,const MilitaryActionCompletionConditionGuard* condition_guard){
    if(!a.actor_injury)return error<OriginalOfficeActionCompletion>(military_capability_failure("actions_step","actor_injury",action.actor_id));
    if(action.kind!="office_accept"||terms.actor_id!=action.actor_id)return error<OriginalOfficeActionCompletion>(fail("InvalidMilitaryAction"));
    bool success=true;for(const auto& person:action.participants)if(!ready(c,a,person,action.id)||c.actor(person)->action_id!=action.id)success=false;
    auto office=find_id(s.offices,terms.office_id);auto definition=find_id(cat.offices,terms.office_id);if(!office||!definition)return error<OriginalOfficeActionCompletion>(fail("UnknownOffice"));
    if(!at(c,action.actor_id,terms.place_id)||office->enabled||organization(s,cat,definition->garrison_id)!=terms.organization_id||!context)success=false;
    if(context){auto interaction=contact(c,a,action.actor_id,*context,"office_accept",action.id);if(!interaction.ok){if(interaction.error.kind!=ErrorKind::rule)return {false,{},interaction.error};success=false;}}
 if(a.military_action_conditions_schema==2&&!condition_guard)return error<OriginalOfficeActionCompletion>(fail("MilitaryActionCompletionConditionGuardRequired"));
 if(condition_guard){auto checked=condition_guard->validate_before_effects(c,a,action);if(!checked.ok)return {false,{},checked.error};}
 if(success)*office={terms.office_id,action.actor_id,true,action.id};
 const std::string_view office_completion_code=success?std::string_view{}:std::string_view{"MilitaryActionInterrupted"};
 const std::string_view office_completion_id=success?std::string_view{terms.office_id}:std::string_view{};
 return {true,OriginalOfficeActionCompletion(OriginalOfficeActionCompletionBuildView{success,office_completion_code,office_completion_id}),{}};
}
// Same original selected-goods derivation, callable with actual historical
// roster rows. No current-state or source-provider lookup is hidden here.
void append_original_formation_good(std::vector<std::pair<StableId,Integer>>& goods,const StableId& id,Integer n){for(auto& p:goods)if(p.first==id){p.second+=n;return;}goods.emplace_back(id,n);}
Outcome<Integer> append_original_formation_unit_equipment(const MilitaryCatalog& cat,const MilitaryUnitRecord& unit,Integer additional_retainers,std::vector<std::pair<StableId,Integer>>& goods){
 Integer mouths=unit.fit.size()+unit.wounded.size();Integer equipped=mouths+additional_retainers;
 const auto* definition=unit_definition(cat,unit);if(!definition)return error<Integer>(fail("MilitaryUnitSourceMissing"));
 for(const auto& equipment:definition->equipment)append_original_formation_good(goods,equipment.item_id,equipped*equipment.quantity_per_person);
 return {true,mouths,{}};
}
Outcome<Integer> read_original_formation_food_quantity(const GarrisonDefinition& camp,Integer mouths,const std::optional<Integer>& requested,bool valid,Integer money,bool money_valid){
 Integer food=requested.value_or(mouths*camp.ration_per_person*2);
 if(!valid||!money_valid||food<0||food>1000000||money<0||money>1000000)return error<Integer>(fail("InvalidMilitarySupply"));
 return {true,food,{}};
}
Integer original_formation_goods_weight_after(Integer accumulated,Integer quantity,Integer item_weight)noexcept{return accumulated+quantity*item_weight;}
const ArmyRecord* active_army_for_actor(const MilitaryState& s,const StableId& actor){for(auto& army:s.armies)if(army.status=="active"&&has(army.member_actor_ids,actor))return &army;return nullptr;}
// 原supply_preview：只计本组织且未被预留的粮批，不扣钱粮、不访问敌军。
Outcome<MilitaryProvisions> preview_army_supply(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& army_id){auto army=find_id(s.armies,army_id);if(!army)return error<MilitaryProvisions>(fail("UnknownArmy"));if(army->status!="active")return error<MilitaryProvisions>(fail("ArmyNotActive"));auto camp=find_id(cat.garrisons,army->garrison_id);if(!camp||!c.inventory(army->inventory_id))return error<MilitaryProvisions>(common_failure("MissingArmySupplySource",army_id,ErrorKind::internal));auto mouths=army_mouths(c,s,*army);if(!mouths.ok)return {false,{},mouths.error};auto available=count(c,army->inventory_id,camp->food_item_id,owner(army->organization_id));if(!available.ok)return {false,{},available.error};return {true,provisions(*camp,static_cast<Integer>(mouths.value.size()),available.value),{}};}
Status military_capability_failure(const StableId& phase,const StableId& kind,const StableId& source){return {false,{},{{"UnsupportedNativeCapability",{}},ErrorKind::unsupported_capability,phase,"military",kind,source}};}
Outcome<MilitaryQuote> preview_military(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryCommand& command,const StableId& own){
    if(std::holds_alternative<MilitaryTrain>(command)||std::holds_alternative<MilitaryTrainCancel>(command)||std::holds_alternative<MilitaryDrillCommand>(command)){if(!cat.military)return error<MilitaryQuote>(fail("MilitaryUnavailable"));return preview_military_training(c,s,cat,a,actor,command,own);}

    if(std::holds_alternative<SiegeStart>(command)||std::holds_alternative<SiegeJoin>(command)||std::holds_alternative<SiegePlan>(command)||std::holds_alternative<SiegeStrategy>(command)||std::holds_alternative<SiegeClose>(command))return preview_siege_world(c,s,cat,a,actor,command);

    if(std::holds_alternative<CampaignManage>(command)||std::holds_alternative<ArmyLogistics>(command)||std::holds_alternative<OfficeResign>(command)||std::holds_alternative<ArmyPledge>(command)){bool redirect=false;if(auto p=std::get_if<CampaignManage>(&command)){auto person=c.actor(actor);auto action=person?c.action(person->action_id):nullptr;redirect=(p->kind=="campaign_return"||p->kind=="campaign_cancel")&&action&&action->kind=="travel";}if(!redirect&&!cat.military)return error<MilitaryQuote>(fail("MilitaryUnavailable"));return preview_military_lifecycle(c,s,cat,a,actor,command,own);}
    if(auto request=std::get_if<MilitaryTacticPracticeClaim>(&command)){if(!cat.battle_catalog.tactic_learning||!a.preview_tactic_practice)return error<MilitaryQuote>(fail("MilitaryTacticLearningReadRequired"));auto q=a.preview_tactic_practice(actor,*request);if(!q.ok)return {false,{},q.error};MilitaryQuote out;out.tactic_practice=std::move(q.value);return {true,std::move(out),{}};}
    if(auto request=std::get_if<MilitaryFieldCommandAssign>(&command)){auto q=preview_military_field_command(c,s,cat,a,actor,*request);if(!q.ok)return {false,{},q.error};MilitaryQuote out;out.field_command=std::move(q.value);return {true,std::move(out),{}};}
    if(auto request=std::get_if<MilitaryOfficerAssign>(&command)){auto q=preview_military_officer_assignment(c,s,cat,a,actor,*request);if(!q.ok)return {false,{},q.error};MilitaryQuote out;out.officer_assignment=std::move(q.value);return {true,std::move(out),{}};}
    if(auto request=std::get_if<MilitaryBattleRequest>(&command))return preview_military_request(c,s,cat,a,actor,*request);
    if(std::holds_alternative<MilitaryBattlePlan>(command)||std::holds_alternative<MilitaryBattleClose>(command))return preview_battle_command(c,s,cat,a,actor,command);
    if(!cat.military)return error<MilitaryQuote>(fail("MilitaryUnavailable"));
    if(auto depart=std::get_if<CampaignDepart>(&command)){
        auto campaign=find_id(s.campaigns,depart->campaign_id);if(!campaign||campaign->actor_id!=actor)return error<MilitaryQuote>(fail("CampaignNotAssigned"));if(campaign->status=="closed")return error<MilitaryQuote>(fail("CampaignClosed"));auto person=c.actor(actor);auto office=find_id(s.offices,campaign->office_id);if(!person||!person->alive||!office||!office->enabled||office->holder_actor_id!=actor)return error<MilitaryQuote>(fail("CampaignAuthorityLost"));auto army=find_id(s.armies,campaign->army_id);auto commander=army?c.actor(army->commander_actor_id):nullptr;if(!commander||!commander->alive||!commander->action_id.empty())return error<MilitaryQuote>(fail("CampaignCommanderBusy"));if(campaign->status!="mustered")return error<MilitaryQuote>(fail("CampaignNotMustered"));if(army->status!="active")return error<MilitaryQuote>(fail("CampaignArmyUnavailable"));MilitaryQuote quote;quote.campaign_id=campaign->id;return {true,quote,{}};
    }
    if(auto accept=std::get_if<CampaignAccept>(&command)){
        auto d=find_id(cat.orders,accept->template_id);if(!d)return error<MilitaryQuote>(fail("UnknownMilitaryOrder"));auto o=find_id(s.offices,d->office_id);if(!o||!o->enabled||o->holder_actor_id!=actor)return error<MilitaryQuote>(fail("CampaignOfficeRequired"));
        auto checked=contact(c,a,actor,accept->context,"military_order");if(!checked.ok)return {false,{},checked.error};if(checked.value.role_id!=d->issuer_role_id)return error<MilitaryQuote>(fail("CampaignIssuerRequired"));for(auto& campaign:s.campaigns)if(campaign.actor_id==actor&&campaign.status!="closed")return error<MilitaryQuote>(fail("CampaignAlreadyActive"));
        StableId recipient;if(d->objective_kind=="deliver_supply"&&d->recipient=="army"){if(!accept->recipient_army_id)return error<MilitaryQuote>(fail("CampaignRecipientArmyRequired"));recipient=*accept->recipient_army_id;auto army=find_id(s.armies,recipient);auto office=find_id(cat.offices,d->office_id);auto organization_id=d->recipient_organization_id.empty()?office->organization_id:d->recipient_organization_id;if(!army||army->status!="active"||army->organization_id!=organization_id)return error<MilitaryQuote>(fail("CampaignRecipientArmyUnauthorized"));}
        MilitaryQuote q;q.template_id=d->id;q.context=checked.value;q.recipient_army_id=recipient;return {true,q,{}};
    }
    if(!a.actor_injury)return error<MilitaryQuote>(military_capability_failure("preview","actor_injury",actor));
    if(!ready(c,a,actor,own))return error<MilitaryQuote>(fail("MilitaryActorUnavailable"));
    if(auto accept=std::get_if<OfficeAccept>(&command)){
        auto d=find_id(cat.offices,accept->office_id);if(!d)return error<MilitaryQuote>(fail("UnknownOffice"));auto check=contact(c,a,actor,accept->context,"office_accept",own);if(!check.ok)return {false,{},check.error};auto role=a.interaction_catalog&&a.interaction_catalog->common?a.interaction_catalog->common->roles.find(check.value.role_id):nullptr;
        if(check.value.role_id!=d->appointing_role_id||!role||role->organization_id!=d->organization_id)return error<MilitaryQuote>(fail("OfficeAppointerRequired"));
        if(organization(s,cat,d->garrison_id)!=d->organization_id)return error<MilitaryQuote>(fail("OfficeAuthorityLost"));auto office=find_id(s.offices,d->id);if(!office)return error<MilitaryQuote>(fail("UnknownOffice"));if(office->enabled)return error<MilitaryQuote>(fail("OfficeOccupied"));for(auto& o:s.offices)if(o.enabled&&o.holder_actor_id==actor)return error<MilitaryQuote>(fail("PublicAppointmentPending"));
        for(auto& payload:s.actions)if(payload.action_id!=own)if(auto terms=std::get_if<OfficeTerms>(&payload.terms);terms&&terms->office_id==d->id)return error<MilitaryQuote>(fail("OfficeAppointmentReserved"));
        MilitaryQuote q;q.duration=1;q.participants={actor,check.value.provider_actor_id};q.context=check.value;q.terms=OfficeTerms{d->id,actor,d->organization_id,check.value.place_id,check.value.provider_actor_id};return {true,q,{}};
    }
    auto form=std::get_if<ArmyForm>(&command);if(form&&!a.in_party)return error<MilitaryQuote>(military_capability_failure("preview","party_membership",actor));if(!form)return error<MilitaryQuote>(fail("UnknownMilitaryCommand"));auto campaign=find_id(s.campaigns,form->order_id);if(!campaign)return error<MilitaryQuote>(fail("UnknownMilitaryOrder"));if(campaign->status!="accepted"||!campaign->army_id.empty()||campaign->actor_id!=actor)return error<MilitaryQuote>(fail("MilitaryOrderNotMusterable"));auto authorized=office_ready(c,s,cat,actor,campaign->office_id);if(!authorized.ok)return {false,{},authorized.error};auto office=find_id(cat.offices,campaign->office_id);if(campaign->garrison_id!=office->garrison_id)return error<MilitaryQuote>(fail("MilitaryOrderCampMismatch"));auto camp=find_id(cat.garrisons,office->garrison_id);if(!camp)return error<MilitaryQuote>(fail("UnknownGarrison"));auto org=organization(s,cat,camp->id);
    if(org!=office->organization_id||!at(c,actor,camp->place_id)||active_army_for_actor(s,actor)||(a.in_party&&a.in_party(actor)))return error<MilitaryQuote>(fail("MilitaryAssemblyRequired"));
    if(!form->unit_ids_valid||!unique(form->unit_ids)||form->unit_ids.empty()||(office->command_scope=="unit"&&form->unit_ids.size()!=1))return error<MilitaryQuote>(fail("MilitaryCommandScope"));
    ArmyFormationTerms t;t.order_id=campaign->id;t.office_id=campaign->office_id;t.actor_id=actor;t.garrison_id=camp->id;t.ownership_id=ownership_id(s,cat,camp->id);t.place_id=camp->place_id;t.organization_id=org;t.unit_ids=form->unit_ids;t.member_actor_ids={actor};t.source_inventory_id=camp->inventory_id;t.source_wallet_id=camp->wallet_id;
    for(std::size_t i=0;i<form->unit_ids.size();++i){auto u=find_id(s.military_units,form->unit_ids[i]);if(!u||u->garrison_id!=camp->id||u->organization_id!=org||!u->army_id.empty()||(!u->mobilizing_action_id.empty()&&u->mobilizing_action_id!=own)||(u->training_action_id&&!u->training_action_id->empty())||u->fit.empty())return error<MilitaryQuote>(fail("GarrisonUnitUnavailable"));auto commander=i==0?actor:u->commander_actor_id;bool duplicate=false;for(auto& p:t.commanders)if(p.second==commander)duplicate=true;if(duplicate||!ready(c,a,commander,own)||!at(c,commander,camp->place_id)||active_army_for_actor(s,commander)||(a.in_party&&a.in_party(commander)))return error<MilitaryQuote>(fail("MilitaryOfficerUnavailable"));t.commanders.emplace_back(u->id,commander);if(!has(t.member_actor_ids,commander))t.member_actor_ids.push_back(commander);}
    if(!form->retainer_ids_valid||!unique(form->retainer_ids))return error<MilitaryQuote>(fail("InvalidRetainerRoster"));for(auto& person:form->retainer_ids){if(has(t.member_actor_ids,person)||!ready(c,a,person,own)||!at(c,person,camp->place_id)||active_army_for_actor(s,person)||(a.in_party&&a.in_party(person)))return error<MilitaryQuote>(fail("RetainerAssemblyRequired"));if(!a.retainer_source)return error<MilitaryQuote>(military_capability_failure("preview","army_pledge",person));auto retainer=a.retainer_source(person,campaign->id);if(!retainer)return error<MilitaryQuote>(fail("PrivateRetainerConsentRequired"));for(auto& army:s.armies)for(auto& previous:army.retainers)if(previous.actor_id==person&&previous.source_id==retainer->source_id)return error<MilitaryQuote>(fail("RetainerPledgeAlreadyUsed"));retainer->actor_id=person;retainer->unit_id=form->unit_ids[0];t.retainers.push_back(*retainer);t.member_actor_ids.push_back(person);}
    Integer mouths=t.member_actor_ids.size();for(auto& id:t.unit_ids){auto u=find_id(s.military_units,id);auto equipment=append_original_formation_unit_equipment(cat,*u,id==t.unit_ids[0]?static_cast<Integer>(t.retainers.size()):Integer{0},t.goods);if(!equipment.ok)return {false,{},equipment.error};mouths+=equipment.value;}
    t.money=form->money.value_or(0);auto original_food=read_original_formation_food_quantity(*camp,mouths,form->food_quantity,form->food_quantity_valid,t.money,form->money_valid);if(!original_food.ok)return {false,{},original_food.error};Integer food=original_food.value;auto inventory=c.inventory(camp->inventory_id);auto wallet=c.wallet(camp->wallet_id);auto own_org=owner(org);if(!inventory||!wallet||!owner_equal(inventory->owner,own_org)||!owner_equal(wallet->owner,own_org))return error<MilitaryQuote>(fail("MilitaryPublicAssetAuthority"));auto funds=c.spendable(camp->wallet_id);if(!funds.ok)return {false,{},funds.error};auto available_food=count(c,camp->inventory_id,camp->food_item_id,own_org);if(!available_food.ok)return {false,{},available_food.error};if(available_food.value<food||funds.value<t.money)return error<MilitaryQuote>(fail("InsufficientMilitarySupply"));if(food>0)append_original_formation_good(t.goods,camp->food_item_id,food);
    if(!a.item_weight)return error<MilitaryQuote>(military_capability_failure("preview","item_weight",actor));Integer weight=0;for(auto& good:t.goods){auto available=count(c,camp->inventory_id,good.first,own_org);if(!available.ok)return {false,{},available.error};if(available.value<good.second)return error<MilitaryQuote>(fail("InsufficientMilitaryEquipmentOrFood"));weight=original_formation_goods_weight_after(weight,good.second,a.item_weight(good.first));}if(weight>camp->army_capacity)return error<MilitaryQuote>(fail("ArmyCapacityExceeded"));MilitaryQuote q;q.duration=1;q.participants=t.member_actor_ids;Integer provision_count=0;for(auto& good:t.goods)if(good.first==camp->food_item_id)provision_count=good.second;q.provisions=provisions(*camp,mouths,provision_count);q.terms=std::move(t);return {true,q,{}};
}
bool original_military_actor_life_ready(bool alive,bool available,Integer hp)noexcept{return alive&&available&&hp>0;}
bool original_military_actor_injury_ready(Integer injury)noexcept{return injury<3;}
// C05 original reservation and completion bodies shared by the two clocks.
Status reserve_original_army_formation_in_candidate(CommonWrite& c,MilitaryState& s,const StableId& action_id,const ArmyFormationTerms& t,MilitaryFormationReservationCapture* capture){
 if(capture){auto opening=capture->capture_original_opening(c,t);if(!opening.ok)return opening;}
 ResourceRecipe recipe;CheckpointVector<const std::pair<StableId,Integer>*> goods;goods.reserve(t.goods.size());for(const auto& good:t.goods)goods.push_back(&good);std::sort(goods.begin(),goods.end(),[](const std::pair<StableId,Integer>* x,const std::pair<StableId,Integer>* y){return *x<*y;});for(const auto* original_good:goods){const auto& good=*original_good;recipe.operations.push_back(ReserveLots{t.source_inventory_id,good.first,good.second,{owner(t.organization_id)},action_id});}recipe.operations.push_back(SetReservation{t.source_wallet_id,action_id,t.money,false});auto r=c.apply_resources(recipe);if(!r.ok)return r;if(capture){auto captured=capture->capture_original_reservation(c);if(!captured.ok)return captured;}for(auto& id:t.unit_ids){auto* unit=find_id(s.military_units,id);if(!unit)return fail("MilitaryUnitSourceMissing");unit->mobilizing_action_id=action_id;}return common_success();
}
Outcome<MilitaryEffect> complete_original_army_formation_in_candidate(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action,const ArmyFormationTerms& terms,Integer actual_supply_period_to,MilitaryActionResourceCapture* resource_capture,const MilitaryActionCompletionConditionGuard* condition_guard){
 if(action.kind!="army_form"||actual_supply_period_to<1)return error<MilitaryEffect>(fail("ArmyFormationSupplyPeriodRequired"));
 if(a.military_action_resources_schema<0||a.military_action_resources_schema>1||bool(resource_capture)!=(a.military_action_resources_schema==1))return error<MilitaryEffect>(fail("MilitaryActionResourceSelectionMismatch"));
 bool success=true;MilitaryEffect effect;
        for(auto& person:action.participants)if(!ready(c,a,person,action.id)||c.actor(person)->action_id!=action.id)success=false;
        auto campaign=find_id(s.campaigns,terms.order_id);if(!campaign||campaign->status!="accepted"||!campaign->army_id.empty()||!office_ready(c,s,cat,action.actor_id,terms.office_id).ok)success=false;for(auto& person:action.participants)if(!at(c,person,terms.place_id))success=false;
        if(a.military_action_conditions_schema==2&&!condition_guard)return error<MilitaryEffect>(fail("MilitaryActionCompletionConditionGuardRequired"));
        if(condition_guard){auto checked=condition_guard->validate_before_effects(c,a,action);if(!checked.ok)return {false,{},checked.error};}
        if(success){auto id=c.next_id("army");if(!id.ok)return {false,{},id.error};auto inventory_id="army_inventory:"+id.value,wallet_id="army_wallet:"+id.value;auto camp=find_id(cat.garrisons,terms.garrison_id);if(!camp)return error<MilitaryEffect>(fail("UnknownGarrison"));auto own=owner(terms.organization_id);
            InventoryView inventory;inventory.id=inventory_id;inventory.owner=own;inventory.capacity=camp->army_capacity;WalletView wallet;wallet.id=wallet_id;wallet.owner=own;ResourceRecipe recipe;recipe.operations.push_back(CreateInventory{inventory});recipe.operations.push_back(CreateWallet{wallet});CheckpointVector<const std::pair<StableId,Integer>*> goods;goods.reserve(terms.goods.size());for(const auto& good:terms.goods)goods.push_back(&good);std::sort(goods.begin(),goods.end(),[](const std::pair<StableId,Integer>* x,const std::pair<StableId,Integer>* y){return *x<*y;});for(const auto* original_good:goods){const auto& good=*original_good;recipe.operations.push_back(TransferLots{terms.source_inventory_id,inventory_id,good.first,good.second,{own},action.id,{}});}auto money_recipe=resource_recipe::append_money_transfer(recipe,terms.source_wallet_id,wallet_id,terms.money,resource_recipe::MoneyTransferPolicy::original_balance);if(!money_recipe.ok)return {false,{},money_recipe.error};auto moved=c.apply_resources(recipe);if(!moved.ok)return {false,{},moved.error};if(resource_capture){auto captured=resource_capture->capture_original_success(c);if(!captured.ok)return {false,{},captured.error};}
            ArmyRecord army;army.id=id.value;army.organization_id=terms.organization_id;army.commander_actor_id=action.actor_id;army.member_actor_ids=terms.member_actor_ids;army.unit_ids=terms.unit_ids;army.inventory_id=inventory_id;army.wallet_id=wallet_id;army.order_id=terms.order_id;army.garrison_id=terms.garrison_id;army.last_supply_tick=actual_supply_period_to;army.retainers=terms.retainers;army.formation_source=action.id;s.armies.push_back(army);
            for(auto& uid:terms.unit_ids){auto unit=find_id(s.military_units,uid);if(!unit)return error<MilitaryEffect>(fail("MilitaryUnitSourceMissing"));unit->army_id=army.id;unit->garrison_id="";for(auto& pair:terms.commanders)if(pair.first==uid)unit->commander_actor_id=pair.second;}
            CheckpointVector<const RetainerRecord*> retainers;retainers.reserve(terms.retainers.size());for(const auto& retainer:terms.retainers)retainers.push_back(&retainer);std::sort(retainers.begin(),retainers.end(),[](const RetainerRecord* x,const RetainerRecord* y){return x->actor_id<y->actor_id;});for(const auto* original_retainer:retainers){const auto& r=*original_retainer;auto unit=find_id(s.military_units,r.unit_id);unit->member_ids.push_back(r.actor_id);unit->fit.push_back(r.actor_id);unit->retainer_sources.emplace_back(r.actor_id,r.source_id);}
            for(auto& person:terms.member_actor_ids)if(person!=action.actor_id){ActorMutation mutation;mutation.actor_id=person;mutation.location=InArmyLocation{army.id};auto r=c.apply_actor(mutation);if(!r.ok)return {false,{},r.error};}
            if(!campaign||campaign->status!="accepted"||!campaign->army_id.empty())return error<MilitaryEffect>(fail("CampaignAlreadyMustered"));campaign->army_id=army.id;campaign->status="mustered";effect.army_id=army.id;effect.order_id=terms.order_id;effect.details_ok=true;
        }
        for(auto& id:terms.unit_ids){auto unit=find_id(s.military_units,id);if(unit&&unit->mobilizing_action_id==action.id)unit->mobilizing_action_id="";}
 effect.success=success;effect.code=success?"":"MilitaryActionInterrupted";return {true,std::move(effect),{}};
}
Status start_military_action(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action,const MilitaryCommand& command,const MilitaryQuote& quote){
    if(auto request=std::get_if<MilitaryTrain>(&command))return start_military_training(c,s,cat,a,action,*request,quote);

    if(auto request=std::get_if<ArmyLogistics>(&command))return start_military_logistics(c,s,cat,a,action,*request,quote);
    if(action.kind!="office_accept"&&action.kind!="army_form")return military_capability_failure("actions_step",action.kind,action.id);
    if((action.kind=="office_accept")!=std::holds_alternative<OfficeAccept>(command)||(action.kind=="army_form")!=std::holds_alternative<ArmyForm>(command))return fail("InvalidMilitaryAction");
    for(auto& f:s.action_facts)if(f.id==action.id)return fail("InvalidMilitaryAction");
    auto current=preview_military(c,s,cat,a,action.actor_id,command,action.id);if(!current.ok)return {false,{},current.error};
    if(action.participants!=current.value.participants||!same_quote(quote,current.value)||action.start_tick!=c.tick()||action.remaining_ticks!=1||!current.value.terms)return fail("StaleMilitaryQuote");
    for(auto& person:action.participants){auto p=c.actor(person);if(!p||p->action_id!=action.id)return fail("MilitaryOccupationMissing");}
    MilitaryActionPayload payload;payload.action_id=action.id;payload.terms=*current.value.terms;payload.context=current.value.context;
    if(auto request=std::get_if<OfficeAccept>(&command))payload.request=*request;else if(auto request=std::get_if<ArmyForm>(&command))payload.request=*request;else return fail("InvalidMilitaryAction");
    if(auto t=std::get_if<ArmyFormationTerms>(&payload.terms)){auto r=reserve_original_army_formation_in_candidate(c,s,action.id,*t);if(!r.ok)return r;}
    auto r=c.append_fact({action.id,"military_action","military"});if(!r.ok)return r;MilitaryActionFact fact;fact.id=action.id;fact.command=action.kind;fact.actor_id=action.actor_id;fact.start_tick=c.tick();fact.terms=payload.terms;fact.context=payload.context;s.action_facts.push_back(std::move(fact));s.actions.push_back(std::move(payload));return common_success();
}
Outcome<MilitaryEffect> apply_military_instant(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryCommand& command){
    if(auto request=std::get_if<MilitaryTacticPracticeClaim>(&command)){if(!cat.battle_catalog.tactic_learning||!a.apply_tactic_practice)return error<MilitaryEffect>(fail("MilitaryTacticLearningReadRequired"));auto applied=a.apply_tactic_practice(actor,*request);if(!applied.ok)return {false,{},applied.error};MilitaryEffect out;out.fact_id=applied.value.practice_fact_id;out.receipt_id=applied.value.acquisition_fact_id;out.source_id=applied.value.training_action_id;out.already_completed=applied.value.replayed;return {true,std::move(out),{}};}
    if(auto request=std::get_if<MilitaryFieldCommandAssign>(&command))return apply_military_field_command(c,s,cat,a,actor,*request);
    if(auto request=std::get_if<MilitaryOfficerAssign>(&command))return apply_military_officer_assignment(c,s,cat,a,actor,*request);
    if(std::holds_alternative<MilitaryTrainCancel>(command)||std::holds_alternative<MilitaryDrillCommand>(command))return apply_military_training(c,s,cat,a,actor,command);

    if(std::holds_alternative<SiegeStart>(command)||std::holds_alternative<SiegeJoin>(command)||std::holds_alternative<SiegePlan>(command)||std::holds_alternative<SiegeStrategy>(command)||std::holds_alternative<SiegeClose>(command))return apply_siege_world(c,s,cat,a,actor,command);

    if(std::holds_alternative<CampaignManage>(command)||std::holds_alternative<OfficeResign>(command)||std::holds_alternative<ArmyPledge>(command))return apply_military_lifecycle(c,s,cat,a,actor,command);
    if(auto request=std::get_if<MilitaryBattleRequest>(&command))return apply_military_request(c,s,cat,a,actor,*request);
    if(std::holds_alternative<MilitaryBattlePlan>(command)||std::holds_alternative<MilitaryBattleClose>(command))return apply_battle_command(c,s,cat,a,actor,command);
    auto q=preview_military(c,s,cat,a,actor,command);if(!q.ok)return {false,{},q.error};if(auto depart=std::get_if<CampaignDepart>(&command)){auto campaign=find_id(s.campaigns,q.value.campaign_id);if(!campaign)return error<MilitaryEffect>(fail("CampaignNotAssigned"));auto definition=find_id(cat.orders,campaign->template_id);auto target=definition?find_id(cat.strongholds,definition->target_stronghold_id):nullptr;if(!target)return error<MilitaryEffect>(fail("UnknownCampaignTarget"));campaign->status="marching";MilitaryEffect effect;effect.campaign_id=depart->campaign_id;effect.journey_request=MilitaryJourneyRequest{campaign->army_id,target->settlement_id};return {true,effect,{}};}
    auto request=std::get_if<CampaignAccept>(&command);if(!request)return error<MilitaryEffect>(fail("MilitaryActionRequired"));auto d=find_id(cat.orders,q.value.template_id);auto office=find_id(cat.offices,d->office_id);auto selected=validate_military_authority_time_write(c,s,a);if(!selected.ok)return {false,{},selected.error};auto id=c.next_owned_id("campaign");if(!id.ok)return {false,{},id.error};CampaignRecord p;p.id=id.value.id();p.template_id=d->id;p.actor_id=actor;p.office_id=d->office_id;p.garrison_id=d->garrison_id;p.issuer_role_id=d->issuer_role_id;p.issuer_organization_id=office->organization_id;p.accepted_tick=c.tick();p.deadline_tick=c.tick()+d->deadline_ticks;p.last_tick=c.tick();p.recipient_army_id=q.value.recipient_army_id;p.return_garrison_id="";p.return_wait_reason="";if(!q.value.context)return error<MilitaryEffect>(fail("MilitaryCampaignAcceptanceContextRequired",p.id));auto exact=capture_military_campaign_acceptance(c,s,a,p.id,actor,p.office_id,*q.value.context);if(!exact.ok)return {false,{},exact.error};if(exact.value){p.exact_acceptance.emplace(*exact.value);auto appended=c.append_fact_borrowed(p.id,"campaign_accepted","military");if(!appended.ok)return {false,{},appended.error};}s.campaigns.push_back(p);{auto authority_postwrite=validate_military_authority_time_after_write(c,s,a);if(!authority_postwrite.ok)return {false,{},authority_postwrite.error};}MilitaryEffect effect;effect.campaign_id=p.id;return {true,effect,{}};
}
Outcome<MilitaryEffect> finish_military_action(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action){
    if(action.kind=="military_train")return finish_military_training(c,s,cat,a,action,c.tick()+1);

    if(action.kind=="army_return"||action.kind=="army_resupply")return finish_military_logistics(c,s,cat,a,action);
    if(!a.actor_injury)return error<MilitaryEffect>(military_capability_failure("actions_step","actor_injury",action.actor_id));
    auto validated=validate_military_action(c,s,cat,a,action,true);if(!validated.ok)return {false,{},validated.error};
    auto fact=find_id(s.action_facts,action.id);if(fact&&fact->phase=="completed"){MilitaryEffect result;result.success=fact->success;result.already_completed=true;if(fact->result){result.office_id=fact->result->office_id;result.army_id=fact->result->army_id;result.order_id=fact->result->order_id;result.details_ok=fact->result->ok;}return {true,result,{}};}MilitaryActionPayload* payload=nullptr;for(auto& p:s.actions)if(p.action_id==action.id)payload=&p;if(!fact||!payload||!same_terms(fact->terms,payload->terms)||!same_context(fact->context,payload->context))return error<MilitaryEffect>(fail("InvalidMilitaryAction"));
    if(action.remaining_ticks!=0||c.tick()+1!=action.start_tick+1)return error<MilitaryEffect>(fail("MilitaryActionNotReady"));bool success=true;
    MilitaryEffect formation_effect;
    std::optional<OriginalOfficeActionCompletion> office_completion;
    if(auto t=std::get_if<OfficeTerms>(&payload->terms)){
        auto completed=complete_original_office_action_in_candidate(c,s,cat,a,action,*t,payload->context);if(!completed.ok)return {false,{},completed.error};office_completion.emplace(std::move(completed.value));success=office_completion->effect.success;
    }else{
        auto formed=complete_original_army_formation_in_candidate(c,s,cat,a,action,std::get<ArmyFormationTerms>(payload->terms),c.tick()+1);if(!formed.ok)return formed;formation_effect=std::move(formed.value);success=formation_effect.success;
    }
    auto& effect=office_completion?office_completion->effect:formation_effect;
    auto r=apply_original_action_resource_release_in_candidate(c,action.id);if(!r.ok)return {false,{},r.error};fact->phase="completed";fact->completed_tick=c.tick()+1;fact->success=success;MilitaryActionResult result;result.ok=effect.details_ok;result.office_id=effect.office_id;result.army_id=effect.army_id;result.order_id=effect.order_id;fact->result=result;effect.success=success;effect.code=success?"":"MilitaryActionInterrupted";
    s.actions.erase(std::remove_if(s.actions.begin(),s.actions.end(),[&](auto& p){return p.action_id==action.id;}),s.actions.end());return {true,effect,{}};
}
namespace {
// These are the original feed inputs, captured before the first exact Opening
// consequence. A later body's +4 cannot change another responsibility's mouths.
StableId copy_supply_text(CheckpointMemoryCharge& owner,const StableId& source){(owner).grow(0);return checkpoint_fresh_string_copy(source);}
struct OriginalSupplyInputs {
 // Own the ordinary string reservations before every field/container: all text
 // is destroyed before its reservation. Node/vector bytes use their allocators.
 CheckpointMemoryCharge text_memory;
 StableId army_id,garrison_id,organization_id,ownership_id,inventory_id;
 const GarrisonDefinition* definition=nullptr;
 CheckpointVector<StableId> units;
 std::set<StableId,std::less<StableId>,CheckpointAllocator<StableId>> mouths;
 OriginalSupplyInputs()=default;
 OriginalSupplyInputs(const OriginalSupplyInputs&)=delete;
 OriginalSupplyInputs& operator=(const OriginalSupplyInputs&)=delete;
 OriginalSupplyInputs(OriginalSupplyInputs&&)=default;
 OriginalSupplyInputs& operator=(OriginalSupplyInputs&&)=delete;
 StableId copy_text(const StableId& source){return copy_supply_text(text_memory,source);}
 void add_unit(const StableId& source){units.push_back(copy_text(source));}
 void add_mouth(const StableId& source){if(mouths.find(source)==mouths.end())mouths.insert(copy_text(source));}
 template<class Range>void add_mouths(const Range& source){for(const auto& id:source)add_mouth(id);}
};
static_assert(std::is_nothrow_move_constructible_v<OriginalSupplyInputs>,"Opening preimage transfer must keep its reservation attached");
Outcome<OriginalSupplyInputs> original_garrison_supply_inputs(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& id,const ExactSupplyAdoption* exact){
 const auto* camp=find_id(s.garrisons,id);const auto* definition=find_id(cat.garrisons,id);
 if(!camp||!definition)return error<OriginalSupplyInputs>(fail("UnknownGarrison"));
 OriginalSupplyInputs out;out.garrison_id=out.copy_text(id);if(const auto* source=organization_source(s,cat,id))out.organization_id=out.copy_text(*source);if(const auto* source=ownership_source(s,cat,id))out.ownership_id=out.copy_text(*source);out.inventory_id=out.copy_text(definition->inventory_id);out.definition=definition;
 for(const auto& u:s.military_units)if(u.garrison_id==id&&u.army_id.empty()&&u.organization_id==out.organization_id)out.add_unit(u.id);
 std::sort(out.units.begin(),out.units.end());
 for(const auto& uid:out.units){const auto* u=find_id(s.military_units,uid);out.add_mouths(u->fit);out.add_mouths(u->wounded);
  const auto* commander=c.actor(u->commander_actor_id);if(commander&&commander->alive&&commander->available&&at(c,commander->id,definition->place_id)&&!active_army_for_actor(s,commander->id))out.add_mouth(commander->id);
  if(u->training_action_id&&!u->training_action_id->empty()){
   Outcome<std::vector<StableId>> training;
   if(exact&&exact->validation.action_time.dynamic_training_facts.find(*u->training_action_id)){
    if(!exact->validation.training)return error<OriginalSupplyInputs>(common_failure("ExactMilitaryTrainingOriginalReadRequired",*u->training_action_id));
    training=exact_military_training_mouths(exact->common,s,exact->validation.action_time,*exact->validation.training,*u,id);
   }else training=military_training_mouths(c,s,cat,a,*u,id);
   if(!training.ok)return {false,{},training.error};out.add_mouths(training.value);
  }
 }
 out.add_mouths(camp->prisoner_ids);return {true,std::move(out),{}};
}
Outcome<OriginalSupplyInputs> original_army_supply_inputs(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id,const ExactSupplyAdoption* exact,const ExactMilitaryStateValidationContext* field=nullptr,const CommonStore* field_common=nullptr){
 const auto* army=find_id(s.armies,id);if(!army||army->status!="active")return error<OriginalSupplyInputs>(fail("ArmyNotActive"));
 const auto* camp=find_id(cat.garrisons,army->garrison_id);if(!camp)return error<OriginalSupplyInputs>(fail("UnknownGarrison"));
 OriginalSupplyInputs out;auto collected=collect_army_mouths(c,s,*army,[&](const StableId& mouth){out.add_mouth(mouth);});if(!collected.ok)return {false,{},collected.error};
 const ArmyFormationTerms* terms=nullptr;std::optional<MilitaryFormationSourceView> actual_formation;
 if(field){auto now=field->clock.read_current_moment();if(!now.ok)return {false,{},now.error};const auto* store=field_common;if(!store)return error<OriginalSupplyInputs>(fail("FieldMilitarySupplyCommonStoreRequired"));auto formed=read_original_military_formation(store->state(),s,*army,now.value,&field->formations,field->budget);if(!formed.ok)return {false,{},formed.error};actual_formation.emplace(std::move(formed.value));terms=actual_formation->original_terms;}
 else if(exact){auto now=exact->validation.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto formed=read_original_military_formation(exact->common.state(),s,*army,now.value,&exact->validation.formations,exact->validation.budget);if(!formed.ok)return {false,{},formed.error};actual_formation.emplace(std::move(formed.value));terms=actual_formation->original_terms;}
 else{const auto* source=find_id(s.action_facts,army->formation_source);terms=source?std::get_if<ArmyFormationTerms>(&source->terms):nullptr;}
 if(!terms)return error<OriginalSupplyInputs>(fail("ArmyFormationSourceMissing"));
 out.army_id=out.copy_text(id);out.organization_id=out.copy_text(army->organization_id);out.ownership_id=out.copy_text(terms->ownership_id);out.inventory_id=out.copy_text(army->inventory_id);out.definition=camp;for(const auto& unit:army->unit_ids)out.add_unit(unit);return {true,std::move(out),{}};
}
Status apply_original_supply_inputs(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const OriginalSupplyInputs& inputs,Integer tick,const ExactSupplyAdoption* exact,const std::vector<StableId>* cause=nullptr){
 if(!inputs.definition)return fail("MilitarySupplyDefinitionRequired");
 auto fed=feed(c,s,cat,a,*inputs.definition,inputs.organization_id,inputs.ownership_id,inputs.inventory_id,inputs.units,inputs.mouths,tick,inputs.army_id,inputs.garrison_id,exact,cause);if(!fed.ok)return fed;
 if(inputs.army_id.empty()){auto* camp=find_id(s.garrisons,inputs.garrison_id);if(!camp)return fail("UnknownGarrison");camp->last_supply_tick=tick;}
 else{auto* army=find_id(s.armies,inputs.army_id);if(!army)return fail("UnknownArmy");army->last_supply_tick=tick;}return common_success();
}
}
static Status military_supply_step_impl(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,Integer tick,const ExactSupplyAdoption* exact){
 CheckpointMemoryCharge id_memory;CheckpointVector<StableId> camp_ids,army_ids;for(const auto& camp:s.garrisons)camp_ids.push_back(copy_supply_text(id_memory,camp.id));for(const auto& army:s.armies)army_ids.push_back(copy_supply_text(id_memory,army.id));std::sort(camp_ids.begin(),camp_ids.end());std::sort(army_ids.begin(),army_ids.end());
 CheckpointVector<OriginalSupplyInputs> opening_inputs;
 for(const auto& id:camp_ids){auto inputs=original_garrison_supply_inputs(c,s,cat,a,id,exact);if(!inputs.ok)return {false,{},inputs.error};if(exact)opening_inputs.push_back(std::move(inputs.value));else{auto r=apply_original_supply_inputs(c,s,cat,a,inputs.value,tick,nullptr);if(!r.ok)return r;}}
 for(const auto& id:army_ids){const auto* army=find_id(s.armies,id);if(!army||army->status!="active")continue;auto inputs=original_army_supply_inputs(c,s,cat,id,exact);if(!inputs.ok)return {false,{},inputs.error};if(exact)opening_inputs.push_back(std::move(inputs.value));else{auto r=apply_original_supply_inputs(c,s,cat,a,inputs.value,tick,nullptr);if(!r.ok)return r;}}
 if(exact){
  // A mouth has one original military responsibility in this period, including
  // full-fed mouths. Preflight the whole from-state before resources or bodies.
  std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>> mouths,containers;
  for(const auto& input:opening_inputs){if(!containers.insert(input.inventory_id).second)return common_failure("SharedMilitaryContainer",input.inventory_id);for(const auto& mouth:input.mouths)if(mouth.empty()||!mouths.insert(mouth).second)return common_failure("DuplicateMilitarySupplyMouthResponsibility",mouth);}
  for(const auto& input:opening_inputs){auto r=apply_original_supply_inputs(c,s,cat,a,input,tick,exact);if(!r.ok)return r;}
 }
 return common_success();
}
Status military_supply_step(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,Integer tick){
 auto valid=validate_military_state(c,s,cat,a);if(!valid.ok)return valid;if(tick==c.tick())return common_success();if(tick!=c.tick()+1)return fail("MilitarySupplyClockMismatch");return military_supply_step_impl(c,s,cat,a,tick,nullptr);
}
Outcome<std::vector<MomentOwnerSource>> open_field_military_supply_in_candidate(
 CommonStore& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,
 const ExactMilitaryStateValidationContext& x,const ExactPeriodContext& p,
 const std::vector<GroupMilitarySupplyRebindSource>& actual_supply_movements){
 return movement_detail::guard<std::vector<MomentOwnerSource>>([&]{
  using namespace movement_detail;
  if(!x.field_supply_source1||x.dynamic_military||!a.actor_conditions)
   movement_detail::fail("FieldMilitarySupplyOriginalConsumerRequired");
  const auto now=take(x.clock.read_current_moment());
  const auto period=take(x.periods.read_training_period(p.opening_at.half_day_tick));
  if(period.period_ref!=p.period_id||period.from_tick!=c.tick()||period.to_tick!=p.through_tick||
     !same_moment(period.opening_at,p.opening_at,x.budget)||!same_moment(now,p.opening_at,x.budget))
   movement_detail::fail("FieldMilitarySupplyActualOpeningRequired",p.period_id);
  take(validate_military_state(c,s,cat,a,x));
  for(const auto& row:s.supply_facts)if(row.tick==period.to_tick)
   return take(read_field_military_supply_opening_sources(c,s,cat,a,x.periods,x.formations,period.from_tick,now,x.budget,x.returns));
  CheckpointVector<StableId> camps,armies;
  for(const auto& row:s.garrisons)camps.push_back(row.id);
  for(const auto& row:s.armies)if(row.status=="active")armies.push_back(row.id);
  std::sort(camps.begin(),camps.end());std::sort(armies.begin(),armies.end());
  CheckpointVector<OriginalSupplyInputs> inputs;
  for(const auto& id:camps)inputs.push_back(take(original_garrison_supply_inputs(c,s,cat,a,id,nullptr)));
  for(const auto& id:armies)inputs.push_back(take(original_army_supply_inputs(c,s,cat,id,nullptr,&x,&c)));
  std::set<StableId> mouths,containers;
  for(const auto& input:inputs){
   if(!containers.insert(input.inventory_id).second)movement_detail::fail("SharedMilitaryContainer",input.inventory_id);
   for(const auto& mouth:input.mouths)if(mouth.empty()||!mouths.insert(mouth).second)
    movement_detail::fail("DuplicateMilitarySupplyMouthResponsibility",mouth);
  }
  std::set<StableId> adopted_armies;
  for(const auto& input:inputs){
   std::vector<StableId> cause{period.original_opening.source_ref.id};
   for(const auto& movement:actual_supply_movements)if(!input.army_id.empty()&&movement.army_id==input.army_id){
    if(!adopted_armies.insert(input.army_id).second||movement.journey_id.empty()||!movement.id.empty()||!movement.cause_fact_ids.empty()||!same_moment(movement.occurred_at,now,x.budget))
     movement_detail::fail("MilitarySupplyPreparedJourneyRepeated",input.army_id);
    const auto* head=c.state().facts.find(movement.journey_id);
    if(!head||head->domain!="travel"||head->kind!="group_journey")movement_detail::fail("MilitarySupplyActualJourneyRequired",movement.journey_id);
    cause={movement.journey_id};
   }
   take(apply_original_supply_inputs(c,s,cat,a,input,period.to_tick,nullptr,&cause));
  }
  if(adopted_armies.size()!=actual_supply_movements.size())movement_detail::fail("MilitarySupplyPreparedJourneyCoverage");
  return take(read_field_military_supply_opening_sources(c,s,cat,a,x.periods,x.formations,period.from_tick,now,x.budget,x.returns));
 });
}
Outcome<std::vector<MomentOwnerSource>> open_exact_military_supply_in_candidate(CommonStore& c,MilitaryState& s,MilitaryExactState& exact,const MilitaryCatalog& cat,const MilitaryAccess& a,const ExactMilitaryStateValidationContext& x,const ExactPeriodContext& p,MilitarySupplyExactConditionWrite& conditions){return movement_detail::guard<std::vector<MomentOwnerSource>>([&]{
 using movement_detail::take;
 const auto now=take(x.clock.read_current_moment());const auto period=take(x.periods.read_training_period(p.opening_at.half_day_tick));
 if(x.dynamic_military!=&exact||!conditions.bound_to(c,s,exact)||!movement_detail::same_moment(now,p.opening_at,x.budget)||p.period_id!=period.period_ref||p.through_tick!=period.to_tick||!movement_detail::same_moment(p.opening_at,period.opening_at,x.budget)||c.tick()!=period.from_tick)movement_detail::fail("MilitarySupplyActualOpeningRequired",p.period_id);
 take(validate_military_state(c,s,cat,a,x));bool replay=false;for(const auto& row:exact.supply_effect_sources.rows())if(row.second.period_ref==period.period_ref)replay=true;
 if(replay)return take(read_exact_military_supply_opening_sources(c,s,exact,cat,a,x.periods,x.formations,period.from_tick,now,x.budget,x.returns));
 // No other source from this period can be silently adopted as a new payment.
 for(const auto& row:s.supply_facts)if(row.tick==period.to_tick)movement_detail::fail("MilitarySupplyExactOpeningSourceRequired",row.id);
 const ExactSupplyAdoption adopted{c,exact,period,x,conditions};take(military_supply_step_impl(c,s,cat,a,period.to_tick,&adopted));
 return take(read_exact_military_supply_opening_sources(c,s,exact,cat,a,x.periods,x.formations,period.from_tick,now,x.budget,x.returns));
});}
// 原军团到达判定允许真实城市位置，或同地图精确城市坐标；不会把相邻半径当成到达。
bool army_at_settlement(const CommonRead& c,const MilitaryState& s,const TravelCatalog& geography,const StableId& army_id,const StableId& settlement_id){
    auto army=find_id(s.armies,army_id);auto commander=army?c.actor(army->commander_actor_id):nullptr;auto target=geography.settlements.find(settlement_id);if(!commander||!target)return false;if(auto city=std::get_if<SettlementLocation>(&commander->location))return city->settlement_id==settlement_id;if(auto wild=std::get_if<WildernessLocation>(&commander->location))return !geography.world_map_id.empty()&&wild->map_id==geography.world_map_id&&wild->point.x==target->map_xy.x&&wild->point.y==target->map_xy.y;return false;
}
Status campaign_step(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const TravelCatalog& geography,Integer tick){
    if(tick!=c.tick()+1)return fail("CampaignInvalidStep");
    for(auto& campaign:s.campaigns){
        if(campaign.last_tick==tick)continue;if(campaign.last_tick!=c.tick())return common_failure("CampaignSkippedStep",campaign.id);campaign.last_tick=tick;if(campaign.status=="closed")continue;
        if(campaign.status!="accepted"&&campaign.status!="mustered"&&campaign.status!="marching"&&campaign.status!="operating")return military_capability_failure("campaign_step",campaign.status,campaign.id);
        auto definition=find_id(cat.orders,campaign.template_id);auto target=definition?find_id(cat.strongholds,definition->target_stronghold_id):nullptr;if(!target)return fail("UnknownCampaignTarget");
        if(campaign.status=="marching"&&army_at_settlement(c,s,geography,campaign.army_id,target->settlement_id)){
            campaign.status="operating";auto id=c.next_id("fact");if(!id.ok)return {false,{},id.error};auto recorded=c.append_fact({id.value,"campaign_arrival","military"});if(!recorded.ok)return recorded;
            CampaignArrivalFact arrival;arrival.id=id.value;arrival.campaign_id=campaign.id;arrival.army_id=campaign.army_id;arrival.actor_id=campaign.actor_id;arrival.tick=tick;arrival.stronghold_id=target->id;s.arrival_facts.push_back(arrival);campaign.arrival_fact_id=id.value;
        }
    }
    // 当前闭包没有成功的围城回执；实际到期、失权或返营仍是明确的下一能力边界。
    for(auto& campaign:s.campaigns){if(campaign.status=="closed")continue;auto actor=c.actor(campaign.actor_id);auto office=find_id(s.offices,campaign.office_id);bool lost=!actor||!actor->alive||!actor->available||!office||!office->enabled||office->holder_actor_id!=campaign.actor_id;if(lost||(!campaign.objective_result&&tick>campaign.deadline_tick))return military_capability_failure("campaign_step",lost?"authority_lost":"expired",campaign.id);}
    return common_success();
}
}
