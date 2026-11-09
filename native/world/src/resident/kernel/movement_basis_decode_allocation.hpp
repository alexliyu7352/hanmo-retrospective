// S1原basis恢复的深值预留借唯一decoder账；不是另一份世界复制计划或临时收费器。
#pragma once
#include "resident/domains/travel/transport_section_codec.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
namespace hanmo::resident {
// 每次真实copy前核同job并预留原typed字段；budget的reservation必须移交decoded world。
class NativeMovementBasisDecodeAllocation final:public MovementBasisDecodeAllocation {
    CodecDecodeBudget& budget_;
public:
    explicit NativeMovementBasisDecodeAllocation(CodecDecodeBudget& budget):budget_(budget){}
    NativeMovementBasisDecodeAllocation(const NativeMovementBasisDecodeAllocation&)=delete;
    NativeMovementBasisDecodeAllocation& operator=(const NativeMovementBasisDecodeAllocation&)=delete;
    // 仅规划普通STL深值；BigNat等实际allocator字段由随后的原copy真实收费。
    Status reserve_original_basis_copy(const MovementBasis&)override;
};
}
