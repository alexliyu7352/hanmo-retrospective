// 遗留控制回执只引用真实本域事实；不从可改缓存取得控制或产权来源。
#pragma once
#include "family_legacy.hpp"
namespace hanmo::resident {
// 由中央先拆开typed回执和唯一history，再交本域核命令、操作者、时刻与精确字段。
Status validate_family_legacy_receipt(const FamilyLegacyState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
}
