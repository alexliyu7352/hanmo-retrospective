#include "propagation_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/model/common_model.hpp"
#include <type_traits>
#include <utility>
namespace hanmo::resident {namespace {
CheckpointMemoryCharge reserve_adoption_copy(const SocialPropagationAdoption& value){
 return CheckpointMemoryCharge(0);
}
// Only the original SourceRef value mapping. Count/anchor/adoption permission
// remains the one Common prefix reader and the actual Social business owner.
template<class Fn>decltype(auto) original_source_text(const FactRecord& fact,Fn&& fn){return fn(fact.domain,fact.kind,fact.id);}
CheckpointMemoryCharge reserve_adoption_prefix(const CurrentCommonFactPrefix& prefix){

 return CheckpointMemoryCharge(0);
}
CommonActualFactPrefix copy_adoption_prefix(const CurrentCommonFactPrefix& prefix){
 CommonActualFactPrefix out;out.common_fact_count=prefix.common_fact_count;
 if(prefix.last_common_fact)out.last_common_fact=original_source_text(*prefix.last_common_fact,[](const auto&... text){return SourceRef{text...};});
 return out;
}
}
SocialPropagationAdoption::SocialPropagationAdoption(const WorldMoment& moment,const CurrentCommonFactPrefix& prefix)
 :copy_memory(reserve_adoption_prefix(prefix)),adopted_at(moment),original_prefix(copy_adoption_prefix(prefix)){}
SocialPropagationAdoption::SocialPropagationAdoption(const SocialPropagationAdoption& other)
 :copy_memory(reserve_adoption_copy(other)),adopted_at(other.adopted_at),original_prefix(other.original_prefix){}
SocialPropagationAdoption::SocialPropagationAdoption(SocialPropagationAdoption&& other):SocialPropagationAdoption(){swap(other);}
SocialPropagationAdoption& SocialPropagationAdoption::operator=(const SocialPropagationAdoption& other){if(this!=&other){SocialPropagationAdoption fresh(other);swap(fresh);}return *this;}
SocialPropagationAdoption& SocialPropagationAdoption::operator=(SocialPropagationAdoption&& other)noexcept{swap(other);return *this;}
void SocialPropagationAdoption::swap(SocialPropagationAdoption& other)noexcept{
 if(this==&other)return;
 static_assert(std::is_nothrow_swappable_v<std::optional<SourceRef>>);
 static_assert(std::is_nothrow_move_constructible_v<CommonActualFactPrefix>);
 static_assert(noexcept(adopted_at.sub_half_day.swap(other.adopted_at.sub_half_day)));
 using std::swap;copy_memory.swap(other.copy_memory);
 swap(adopted_at.half_day_tick,other.adopted_at.half_day_tick);
 adopted_at.sub_half_day.swap(other.adopted_at.sub_half_day);
 swap(original_prefix.common_fact_count,other.original_prefix.common_fact_count);
 original_prefix.last_common_fact.swap(other.original_prefix.last_common_fact);
}
}
