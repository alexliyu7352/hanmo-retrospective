// 建造仍使用共同人物占用和通用行动头；本域只拥有冻结条款和永久开工/完工事实。
#include "resident/storage/money_transfer_recipe.hpp"
#include "estate_model.hpp"
#include <limits>
#include <tuple>
#include <charconv>
namespace hanmo::resident {
namespace {
// 设施名只允许作者原有三个入口，非法名称不能默认为任一设施。
Integer* level(EstateFacilities& f,const StableId& name){if(name=="farm")return &f.farm;if(name=="granary")return &f.granary;if(name=="palisade")return &f.palisade;return nullptr;}
// 旧所有人仅要求仍是真人；继任发生后历史成本与开工依据继续保留。
Status valid_terms(const CommonStore& c,const EstateCatalog& catalog,const EstateBuildTerms& t){
    auto* d=catalog.estates.find(t.estate_id);if(!d||!c.actor(t.owner_actor_id))return common_failure("InvalidEstateBuildTerms");EstateRecord e;e.id=t.estate_id;e.owner_actor_id=t.owner_actor_id;auto* previous=level(e.facilities,t.facility);
    if(!previous||t.from_level<0||t.from_level>=d->construction.max_level)return common_failure("InvalidEstateBuildTerms");*previous=t.from_level;
    return same_estate_build_terms(t,estate_build_terms(*d,e,t.facility))?common_success():common_failure("InvalidEstateBuildTerms");
}
// 完工允许当前建设行动和本人管理会话，其余伤病、离开和占用会留下失败事实。
bool builder_ready(const CommonStore& c,const ActivityRead& activity,const ActionCoreView& a,const EstateBuildTerms& t){
    auto* actor=c.state().actors.find(a.actor_id);if(!actor||!actor->alive||!actor->available||actor->injury>0||(!actor->action_id.empty()&&actor->action_id!=a.id))return false;auto location=c.resolved_location(a.actor_id);auto* at=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    return at&&at->place_id==t.place_id&&!activity.in_open_battle(a.actor_id)&&!activity.in_interaction(a.actor_id,a.actor_id);
}
}
// 每升一级按目标等级放大真实工期与钱料，不在预览时改变任何余额。
EstateBuildTerms estate_build_terms(const EstateDefinition& d,const EstateRecord& e,const StableId& facility){
    auto f=e.facilities;auto* previous=level(f,facility);Integer from=previous?*previous:0,target=from+1;
    return {e.id,facility,from,target,d.construction.build_ticks*target,d.construction.money_cost*target,d.construction.material_item_id,d.construction.material_cost*target,d.wallet_id,d.construction.payee_wallet_id,d.inventory_id,e.owner_actor_id,d.place_id,{}};
}
// 字段顺序不参与业务条款相等，但所有钱粮来源和地点都必须匹配。
bool same_estate_build_terms(const EstateBuildTerms& a,const EstateBuildTerms& b){
    return std::tie(a.estate_id,a.facility,a.from_level,a.to_level,a.duration,a.money_cost,a.material_item_id,a.material_quantity,a.payer_wallet_id,a.payee_wallet_id,a.inventory_id,a.owner_actor_id,a.place_id)==std::tie(b.estate_id,b.facility,b.from_level,b.to_level,b.duration,b.money_cost,b.material_item_id,b.material_quantity,b.payer_wallet_id,b.payee_wallet_id,b.inventory_id,b.owner_actor_id,b.place_id);
}
// 开始复核必须发生在中央建立行动头并占用人物之后；扣料和支付使用同一资源recipe。
Status begin_estate_build(CommonStore& c,EstateState& s,const EstateCatalog& catalog,const CommonCatalog& common,const ActivityRead& activity,const ActionCoreView& action,const EstateQuote& quote,const EstateAccess* access){
    if(action.kind!="estate_build"||!std::holds_alternative<EstateBuild>(quote.command)||c.state().facts.find(action.id)||s.actions.find(action.id))return common_failure("InvalidEstateBuildAction");
    auto current=preview_estate(c,s,catalog,common,activity,action.actor_id,quote.command,access,action.id);if(!current.ok)return {false,{},current.error};
    if(!quote.terms||!current.value.terms||!same_estate_build_terms(*quote.terms,*current.value.terms)||quote.duration!=current.value.duration||quote.participants!=current.value.participants||quote.cash||quote.transfer||!quote.actor_ids.empty())return common_failure("StaleEstateQuote");
    auto* stored=c.action(action.id);auto* person=c.actor(action.actor_id);
    if(!stored||stored->kind!=action.kind||stored->actor_id!=action.actor_id||stored->participants!=action.participants||stored->start_tick!=action.start_tick||stored->remaining_ticks!=action.remaining_ticks||action.participants!=std::vector<StableId>{action.actor_id}||!person||person->action_id!=action.id||action.start_tick!=c.tick()||action.remaining_ticks!=current.value.duration)return common_failure("InvalidEstateBuildOccupation");
    auto terms=*current.value.terms;if(action.start_tick>std::numeric_limits<Integer>::max()-terms.duration)return common_failure("InvalidEstateBuildTime");Owner owner{Owner::Kind::actor,terms.owner_actor_id,{}};
    ResourceRecipe recipe;recipe.operations.push_back(ConsumeLots{terms.inventory_id,terms.material_item_id,terms.material_quantity,{owner},{}});auto paid=resource_recipe::append_money_transfer(recipe,terms.payer_wallet_id,terms.payee_wallet_id,terms.money_cost,resource_recipe::MoneyTransferPolicy::spendable);if(!paid.ok)return paid;paid=c.apply_resources(recipe);if(!paid.ok)return paid;
    auto added=c.append_fact({action.id,"estate_construction","estate"});if(!added.ok)return added;
    EstateConstructionFact fact;fact.actor_id=action.actor_id;fact.start_tick=c.tick();fact.terms=terms;s.facts.insert(action.id,std::move(fact));s.actions.insert(action.id,{std::move(terms),{}});return common_success();
}
// 存档与到期处理共享只读校验，完成重放不得再次升级或收费。
Status validate_estate_action(const CommonStore& c,const EstateState& s,const EstateCatalog& catalog,const ActionCoreView& action,bool finishing){
    if(action.kind!="estate_build")return common_failure("InvalidEstateBuildAction");auto* payload=s.actions.find(action.id);if(!payload)return common_failure("InvalidEstateBuildTerms");auto valid=valid_terms(c,catalog,payload->terms);if(!valid.ok)return valid;
    auto* row=s.facts.find(action.id);auto* fact=row?std::get_if<EstateConstructionFact>(row):nullptr;auto& terms=payload->terms;
    if(!fact||fact->kind!="estate_construction"||fact->actor_id!=action.actor_id||fact->start_tick!=action.start_tick||!same_estate_build_terms(fact->terms,terms))return common_failure("EstateBuildFactMismatch");
    auto* person=c.actor(action.actor_id);if(action.participants!=std::vector<StableId>{action.actor_id}||!person)return common_failure("InvalidEstateBuildOccupation");
    if(action.start_tick<0||action.start_tick>std::numeric_limits<Integer>::max()-terms.duration)return common_failure("InvalidEstateBuildTime");
    if(fact->phase=="completed"){if(!finishing||fact->completed_tick!=action.start_tick+terms.duration||action.remaining_ticks!=0)return common_failure("InvalidEstateBuildTime");return common_success();}
    if(fact->phase!="started"||fact->completed_tick!=-1||fact->success||person->action_id!=action.id)return common_failure("InvalidEstateBuildOccupation");
    if(action.start_tick>c.tick()||action.remaining_ticks<(finishing?0:1)||action.remaining_ticks>terms.duration||(c.tick()==std::numeric_limits<Integer>::max()&&finishing))return common_failure("InvalidEstateBuildTime");
    if(action.start_tick+terms.duration-action.remaining_ticks!=c.tick()+(finishing?1:0))return common_failure("InvalidEstateBuildTime");return common_success();
}
// 永久建设事实使用action实例号；进行中事实必须反向找到同一行动，完成后仍检查原工期。
Status validate_estate_construction_fact(const CommonStore& c,const EstateState& s,const EstateCatalog& catalog,const StableId& id,const EstateConstructionFact& f){
    if(id.rfind("action:",0)!=0)return common_failure("InvalidEstateFactId",id);auto suffix=id.substr(7);Integer n=0;auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=suffix.data()+suffix.size()||std::to_string(n)!=suffix||n<1||n>=c.state().next_instance_id)return common_failure("InvalidEstateFactId",id);
    auto terms=valid_terms(c,catalog,f.terms);if(!terms.ok)return common_failure("InvalidEstateConstructionFact",id);if(f.kind!="estate_construction"||!c.actor(f.actor_id)||f.start_tick<0||f.start_tick>c.tick()||f.start_tick>std::numeric_limits<Integer>::max()-f.terms.duration)return common_failure("InvalidEstateConstructionFact",id);
    if(f.phase=="started"){auto* action=c.action(id);if(f.completed_tick!=-1||f.success||!action)return common_failure("InvalidEstateConstructionFact",id);return validate_estate_action(c,s,catalog,*action);}
    if(f.phase!="completed"||f.completed_tick!=f.start_tick+f.terms.duration||f.completed_tick>c.tick())return common_failure("InvalidEstateConstructionFact",id);return common_success();
}
// 生产先结算，建设到期随后升级；失败仍保留已经支出的真实成本。
Outcome<EstateBuildResult> finish_estate_build(CommonStore& c,EstateState& s,const EstateCatalog& catalog,const ActivityRead& activity,const ActionCoreView& action){
    auto valid=validate_estate_action(c,s,catalog,action,true);if(!valid.ok)return {false,{},valid.error};auto& fact=std::get<EstateConstructionFact>(*s.facts.find(action.id));
    if(fact.phase=="completed")return {true,{fact.success,true,{}},{}};auto& terms=s.actions.find(action.id)->terms;
    if(action.remaining_ticks!=0||c.tick()+1!=action.start_tick+terms.duration)return {false,{},common_failure("EstateBuildNotReady").error};auto* estate=s.estates.find(terms.estate_id);auto* d=catalog.estates.find(terms.estate_id);if(!estate||!d)return {false,{},common_failure("UnknownEstate").error};auto* facility=level(estate->facilities,terms.facility);
    bool success=estate->owner_actor_id==terms.owner_actor_id&&facility&&*facility==terms.from_level&&builder_ready(c,activity,action,terms);
    if(success){auto capacity=d->base_capacity+(terms.facility=="granary"?terms.to_level:estate->facilities.granary)*d->granary_capacity_per_level;auto* inventory=c.inventory(terms.inventory_id);if(!inventory)return {false,{},common_failure("UnknownInventory",terms.inventory_id).error};if(inventory->capacity!=capacity){auto changed=c.apply_resources({{SetInventoryCapacity{terms.inventory_id,capacity},AdjustLots{terms.inventory_id,{},true}},{}});if(!changed.ok)return {false,{},changed.error};}*facility=terms.to_level;}
    fact.phase="completed";fact.completed_tick=c.tick()+1;fact.success=success;return {true,{success,false,success?StableId{}:StableId{"EstateBuildInterrupted"}},{}};
}
// 中央释放共同占用后只删短命payload，永久建设事实不能一并抹去。
Status erase_estate_action_payload(EstateState& s,const StableId& id){s.actions.erase(id);return common_success();}
} // 外围事务承担失败回滚；本领域不复制世界、不独立推进时钟。
