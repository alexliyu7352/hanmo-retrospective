// 旁支控制只借同一World的真实Known会面与原唯一Legacy账；工厂拥有所有被回调借用的对象。
#pragma once
#include "collateral_control_commands.hpp"
#include "collateral_control_payloads.hpp"
#include "collateral_recognition_receipts.hpp"
namespace hanmo::resident {
struct WorldBundle;class KnownPersonalDialogueWorldAccess;
class CollateralControlWorldAccess {
public:
 static Outcome<std::shared_ptr<CollateralControlWorldAccess>> create(const WorldBundle&);
 CollateralControlWorldAccess(const CollateralControlWorldAccess&)=delete;
 CollateralControlWorldAccess& operator=(const CollateralControlWorldAccess&)=delete;
 const CollateralRecognitionContext& recognition()const{return recognition_;}
 const CollateralControlDecisionContext& context()const{return context_;}
private:
 CollateralControlWorldAccess(const WorldBundle&,std::shared_ptr<KnownPersonalDialogueWorldAccess>);
 const WorldBundle& world_;
 std::shared_ptr<KnownPersonalDialogueWorldAccess> meetings_;
 CollateralRecognitionContext recognition_;
 CollateralControlDecisionContext context_;
};
// Known及完整身份/生死后先验认可，再完整Legacy3；二者均不反入World或全Legacy递归。
Status validate_family_collateral_recognition_world(const WorldBundle&);
Status validate_family_collateral_world(const WorldBundle&);
Status bind_family_collateral_result_world(WorldBundle&,const FamilyLegacyState* before_legacy,const CollateralControlState* before,const StableId& actor,Integer request,const CommitResult&);
Outcome<CollateralControlQuote> preview_family_collateral_world(const WorldBundle&,const StableId&,const CollateralControlCommand&);
Outcome<FamilyEffect> apply_family_collateral_world(WorldBundle&,const StableId&,const CollateralControlCommand&,const CollateralControlQuote&);
Outcome<BoundaryValue> query_family_collateral_world(const WorldBundle&,Integer page);
}
