// 新独立段唯一JSON/二进制形状；不修改旧PoliticalDismiss命令或Effects格式。
#pragma once
#include "political_dismissal_state.hpp"
namespace hanmo::resident {
Status validate_political_dismissal_state_layout(const PoliticalDismissalSourceState&);
Outcome<PoliticalDismissalSourceState> decode_political_dismissal_sources(const BoundaryValue&);
BoundaryValue encode_political_dismissal_sources(const PoliticalDismissalSourceState&);
Status visit_political_dismissal_sources(CodecVisitor&,PoliticalDismissalSourceState&);
Outcome<PoliticalDismissalSourceCatalog> decode_political_dismissal_source_catalog(const BoundaryValue&);
BoundaryValue encode_political_dismissal_source_catalog(const PoliticalDismissalSourceCatalog&);
}
