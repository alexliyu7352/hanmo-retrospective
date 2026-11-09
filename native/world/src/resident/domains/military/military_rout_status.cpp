// started/blocked不是战法到期轮，不能被原每轮expired清理或等待士气自动解除。
#include "military_rout_status.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {constexpr const char* started="military_rout_started_v1";constexpr const char* blocked="military_rout_blocked_v1";}
const MilitaryRoutRetreatRule* military_rout_rule_in_use(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog){if(!battle.rout_retreat_enabled||!catalog.rout_retreat_enabled)return nullptr;for(const auto& board:catalog.battlefields)if(board.id==battle.battlefield_id)return board.rout_retreat?&*board.rout_retreat:nullptr;return nullptr;}
bool military_rout_status_key(const StableId& key){return key==started||key==blocked;}
bool military_unit_routing(const BattleUnit& unit){if(unit.withdrawn||unit.fit.empty()||unit.morale!=0)return false;for(const auto& s:unit.statuses)if(s.first==started&&s.second>0)return true;return false;}
Outcome<std::vector<MilitaryRoutProgress>> read_military_rout_progress(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog){
 const auto* rule=military_rout_rule_in_use(battle,catalog);std::vector<MilitaryRoutProgress> result;std::set<StableId> units;
 for(const auto& u:battle.units){if(!units.insert(u.unit_id).second)return {false,{},common_failure("DuplicateMilitaryUnit",u.unit_id).error};std::optional<Integer> first,wait;
  for(const auto& s:u.statuses){auto* target=s.first==started?&first:s.first==blocked?&wait:nullptr;if(!target)continue;if(*target)return {false,{},common_failure("MilitaryRoutProgressRepeated",u.unit_id).error};*target=s.second;}
  if(!first&&!wait){if(rule&&!u.withdrawn&&!u.fit.empty()&&u.morale==0)return {false,{},common_failure("MilitaryRoutProgressMissing",u.unit_id).error};continue;}
  if(!rule||!first||!wait||*first<1||*first>battle.round||*wait<0||*wait>rule->blocked_rounds_before_capture||!military_unit_routing(u))return {false,{},common_failure("MilitaryRoutProgressSourceMismatch",u.unit_id).error};result.push_back({u.unit_id,*first,*wait});
 }
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.unit_id<b.unit_id;});return {true,std::move(result),{}};
}
Status write_military_rout_progress(MilitaryBattleRecord& battle,const std::vector<MilitaryRoutProgress>& progress){
 std::map<StableId,MilitaryRoutProgress> rows;for(const auto& p:progress)if(p.unit_id.empty()||p.started_round<1||p.started_round>battle.round||p.blocked_rounds<0||p.blocked_rounds>2||!rows.emplace(p.unit_id,p).second)return common_failure("MilitaryRoutProgressSourceMismatch",p.unit_id);
 // 先全量核已存在单位，避免错误调用把未知进度静默丢弃。
 for(const auto& row:rows){const BattleUnit* unit=nullptr;for(const auto& u:battle.units)if(u.unit_id==row.first)unit=&u;if(!unit||unit->withdrawn||unit->fit.empty()||unit->morale!=0)return common_failure("MilitaryRoutProgressSourceMismatch",row.first);}
 for(auto& u:battle.units){u.statuses.erase(std::remove_if(u.statuses.begin(),u.statuses.end(),[](const auto& s){return military_rout_status_key(s.first);}),u.statuses.end());const auto p=rows.find(u.unit_id);if(p!=rows.end()){u.statuses.emplace_back(started,p->second.started_round);u.statuses.emplace_back(blocked,p->second.blocked_rounds);}}
 return common_success();
}
}
