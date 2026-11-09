// 私人生活只生成有限正式提案；服务认知与地点路径不修改共同资源。
#pragma once
#include "director_model.hpp"
namespace hanmo::resident {
// 已知服务现场调用原报价；远地只投影作者公开条款，不读取实时岗位/余额。
KnownSubjectView with_livelihood_services(const CommonStore&,const CommonCatalog&,
    const GrowthCatalog&,const GrowthAccess&,const TravelCatalog&,
    const DirectorPolicy&,const KnownSubjectView&);
// 跨城先走到当前城口；同城只返回一个实际相邻通道。
std::shared_ptr<const CommandPayload> director_move_toward(
    const TravelCatalog&,const KnownSubjectView&,const StableId& target_place);
}
