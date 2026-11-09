// 生身控制的同栈来源组合；持有S3工厂对象直到全部原报价/末验证完成，不复制另一份World。
#pragma once
#include "biological_control_commands.hpp"
#include "biological_control_receipts.hpp"
namespace hanmo::resident {
struct WorldBundle;class BornPersonalDialogueWorldAccess;
class BiologicalControlWorldAccess {
public:
    static Outcome<std::shared_ptr<BiologicalControlWorldAccess>> create(const WorldBundle&);
    BiologicalControlWorldAccess(const BiologicalControlWorldAccess&)=delete;
    BiologicalControlWorldAccess& operator=(const BiologicalControlWorldAccess&)=delete;
    const BiologicalControlDecisionContext& context()const{return context_;}
private:
    BiologicalControlWorldAccess(const WorldBundle&,std::shared_ptr<BornPersonalDialogueWorldAccess>);
    const WorldBundle& world_;
    std::shared_ptr<BornPersonalDialogueWorldAccess> born_;
    BiologicalControlDecisionContext context_;
};
Status validate_family_biological_world(const WorldBundle&);
Status bind_family_biological_result_world(WorldBundle&,const FamilyLegacyState* before,const BiologicalControlState* before_bindings,const StableId& actor,Integer request,const CommitResult& actual_result);
Outcome<LegacyControlDecisionQuote> preview_family_biological_world(const WorldBundle&,const StableId&,const BiologicalControlCommand&);
Outcome<FamilyEffect> apply_family_biological_world(WorldBundle&,const StableId&,const BiologicalControlCommand&,const LegacyControlDecisionQuote&);
Outcome<BoundaryValue> query_family_biological_world(const WorldBundle&,Integer page);
}
