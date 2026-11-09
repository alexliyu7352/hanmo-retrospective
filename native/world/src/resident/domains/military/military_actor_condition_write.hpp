// 原军事组写后的有限绑定合同；服务只借当前候选，不建立保存点或直接改HP/疲劳。
#pragma once
#include "military_actor_condition_source.hpp"
namespace hanmo::resident {
class MilitaryActorConditionWrite {
public:
 virtual ~MilitaryActorConditionWrite()=default;
 // Supply为唯一刚落源；Land为本次step/terminate的源组；Naval等原头/载荷均落后才调用。
 // 必须核同Common实际capture未消费尾与每个原cause/规则；0效应不造条件账。
 // 多源必须整组预核后再逐项record，失败沿外层Common＋Military＋Naval全部回滚。
 virtual Status after_military_condition_sources(CommonWrite&,const MilitaryState&,
  const std::vector<StableId>& actual_source_ids)=0;
};
// Land在实际写前比较本次完整HP变化子序，精确source不能从同tick最新记录猜测。
Status validate_military_actor_condition_changes(const MilitaryState&,
 const std::vector<MilitaryActorChange>&,const std::vector<StableId>& actual_casualty_sources);
// callback为空沿原旧行为；非空失败原样传播，不静默忽略或补造来源。
Status finish_military_actor_condition_sources(CommonWrite&,const MilitaryState&,
 const MilitaryAccess&,const std::vector<StableId>& actual_source_ids);
}
