// S2 训练只借 S5 原期间来源；此适配器不拥有第二份日历或供养进度。
#pragma once
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/local_economy/economic_calendar.hpp"
namespace hanmo::resident {
class NativeMilitaryTrainingPeriodRead final:public ExactTrainingPeriodRead {
    EconomicCalendarIdentityReadContext calendar_;
    ExactArithmeticBudget budget_;
public:
    // Context 按值保存其原引用；所有被借原 owner 必须由外层同世界 lease 持有。
    explicit NativeMilitaryTrainingPeriodRead(EconomicCalendarIdentityReadContext calendar,ExactArithmeticBudget budget={})
        :calendar_(calendar),budget_(budget){}
    // Preserve the full-context entry, but retain only the six roots actually
    // consumed by the sole original identity reader. No late provider binding.
    explicit NativeMilitaryTrainingPeriodRead(EconomicCalendarReadContext calendar,ExactArithmeticBudget budget={})
        :NativeMilitaryTrainingPeriodRead(economic_calendar_identity_context(calendar),budget){}
    bool bound_to(const CommonStore&,const EconomicWorkSourcesState&)const noexcept;
    Outcome<ExactTrainingPeriodView> read_training_period(Integer actual_from_tick)const override;
};
}
