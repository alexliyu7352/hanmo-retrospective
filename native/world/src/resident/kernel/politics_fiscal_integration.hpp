// 财政薄叶显式收同一候选对象；中央负责字段、variant和能力登记。
#pragma once
#include "resident/domains/politics/politics_fiscal.hpp"
#include <functional>
namespace hanmo::resident {
struct PoliticalFiscalReadContext {
    const CommonStore& common;const CommonCatalog& common_catalog;const PoliticsState& politics;
    const PoliticalFiscalState& fiscal;const PoliticalFiscalCatalog& fiscal_catalog;
    const OrganizationState& organization;const OrganizationCatalog& organization_catalog;const ActivityRead& activity;
};
// 政策命令只改变共同事实与财政历史，Office/钱粮仍从原域借用。
struct PoliticalFiscalWriteContext {
    CommonStore& common;const CommonCatalog& common_catalog;const PoliticsState& politics;
    PoliticalFiscalState& fiscal;const PoliticalFiscalCatalog& fiscal_catalog;
    const OrganizationState& organization;const OrganizationCatalog& organization_catalog;const ActivityRead& activity;
    PoliticalFiscalReadContext read()const;
};
using PoliticalFiscalReceiptReader=std::function<const PoliticalFiscalEffects*(const CommitResult&)>;
Outcome<PoliticalFiscalQuote> preview_political_fiscal_business(const PoliticalFiscalReadContext&,const StableId& actor,const PoliticalSetTaxCommand&);
// apply只供中央完整候选或NPC子事务使用；失败由调用方回滚Common和领域写集。
Outcome<PoliticalFiscalEffects> apply_political_fiscal_business(PoliticalFiscalWriteContext&,const StableId& actor,const PoliticalSetTaxCommand&,const PoliticalFiscalQuote&);
Status validate_political_fiscal_business(const PoliticalFiscalReadContext&,bool enabled,const PoliticalFiscalReceiptReader&);
// 原正文已由真 Fiscal2 context 全验后，仍逐原缓存/历史/分配区间检查。
Status validate_political_fiscal_business_receipts(const PoliticalFiscalReadContext&,bool enabled,const PoliticalFiscalReceiptReader&);
Status validate_political_fiscal_receipt(const PoliticalFiscalReadContext&,const StableId& actor,Integer tick,const PoliticalFiscalEffects&);
}
