// 围城五命令只读解析与报价边界；动态会话仍是siege_model的固定typed记录。
#pragma once
#include "military_codec.hpp"
namespace hanmo::resident {
Outcome<MilitaryCommand> decode_siege_command(const StableId&,const BoundaryValue&);
Outcome<BoundaryValue> encode_siege_quote(const MilitaryQuote&,const MilitaryCommand&);
}
