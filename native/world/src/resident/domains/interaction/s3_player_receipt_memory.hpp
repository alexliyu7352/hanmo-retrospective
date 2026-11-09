// Construction mechanics for the three original four-field player receipts.
// This borrowed input is not a new receipt, source or validation policy.
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <string_view>
namespace hanmo::resident {
struct S3PlayerReceiptBuildView {
 std::string_view result_id,request_actor_id;
 Integer request_sequence,result_revision;
};

}
