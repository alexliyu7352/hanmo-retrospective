// 中央只将同一个候选的真实引用交给领域接口；禁用profile先由调用门拒绝。
#include "world_bundle.hpp"
namespace hanmo::resident {
GovernanceReadContext WorldBundle::governance_read()const{return {common,*catalog->common,politics,*catalog->politics,politics_governance,*catalog->politics_governance,organization,*catalog->organization,local_economy,*catalog->public_relief,public_relief,tasks,governance_tasks,services,&politics_view_index,&governance_view_index,catalog->political_governance_invitation_replies?&services:nullptr,catalog->political_governance_agencies?&services:nullptr};}
GovernanceWriteContext WorldBundle::governance_write(){return {common,*catalog->common,politics,*catalog->politics,politics_governance,*catalog->politics_governance,organization,*catalog->organization,local_economy,*catalog->public_relief,public_relief,tasks,governance_tasks,services,[this](const StableId& id){return release_action(*this,id);},catalog->political_governance_invitation_replies?&services:nullptr,catalog->political_governance_agencies?&services:nullptr};}
}
