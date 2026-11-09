// S1 最终三段的唯一作者/目录选择；旧Travel3不由空状态或版本大小推新能力。
#pragma once
#include "resident/domains/travel/transport_section_codec.hpp"
namespace hanmo::resident {
struct WorldBundle;struct InitialWorldSeed;struct PreparedCatalog;
// 0表示未选择；group须配manifest，暂定允许仅manifest2的原静置箱组合。
struct NativeTransportSectionVersions {
    Integer travel=0,group=0,manifest=0;
    bool carrier_location=false;
};
Outcome<NativeTransportSectionVersions> selected_native_transport_versions(const CatalogIdentity&);
Status append_native_transport_modules(const BoundaryValue& scenario,CatalogIdentity&);
// 在原Common作者候选上设置同P06技术位，保持其它目录借用指针不变。
Status prepare_native_transport_selection(const BoundaryValue& scenario,PreparedCatalog&,CommonCatalog& original_common);
Status validate_native_transport_catalog(const PreparedCatalog&);
Outcome<TransportPhysicalCodecSelection> native_transport_physical_selection(const PreparedCatalog&);
// 仅真实新局生成原空间绑定及箱初态；restore由原codec读历史，不能调用此口补根。
Status initialize_native_transport_roots(const PreparedCatalog&,const CommonStore& original_initial,InitialWorldSeed&);
// 只核选择/持值和未选组的逐项缺席；全业务冷门仍由原完整owning验证。
Status validate_native_transport_layout(const WorldBundle&);
// 个人runtime空辅助根不传成已选group物理根；唯一payments仍可有原个人来源。
Outcome<std::optional<TransportPhysicalCodecRoots>> native_transport_physical_roots(WorldBundle&);
// Restore-only pure derived adoption, after every original source root exists
// and before Director cache/full cold validation. Never used to repair hot state.
Status restore_native_transport_derived_values_in_candidate(WorldBundle&,ExactArithmeticBudget={});
}
