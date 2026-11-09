// 新请求只扩展现Military世界事务；公共命令注册与checkpoint域版本由中央串合。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
Outcome<MilitaryQuote> preview_military_request(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryBattleRequest&);
Outcome<MilitaryEffect> apply_military_request(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryBattleRequest&);
// 共同结果缓存只能引用原请求事实的公开投影；actor从共同history取得。
Status validate_military_request_cached_answer(const MilitaryState&,const MilitaryRequestAnswer&,const StableId& actor);
Status validate_military_requests(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
}
