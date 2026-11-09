// 原Family唯一生前接续写者的候选内部接口；调用方必须先完成具名来源/稳定处境验证。
#pragma once
#include "family_model.hpp"
namespace hanmo::resident {
// 不开/提交Common保存点，不替换Family对象。任何失败由外层完整候选回滚Common+Family+来源尾。
// 旧收养与显式新路径均使用这两个原事实写者；不得直接调用以绕过本人来源或责任准入。
Outcome<StableId> record_succession_consent_in_candidate(CommonStore&,FamilyState&,const SuccessionConsentSource&);
Outcome<StableId> execute_succession_plan_in_candidate(CommonStore&,FamilyState&,const StableId& plan_id,
    const SuccessionPersonalQuote&);
}
