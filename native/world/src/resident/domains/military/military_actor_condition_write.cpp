// 精确因果组只承接原writer已经决定的顺序；不根据人物当前数值选择或推造历史。
#include "military_actor_condition_write.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
Status validate_military_actor_condition_changes(const MilitaryState& s,const std::vector<MilitaryActorChange>& changes,const std::vector<StableId>& sources){using Key=std::tuple<StableId,StableId,StableId,StableId,std::optional<StableId>>;std::vector<Key> actual,expected;auto append=[](const MilitaryActorChange& x,std::vector<Key>& out){if(x.status=="dead"||x.status=="wounded")out.emplace_back(x.actor_id,x.status,x.army_id,x.captor_army_id,x.garrison_id);};for(const auto& x:changes)append(x,actual);std::set<StableId> ids;for(const auto& id:sources){if(id.empty()||!ids.insert(id).second)return common_failure("MilitaryActorConditionGroupSourceRequired",id);const MilitaryCasualtyFact* found=nullptr;for(const auto& f:s.casualty_facts)if(f.id==id){if(found)return common_failure("MilitaryActorConditionGroupSourceRequired",id);found=&f;}if(!found)return common_failure("MilitaryActorConditionGroupSourceRequired",id);for(const auto& x:found->actor_changes)append(x,expected);}return actual==expected?common_success():common_failure("MilitaryActorConditionGroupMismatch");}
Status finish_military_actor_condition_sources(CommonWrite& c,const MilitaryState& s,const MilitaryAccess& a,const std::vector<StableId>& ids){if(!a.actor_conditions||ids.empty())return common_success();return a.actor_conditions->after_military_condition_sources(c,s,ids);}
}
