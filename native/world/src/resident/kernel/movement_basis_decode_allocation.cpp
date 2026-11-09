// 原实际 basis 复制仍在 S1；这里只核 decoder/current resource 同 job 和首错。
#include "movement_basis_decode_allocation.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
Status NativeMovementBasisDecodeAllocation::reserve_original_basis_copy(const MovementBasis&) {
  return budget_.account_allocator_candidate(0, current_checkpoint_memory());
}
}
