// One original cancellation field table for JSON, typed checkpoint and projection.
#pragma once
#include "control_retirement_cancel.hpp"
#include "control_continuation_codec_fields.hpp"
namespace hanmo::resident::retirement_cancel_codec {
template<class C>Status request(C& c,CancelPendingRetirement& value){return continuation_codec::pending_request(c,value);}
template<class C>Status receipt(C& c,PendingRetirementCancelled& value){c.field("pending_id",value.pending_id);c.field("generation",value.generation);c.object("original_withdrawn_preparation",[&](C& child){return continuation_codec::cause(child,value.original_withdrawn_preparation);});if(!c.status().ok)return c.status();auto valid=validate_pending_retirement_cancelled_shape(value);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
