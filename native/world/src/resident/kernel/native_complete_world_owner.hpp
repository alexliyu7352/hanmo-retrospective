// Concrete same-candidate Complete owner. No state is cloned or duplicated.
#pragma once
#include "native_complete_world_registry.hpp"
namespace hanmo::resident {
class NativeCompleteWorldBindingOwner final:public CompleteExactWorldBindingOwner,public CheckpointAllocatedObject {
 const WorldBundle& world_;
 WorldBundle* writable_;
 // Copy only the finite compiled function table; never retain a caller-local
 // registry object. The real source graph outlives every borrowed projection.
 const NativeCompleteWorldFactoryRegistry factories_;
 std::unique_ptr<NativeCompleteWorldSourceOwner> sources_;
 bool ready_=false;
 NativeCompleteWorldWriterExports writer_exports();
 NativeCompleteWorldBindingOwner(const WorldBundle&,WorldBundle*,NativeCompleteWorldFactoryRegistry,std::unique_ptr<NativeCompleteWorldSourceOwner>);
 Status actual(const WorldMoment&)const;
 Status mutable_actual(const WorldMoment&)const;
 Status capture(const WorldMoment&,const ExactPhaseWriteContext&)const;
public:
 ~NativeCompleteWorldBindingOwner()override;
 NativeCompleteWorldBindingOwner(const NativeCompleteWorldBindingOwner&)=delete;
 NativeCompleteWorldBindingOwner&operator=(const NativeCompleteWorldBindingOwner&)=delete;
 NativeCompleteWorldBindingOwner(NativeCompleteWorldBindingOwner&&)=delete;
 NativeCompleteWorldBindingOwner&operator=(NativeCompleteWorldBindingOwner&&)=delete;
 static Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> create(const WorldBundle&,const NativeCompleteWorldFactoryRegistry&);
 static Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> create(WorldBundle&,const NativeCompleteWorldFactoryRegistry&);
 const CompleteExactWorldReadBindings& reads()const override;
 Outcome<std::shared_ptr<const EconomicExactReadLease>> read_economic(const WorldMoment&)const override;
 Outcome<std::shared_ptr<EconomicExactWriteLease>> write_economic(const WorldMoment&,const DynamicStepInput*) override;
 Outcome<std::shared_ptr<const CraftExactReadLease>> read_craft(const WorldMoment&)const override;
 Outcome<std::shared_ptr<CraftExactWriteLease>> write_craft(const WorldMoment&,ExactPhaseWriteContext&) override;
 Outcome<std::shared_ptr<const RepairExactReadLease>> read_repair(const WorldMoment&)const override;
 Outcome<std::shared_ptr<RepairExactWriteLease>> write_repair(const WorldMoment&,ExactPhaseWriteContext&) override;
 Outcome<std::shared_ptr<const CraftFacilityReadLease>> read_craft_facility(const WorldMoment&)const override;
 Outcome<std::shared_ptr<CraftFacilityWriteLease>> write_craft_facility(const WorldMoment&,ExactPhaseWriteContext&) override;
 Outcome<std::shared_ptr<const HouseholdServiceReadLease>> read_household_service(const WorldMoment&)const override;
 Outcome<std::shared_ptr<HouseholdServiceWriteLease>> write_household_service(const WorldMoment&) override;
 Outcome<std::shared_ptr<const MotionExactReadLease>> read_motion(const WorldMoment&)const override;
 Outcome<std::shared_ptr<MotionExactWriteLease>> write_motion(const WorldMoment&) override;
 Outcome<std::shared_ptr<const StateNeedReadLease>> read_state_need(const WorldMoment&)const override;
 Outcome<std::shared_ptr<StateNeedWriteLease>> write_state_need(const WorldMoment&) override;
 Outcome<std::shared_ptr<const PoliticalExactReadLease>> read_political(const WorldMoment&)const override;
 Outcome<std::shared_ptr<PoliticalExactWriteLease>> write_political(const WorldMoment&) override;
 Outcome<std::shared_ptr<const DirectorExactReadLease>> read_director(const WorldMoment&)const override;
 Outcome<std::shared_ptr<DirectorExactWriteLease>> write_director(const WorldMoment&) override;
 Outcome<std::shared_ptr<const TaskWorkReadLease>> read_task_work(const WorldMoment&)const override;
 Outcome<std::shared_ptr<TaskWorkWriteLease>> write_task_work(const WorldMoment&) override;
 Outcome<std::shared_ptr<const MilitaryExactReadLease>> read_military(const WorldMoment&)const override;
 Outcome<std::shared_ptr<MilitaryExactWriteLease>> write_military(const WorldMoment&,ExactPhaseWriteContext&) override;
 GrowthCapabilityRevisionWrite* growth_capability_writer()override;
 WorkCompletedResultConsumer* work_completed_growth_consumer()override;
 CareLifeConditionWrite* care_life_writer()override;
 GrowthStartedParticipationWrite* growth_start_participation_writer()override;
 OriginalGrowthServiceRelationWrite* original_growth_service_relation_writer()override;
 OriginalTaskReportRelationWrite* original_task_report_relation_writer()override;
 OriginalSocialCalendarRelationWrite* original_social_calendar_relation_writer()override;
};
}
