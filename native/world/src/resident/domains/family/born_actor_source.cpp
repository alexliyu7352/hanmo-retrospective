// 新机独立重建出生本人来源；原b135实现丢失保持记录，不把本实现伪称原字节恢复。
#include "born_actor_source.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {namespace {
// 先核真实存在/生死，再按实际出生账取得父母与原型；今日人物出现本身不作来源。
Outcome<std::optional<BornActorSource>> read(const CommonStore& c,const CommonCatalog& catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const FamilyDeathOriginRead& deaths,const StableId& actor,Integer tick,const StableId& boundary,bool historical,const DynamicStepInput* phase=nullptr){
    using Result=Outcome<std::optional<BornActorSource>>;
    auto fail=[&](const StableId& code)->Result{return {false,{},common_failure(code,actor).error};};
    if(phase&&(!historical||c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1||tick<0||tick>c.tick()))return fail("BornActorHistoricalPhaseRequired");
    auto present=historical?read_actor_presence_before_fact(c,catalog,deaths,actor,tick,boundary,phase):read_actor_presence(c,catalog,deaths,actor);
    if(!present.ok)return {false,{},present.error};
    const auto* created=c.state().facts.find(actor);
    if(std::find(catalog.actor_ids.begin(),catalog.actor_ids.end(),actor)!=catalog.actor_ids.end()||(created&&created->domain=="family"))return {true,std::nullopt,{}};
    if(!catalog.family_birth_identity_enabled||!lineage.enabled)return fail("BornActorProfileRequired");
    if(!created||created->kind!="actor_creation"||created->domain!="family_birth")return fail("BornActorCreationRequired");
    auto age=historical?read_actor_age_before_fact(c,catalog,births,actor,tick,boundary):read_actor_age(c,catalog,births,actor);
    if(!age.ok)return {false,{},age.error};
    auto projection=read_birth_lineage_projection(c,catalog,lineage,births);if(!projection.ok)return {false,{},projection.error};
    const auto* birth_id=projection.value.birth_sources.find(actor);const auto* parentage=projection.value.origins.find(actor);
    const auto* birth=birth_id?births.births.find(*birth_id):nullptr;const auto* person=c.state().actors.find(actor);
    if(!birth||!parentage||!person||birth->actor_id!=actor||parentage->actor_id!=actor||birth->biological_parent_ids.size()!=2||parentage->parent_actor_ids!=birth->biological_parent_ids||birth->prototype_id.empty()||person->prototype_id!=birth->prototype_id||birth->tick!=age.value.birth_tick||present.value.creation_fact_id!=actor)return fail("BornActorSourceMismatch");
    BornActorSource out;out.source_identity=c.state().identity;out.actor_id=actor;out.birth_fact_id=*birth_id;out.creation_fact_id=actor;out.prototype_id=birth->prototype_id;out.before_fact_id=boundary;
    out.biological_parent_ids=birth->biological_parent_ids;out.birth_tick=birth->tick;out.observed_tick=tick;out.adult=age.value.adult;out.alive=present.value.alive;
    return {true,std::move(out),{}};
}
}
Outcome<std::optional<BornActorSource>> read_born_actor_source(const CommonStore& c,const CommonCatalog& catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const FamilyDeathOriginRead& deaths,const StableId& actor){return read(c,catalog,lineage,births,deaths,actor,c.tick(),{},false);}
Outcome<std::optional<BornActorSource>> read_born_actor_source_before_fact(const CommonStore& c,const CommonCatalog& catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const FamilyDeathOriginRead& deaths,const StableId& actor,Integer tick,const StableId& boundary){return read(c,catalog,lineage,births,deaths,actor,tick,boundary,true);}
Outcome<std::optional<BornActorSource>> read_born_actor_source_before_fact(const CommonStore& c,const CommonCatalog& catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const FamilyDeathOriginRead& deaths,const StableId& actor,Integer tick,const StableId& boundary,const DynamicStepInput& phase){return read(c,catalog,lineage,births,deaths,actor,tick,boundary,true,&phase);}
// 明确两种函数型，防增加具名重载后声明与实际定义参数静默不一致。
static_assert(std::is_pointer_v<decltype(&read_born_actor_source)>);
using Before=Outcome<std::optional<BornActorSource>>(*)(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const FamilyDeathOriginRead&,const StableId&,Integer,const StableId&);
using BeforePhase=Outcome<std::optional<BornActorSource>>(*)(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const FamilyDeathOriginRead&,const StableId&,Integer,const StableId&,const DynamicStepInput&);
static_assert(std::is_same_v<Before,decltype(static_cast<Before>(&read_born_actor_source_before_fact))>);
static_assert(std::is_same_v<BeforePhase,decltype(static_cast<BeforePhase>(&read_born_actor_source_before_fact))>);
}
