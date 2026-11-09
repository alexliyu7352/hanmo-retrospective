// 永久标签只引用已有权威源，热冷均重读原谱系；保存一个标签本身不会授予控制许可。
#include "biological_control_origin.hpp"
#include <charconv>
#include <tuple>
namespace hanmo::resident {namespace {
// 标签的持久字段取自已核来源，当前WorldIdentity留在原Legacy决定中而非重存漂移修订。
BiologicalControlOrigin freeze(const BiologicalChildOrigin& source){BiologicalControlOrigin out;out.kind=source.kind;out.parent_actor_id=source.parent_actor_id;out.child_actor_id=source.child_actor_id;out.original_catalog_actor_id=source.original_catalog_actor_id;out.birth_fact_id=source.birth_fact_id;out.creation_fact_id=source.creation_fact_id;out.source_available_tick=source.source_available_tick;return out;}
// 实例号只检分配范围；真正时间先后由原Common事实序与payload.tick共同决定。
bool legacy_boundary(const CommonStore& c,const StableId& id){const StableId prefix="family_legacy:";if(id.rfind(prefix,0)!=0)return false;const auto digits=id.substr(prefix.size());Integer n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);const auto* h=c.state().facts.find(id);return r.ec==std::errc{}&&r.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits&&h&&h->id==id&&h->domain=="family_legacy"&&(h->kind=="family_legacy_control_consent"||h->kind=="family_legacy_control_revocation");}
}
Outcome<BiologicalControlOrigin> make_biological_control_origin(const BiologicalLegacyBasisContext& x,const StableId& parent,const StableId& child){auto source=read_biological_child_origin(x,parent,child);if(!source.ok)return {false,{},source.error};if(!source.value)return {false,{},common_failure("BiologicalControlDirectParentRequired",child).error};return {true,freeze(*source.value),{}};}
Status validate_biological_control_origin_before_decision(const BiologicalLegacyBasisContext& x,const BiologicalControlOrigin& source,Integer tick,const StableId& boundary){if(!legacy_boundary(x.common,boundary))return common_failure("BiologicalControlDecisionBoundaryRequired",boundary);auto original=read_biological_child_origin_before_fact(x,source.parent_actor_id,source.child_actor_id,tick,boundary);if(!original.ok)return {false,{},original.error};if(!original.value)return common_failure("BiologicalControlDirectParentRequired",source.child_actor_id);return biological_control_origin_equal(source,freeze(*original.value))?common_success():common_failure("BiologicalControlOriginMismatch",boundary);}
bool biological_control_origin_equal(const BiologicalControlOrigin& a,const BiologicalControlOrigin& b){return std::tie(a.kind,a.parent_actor_id,a.child_actor_id,a.original_catalog_actor_id,a.birth_fact_id,a.creation_fact_id,a.source_available_tick)==std::tie(b.kind,b.parent_actor_id,b.child_actor_id,b.original_catalog_actor_id,b.birth_fact_id,b.creation_fact_id,b.source_available_tick);}
}
