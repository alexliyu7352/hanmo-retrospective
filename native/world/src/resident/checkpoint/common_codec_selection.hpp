// P06唯一有限表；布局选择不授予缺失producer/validator的安装能力。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct CommonCodecFeatures {
    bool domain_activity=false, item_instances=false, lot_properties=false;
    bool appearance=false, carrier_location=false, equipment_instances=false;
};
struct CommonCodecSelection {
    Integer version=0, base_version=0;
    CommonCodecFeatures features{};
};
bool same_common_codec_features(const CommonCodecFeatures&,const CommonCodecFeatures&) noexcept;
// 调用方先按原合法profile获得base；feature仅来自已验证目录的确切module版本。
Outcome<CommonCodecSelection> select_common_codec(Integer base_version,const CatalogIdentity& verified_catalog);
// 仅识别注册布局；不能凭本函数成功发布世界，仍须成对codec及所有真实域依赖。
Outcome<CommonCodecSelection> decode_common_codec_selection(Integer encoded_version);
Status validate_common_codec_selection(Integer base_version,const CatalogIdentity&,Integer encoded_version);
bool common_codec_dynamic_base(const CommonCodecSelection&) noexcept;
bool common_codec_extended_skills(const CommonCodecSelection&) noexcept;
}
