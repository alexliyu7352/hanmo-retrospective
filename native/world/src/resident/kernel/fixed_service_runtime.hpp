// 所有F原owner共用的唯一服务状态转换；真实开始/资格/原因/终局先在原owner成立。
#pragma once
#include "action_time_validation.hpp"
namespace hanmo::resident {
// The original two-row adoption basis from Task and the three S3 owners.
// Caller rule text and original identity already have their own input lifetime.
// This is not the ordered all-applied Supervision basis.
class FixedServiceAdoptionInputs {
    CheckpointMemoryCharge copied_fields_;
    std::vector<FixedServiceInputRef> values_;
public:
    FixedServiceAdoptionInputs(const SourceRefBuildView&,Integer source_schema,const CatalogIdentity&,std::string_view rule);
    FixedServiceAdoptionInputs(const FixedServiceAdoptionInputs&)=delete;
    FixedServiceAdoptionInputs(FixedServiceAdoptionInputs&&)=delete;
    FixedServiceAdoptionInputs& operator=(const FixedServiceAdoptionInputs&)=delete;
    FixedServiceAdoptionInputs& operator=(FixedServiceAdoptionInputs&&)=delete;
    const std::vector<FixedServiceInputRef>& values()const noexcept{return values_;}
};
// The original first-transition temporary owner, extracted from Task's actual
// producer. It keeps a fresh fourteen-field row alive through prepare; Prepared
// and the retained Store independently reserve their own target copies.
class FixedServiceTransitionInput {
    CheckpointMemoryCharge copied_fields_;
    FixedServiceTransition row_;
    template<class OriginalBuildView>FixedServiceTransitionInput(const OriginalBuildView&,int);
public:
    explicit FixedServiceTransitionInput(const FixedServiceAdoptionTransitionBuildView&);
    explicit FixedServiceTransitionInput(const FixedServiceTerminalTransitionBuildView&);
    FixedServiceTransitionInput(const FixedServiceTransitionInput&)=delete;
    FixedServiceTransitionInput(FixedServiceTransitionInput&&)=delete;
    FixedServiceTransitionInput& operator=(const FixedServiceTransitionInput&)=delete;
    FixedServiceTransitionInput& operator=(FixedServiceTransitionInput&&)=delete;
    const FixedServiceTransition& row()const noexcept{return row_;}
};
struct PreparedFixedServiceChangeBuildView {
    const ActionElapsedState& entry;const FixedServiceTransition& transition;
    const ActionElapsedTerminalBinding* terminal=nullptr;bool replay=false;
};
struct PreparedFixedServiceChange {
    CheckpointMemoryCharge copied_fields;
    ActionElapsedState entry;FixedServiceTransition transition;std::optional<ActionElapsedTerminalBinding> terminal;bool replay=false;
    PreparedFixedServiceChange()=default;
    explicit PreparedFixedServiceChange(const PreparedFixedServiceChangeBuildView&);
    PreparedFixedServiceChange(const PreparedFixedServiceChange&);
    PreparedFixedServiceChange(PreparedFixedServiceChange&& value):PreparedFixedServiceChange(static_cast<const PreparedFixedServiceChange&>(value)){}
    PreparedFixedServiceChange& operator=(const PreparedFixedServiceChange&)=delete;
    PreparedFixedServiceChange& operator=(PreparedFixedServiceChange&&)=delete;
};
// 开始入口只采用已核原terms生成的entry；此处绝不分Action/收开工费或补造原来源。
Outcome<PreparedFixedServiceChange> prepare_fixed_service_adoption(const ActionCoreView&,const ActionElapsedState&,const FixedServiceTransition&,const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 暂停/恢复/输入变更/终止都先结原服务至实际M；暂停间隔不赚credit，满额只原真实terminal。
Outcome<PreparedFixedServiceChange> prepare_fixed_service_change(const ActionElapsedStore&,const StableId& action,const FixedServiceTransition&,const std::optional<StableId>& original_suspension_reason,const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 私有同候选采用。跨域失败必须丢该WorldBundle；晚失败不可只回滚这一行却保收费/身体。
Status apply_fixed_service_change_in_candidate(ActionElapsedStore&,PreparedFixedServiceChange,const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
}
