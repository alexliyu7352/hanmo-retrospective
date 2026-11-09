// Field fixture 的唯一 M-PASSAGE/3 根与有限 catalog 选择；不启用 P2 后果。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/politics/treaty/field_group_passage_catalog.hpp"
#include "resident/domains/politics/treaty/political_passage_source.hpp"
#include "resident/domains/politics/treaty/safe_exit_entry.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct ExactTimeProfile;
Outcome<Integer> selected_native_field_passage_version(const CatalogIdentity &);
Status append_native_field_passage_modules(const BoundaryValue &,
                                           CatalogIdentity &,
                                           const ExactTimeProfile *);
Status prepare_native_field_passage_catalog(const BoundaryValue &,
                                            PreparedCatalog &);
Status validate_native_field_passage_catalog(const PreparedCatalog &);
Status validate_native_field_passage_layout(const WorldBundle &);
// 明选纯 Army 运输：真实民用登记/载具/人口绑定出现即拒，不造空商业来源。
Status validate_native_field_army_transport_roots(const WorldBundle &);
// 只核本有限 section 的值形状；完整历史必须另经同 World Group owner 反扫。
Status
validate_field_passage_checkpoint_shape(const PoliticalPassageDecisionState &,
                                        const SafeExitState &);
Status visit_field_passage_checkpoint(CodecVisitor &,
                                      PoliticalPassageDecisionState &,
                                      SafeExitState &);
Status emit_field_passage_projection(ProjectionWriter &, std::string_view,
                                     const PoliticalPassageDecisionState &,
                                     const SafeExitState &);
Status decode_field_passage_checkpoint(const BoundaryValue &,
                                       PoliticalPassageDecisionState &,
                                       SafeExitState &);
} // namespace hanmo::resident
