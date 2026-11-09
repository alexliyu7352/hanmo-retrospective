// 复用唯一出生谱系及具名直接亲代读；不写第二份关系、控制或资产账。
#include "collateral_family_source.hpp"
#include <algorithm>
#include <map>
#include <type_traits>
namespace hanmo::resident {namespace {
struct PathIndex {
    std::map<StableId,std::size_t> depth;
    std::map<StableId,StableId> descendant;
};
// 原图先验无环；广度优先保最短真实路径，同长度按稳定ID排序，不靠今日亲密度选亲属。
PathIndex index(const OrderedTable<BiologicalOrigin>& origins,const StableId& actor){
    PathIndex out;out.depth.emplace(actor,0);std::vector<StableId> pending{actor};
    for(std::size_t i=0;i<pending.size();++i){const auto child=pending[i];const auto* row=origins.find(child);if(!row)continue;auto parents=row->parent_actor_ids;std::sort(parents.begin(),parents.end());for(const auto& parent:parents)if(out.depth.emplace(parent,out.depth.at(child)+1).second){out.descendant.emplace(parent,child);pending.push_back(parent);}}
    return out;
}
Outcome<OrderedTable<BiologicalOrigin>> origins(const BiologicalLegacyBasisContext& x){
    if(!x.lineage.enabled)return {false,{},common_failure("CollateralLineageRequired").error};
    if(x.catalog.family_birth_identity_enabled&&!x.births)return {false,{},common_failure("CollateralBirthStateRequired").error};
    if(x.births){if(!x.catalog.family_birth_identity_enabled&&!x.births->births.empty())return {false,{},common_failure("CollateralBirthProfileRequired").error};auto all=read_birth_lineage_projection(x.common,x.catalog,x.lineage,*x.births);if(!all.ok)return {false,{},all.error};return {true,std::move(all.value.origins),{}};}
    for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="family_birth"&&row.second.kind=="family_birth_recorded")return {false,{},common_failure("CollateralBirthStateRequired").error};
    auto valid=validate_family_lineage_catalog(x.common,x.catalog,x.lineage);return valid.ok?Outcome<OrderedTable<BiologicalOrigin>>{true,x.lineage.origins,{}}:Outcome<OrderedTable<BiologicalOrigin>>{false,{},valid.error};
}
Outcome<std::vector<BiologicalChildOrigin>> path(const BiologicalLegacyBasisContext& x,const PathIndex& tree,const StableId& ancestor,const StableId& actor,Integer tick,const StableId& before){
    std::vector<BiologicalChildOrigin> out;auto parent=ancestor;
    while(parent!=actor){const auto next=tree.descendant.find(parent);if(next==tree.descendant.end())return {false,{},common_failure("CollateralParentPathIncomplete",parent).error};auto edge=before.empty()?read_biological_child_origin(x,parent,next->second):read_biological_child_origin_before_fact(x,parent,next->second,tick,before);if(!edge.ok)return {false,{},edge.error};if(!edge.value)return {false,{},common_failure("CollateralParentSourceRequired",next->second).error};out.push_back(std::move(*edge.value));parent=next->second;}
    return {true,std::move(out),{}};
}
Outcome<std::optional<CollateralFamilySource>> read(const BiologicalLegacyBasisContext& x,const StableId& first,const StableId& second,Integer tick,const StableId& before,bool historical){
    if(first.empty()||second.empty()||first==second)return {false,{},common_failure("CollateralDistinctPartiesRequired").error};
    if(tick<0||tick>x.common.tick()||historical==before.empty()||(!historical&&tick!=x.common.tick()))return {false,{},common_failure("CollateralActualBoundaryRequired").error};
    // 当时存在与当时在世是两种资格；祖先以后死亡不消除已经成立的亲代来源。
    for(const auto* actor:{&first,&second}){auto present=historical?read_actor_presence_before_fact(x.common,x.catalog,x.death_origin,*actor,tick,before):read_actor_presence(x.common,x.catalog,x.death_origin,*actor);if(!present.ok)return {false,{},present.error};}
    auto graph=origins(x);if(!graph.ok)return {false,{},graph.error};const auto a=index(graph.value,first),b=index(graph.value,second);
    if(a.depth.count(second)||b.depth.count(first))return {true,std::nullopt,{}};
    StableId ancestor;std::size_t length=0;
    for(const auto& row:a.depth){const auto other=b.depth.find(row.first);if(other==b.depth.end())continue;const auto total=row.second+other->second;if(ancestor.empty()||total<length){ancestor=row.first;length=total;}}
    if(ancestor.empty())return {true,std::nullopt,{}};
    auto left=path(x,a,ancestor,first,tick,before);if(!left.ok)return {false,{},left.error};auto right=path(x,b,ancestor,second,tick,before);if(!right.ok)return {false,{},right.error};
    return {true,CollateralFamilySource{x.common.state().identity,first,second,ancestor,before,tick,std::move(left.value),std::move(right.value)},{}};
}
}
Outcome<std::optional<CollateralFamilySource>> read_collateral_family_source(const BiologicalLegacyBasisContext& x,const StableId& a,const StableId& b){return read(x,a,b,x.common.tick(),{},false);}
Outcome<std::optional<CollateralFamilySource>> read_collateral_family_source_before_fact(const BiologicalLegacyBasisContext& x,const StableId& a,const StableId& b,Integer tick,const StableId& before){return read(x,a,b,tick,before,true);}
static_assert(std::is_pointer_v<decltype(&read_collateral_family_source)>);
static_assert(std::is_pointer_v<decltype(&read_collateral_family_source_before_fact)>);
}
