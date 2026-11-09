// 工程准备只绑定原纯规则观察，所有报价/AI/resolve共用原攻门与材料路径。
#pragma once
#include "siege_model.hpp"
namespace hanmo::resident {
// 已验真实资格或只读报价准备必须与此围城/半日/名册逐项相等；失败不改变观察。
Status bind_military_siege_engineering_observation(
 const SiegeRecord&,const MilitarySiegeEngineeringPreparation&,SiegeObservation&);
}
