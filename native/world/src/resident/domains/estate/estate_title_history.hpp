// Original Estate legal title only. Beneficial shares and use permission remain
// their own accepted sources; owning this title does not grant either of them.
#pragma once
#include "estate_rest_adoption.hpp"
namespace hanmo::resident {
struct EstateRestHistoryContext;struct EstateState;
struct EstateAssetTitleView {
 StableId estate_id,inventory_id,wallet_id;
 Owner legal_owner;
 ScenarioOriginRef initial_origin;
 std::vector<SourceRef> original_title_applications;
};
class EstateAssetTitleRead {public:virtual ~EstateAssetTitleRead()=default;
 virtual Outcome<EstateAssetTitleView> read_current_estate_title(const StableId&)const=0;
 // Exclusive actual Common adoption point, including same-Moment ordering.
 virtual Outcome<EstateAssetTitleView> read_estate_title_before_source(const StableId&,const SourceRef&)const=0;
 // Actual title applications strictly after the already-adopted Common cut.
 virtual Outcome<std::vector<SourceRef>> read_estate_title_applications_after_source(const StableId&,const SourceRef&)const=0;
 virtual Outcome<std::vector<SourceRef>> read_estate_title_applications_after_adoption(const EstateRestAdoptionBasis&)const=0;
};
class NativeEstateAssetTitleRead final:public EstateAssetTitleRead {
 const EstateRestHistoryContext& x_;const EstateState& state_;
public:NativeEstateAssetTitleRead(const EstateRestHistoryContext&x,const EstateState&s):x_(x),state_(s){}
 Outcome<EstateAssetTitleView> read_current_estate_title(const StableId&)const override;
 Outcome<EstateAssetTitleView> read_estate_title_before_source(const StableId&,const SourceRef&)const override;
 Outcome<std::vector<SourceRef>> read_estate_title_applications_after_source(const StableId&,const SourceRef&)const override;
 Outcome<std::vector<SourceRef>> read_estate_title_applications_after_adoption(const EstateRestAdoptionBasis&)const override;
};
}
