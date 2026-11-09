// 借同一个受信世界读钟给原日历消费者；不保存第二份Now。
#pragma once
#include "office_authority.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
class OrganizationCurrentMomentAdapter final:public CurrentMomentRead {
 const OrganizationAuthorityClock& clock_;
public:
 explicit OrganizationCurrentMomentAdapter(const OrganizationAuthorityClock& clock):clock_(clock){}
 Outcome<WorldMoment> read_current_moment()const override{return clock_.read_organization_authority_now();}
};
}
