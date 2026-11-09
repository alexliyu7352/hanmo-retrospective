// Social 原状态只有一份；显式知识切点选择贯穿作者、目录及所有冷出口。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/social/social_time2_codec.hpp"
#include "resident/domains/social/social_relation_state_codec.hpp"
#include "resident/domains/social/social_time2_version.hpp"
namespace hanmo::resident {
// 缺省保原版本；该工程选择不添加渠道、知识或传播规则。
Outcome<bool> selected_native_social_knowledge_cut(const CatalogIdentity&);
Outcome<bool> selected_native_social_propagation_cut(const CatalogIdentity&);
Outcome<Integer> selected_native_original_social_version(const CatalogIdentity&);
Status append_native_social_source_modules(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_social_source_selection(const BoundaryValue& effective_data,const BoundaryValue& scenario,PreparedCatalog&,SocialCatalog& original_candidate);
Status validate_native_social_catalog_selection(const PreparedCatalog&);
Outcome<Integer> selected_native_dynamic_social_version(const PreparedCatalog&);
Status validate_native_social_state_selection(const SocialState&,const PreparedCatalog&);
Outcome<SocialState> decode_native_social_seed(const BoundaryValue&,const PreparedCatalog&);
Outcome<BoundaryValue> encode_native_social_state(const SocialState&,const PreparedCatalog&);
Status visit_native_social_checkpoint(CodecVisitor&,SocialState&,const PreparedCatalog&);
Status visit_native_dynamic_social_checkpoint(CodecVisitor&,DynamicSocialState&,const PreparedCatalog&);
class ProjectionWriter;
Status emit_native_social_state_fields(ProjectionWriter&,const SocialState&,const PreparedCatalog&);
}
