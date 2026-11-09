#include "resident/domains/military/military_model.hpp"
// 人口军籍私有只读查询；不缓存名册分类，不替代两个领域的唯一权威。
#pragma once
#include "population_military.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident::population_military_detail {
// Only the original context clock may supply the authority observation.
class PopulationAuthorityClock final : public OrganizationAuthorityClock {
  const PopulationMilitaryReadContext &context_;

public:
  explicit PopulationAuthorityClock(const PopulationMilitaryReadContext &c)
      : context_(c) {}
  Outcome<WorldMoment> read_organization_authority_now() const override {
    if (context_.selected_version != 4 || !context_.actual_clock)
      return {false,
              {},
              common_failure("PopulationMilitaryExactSourcesRequired").error};
    return context_.actual_clock->read_current_moment();
  }
};
// 精确ID查找保持原表序，缺少引用即交给调用者拒绝。
template<class T> inline const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T> inline T* find(std::vector<T>& rows,const StableId& id){for(auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 成员身份比较只接受完整token，不按前缀猜人数。
template<class T> inline bool has(const std::vector<T>& rows,const T& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 重复或空ID不是合法来源集合。
inline bool unique(const std::vector<StableId>& rows){std::set<StableId> ids;for(const auto& id:rows)if(id.empty()||!ids.insert(id).second)return false;return true;}
// 当前分类从原军事名单读取；退籍是唯一不在当前军事名册的永久身份。
inline Outcome<PopulationSoldierStatus> actual_status(const MilitaryUnitRecord& u,const StableId& id){using S=PopulationSoldierStatus;Integer n=0;S status=S::released;for(const auto& row:{std::pair<const std::vector<StableId>*,S>{&u.fit,S::fit},{&u.wounded,S::wounded},{&u.dead,S::dead},{&u.captured,S::captured},{&u.deserted,S::deserted}})if(has(*row.first,id)){++n;status=row.second;}if(n>1||(n==1)!=has(u.member_ids,id))return {false,{},common_failure("PopulationMilitaryClassificationMismatch",id).error};return {true,status,{}};}
// 军事分类映射到作者显式群体；死亡没有伪造的存活目标群体。
inline StableId cohort(const PopulationMilitarySource& s,PopulationSoldierStatus status){using S=PopulationSoldierStatus;switch(status){case S::fit:return s.serving_cohort_id;case S::wounded:return s.wounded_cohort_id;case S::captured:return s.captive_cohort_id;case S::deserted:return s.displaced_cohort_id;case S::released:return s.reserve_cohort_id;case S::dead:return {}; }return {};}
// 已消费历史顺序只用于校验和增量对账，不保存第二份分类值。
inline PopulationSoldierStatus recorded_status(const PopulationMilitaryState& s,const StableId& id){auto status=PopulationSoldierStatus::released;for(const auto& fact:s.facts.rows())for(const auto& move:fact.second.movements)if(move.member_id==id)status=move.after;return status;}
// 原籍查询覆盖现役和退籍token，历史记录永不通过当前名册反推。
inline const PopulationSoldierOrigin* origin(const PopulationEnrollment& e,const StableId& id){for(const auto& member:e.members)if(member.member_id==id)return &member;return nullptr;}
// 请求只比较有限业务字段；冷字段顺序不改变授权语义。
inline bool same_request(const PopulationMilitaryCommand& a,const PopulationMilitaryCommand& b){if(a.index()!=b.index())return false;const auto authority=[](const ActingAuthority& x,const ActingAuthority& y){return x.organization_id==y.organization_id&&x.office_id==y.office_id&&x.authority_epoch==y.authority_epoch&&x.actor_id==y.actor_id;};if(auto* x=std::get_if<PopulationEnrollCommand>(&a)){const auto& y=std::get<PopulationEnrollCommand>(b);return authority(x->authority,y.authority)&&x->source_id==y.source_id&&x->count==y.count&&x->labor_count==y.labor_count&&x->release_allocation_ids==y.release_allocation_ids;}const auto& x=std::get<PopulationReleaseCommand>(a);const auto& y=std::get<PopulationReleaseCommand>(b);return authority(x.authority,y.authority)&&x.enrollment_id==y.enrollment_id;}
// 只有中央显式未提交半日上下文允许下一边界，冷准入默认严格当前tick。
inline Outcome<Integer> boundary_tick(const PopulationMilitaryReadContext& c){return read_population_military_boundary_tick(c);}
// 军职条款和宏观授权的核心身份冻结，候选版本不被当成永久权限。
inline bool same_terms(const PopulationMilitaryTerms &a,
                       const PopulationMilitaryTerms &b) {
  return same_authority_grant_binding(a.authority, b.authority) &&
         a.source_id == b.source_id && a.organization_id == b.organization_id &&
         a.unit_id == b.unit_id && a.garrison_id == b.garrison_id &&
         a.place_id == b.place_id &&
         a.household_group_id == b.household_group_id &&
         a.reserve_cohort_id == b.reserve_cohort_id &&
         a.serving_cohort_id == b.serving_cohort_id &&
         a.actor_id == b.actor_id &&
         a.military_office_id == b.military_office_id &&
         a.military_appointment_id == b.military_appointment_id &&
         a.authority.organization_id == b.authority.organization_id &&
         a.authority.office_id == b.authority.office_id &&
         a.authority.actor_id == b.authority.actor_id &&
         a.authority.authority_epoch == b.authority.authority_epoch &&
         a.authority.command_kind == b.authority.command_kind &&
         a.authority.scope.kind == b.authority.scope.kind &&
         a.authority.scope.id == b.authority.scope.id &&
         a.authority.amount == b.authority.amount && a.count == b.count &&
         a.labor_count == b.labor_count && a.member_ids == b.member_ids &&
         a.release_allocation_ids == b.release_allocation_ids &&
         a.enrollment_id == b.enrollment_id;
}
// 一份声明只能指向同一营地、组织、家户及五个互异群体。
Status source_valid(const PopulationMilitaryReadContext&,const PopulationMilitarySource&);
// 历史回执反查当时两份真实任职；当前失权不撤销已完成事实。
Status historical_authority(const PopulationMilitaryReadContext&,const PopulationMilitaryActionFact&);
} // 所有写入仍通过军事规则与人口owner入口完成。
