// G36明确入口保留既有G3/G4 codec签名与字节；未实现其它G+32组合不猜布局。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
Outcome<GrowthState> decode_growth_fixed_service_seed(const BoundaryValue&,bool regional_life=true,bool cards=false,bool teaching_requests=false,bool study_teaching_requests=false,bool innate_learning=false);
Outcome<BoundaryValue> encode_growth_fixed_service_state(const GrowthState&,bool regional_life=true,bool cards=false,bool teaching_requests=false,bool study_teaching_requests=false,bool innate_learning=false);
// 嵌入原Growth段，外层仍须finish、完整World验证及原子发布。
Status visit_growth_fixed_service_checkpoint(CodecVisitor&,GrowthState&,bool regional_life=true,bool cards=false);
// 完整G+32只从已经准入的GrowthCatalog分派；与两旧实验入口并存，不能靠payload猜布局。
Outcome<GrowthState> decode_growth_complete_fixed_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_complete_fixed_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_complete_fixed_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);
} // 无新世界封套或另一份固定边界布局。

namespace hanmo::resident {
// Explicit G+64=67..98. Does not alter the G/G+32 entrypoints above.
Outcome<GrowthState> decode_growth_exact_period_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_exact_period_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_exact_period_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);
}

namespace hanmo::resident {
Outcome<GrowthState> decode_growth_exact_command_cut_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_exact_command_cut_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_exact_command_cut_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);
Outcome<GrowthState> decode_growth_exact_rest_mouth_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_exact_rest_mouth_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_exact_rest_mouth_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);

}

namespace hanmo::resident {
// Explicit phase4/G+160 source-local capability predecessor tail.
Outcome<GrowthState> decode_growth_capability_chain_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_capability_chain_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_capability_chain_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);
}

namespace hanmo::resident {
// 显式 physical base+192，业务 phase4 不变；grant 持值表仍在独立 trade-grant/1。
Outcome<GrowthState> decode_growth_trade_grant_seed(const BoundaryValue&,const GrowthCatalog&);
Outcome<BoundaryValue> encode_growth_trade_grant_state(const GrowthState&,const GrowthCatalog&);
Status visit_growth_trade_grant_checkpoint(CodecVisitor&,GrowthState&,const GrowthCatalog&);
}
