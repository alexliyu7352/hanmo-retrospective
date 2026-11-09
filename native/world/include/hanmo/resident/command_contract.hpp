// 命令结果的共同外壳；领域报价/明细保持typed，缓存不再定义另一份近似回执。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PreparedDomainOp; struct QuoteDetails; struct ReceiptDetails;
// 此对象仅为当前读取点的准备结果；submit按原顺序重核，不把旧preview当新权限。
struct PreviewResult {
    WorldIdentity source;
    StableId actor_id,kind;
    std::shared_ptr<const PreparedDomainOp> operation;
    std::shared_ptr<const QuoteDetails> quote;
};
// 只存成功命令的原完整回执。nullptr details精确代表旧空字典，不代表丢弃未知字段。
struct CommitResult {
    bool ok=true;
    StableId kind,result_id;
    Integer tick=0,revision=0;
    std::shared_ptr<const ReceiptDetails> details;
    FieldOrder field_order{};
};
} // A存储不可变回执指针；中央组合领域明细，D仅编解码和深拥有DTO转换。
