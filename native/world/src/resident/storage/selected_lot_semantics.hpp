// Common只管真实lot；S9在同candidate纯核独特实例/性质及已接受的终结来源。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
enum class SelectedLotUse { reserve,transfer,consume };
class SelectedLotSemanticRead {
public:
 virtual ~SelectedLotSemanticRead()=default;
 virtual Status validate_live_lot(const CommonRead&,const StableId& inventory_id,const LotView&) const=0;
 virtual Status validate_selected_use(const CommonRead&,const StableId& inventory_id,const LotView&,Integer quantity,SelectedLotUse) const=0;
 virtual Status validate_materialized_lot(const CommonRead&,const StableId& inventory_id,const LotView&) const=0;
};
}
