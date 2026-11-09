// 显式新尾只保存原司法业务与真实玩家提交结果的永久对应；不改法律1表或Common格式。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "resident/model/common_model.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct PoliticalShopLicensingState;struct CodecVisitor;struct BoundaryValue;class CommonStore;
inline constexpr const char* political_shop_licensing_receipts_module="political_shop_licensing_receipts";
struct PoliticalShopLicensingPlayerResult {
    StableId source_fact_id,actor_id,command_kind,result_id;
    Integer tick=0,request_sequence=0,result_revision=0;
};
struct PoliticalShopLicensingReceiptState {
    Integer version=1;
    OrderedTable<PoliticalShopLicensingPlayerResult> bindings;
};
// 缺省不推测历史绑定；只有明确开启的新局/冷段才能选择严格回执读。
Status validate_political_shop_licensing_receipt_layout(
    const std::vector<std::pair<StableId,StableId>>&,bool enabled,
    const PoliticalShopLicensingReceiptState*);
Outcome<bool> decode_political_shop_licensing_receipt_profile(
    const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
// 只从可信原业务writer的同次前后候选取唯一新source；不可拿两个任意旧状态重造授权。
Outcome<StableId> political_shop_licensing_new_player_source(
    const PoliticalShopLicensingState& before,const PoliticalShopLicensingState& after,
    const StableId& actor,const StableId& kind,Integer tick);
// 仅commit_tail已真实保存result/history后调用；写入前完整检查，失败不留下半绑定。
Status bind_political_shop_licensing_player_result(
    const CommonStore&,const PoliticalShopLicensingState&,PoliticalShopLicensingReceiptState&,
    const StableId& source,const StableId& actor,Integer request,const CommitResult&,
    const std::function<bool(const CommitResult&)>& is_done);
// 原法律全史须先通过；本口再核每个永久绑定和每个保留缓存的精确业务来源。
Status validate_political_shop_licensing_player_results(
    const CommonStore&,const PoliticalShopLicensingState&,const PoliticalShopLicensingReceiptState&,
    const std::function<bool(const CommitResult&)>& is_done);
BoundaryValue encode_political_shop_licensing_receipts(const PoliticalShopLicensingReceiptState&);
Outcome<PoliticalShopLicensingReceiptState> decode_political_shop_licensing_receipts(const BoundaryValue&);
Status visit_political_shop_licensing_receipts_checkpoint(CodecVisitor&,PoliticalShopLicensingReceiptState&);
}
