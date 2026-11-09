// 独立段只存原决定到精确经济草案的绑定；旧political_takeover/1无新增字节。
#pragma once
#include "politics_supply_contract.hpp"
#include "resident/domains/politics/takeover/politics_takeover_rules.hpp"
namespace hanmo::resident {
struct PoliticalSupplyAdoptionRecord {StableId decision_fact_id,policy_issue_fact_id;};
struct PoliticalSupplyAdoptionState {Integer version=1;OrderedTable<PoliticalSupplyAdoptionRecord> bindings;};
BoundaryValue encode_political_supply_adoptions(const PoliticalSupplyAdoptionState&);
Outcome<PoliticalSupplyAdoptionState> decode_political_supply_adoptions(const BoundaryValue&);
Status visit_political_supply_adoptions_checkpoint(CodecVisitor&,PoliticalSupplyAdoptionState&);
// 只核绑定形状与原政治/经济引用，不递归整个接管或经济验证。
Status validate_political_supply_adoption_bindings(const PoliticalTakeoverContext&);
Outcome<PoliticalResponsibilityDecision> derive_political_supply_decision(const PoliticalTakeoverContext&,
    const PoliticalTakeoverRegistration&,const PoliticalOriginalResponsibility&,const PoliticalResponsibilityRequest&,
    const std::optional<StableId>& before_approval_fact_id={});
class PoliticalSupplyAdoptionAccess final:public PoliticalSupplyAdoptionRead {
public:
    explicit PoliticalSupplyAdoptionAccess(PoliticalTakeoverContext context):context_(context){}
    Outcome<std::optional<PoliticalSupplyAdoptionSource>> political_supply_adoption(const StableId&)const override;
    Outcome<std::optional<PoliticalSupplyAdoptionSource>> political_supply_adoption_before_observation(const StableId&,const StableId&,Integer)const override;
private:PoliticalTakeoverContext context_;
};
}
