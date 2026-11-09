// 生物与收养各自保唯一权源；合并仅为资格只读，绝不把养亲写成生物亲代。
#include "family_birth_kinship.hpp"
#include "born_character_source.hpp"
#include <algorithm>
#include <set>
#include <array>
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
Outcome<std::map<StableId,StableId>> read_birth_adoptive_parents(const CommonStore& c,const FamilyState& family,const StableId& before){
    const auto& order=c.state().facts.order();const auto limit=before.empty()?order.end():std::find(order.begin(),order.end(),before);
    if(!before.empty()&&limit==order.end())return {false,{},common_failure("BirthKinshipFactBoundaryRequired").error};
    std::map<StableId,StableId> parents;
    for(auto at=order.begin();at!=limit;++at){const auto* record=family.facts.find(*at);if(!record)continue;
        if(const auto* adoption=std::get_if<AdultAdoptionFact>(record))parents[adoption->agreement.adoptee_actor_id]=adoption->agreement.adopter_actor_id;
        if(const auto* creation=std::get_if<IdentityCreationFact>(record)){const auto* source=family.facts.find(creation->source);const auto* adoption=source?std::get_if<AdoptionFact>(source):nullptr;if(adoption&&adoption->success&&adoption->child_actor_id==creation->actor_id)parents[creation->actor_id]=adoption->guardian_actor_id;}
    }
    return {true,std::move(parents),{}};
}
namespace {
Status pair_from_original_origins(const CommonStore& c,const OrderedTable<BiologicalOrigin>& origins,const std::map<StableId,StableId>& adopted,const StableId& first,const StableId& second){
    if(first==second||!c.actor(first)||!c.actor(second))return common_failure("BirthKnownCloseRelation");
    std::map<StableId,std::set<StableId>> parents;
    for(const auto& row:origins.rows())parents[row.first].insert(row.second.parent_actor_ids.begin(),row.second.parent_actor_ids.end());
    for(const auto& row:adopted){if(!c.actor(row.first)||!c.actor(row.second))return common_failure("BirthKinshipIdentityUnavailable");parents[row.first].insert(row.second);}
    // 两个各自无环的谱系仍可能合成环，因此按真实有向亲代边额外核验。
    std::map<StableId,int> color;std::vector<StableId> nodes;for(const auto& row:parents)nodes.push_back(row.first);
    // 显式栈避免恶意长谱系耗尽C++调用栈；没有递归World或递归领域验证。
    for(const auto& root:nodes){std::vector<std::pair<StableId,bool>> stack{{root,false}};while(!stack.empty()){auto step=std::move(stack.back());stack.pop_back();auto& mark=color[step.first];if(step.second){mark=2;continue;}if(mark==2)continue;if(mark==1)return common_failure("BirthMixedKinshipCycle",step.first);mark=1;stack.emplace_back(step.first,true);for(const auto& parent:parents[step.first]){if(color[parent]==1)return common_failure("BirthMixedKinshipCycle",parent);if(color[parent]!=2)stack.emplace_back(parent,false);}}}
    auto ancestors=[&](const StableId& actor){std::set<StableId> out;std::vector<StableId> pending{actor};while(!pending.empty()){auto id=pending.back();pending.pop_back();for(const auto& parent:parents[id])if(out.insert(parent).second)pending.push_back(parent);}return out;};
    const auto a=ancestors(first),b=ancestors(second);if(a.count(second)||b.count(first))return common_failure("BirthKnownCloseRelation");
    for(const auto& parent:parents[first])if(parents[second].count(parent))return common_failure("BirthKnownCloseRelation");
    return common_success();
}
}
Status validate_birth_known_family_pair(const CommonStore& c,const CommonCatalog& common,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const std::map<StableId,StableId>& adopted,const StableId& first,const StableId& second){
 auto original=read_birth_lineage_projection(c,common,lineage,births);return original.ok?pair_from_original_origins(c,original.value.origins,adopted,first,second):Status{false,{},original.error};
}
Status validate_birth_known_family_pair4(const BornCharacterSourceContext& x,const std::map<StableId,StableId>& adopted,const StableId& first,const StableId& second){
 auto valid=validate_born_character_source_structure(x);if(!valid.ok)return valid;
 if(first==second||!x.common.actor(first)||!x.common.actor(second))return common_failure("BirthKnownCloseRelation");
 // Read the same graph rule using borrowed original IDs. No copied world/tree,
 // map nodes or owning string sets are allocated in the complete cold path.
 CheckpointVector<const StableId*> nodes;nodes.reserve(x.common.state().actors.size());for(const auto& actor:x.common.state().actors.rows())nodes.push_back(&actor.first);
 const auto find=[&](const StableId& id){for(std::size_t n=0;n<nodes.size();++n)if(*nodes[n]==id)return n;return nodes.size();};
 struct Parents{std::array<std::size_t,3> indices{};std::size_t count=0;};
 auto parents=[&](std::size_t n)->Outcome<Parents>{Parents out;auto add=[&](const StableId& id){const auto index=find(id);if(index==nodes.size())return false;for(std::size_t i=0;i<out.count;++i)if(out.indices[i]==index)return true;if(out.count==out.indices.size())return false;out.indices[out.count++]=index;return true;};
  if(const auto* original=x.lineage.origins.find(*nodes[n]))for(const auto& id:original->parent_actor_ids)if(!add(id))return {false,{},common_failure("BirthKinshipIdentityUnavailable",id).error};
  for(const auto& birth:x.births.births.rows())if(birth.second.actor_id==*nodes[n])for(const auto& id:birth.second.biological_parent_ids)if(!add(id))return {false,{},common_failure("BirthKinshipIdentityUnavailable",id).error};
  if(const auto selected=adopted.find(*nodes[n]);selected!=adopted.end())if(!add(selected->second))return {false,{},common_failure("BirthKinshipIdentityUnavailable",selected->second).error};return {true,out,{}};};
 for(const auto& item:adopted)if(find(item.first)==nodes.size()||find(item.second)==nodes.size())return common_failure("BirthKinshipIdentityUnavailable");
 CheckpointVector<unsigned char> color(nodes.size(),0);CheckpointVector<std::pair<std::size_t,bool>> stack;
 for(std::size_t root=0;root<nodes.size();++root){stack.emplace_back(root,false);while(!stack.empty()){const auto step=stack.back();stack.pop_back();auto& mark=color[step.first];if(step.second){mark=2;continue;}if(mark==2)continue;if(mark==1)return common_failure("BirthMixedKinshipCycle",*nodes[step.first]);mark=1;stack.emplace_back(step.first,true);auto p=parents(step.first);if(!p.ok)return {false,{},p.error};for(std::size_t i=0;i<p.value.count;++i){const auto parent=p.value.indices[i];if(color[parent]==1)return common_failure("BirthMixedKinshipCycle",*nodes[parent]);if(color[parent]!=2)stack.emplace_back(parent,false);}}}
 auto reaches=[&](std::size_t root,std::size_t target)->Outcome<bool>{CheckpointVector<unsigned char> seen(nodes.size(),0);CheckpointVector<std::size_t> pending;pending.push_back(root);while(!pending.empty()){const auto n=pending.back();pending.pop_back();auto p=parents(n);if(!p.ok)return {false,false,p.error};for(std::size_t i=0;i<p.value.count;++i){const auto parent=p.value.indices[i];if(parent==target)return {true,true,{}};if(!seen[parent]){seen[parent]=1;pending.push_back(parent);}}}return {true,false,{}};};
 const auto a=find(first),b=find(second);auto ab=reaches(a,b),ba=reaches(b,a);if(!ab.ok)return {false,{},ab.error};if(!ba.ok)return {false,{},ba.error};if(ab.value||ba.value)return common_failure("BirthKnownCloseRelation");auto pa=parents(a),pb=parents(b);if(!pa.ok)return {false,{},pa.error};if(!pb.ok)return {false,{},pb.error};for(std::size_t i=0;i<pa.value.count;++i)for(std::size_t j=0;j<pb.value.count;++j)if(pa.value.indices[i]==pb.value.indices[j])return common_failure("BirthKnownCloseRelation");return common_success();
}

}
