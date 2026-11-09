// 弟子政策的独立结果尾只绑定原 legacy 事实与真实请求；不存第二份许可或控制链。
#pragma once
#include "disciple_control_sources.hpp"
#include "family_legacy.hpp"
namespace hanmo::resident {
struct DiscipleControlCommitBinding {
    StableId result_id,actor_id,kind;
    Integer request_seq=0,revision=0;
    FieldOrder field_order;
};
struct DiscipleControlState {
    OrderedTable<DiscipleControlCommitBinding> commits;
    FieldOrder field_order;
};
Status validate_disciple_control_shape(const DiscipleControlState&);
// 原 legacy 全史与所选师承来源须另外先验；本门不递归领域历史。
Status validate_disciple_control_result_bindings(const CommonStore&,const FamilyLegacyState&,const DiscipleControlCatalog&,const DiscipleControlState&);
// 只绑定本请求真正新增的原 legacy 来源；before 必须是提交前原件，实际缓存对象必须同一份。
Status bind_disciple_control_result_in_candidate(CommonStore&,const FamilyLegacyState& before,const FamilyLegacyState& after,const DiscipleControlCatalog&,const DiscipleControlState& before_bindings,DiscipleControlState&,const StableId& actor,Integer request_seq,const CommitResult& actual_result,FamilyLegacyVersion source_version=FamilyLegacyVersion::original1);
Outcome<StableId> read_disciple_control_source_for_result(const CommonStore&,const FamilyLegacyState&,const DiscipleControlCatalog&,const DiscipleControlState&,const CommitResult& actual_result);
Outcome<DiscipleControlState> decode_disciple_control_state(const BoundaryValue&);
BoundaryValue encode_disciple_control_state(const DiscipleControlState&);
Status visit_disciple_control_checkpoint(CodecVisitor&,DiscipleControlState&);
}
