// 接续方案逐项选择本人私钱和随身物资；不把生产产权、公产或家庭管理权当作随身资产。
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct SuccessionItemSelection { StableId item_id;Integer quantity=0;FieldOrder field_order; };
struct SuccessionPersonalSelection { Integer money=0;std::vector<SuccessionItemSelection> goods;FieldOrder field_order; };
struct SuccessionPersonalLot { StableId lot_id,item_id;Integer quantity=0;FieldOrder field_order; };
// 该永久条款只描述已同意交割的明确资产；未选部分仍归原人，不代表债务或职事承接。
struct SuccessionPersonalTerms {
    StableId predecessor_actor_id,successor_actor_id,source_wallet_id,destination_wallet_id;
    StableId source_inventory_id,destination_inventory_id;
    Integer money=0;
    std::vector<SuccessionPersonalLot> lots;
    FieldOrder field_order;
};
struct SuccessionPersonalQuote {
    WorldIdentity source_identity;
    SuccessionPersonalSelection selection;
    SuccessionPersonalTerms terms;
    Integer source_spendable=0,retained_spendable=0,destination_weight=0,transferred_weight=0;
    Integer source_balance=0,destination_balance=0,source_inventory_revision=0,destination_inventory_revision=0;
};
// 报价不授继任资格；调用者还须核双方接受、合法接续关系和稳定提交边界。
Outcome<SuccessionPersonalQuote> preview_succession_personal_assets(
    const CommonStore&,const StableId& predecessor,const StableId& successor,const SuccessionPersonalSelection&);
// 提交前完整重报价并逐批绑定；只返回共同资源recipe，由原世界事务统一执行与记事实。
Outcome<ResourceRecipe> succession_personal_asset_recipe(const CommonStore&,const SuccessionPersonalQuote&);
bool succession_personal_terms_equal(const SuccessionPersonalTerms&,const SuccessionPersonalTerms&);
class CodecVisitor;
struct BoundaryValue;
// 新接续入口在任何find/字段提取前核整棵有限JSON树的键唯一性；旧profile不自动套用。
Status validate_succession_json_unique(const BoundaryValue&);
// 只核条款结构与来源ID；不凭一份可解码条款授予资产或控制人物的权限。
Status validate_succession_personal_terms(const SuccessionPersonalTerms&);
Outcome<SuccessionPersonalSelection> decode_succession_personal_selection(const BoundaryValue&);
BoundaryValue encode_succession_personal_selection(const SuccessionPersonalSelection&);
Outcome<SuccessionPersonalTerms> decode_succession_personal_terms(const BoundaryValue&);
BoundaryValue encode_succession_personal_terms(const SuccessionPersonalTerms&);
// 精确保存所选原批次与字段顺序；坏档不能静默删字段或扩大数量。
Status visit_succession_personal_terms(CodecVisitor&,SuccessionPersonalTerms&);
}
