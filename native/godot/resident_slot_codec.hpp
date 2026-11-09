// 只移植既有HM-RESIDENT-SAVE物理封装，不定义新槽/2，也不解释checkpoint世界语义。
#pragma once
#include "resident_slot_media.hpp"

namespace hanmo::resident::storage {
inline constexpr std::size_t resident_slot_byte_limit=64u*1024u*1024u;
inline constexpr std::size_t resident_slot_continuation_limit=2048;
// 数值、摘要与深拥有载荷严格来自同一旧封套，不能当世界准入证明。
struct ResidentSlotEnvelope {
    Integer format_version=1,generation=0;
    std::string save_id,payload_sha256,continuation_sha256;
    Bytes payload;
    std::optional<Bytes> continuation;
};
// 仅显式旧物理损坏可以给A/B回退许可，资源、未知格式及语义失败不在此许可内。
struct ResidentSlotDecoded {
    bool ok=false,physical_corruption=false;
    std::string code;
    ResidentSlotEnvelope envelope;
};
// 选代保留原同代不同text冲突和已恢复标记，缺档与全损坏分别处理。
struct ResidentSlotSelected {
    bool ok=false,not_found=false,recovered=false,physical_corruption=false;
    std::string code;
    SlotBank bank=SlotBank::b;
    ResidentSlotEnvelope envelope;
};
// 编码只适配原七/十一字段；核心实际生成与核验来源由coordinator保证。
Outcome<Bytes> encode_resident_slot(const Bytes&,Integer,const std::string&,const std::optional<Bytes>& continuation={});
// 严格JSON、原整数归一化、base64及摘要检查；不安装或复制世界。
ResidentSlotDecoded decode_resident_slot(const Bytes&);
// 按原物理generation选A/B；前代是否语义有效仍须同一core另核。
ResidentSlotSelected select_resident_slot(const SlotBankPair&,const std::string&);
// 摘要只绑定真实已拥有字节，绝不能代替物理屏障或世界核验。
std::string resident_slot_sha256(const Bytes&);
} // 旧HM-SAVE及其GD迁移/引擎兼容语义仍由原路径负责。
