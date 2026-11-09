// 冷边界组合各域唯一codec；仅在install/export/checkpoint调用，不在热phase存JSON。
#pragma once
#include "world_bundle.hpp"
#include "hanmo/resident_world_api.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
class ResidentResultCodec final : public CommonResultCodec {
public:
    Outcome<std::shared_ptr<const CommitResult>> decode(const BoundaryValue&) const override;
    BoundaryValue encode(const CommitResult&) const override;
    Status emit(ProjectionWriter&,const StableId&,const CommitResult&)const override;
    Status visit(CodecVisitor&,std::shared_ptr<const CommitResult>&) const override;
};
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_catalog(const BoundaryValue&,const BoundaryValue& source_state);
Outcome<InitialWorldSeed> decode_world_seed(const BoundaryValue&,const PreparedCatalog&);
Outcome<BoundaryValue> export_world_state(const WorldBundle&);
Status validate_world_export_shape(const WorldBundle&,const CapacityProfile&,CheckpointJobBudget&);
Outcome<Bytes> encode_world_checkpoint(WorldBundle&);
Outcome<InitialWorldSeed> decode_world_checkpoint(const Bytes&,const PreparedCatalog&);
struct DecodedWorldSeedOwner;
CheckpointJobResult<DecodedWorldSeedOwner> decode_world_checkpoint_stream(CheckpointReadSource&,const PreparedCatalog&,const CapacityProfile&,CodecDecodeBudget&,CheckpointJobBudget&,const CheckpointDigest* expected_full_digest=nullptr);
Outcome<BoundaryValue> inspect_world_checkpoint_selection(const Bytes&);
} // 每域必须覆盖全部拥有记录；重复事实/行动payload或缺失均拒绝。
