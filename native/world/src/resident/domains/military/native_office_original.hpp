#pragma once
#include "office_exact.hpp"
#include "military_authority_time.hpp"
#include "military_action_conditions.hpp"
#include "naval_asset_history.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
namespace hanmo::resident {
struct PersonalState;struct NavalState;struct NavalCatalog;struct TravelState;struct NavalHistoryAccess;
struct MilitaryFieldHistoryRead;struct MilitaryExactState;class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;
struct NativeOfficeNavalSources {
 const NavalState& state;const NavalCatalog& catalog;const TravelState& travel;
 const NavalHistoryAccess& history;ExactNavalAssetHistoryContext assets;
};
// All members are borrowed roots or copies of finite borrowed contexts. The
// enclosing World owner retains the body snapshot and all delegates. Neither
// this context nor its constructor performs whole validation or reads S0.
struct NativeOfficeOriginalContext {
 MilitaryAuthoritySourceContext authority;MilitaryActionConditionContext conditions;
 const MilitaryExactState* exact;const CatalogIdentity& identity;
 ActorSpatialHeadSourceReadContext spatial;
 const MilitaryFormationSourceRead& formations;const MilitaryReturnSourceRead& returns;
 const MilitarySpatialHandoffState* handoffs=nullptr;
 const PersonalState* personal=nullptr;const NativeOfficeNavalSources* naval=nullptr;
 Integer selected_personal=0,selected_personal_lifecycle=0,selected_naval=0,selected_land_lifecycle=0;
 ExactArithmeticBudget budget{};
 const MilitaryFieldHistoryRead* field_history=nullptr;
};
class NativeOfficeOriginalRead final:public ExactOfficeOriginalRead {
 NativeOfficeOriginalContext context_;
public:
 explicit NativeOfficeOriginalRead(NativeOfficeOriginalContext context):context_(context){}
 NativeOfficeOriginalRead(const NativeOfficeOriginalRead&)=delete;
 NativeOfficeOriginalRead& operator=(const NativeOfficeOriginalRead&)=delete;
 NativeOfficeOriginalRead(NativeOfficeOriginalRead&&)=delete;
 NativeOfficeOriginalRead& operator=(NativeOfficeOriginalRead&&)=delete;
 bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept override;
 Status validate_office_acceptance(const DynamicMilitaryActionFact2&)const override;
 Status validate_office_actual_service(const DynamicMilitaryActionFact2&,const MomentSpan&)const override;
 Status validate_office_completed_result(const DynamicMilitaryActionFact2&)const override;
};
}
