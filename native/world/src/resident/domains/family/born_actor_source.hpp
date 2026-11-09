// 原出生实例的具名纯来源，供私人会面模板绑定实际本人；不生成对白、许可或控制权。
#pragma once
#include "family_birth_lineage.hpp"
#include "actor_age_birth_read.hpp"
#include "actor_presence_read.hpp"
namespace hanmo::resident {
struct BornActorSource {
    WorldIdentity source_identity;
    StableId actor_id,birth_fact_id,creation_fact_id,prototype_id,before_fact_id;
    std::vector<StableId> biological_parent_ids;
    Integer birth_tick=0,observed_tick=0;
    bool adult=false,alive=false;
};
// 同候选完整Common/Family/Creation/Birth/Lineage及Military/Life来源先验；不反入World总验证。
// 原作者或旧Family创建者返回空；真实Birth源损坏必须报错，不能退回原型岗位人。
Outcome<std::optional<BornActorSource>> read_born_actor_source(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const FamilyDeathOriginRead&,const StableId& actor);
// 消费方先验自己的真实payload.tick；后来死亡不抹旧会面，同tick创建仍按Common原事实序。
Outcome<std::optional<BornActorSource>> read_born_actor_source_before_fact(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const FamilyDeathOriginRead&,const StableId& actor,Integer tick,const StableId& actual_before_fact);
// 唯一preclock历史借口：只读原已存在且tick≤from的业务头，允许看到本轮to真实死因。
// phase须from==Common.tick/to==from+1；Age/Lineage仍完整原from源，不准未来出生/当前新会面。
Outcome<std::optional<BornActorSource>> read_born_actor_source_before_fact(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const FamilyDeathOriginRead&,const StableId& actor,Integer tick,const StableId& actual_before_fact,const DynamicStepInput&);
using BornActorSourceRead=std::function<Outcome<std::optional<BornActorSource>>(const StableId& actor)>;
using BornActorSourceBeforeRead=std::function<Outcome<std::optional<BornActorSource>>(const StableId& actor,Integer tick,const StableId& actual_before_fact)>;
}
