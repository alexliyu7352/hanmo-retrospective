// 计划同意不自动交割；最终交割不复制前代本领、社会关系、公职或未选财物。
#include "voluntary_succession.hpp"
#include "voluntary_succession_candidate.hpp"
#include "historical_player.hpp"
#include "family_legacy.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 永久家事来源必须是共同计数器已分配的规范ID。
bool family_id(const StableId& id,Integer next){if(id.rfind("family:",0)!=0)return false;const auto suffix=id.substr(7);Integer n=0;const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&n>0&&n<next&&std::to_string(n)==suffix;}
// 只取具名typed事实，不按相同人名或控制链末尾猜计划。
template<class T> const T* fact(const FamilyState& family,const StableId& id){const auto* value=family.facts.find(id);return value?std::get_if<T>(value):nullptr;}
// 同一时刻的先后沿永久Common事实序，不能只比较相同tick。
bool precedes(const CommonStore& common,const StableId& before,const StableId& after){const auto& ids=common.state().facts.order();const auto a=std::find(ids.begin(),ids.end(),before),b=std::find(ids.begin(),ids.end(),after);return a!=ids.end()&&b!=ids.end()&&a<b;}
// Build the original final order once. The enclosing first-member lease has
// already reserved this exact requested capacity and every copied string.
FieldOrder cancelled_plan_order(const FieldOrder& original){
 if(original.empty())return {};
 FieldOrder result;if(original.size()>=result.max_size())throw std::bad_alloc();
 result.reserve(original.size()+1);
 for(const auto& field:original)result.push_back(checkpoint_fresh_string_copy(field));
 result.emplace_back("conclusion");return result;
}
struct CancelledSuccessionPlanConstruction {
 CheckpointMemoryCharge copied_fields;FamilyFact value;
 explicit CancelledSuccessionPlanConstruction(const SuccessionPlanCancellationBuildView& view)
  :copied_fields([&]{return CheckpointMemoryCharge(0);}()),
   value(SuccessionConsentFact{checkpoint_fresh_string_copy(view.original.kind),view.original.agreement,
    SuccessionPlanConclusion{"cancelled",view.cancelled_tick,{},{}},cancelled_plan_order(view.original.field_order)}){}
};
}
// 共同的原Family写者；只被已验证的旧入口或显式新路径候选调用，不另建控制链。
Outcome<StableId> record_succession_consent_in_candidate(CommonStore& common,FamilyState& family,const SuccessionConsentSource& source){
    if(source.personal.predecessor_actor_id!=common.state().metadata.player_actor_id||source.accepted_tick!=common.tick()||
       source.accepted_revision!=common.revision()||source.world_id!=common.state().identity.world_id||
       source.scenario_id!=common.state().metadata.scenario_id)return {false,{},common_failure("StaleSuccessionConsent").error};
    auto terms=validate_succession_personal_terms(source.personal);if(!terms.ok)return {false,{},terms.error};
    for(const auto& row:family.facts.rows())if(const auto* old=std::get_if<SuccessionConsentFact>(&row.second)){
        if(succession_consent_source_equal(old->agreement,source))return old->conclusion?Outcome<StableId>{false,{},common_failure("SuccessionConsentSourceConsumed",row.first).error}:Outcome<StableId>{true,row.first,{}};
        if(!old->conclusion&&old->agreement.personal.predecessor_actor_id==source.personal.predecessor_actor_id)
            return {false,{},common_failure("SuccessionPlanAlreadyPending",row.first).error};
    }
    auto id=common.next_id("family");if(!id.ok)return id;SuccessionConsentFact fact;fact.agreement=source;
    if(!family.facts.insert(id.value,std::move(fact)))return {false,{},common_failure("DuplicateSuccessionPlanFact").error};
    auto valid=common.append_fact({id.value,"family_succession_consent","family"});
    return valid.ok?Outcome<StableId>{true,id.value,{}}:Outcome<StableId>{false,{},valid.error};
}
Outcome<StableId> execute_succession_plan_in_candidate(CommonStore& common,FamilyState& family,const StableId& plan_id,const SuccessionPersonalQuote& quote){
    auto* value=family.facts.find(plan_id);auto* plan=value?std::get_if<SuccessionConsentFact>(value):nullptr;
    if(!plan||plan->conclusion||!succession_personal_terms_equal(plan->agreement.personal,quote.terms))
        return {false,{},common_failure("SuccessionAcceptedAssetsChanged",plan_id).error};
    auto recipe=succession_personal_asset_recipe(common,quote);if(!recipe.ok)return {false,{},recipe.error};
    auto id=common.next_id("family");if(!id.ok)return id;auto valid=common.apply_resources(recipe.value);if(!valid.ok)return {false,{},valid.error};
    valid=common.set_player_actor(quote.terms.successor_actor_id);if(!valid.ok)return {false,{},valid.error};
    if(!plan->field_order.empty())plan->field_order.push_back("conclusion");
    plan->conclusion=SuccessionPlanConclusion{"completed",common.tick(),id.value,{}};
    VoluntarySuccessionFact result;result.consent_fact_id=plan_id;result.tick=common.tick();
    if(!family.facts.insert(id.value,std::move(result)))return {false,{},common_failure("DuplicateVoluntarySuccessionFact").error};
    family.successions.push_back(id.value);valid=common.append_fact({id.value,"family_voluntary_succession","family"});
    return valid.ok?Outcome<StableId>{true,id.value,{}}:Outcome<StableId>{false,{},valid.error};
}
// 所有版本共享同一机械链；新路径早门只用本门，真实来源必须在原Known/关系全史之后另核。
namespace {
Status structure(const CommonStore& common,const FamilyState& family,const std::function<Status(const SuccessionConsentSource&,const StableId&)>& source){
    std::set<StableId> pending;std::set<std::tuple<StableId,StableId,StableId,StableId>> sources;std::set<StableId> completed_plans;
    for(const auto& row:family.facts.rows()){
        const auto* plan=std::get_if<SuccessionConsentFact>(&row.second);const auto* done=std::get_if<VoluntarySuccessionFact>(&row.second);if(!plan&&!done)continue;
        const auto* head=common.state().facts.find(row.first);if(!family_id(row.first,common.state().next_instance_id)||!head||head->domain!="family")return common_failure("InvalidSuccessionPlanFact",row.first);
        if(plan){
            if(plan->kind!="family_succession_consent"||head->kind!=plan->kind)return common_failure("InvalidSuccessionPlanFact",row.first);const auto& a=plan->agreement;
            auto valid=validate_succession_personal_terms(a.personal);if(!valid.ok)return valid;
            if(a.request_id.empty()||a.session_id.empty()||a.accepted_tick<0||a.accepted_tick>common.tick()||a.accepted_revision<0||a.accepted_revision>common.revision())return common_failure("InvalidSuccessionPlanFact",row.first);
            if(source){valid=source(a,row.first);if(!valid.ok)return valid;}
            if(!sources.emplace(a.request_id,a.session_id,a.personal.predecessor_actor_id,a.personal.successor_actor_id).second)return common_failure("SuccessionConsentSourceConsumed",row.first);
            if(!plan->conclusion){if(!pending.insert(a.personal.predecessor_actor_id).second)return common_failure("MultiplePendingSuccessionPlans",row.first);continue;}
            const auto& end=*plan->conclusion;if(end.tick<a.accepted_tick||end.tick>common.tick())return common_failure("InvalidSuccessionPlanConclusion",row.first);
            if(end.kind=="cancelled"){if(end.succession_fact_id)return common_failure("InvalidSuccessionPlanConclusion",row.first);}
            else if(end.kind=="completed"){
                const auto* result=end.succession_fact_id?fact<VoluntarySuccessionFact>(family,*end.succession_fact_id):nullptr;
                if(!result||result->consent_fact_id!=row.first||result->tick!=end.tick||!precedes(common,row.first,*end.succession_fact_id))return common_failure("SuccessionPlanCompletionMismatch",row.first);
            }else return common_failure("InvalidSuccessionPlanConclusion",row.first);
        }else{
            if(done->kind!="family_voluntary_succession"||head->kind!=done->kind||!completed_plans.insert(done->consent_fact_id).second)return common_failure("InvalidVoluntarySuccessionFact",row.first);
            const auto* source=fact<SuccessionConsentFact>(family,done->consent_fact_id);if(!source||!source->conclusion||source->conclusion->kind!="completed"||source->conclusion->succession_fact_id!=row.first||source->conclusion->tick!=done->tick)return common_failure("SuccessionPlanCompletionMismatch",row.first);
            if(std::count(family.successions.begin(),family.successions.end(),row.first)!=1)return common_failure("VoluntarySuccessionIndexMismatch",row.first);
        }
    }
    for(const auto& row:common.state().facts.rows())if(row.second.domain=="family"&&(row.second.kind=="family_succession_consent"||row.second.kind=="family_voluntary_succession")){
        const auto* value=family.facts.find(row.first);if(!value||(row.second.kind=="family_succession_consent"?!std::holds_alternative<SuccessionConsentFact>(*value):!std::holds_alternative<VoluntarySuccessionFact>(*value)))return common_failure("MissingSuccessionPlanFact",row.first);
    }
    return common_success();
}
}
Status validate_voluntary_succession_structure(const CommonStore& common,const FamilyState& family){return structure(common,family,{});}
// 旧1..16仍沿原完整来源门；没有显式新路径读取时，不默许任何新增来源。
Status validate_voluntary_succession_facts(const CommonStore& common,const FamilyState& family,const CommonCatalog& shared,const InteractionCatalog* interaction,const SuccessionConsentCatalog* catalog,const FamilyLegacyState* legacy){
    if(catalog&&catalog->enabled){if(!interaction)return common_failure("SuccessionInteractionRequired");auto valid=validate_succession_consent_catalog(*catalog,shared,*interaction);if(!valid.ok)return valid;}
    return structure(common,family,[&](const SuccessionConsentSource& a,const StableId& id){
        if(!catalog||!catalog->enabled||!interaction)return common_failure("VoluntarySuccessionCapabilityRequired",id);
        auto valid=validate_succession_consent_source(common,family,shared,*interaction,*catalog,a);if(!valid.ok)return valid;
        if(!precedes(common,a.relation_fact_id,id))return common_failure("SuccessionConsentBeforeRelationship",id);
        auto historical=legacy?family_player_before_fact_with_legacy(common,family,*legacy,a.accepted_tick,id):family_player_before_fact(common,family,a.accepted_tick,id);
        if(!historical.ok)return Status{false,{},historical.error};
        return historical.value==a.personal.predecessor_actor_id?common_success():common_failure("SuccessionConsentHistoricalPlayerMismatch",id);
    });
}
// 真实节点与全部资产重报价后只记录一次同意来源；相同会话不能在撤销后偷偷重开。
Outcome<StableId> accept_succession_plan(CommonStore& common,FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const SuccessionConsentCatalog& catalog,const SuccessionConsentQuote& quote,const FamilyLegacyState* legacy){
    if(!interaction.common)return {false,{},common_failure("SuccessionConsentCapabilityRequired").error};auto valid=validate_voluntary_succession_facts(common,family,*interaction.common,&interaction,&catalog,legacy);if(!valid.ok)return {false,{},valid.error};valid=revalidate_succession_consent(common,family,state,interaction,access,catalog,quote);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:family.facts.rows())if(const auto* old=std::get_if<SuccessionConsentFact>(&row.second)){
        if(succession_consent_source_equal(old->agreement,quote.agreement))return old->conclusion?Outcome<StableId>{false,{},common_failure("SuccessionConsentSourceConsumed",row.first).error}:Outcome<StableId>{true,row.first,{}};
        if(!old->conclusion&&old->agreement.personal.predecessor_actor_id==quote.agreement.personal.predecessor_actor_id)return {false,{},common_failure("SuccessionPlanAlreadyPending",row.first).error};
    }
    auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};auto next=family;auto abort=[&](Status error)->Outcome<StableId>{auto rolled=common.rollback_savepoint(point.value);return {false,{},rolled.ok?error.error:rolled.error};};auto id=record_succession_consent_in_candidate(common,next,quote.agreement);if(!id.ok)return abort({false,{},id.error});valid=validate_voluntary_succession_facts(common,next,*interaction.common,&interaction,&catalog,legacy);if(!valid.ok)return abort(valid);valid=common.commit_savepoint(point.value);if(!valid.ok)return {false,{},valid.error};family=std::move(next);return {true,id.value,{}};
}
// 明确撤销不动资源；已完成计划不能倒退成未交割状态。
Status cancel_succession_plan(CommonStore& common,FamilyState& family,const StableId& actor,const StableId& plan_id){
    const auto* value=family.facts.find(plan_id);const auto* plan=value?std::get_if<SuccessionConsentFact>(value):nullptr;if(!plan)return common_failure("UnknownSuccessionPlan",plan_id);if(actor!=common.state().metadata.player_actor_id||actor!=plan->agreement.personal.predecessor_actor_id)return common_failure("SuccessionPlanAuthorityDenied",plan_id);if(plan->conclusion)return plan->conclusion->kind=="cancelled"?common_success():common_failure("SuccessionPlanAlreadyCompleted",plan_id);
    CancelledSuccessionPlanConstruction produced{SuccessionPlanCancellationBuildView{*plan,common.tick()}};
    // The existing table const-reference set owns the one persistent target
    // precharge. The local token covers only this distinct transient payload.
    // No borrowed child of the replaced original payload is used afterwards.
    family.facts.set(plan_id,static_cast<const FamilyFact&>(produced.value));return common_success();
}
// 未结计划必须仍属于当前玩家；实际批次已变即要求重谈，不变更已接受条款。
Outcome<SuccessionPersonalQuote> preview_accepted_succession_assets(const CommonStore& common,const FamilyState& family,const CommonCatalog& shared,const InteractionCatalog& interaction,const SuccessionConsentCatalog& catalog,const StableId& plan_id,const FamilyLegacyState* legacy){
    auto valid=validate_voluntary_succession_facts(common,family,shared,&interaction,&catalog,legacy);if(!valid.ok)return {false,{},valid.error};const auto* plan=fact<SuccessionConsentFact>(family,plan_id);if(!plan)return {false,{},common_failure("UnknownSuccessionPlan",plan_id).error};if(plan->conclusion)return {false,{},common_failure("SuccessionPlanAlreadyClosed",plan_id).error};const auto& accepted=plan->agreement;const auto* d=catalog.definitions.find(accepted.request_id);if(!d)return {false,{},common_failure("SuccessionConsentBindingMismatch",plan_id).error};auto quote=preview_succession_personal_assets(common,accepted.personal.predecessor_actor_id,accepted.personal.successor_actor_id,d->personal);if(!quote.ok)return quote;if(!succession_personal_terms_equal(accepted.personal,quote.value.terms))return {false,{},common_failure("SuccessionAcceptedAssetsChanged",plan_id).error};return quote;
}
// 完成仅提交已接受的资源recipe、控制权与本域事实；失败完整回滚Common及Family。
Outcome<StableId> complete_succession_plan(CommonStore& common,FamilyState& family,const CommonCatalog& shared,const InteractionCatalog& interaction,const SuccessionConsentCatalog& catalog,const StableId& plan_id,const SuccessionPersonalQuote& quote,const FamilyLegacyState* legacy){
    auto fresh=preview_accepted_succession_assets(common,family,shared,interaction,catalog,plan_id,legacy);if(!fresh.ok)return {false,{},fresh.error};if(!succession_personal_terms_equal(fresh.value.terms,quote.terms))return {false,{},common_failure("StaleSuccessionPersonalQuote").error};auto recipe=succession_personal_asset_recipe(common,quote);if(!recipe.ok)return {false,{},recipe.error};
    auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};auto next=family;auto abort=[&](Status error)->Outcome<StableId>{auto rolled=common.rollback_savepoint(point.value);return {false,{},rolled.ok?error.error:rolled.error};};auto id=execute_succession_plan_in_candidate(common,next,plan_id,quote);if(!id.ok)return abort({false,{},id.error});auto valid=validate_voluntary_succession_facts(common,next,shared,&interaction,&catalog,legacy);if(!valid.ok)return abort(valid);valid=common.commit_savepoint(point.value);if(!valid.ok)return {false,{},valid.error};family=std::move(next);return {true,id.value,{}};
}
// 统一读取既有与新式控制链；不会让普通同意计划提前改变当前玩家。
Outcome<FamilySuccessionStep> family_succession_step(const FamilyState& family,const StableId& id){
    if(const auto* legacy=fact<FamilyChoice>(family,id);legacy&&legacy->kind=="family_succession")return {true,{legacy->guardian_actor_id,legacy->heir_actor_id,legacy->tick},{}};
    const auto* completed=fact<VoluntarySuccessionFact>(family,id);const auto* plan=completed?fact<SuccessionConsentFact>(family,completed->consent_fact_id):nullptr;
    if(!completed||completed->kind!="family_voluntary_succession"||!plan||!plan->conclusion||plan->conclusion->kind!="completed"||plan->conclusion->succession_fact_id!=id||plan->conclusion->tick!=completed->tick)return {false,{},common_failure("InvalidSuccessionChain",id).error};return {true,{plan->agreement.personal.predecessor_actor_id,plan->agreement.personal.successor_actor_id,completed->tick},{}};
}
}
