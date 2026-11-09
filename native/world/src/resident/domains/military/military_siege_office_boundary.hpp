// 仅新围城生命周期借原军职历史前调用；旧军职读口行为不变，不能以实例号替换实际Common顺序。
#pragma once
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
struct MilitarySiegeOfficeSourceRef {StableId id,kind;};
// 所有原军职相关头均须同侧且同序：接受在开始时追加，完成tick仍由原军职折叠另核。
inline Status validate_military_siege_office_boundary(const CommonState& c,const StableId& boundary,const std::vector<MilitarySiegeOfficeSourceRef>& refs){
 auto sequence=[&](const StableId& id,const StableId& prefix)->Integer{if(id.rfind(prefix,0)!=0)return -1;const auto tail=id.substr(prefix.size());Integer n=0;const auto p=std::from_chars(tail.data(),tail.data()+tail.size(),n);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&n>0&&n<c.next_instance_id&&std::to_string(n)==tail?n:-1;};
 const auto& order=c.facts.order();const auto edge=std::find(order.begin(),order.end(),boundary);const auto* head=c.facts.find(boundary);const auto limit=sequence(boundary,"military_siege_lifecycle:");
 if(!head||head->id!=boundary||head->domain!="military"||limit<1||edge==order.end()||(head->kind!="military_siege_created"&&head->kind!="military_siege_joined"&&head->kind!="military_siege_strategic"&&head->kind!="military_siege_terminated"))return common_failure("MilitarySiegeOfficeBoundaryRequired",boundary);
 std::set<StableId> ids;std::vector<std::pair<Integer,std::size_t>> observed;
 for(const auto& ref:refs){const bool action=ref.kind=="military_action";if(!action&&ref.kind!="office_resignation"&&ref.kind!="military_occupation"&&ref.kind!="military_integration")return common_failure("MilitarySiegeOfficeSourceKind",ref.id);const auto n=sequence(ref.id,action?"action:":"military:");const auto* source=c.facts.find(ref.id);const auto at=std::find(order.begin(),order.end(),ref.id);
  if(n<1||n==limit||!ids.insert(ref.id).second||!source||source->id!=ref.id||source->kind!=ref.kind||source->domain!="military"||at==order.end())return common_failure("MilitarySiegeOfficeSourceRequired",ref.id);
  if((n<limit)!=(at<edge))return common_failure("MilitarySiegeOfficeSourceOrder",ref.id);observed.emplace_back(n,std::size_t(at-order.begin()));
 }
 std::sort(observed.begin(),observed.end());for(std::size_t i=1;i<observed.size();++i)if(observed[i-1].first==observed[i].first||observed[i-1].second>=observed[i].second)return common_failure("MilitarySiegeOfficeSourceOrder",boundary);
 return common_success();
}
// 原release-invalid目前只有三种具名来源；谈判truce是另一条真实strategic命令，不伪装为无源终止。
inline Status validate_military_siege_tracked_termination_reason(const StableId& reason){return reason=="responsible_actor_lost"||reason=="authority_lost"||reason=="cancelled"?common_success():common_failure("MilitarySiegeLifecycleTerminationSourceUnavailable",reason);}
}
