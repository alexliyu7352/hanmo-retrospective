// 请求只接受原已知Offer ID，不能从参数选择预算金额、sender或库存。
#pragma once
#include "task_personal_acceptance_contract.hpp"
namespace hanmo::resident {Outcome<TaskPersonalAcceptance> decode_task_personal_acceptance(const StableId&,const BoundaryValue&);BoundaryValue encode_task_personal_acceptance_quote(const TaskPersonalAcceptanceQuote&);}
