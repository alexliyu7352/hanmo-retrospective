// 五类旧语义离线组件与唯一关联查询；不声明原生权威世界或持久world_id。
#pragma once
#include "hanmo/projection_types.hpp"
#include <memory>
#include <optional>
#include <set>

namespace hanmo::detail {
// 原位置及额外字段值拥有；没有生日时保持optional缺失，不造年龄。
struct ActorState {
    std::string id,wallet_id,inventory_id,action_id;
    bool alive=false,available=false;
    std::int64_t hp=0,fatigue=0,merit=0;
    std::optional<std::string> prototype_id,display_name;
    std::optional<std::int64_t> birth_tick,created_tick;
    Value location;
    Value::Dict extras;
};
// 钱预留按原action键保留，包括金额0；不另存可花余额。
struct WalletState {
    std::string id;
    Value owner;
    std::int64_t balance=0;
    std::map<std::string,std::int64_t> reserved;
    Value::Dict extras;
};
// 旧七字段与扩展分区；参与者原序和params类型不改变。
struct LegacyActivity {
    std::string id,kind,actor_id;
    std::vector<std::string> participants;
    std::int64_t start_tick=0,remaining_ticks=0;
    Value::Dict params,extras;
};
// 运行任职记录与作者定义分存；disabled历史引用允许未解析。
struct LegacyOffice {
    std::string id,holder_actor_id,appointment_id;
    bool enabled=false;
    Value::Dict extras,definition;
};
// 原目录组织不是新运行组织；目录领袖引用不要求已激活真人。
struct OrganizationDefinition {
    std::string id,name_key,kind;
    std::optional<std::string> leader_actor;
    Value::Dict extras;
};
// 必要来源事实按独立记录域索引，允许与Actor/Action相同原字符串ID。
struct LegacyFact {
    std::string id,kind;
    Value::Dict extras;
};
// item主字段复用原承重来源，额外目录字段不丢失。
struct ItemDefinition {
    std::string id;
    std::int64_t weight=0;
    Value::Dict extras;
};
// 完整解码候选只拥有本片记录与引用；不含Callable、Flecs句柄或整state。
struct LegacyProjectionInput {
    std::string input_sha256,payload_sha256;
    Value::Dict source,provenance;
    std::vector<ActorState> actors;
    std::vector<WalletState> wallets;
    std::vector<InventoryState> inventories;
    std::vector<LegacyActivity> activities;
    std::vector<LegacyOffice> offices;
    std::vector<OrganizationDefinition> organizations;
    std::vector<LegacyFact> facts;
    std::vector<ItemDefinition> items;
    std::map<std::string,std::set<std::string>> references;
    Value coverage;
};
// 字段名区分目录和运行对象；列表只作存在性引用，不补建实体。
inline constexpr const char* legacy_reference_names[]={
    "tasks","parties","armies","catalog_actors","places","settlements",
    "routes","world_maps","items","offices","garrisons","roles"
};
// 单批query顺序保持，actor输出排序；不存在人物由执行前整批核定。
struct ActorCommitmentsQuery {
    std::string query_id;
    std::vector<std::string> actor_ids;
};
Result<LegacyProjectionInput> decode_legacy_artifact(const std::string& bytes);
Result<std::vector<ActorCommitmentsQuery>> decode_actor_queries(const std::string& bytes,const std::string& input_sha256);
} // 类型验证仅覆盖已声明profile，不移植完整旧生命周期。

namespace hanmo {
// 只列当前行动真实资金锁定，owner是完整原值。
struct WalletReservationRow {
    std::string wallet_id;
    detail::Value owner;
    std::int64_t amount=0;
};
// 物理位置及原下标使关联批次不丢原序，也不把容器产权当货权。
struct GoodsReservationRow {
    std::string inventory_id;
    detail::Value container_owner;
    std::size_t lot_index=0;
    detail::Lot lot;
};
// 任职只是事实展示；resolved不等于有权执行命令。
struct OfficeCommitmentRow {
    std::string office_id,appointment_id,organization_id;
    bool enabled=false,appointment_resolved=false;
    detail::OrganizationDefinition organization;
};
// 一个真实人物的有限承诺结果，不回传全actor或世界。
struct ActorCommitmentRow {
    std::string actor_id,wallet_id,inventory_id;
    detail::Value location;
    std::optional<detail::LegacyActivity> current_activity;
    std::vector<WalletReservationRow> wallet_reservations;
    std::vector<GoodsReservationRow> goods_reservations;
    std::vector<OfficeCommitmentRow> offices;
};
// batch行保持query输入顺序；内部actors按原ID排序。
struct ActorCommitmentsResult {
    std::string query_id;
    std::vector<ActorCommitmentRow> actors;
};
// 同一个Flecs存储实现兼容两profile，v1不伪造新profile实体。
class ProjectionStore {
public:
    ProjectionStore();
    ~ProjectionStore();
    ProjectionStore(const ProjectionStore&)=delete;
    ProjectionStore& operator=(const ProjectionStore&)=delete;
    Result<bool> install_once(const std::string& artifact_bytes);
    Result<std::vector<CountRow>> query_count_batch(const std::string& query_bytes) const;
    Result<std::vector<ActorCommitmentsResult>> actor_commitments(const std::string& query_bytes) const;
    ProjectionAudit audit() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // 只读结果均为独立值，不提供刷新、提交或恢复入口。
