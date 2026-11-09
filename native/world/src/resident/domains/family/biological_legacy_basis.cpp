// 亲生接续首先证明原亲代与唯一身份，再核原死因之前的真实存在，不创建任何继任事实。
#include "biological_legacy_basis.hpp"
#include "family_continuation_people4.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {namespace {
// 完整出生模式不能缺本账退回只看作者；旧模式若出现出生头也不能悄悄略过。
Status context(const BiologicalLegacyBasisContext& x){
    if(!x.lineage.enabled)return common_failure("BiologicalLegacyLineageRequired");
    if(x.complete_people&&(!x.life||!x.complete_people->bound_to(x.common,*x.life)))return common_failure("FamilyContinuationCandidateMismatch",{},ErrorKind::internal);
    if(x.catalog.family_birth_identity_enabled&&!x.births)return common_failure("BiologicalLegacyBirthStateRequired");
    if(x.births&&!x.catalog.family_birth_identity_enabled&&!x.births->births.empty())return common_failure("BiologicalLegacyBirthProfileRequired");
    if(!x.births)for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="family_birth"&&row.second.kind=="family_birth_recorded")return common_failure("BiologicalLegacyBirthStateRequired");
    return common_success();
}
// 同一投影的选择只核直接亲代；未知父母保持未知，不能因目录未列另一人推导反证。
Outcome<std::optional<BiologicalChildOrigin>> origin(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child,Integer tick,const StableId& before,bool historical){
    using Result=Outcome<std::optional<BiologicalChildOrigin>>;auto fail=[](const StableId& code)->Result{return {false,{},common_failure(code).error};};
    auto valid=context(x);if(!valid.ok)return {false,{},valid.error};if(parent.empty()||child.empty()||parent==child)return fail("BiologicalLegacyPartiesRequired");
    if(tick<0||tick>x.common.tick()||(!historical&&(tick!=x.common.tick()||!before.empty())))return fail("BiologicalLegacyBoundaryInvalid");
    // 当时存在性不等于当时活着；关系可在父母亡故后继续，来源本身也不授死亡知情。
    for(const auto* actor:{&parent,&child}){const auto* h=historical?x.common.state().facts.find(before):nullptr;if(historical&&!h)return fail("BiologicalLegacyBoundaryInvalid");auto present=x.complete_people?(historical?x.complete_people->read_presence_before_original_fact(*actor,tick,{h->domain,h->kind,before}):x.complete_people->read_current_presence(*actor)):(historical?read_actor_presence_before_fact(x.common,x.catalog,x.death_origin,*actor,tick,before):read_actor_presence(x.common,x.catalog,x.death_origin,*actor));if(!present.ok)return {false,{},present.error};}
    BirthLineageProjection projection;
    if(x.births){auto read=read_birth_lineage_projection(x.common,x.catalog,x.lineage,*x.births);if(!read.ok)return {false,{},read.error};projection=std::move(read.value);}
    else{valid=validate_family_lineage_catalog(x.common,x.catalog,x.lineage);if(!valid.ok)return {false,{},valid.error};projection.origins=x.lineage.origins;}
    const auto* parents=projection.origins.find(child);if(!parents||std::find(parents->parent_actor_ids.begin(),parents->parent_actor_ids.end(),parent)==parents->parent_actor_ids.end())return {true,std::nullopt,{}};
    BiologicalChildOrigin out;out.source_identity=x.common.state().identity;out.parent_actor_id=parent;out.child_actor_id=child;out.before_fact_id=before;out.observed_tick=tick;
    const auto* birth_id=projection.birth_sources.find(child);
    if(!birth_id){if(std::find(x.catalog.actor_ids.begin(),x.catalog.actor_ids.end(),child)==x.catalog.actor_ids.end()||!x.lineage.origins.find(child))return fail("BiologicalLegacyOriginalLineageRequired");out.original_catalog_actor_id=child;return {true,std::move(out),{}};}
    const auto* birth=x.births->births.find(*birth_id);const auto* person=x.common.state().actors.find(child);const auto& facts=x.common.state().facts.order();const auto at=std::find(facts.begin(),facts.end(),*birth_id),created=std::find(facts.begin(),facts.end(),child),boundary=historical?std::find(facts.begin(),facts.end(),before):facts.end();
    if(!birth||!person||birth->actor_id!=child||birth->tick>tick||person->birth_tick!=birth->tick||person->created_tick!=birth->tick||at==facts.end()||created==facts.end()||at>=created||(historical&&(boundary==facts.end()||created>=boundary)))return fail("BiologicalLegacyBirthBoundaryMismatch");
    out.kind=BiologicalChildOriginKind::recorded_birth;out.birth_fact_id=*birth_id;out.creation_fact_id=child;out.source_available_tick=birth->tick;return {true,std::move(out),{}};
}
// 原登记证明稳定死亡，原casualty才是关系截止点；生后/死后声明不得倒填到较晚登记之前。
Outcome<BiologicalLegacyBasis> basis(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child,Integer tick,const StableId& before,bool historical,const CommonActualFactPrefix* prefix=nullptr){
    auto fail=[](const StableId& code)->Outcome<BiologicalLegacyBasis>{return {false,{},common_failure(code).error};};auto valid=context(x);if(!valid.ok)return {false,{},valid.error};if(x.complete_people){
        if(prefix){auto cut=read_actual_common_fact_prefix(x.common,*prefix);if(!cut.ok)return {false,{},cut.error};}const auto* h=historical&&!prefix?x.common.state().facts.find(before):nullptr;if(historical&&!prefix&&!h)return fail("BiologicalLegacyBoundaryInvalid");
        auto death=prefix?x.complete_people->life().read_actor_death_at_original_prefix(parent,tick,*prefix):historical?x.complete_people->life().read_actor_death_before_original_fact(parent,tick,{h->domain,h->kind,before}):x.complete_people->life().read_actor_death(parent);if(!death.ok)return {false,{},death.error};if(!death.value)return fail("BiologicalLegacyDeathRequired");const auto& died=*death.value;
        auto relation=origin(x,parent,child,died.effective_at.exact_moment.half_day_tick,died.cause_ref.id,true);if(!relation.ok)return {false,{},relation.error};if(!relation.value)return fail("BiologicalLegacyParentageRequired");
        auto presence=prefix?x.complete_people->read_presence_at_original_prefix(child,tick,*prefix):historical?x.complete_people->read_presence_before_original_fact(child,tick,{h->domain,h->kind,before}):x.complete_people->read_current_presence(child);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return fail("BiologicalLegacyLivingSuccessorRequired");
        auto age=prefix?x.complete_people->read_age_at_original_prefix(child,tick,*prefix):historical?x.complete_people->read_age_before_original_fact(child,tick,{h->domain,h->kind,before}):x.complete_people->read_current_age(child);if(!age.ok)return {false,{},age.error};if(age.value.age_years<18)return fail("BiologicalLegacyAdultRequired");
        BiologicalLegacyBasis out;out.relation=std::move(*relation.value);out.death_fact_id=died.death_ref.id;if(died.cause_kind=="military_casualty")out.casualty_fact_id=died.cause_ref.id;out.before_fact_id=before;out.death_tick=died.effective_at.exact_moment.half_day_tick;out.successor_birth_tick=x.common.state().actors.find(child)->birth_tick;out.observed_tick=tick;out.verified_death=died;return {true,std::move(out),{}};
    }
    if(prefix)return fail("BiologicalLegacyCompletePrefixReaderRequired");
    if(!x.life||!x.death_origin)return fail("BiologicalLegacyDeathSourceRequired");
    auto death=historical?read_actor_death_before_fact(x.common,*x.life,parent,tick,before):read_actor_death(x.common,*x.life,parent);if(!death.ok)return {false,{},death.error};if(!death.value)return fail("BiologicalLegacyDeathRequired");const auto& died=*death.value;const auto* original=x.life->deaths.find(died.death_fact_id);if(!original)return fail("BiologicalLegacyDeathSourceRequired");valid=validate_family_death_record(x.common,*original,x.death_origin);if(!valid.ok)return {false,{},valid.error};
    auto relation=origin(x,parent,child,died.death_tick,died.cause_fact_id,true);if(!relation.ok)return {false,{},relation.error};if(!relation.value)return fail("BiologicalLegacyParentageRequired");
    auto presence=historical?read_actor_presence_before_fact(x.common,x.catalog,x.death_origin,child,tick,before):read_actor_presence(x.common,x.catalog,x.death_origin,child);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return fail("BiologicalLegacyLivingSuccessorRequired");
    auto age=x.births?(historical?read_actor_age_before_fact(x.common,x.catalog,*x.births,child,tick,before):read_actor_age(x.common,x.catalog,*x.births,child)):(historical?read_actor_age_before_fact(x.common,x.catalog,child,tick,before):read_actor_age(x.common,x.catalog,child));if(!age.ok)return {false,{},age.error};if(!age.value.adult)return fail("BiologicalLegacyAdultRequired");
    return {true,{std::move(*relation.value),died.death_fact_id,died.cause_fact_id,before,died.death_tick,age.value.birth_tick,tick},{}};
}
}
Outcome<std::optional<BiologicalChildOrigin>> read_biological_child_origin(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child){return origin(x,parent,child,x.common.tick(),{},false);}
Outcome<std::optional<BiologicalChildOrigin>> read_biological_child_origin_before_fact(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child,Integer tick,const StableId& before){return origin(x,parent,child,tick,before,true);}
Outcome<BiologicalLegacyBasis> read_biological_legacy_basis(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child){return basis(x,parent,child,x.common.tick(),{},false);}
Outcome<BiologicalLegacyBasis> read_biological_legacy_basis_before_fact(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child,Integer tick,const StableId& before){return basis(x,parent,child,tick,before,true);}
Outcome<BiologicalLegacyBasis> read_biological_legacy_basis_at_original_prefix(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child,Integer tick,const CommonActualFactPrefix& prefix){return basis(x,parent,child,tick,{},true,&prefix);}
// 对外每个入口唯一声明/定义，禁止默认参数误造重载而只过语法、链接时才缺符号。
static_assert(std::is_pointer_v<decltype(&read_biological_child_origin)>);
static_assert(std::is_pointer_v<decltype(&read_biological_child_origin_before_fact)>);
static_assert(std::is_pointer_v<decltype(&read_biological_legacy_basis)>);
static_assert(std::is_pointer_v<decltype(&read_biological_legacy_basis_before_fact)>);
}
