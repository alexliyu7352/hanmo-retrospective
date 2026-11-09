// 亲生政策的独立结果尾只绑定原 legacy 事实与真实请求；不存第二份许可或控制链。
#pragma once
#include "biological_control_sources.hpp"
#include "family_legacy.hpp"
namespace hanmo::resident {
struct BiologicalControlCommitBinding {
    StableId result_id,actor_id,kind;
    Integer request_seq=0,revision=0;
    FieldOrder field_order;
};
struct BiologicalControlState {
    OrderedTable<BiologicalControlCommitBinding> commits;
    FieldOrder field_order;
};
Status validate_biological_control_receipt(const FamilyLegacyState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
Status validate_biological_control_shape(const BiologicalControlState&);
// 原 legacy 全史与所选亲生关系及本人声明来源须另外先验；本门不递归领域历史。
Status validate_biological_control_result_bindings(const CommonStore&,const FamilyLegacyState&,const BiologicalControlCatalog&,const BiologicalControlState&);
// 只绑定本请求真正新增的原 legacy 来源；before 必须是提交前原件，实际缓存对象必须同一份。
Status bind_biological_control_result_in_candidate(CommonStore&,const FamilyLegacyState& before,const FamilyLegacyState& after,const BiologicalControlCatalog&,const BiologicalControlState& before_bindings,BiologicalControlState&,const StableId& actor,Integer request_seq,const CommitResult& actual_result,FamilyLegacyVersion source_version=FamilyLegacyVersion::biological2);
Outcome<StableId> read_biological_control_source_for_result(const CommonStore&,const FamilyLegacyState&,const BiologicalControlCatalog&,const BiologicalControlState&,const CommitResult& actual_result);
Outcome<BiologicalControlState> decode_biological_control_state(const BoundaryValue&);
BoundaryValue encode_biological_control_state(const BiologicalControlState&);
Status visit_biological_control_checkpoint(CodecVisitor&,BiologicalControlState&);
}
