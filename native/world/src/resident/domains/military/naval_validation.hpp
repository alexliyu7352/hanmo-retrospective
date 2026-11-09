// 冷态来源校验接原已验证Common/Military，不以持久舰队条目反向发明军职或资产。
#pragma once
#include "naval_world.hpp"
namespace hanmo::resident {
struct NavalOriginalSourceContext;
// 海陆格式共用历史编组/解编权源，调用者分别核其实际资产序列。
Status validate_naval_fleet_origin(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalFleetRecord&,const NavalAssetCatalog&);
Status validate_naval_fleet_origin(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalFleetRecord&,const NavalAssetCatalog&,const NavalOriginalSourceContext&);
// 原Common事实头的kind/domain精确对应由WorldBundle统一核；本口核对应领域事实及所有引用。
Status validate_naval_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&);
}
