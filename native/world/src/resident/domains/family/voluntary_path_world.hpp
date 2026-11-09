// 同World原关系、Known本人声明及唯一Family控制账；所有临时借用保存在同栈工厂对象。
#pragma once
#include "voluntary_path_views.hpp"
#include "voluntary_path_payloads.hpp"
namespace hanmo::resident {
struct WorldBundle;class KnownPersonalDialogueWorldAccess;
class VoluntaryPathWorldAccess {
public:
 static Outcome<std::shared_ptr<VoluntaryPathWorldAccess>> create(const WorldBundle&);
 VoluntaryPathWorldAccess(const VoluntaryPathWorldAccess&)=delete;
 VoluntaryPathWorldAccess& operator=(const VoluntaryPathWorldAccess&)=delete;
 const VoluntaryPathContext& context()const{return context_;}
private:
 VoluntaryPathWorldAccess(const WorldBundle&,std::shared_ptr<KnownPersonalDialogueWorldAccess>);
 const WorldBundle& world_;std::shared_ptr<KnownPersonalDialogueWorldAccess> meetings_;
 VoluntaryPathContext context_;
};
Status validate_family_voluntary_paths_profile(const WorldBundle&);
// 完整Known/出生/生命/师承/认可/Legacy之后，所有新历史本人消费者之前，禁止反调World总验。
Status validate_family_voluntary_paths_world(const WorldBundle&);
Status bind_family_voluntary_paths_result_world(WorldBundle&,const WorldIdentity& before_identity,
 const FamilyState* before_family,const VoluntaryPathState* before,const StableId& actor,Integer request,const CommitResult&);
Outcome<VoluntaryPathQuote> preview_family_voluntary_path_world(const WorldBundle&,const StableId&,const VoluntaryPathCommand&);
Outcome<FamilyEffect> apply_family_voluntary_path_world(WorldBundle&,const StableId&,const VoluntaryPathCommand&,const VoluntaryPathQuote&);
Outcome<FamilyQuote> preview_family_voluntary_original_world(const WorldBundle&,const StableId&,const FamilyCommand&);
Outcome<FamilyEffect> apply_family_voluntary_original_world(WorldBundle&,const StableId&,const FamilyCommand&,const FamilyQuote&);
Outcome<BoundaryValue> query_family_voluntary_paths_world(const WorldBundle&,Integer page);
}
