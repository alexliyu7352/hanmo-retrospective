// S3 原独立段的显式版本配对；只选择真实已编译叶，不从空历史升级。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PreparedCatalog;struct WorldBundle;struct InteractionState;struct BoundaryValue;
class CodecVisitor;class ProjectionWriter;
struct NativeInteractionSectionVersions {
 Integer messages=0,personal=0,inquiry=0,invitations=0,meetings=0,receipts=0,replies=0;
 bool exact_actions()const noexcept{return personal==2||inquiry==2||meetings==2;}
 bool exact_sources()const noexcept{return messages>=2||receipts==2||replies>=3||exact_actions();}
};
// 只读唯一 CatalogIdentity；无目录或实际状态推选。
Outcome<NativeInteractionSectionVersions> selected_native_interaction_versions(const CatalogIdentity&);
// Main physical5..13 is explicit in the same original interaction module.
// This binding check does not grant whole-profile admission.
Outcome<Integer> decode_native_interaction_response_selection(const BoundaryValue& scenario);
Status append_native_interaction_response_module(const BoundaryValue& scenario,CatalogIdentity&);
Outcome<Integer> selected_native_interaction_response_version(const CatalogIdentity&);
Outcome<Integer> native_interaction_physical_version(const PreparedCatalog&);
Status validate_native_interaction_response_layout(const PreparedCatalog&,const InteractionState&);
Outcome<InteractionState> decode_native_interaction_seed(const BoundaryValue&,const PreparedCatalog&);
Status visit_native_interaction_checkpoint(CodecVisitor&,InteractionState&,const PreparedCatalog&);
Status emit_native_interaction_state_fields(ProjectionWriter&,const InteractionState&,const PreparedCatalog&);
Status validate_native_interaction_catalog(const PreparedCatalog&);
// 原 fresh 构造入口；仅空初态可调用，restore 不补任何来源。
Status initialize_native_interaction_roots(const PreparedCatalog&,InteractionState&);
// 目录、模块与所有原状态版本逐项相同，完整业务验源仍由各原 owner 执行。
Status validate_native_interaction_layout(const PreparedCatalog&,const InteractionState&);
}
