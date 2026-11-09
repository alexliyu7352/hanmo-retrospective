// 原writer与preview共用唯一只读前置；不产生可复用许可或持久中间结果。
#pragma once
#include "foundation_asset_quote.hpp"
#include "foundation_asset_read.hpp"
namespace hanmo::resident::foundation_asset_command_preflight {
// requested只借调用者实参，调用者必须保活；不返回函数内部临时请求地址。
Outcome<FoundationPrivateAssetQuoteRead>
offer(const FoundationAssetReadContext &, const StableId &actual_actor,
      const FoundationPrivateAssetRequest &, const ReadStamp &,
      const ReadStampAuthority &, const DomainReadVersionSource &);
// 本人表达、同版意向冻结、真实获知与未重复同意，沿原顺序只读。
Outcome<WorldMoment>
consent(const FoundationAssetReadContext &, const StableId &actual_actor,
        const SourceRef &original_offer, const MomentOwnerSource &actual_intent,
        Integer expected_terms_revision,
        const MomentOwnerSource &actual_content, const ReadStamp &,
        const ReadStampAuthority &, const DomainReadVersionSource &);
} // namespace hanmo::resident::foundation_asset_command_preflight
