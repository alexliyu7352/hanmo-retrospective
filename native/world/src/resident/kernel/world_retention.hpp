// 原内存拥有图已退役；保留真实异步 owner 与 JSON 封印的生命周期入口。
#pragma once
#include "resident/checkpoint/checkpoint_retention.hpp"
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PreparedCatalog;struct WorldBundle;struct InitialWorldSeed;struct CommonState;
Status retain_prepared_catalog_owned_graph(const std::shared_ptr<const PreparedCatalog>&,CheckpointRetentionCensus&);
Status retain_initial_world_seed_graph(const InitialWorldSeed&,const CommonState* adopted_common,CheckpointRetentionCensus&);
Status retain_world_checkpoint_graph(const WorldBundle&,CheckpointRetentionCensus&);
}
