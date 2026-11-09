// 原产权交割的实际现金证据，只核同候选真执行，不替代双方同意或授予付款权。
#pragma once
#include "production_asset_lifecycle.hpp"
#include "resident/storage/resource_execution_draft.hpp"

namespace hanmo::resident {
class CommonStore;
Status validate_production_asset_transfer_cash_capture(
    const CommonStore &, const ProductionAssetTransferTerms &,
    const ProductionAssetTitleTransfer &, const ResourceExecutionDraft &,
    const CurrentMomentRead &);
} // namespace hanmo::resident
