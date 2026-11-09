#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "work_follow_through_codec.hpp"
namespace hanmo::resident {
Status visit_work_follow_through_ref(CodecVisitor&a,AcceptedWorkFollowThroughRef&v){return work_follow_through_codec::follow_through(a,v);}
Status visit_work_follow_through_terminal(CodecVisitor&a,FollowThroughTerminalView&v){return work_follow_through_codec::terminal(a,v);}
Status visit_work_commission_terminal(CodecVisitor&a,WorkCommissionTaskTerminalSource&v){return work_follow_through_codec::task_terminal(a,v);}
}
