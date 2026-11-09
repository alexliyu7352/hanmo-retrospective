// 已校验旧库存的离线只读接口；公开方法与v1首错保持。
#pragma once
#include "hanmo/projection_types.hpp"
#include <memory>

namespace hanmo {
// 一次成功安装之后只读；失败安装保留空实例，查询按整批验证后返回。
class InventoryProjection {
public:
    InventoryProjection();
    ~InventoryProjection();
    InventoryProjection(const InventoryProjection&)=delete;
    InventoryProjection& operator=(const InventoryProjection&)=delete;
    Result<bool> install_once(const std::string& artifact_bytes);
    Result<std::vector<CountRow>> query_batch(const std::string& query_bytes) const;
    ProjectionAudit audit() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // 旧库存profile兼容边界。
