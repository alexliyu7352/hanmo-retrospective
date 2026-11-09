// 可选的真实辖区控制来源；普通政治状态不能靠改一个字段宣称占领。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PoliticalTerritoryControl {
    WorldIdentity source_identity;
    StableId jurisdiction_id,controlling_organization_id,occupation_fact_id;
    Integer revision=0;
};
class PoliticalTerritoryRead {
public:
    virtual ~PoliticalTerritoryRead()=default;
    virtual Outcome<std::optional<PoliticalTerritoryControl>> jurisdiction_control(const StableId&) const=0;
    virtual Outcome<Integer> jurisdiction_revision_before_fact(const StableId& jurisdiction_id,
        const StableId& fact_id,Integer fact_tick) const=0;
};
}
