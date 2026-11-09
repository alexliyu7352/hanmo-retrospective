// The one compiled construction boundary for all Complete world routes.
// It is C++ code, never authored data, checkpoint state or a mutable registry.
#pragma once
#include "exact_complete_world.hpp"
#include "motion_exact_world_bindings.hpp"
#include "resident/domains/local_economy/economic_native_lease.hpp"
#include "resident/domains/tasks/work_execution/work_native_lease.hpp"
#include "resident/domains/director/director_native_lease.hpp"
#include "resident/domains/craft/craft_exact_phase.hpp"
#include "resident/domains/craft/repair_exact_phase.hpp"
#include "resident/domains/craft/craft_facility_runtime.hpp"
#include "resident/domains/local_economy/household_service_runtime.hpp"
#include "resident/domains/affairs/state_need_exact_phase.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include <array>
namespace hanmo::resident {
// These are borrowed exports of the original owning graph, not substitute
// providers or a second collection of World state. The graph owner below owns
// every context/adapter behind them. Null is diagnosed, never a success route.
struct NativeCompleteWorldSourceExports {
 const NativeEconomicExactDependencies* economic=nullptr;
 const NativeEconomicExactWriteDependencies* economic_write=nullptr;
 const CraftExactReadBinding* craft=nullptr;
 const CraftExactWriteBinding* craft_write=nullptr;
 const RepairExactReadBinding* repair=nullptr;
 const RepairExactWriteBinding* repair_write=nullptr;
 const CraftFacilityReadBinding* craft_facility=nullptr;
 const CraftFacilityWriteBinding* craft_facility_write=nullptr;
 const HouseholdServiceReadBinding* household=nullptr;
 const HouseholdServiceWriteBinding* household_write=nullptr;
 std::shared_ptr<const MotionGroupReadDependencies> motion_group;
 std::shared_ptr<const MotionCargoReadDependencies> motion_cargo;
 std::shared_ptr<const PatrolCheckpointOriginalRead> motion_patrol;
 std::shared_ptr<MotionGroupWriteDependencies> motion_group_write;
 std::shared_ptr<MotionCargoWriteDependencies> motion_cargo_write;
 const StateNeedReadBinding* state_need=nullptr;
 const StateNeedWriteBinding* state_need_write=nullptr;
 const PoliticalExactReadBinding* political=nullptr;
 const PoliticalExactWriteBinding* political_write=nullptr;
 const NativeTaskWorkDependencies* task_work=nullptr;
 const NativeTaskWorkWriteDependencies* task_work_write=nullptr;
 const MilitaryExactReadBinding* military=nullptr;
 const MilitaryExactWriteBinding* military_write=nullptr;
 const NativeDirectorExactDependencies* director=nullptr;
 const NativeDirectorExactWriteDependencies* director_write=nullptr;
 // 同实例原拥有图的有限出口；与旧依赖构造互斥，不允许缺源降级。
 const EconomicExactReadBinding* economic_binding=nullptr;
 const EconomicExactWriteBinding* economic_write_binding=nullptr;
 // Actual personal Motion leases, owned by the same route scope; not empty extensions.
 std::shared_ptr<const MotionExactReadLease> motion_personal_read;
 std::shared_ptr<MotionExactWriteLease> motion_personal_write;
};
enum class NativeCompleteWorldRoute { source_graph,economic,craft,repair,craft_facility,household,motion,state_need,political,task_work,military,director,family_birth_due,family_guardianship,estate_phase,commerce_phase,medicine_phase,study_phase,arcana_phase };
struct NativeCompleteWorldWriterExports {
 GrowthCapabilityRevisionWrite* growth_capability=nullptr;
 WorkCompletedResultConsumer* work_completed_growth=nullptr;
 CareLifeConditionWrite* care_life=nullptr;
 GrowthStartedParticipationWrite* growth_start=nullptr;
 OriginalGrowthServiceRelationWrite* social_growth=nullptr;
 OriginalTaskReportRelationWrite* social_task_report=nullptr;
 OriginalSocialCalendarRelationWrite* social_calendar=nullptr;
};
// Fresh actual-M construction scope, owned by every inner lease that borrows
// its exports. A domain request cannot borrow a previous acquisition's Body or
// current-time context. Only the requested original graph must be materialized.
class NativeCompleteWorldSourceScope:public CheckpointAllocatedObject {
public:
 NativeCompleteWorldSourceScope()=default;
 virtual ~NativeCompleteWorldSourceScope()=default;
 NativeCompleteWorldSourceScope(const NativeCompleteWorldSourceScope&)=delete;
 NativeCompleteWorldSourceScope&operator=(const NativeCompleteWorldSourceScope&)=delete;
 NativeCompleteWorldSourceScope(NativeCompleteWorldSourceScope&&)=delete;
 NativeCompleteWorldSourceScope&operator=(NativeCompleteWorldSourceScope&&)=delete;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual NativeCompleteWorldRoute route()const noexcept=0;
 virtual const WorldMoment& actual_at()const noexcept=0;
 virtual const NativeCompleteWorldSourceExports& sources()const noexcept=0;
};
// Each domain keeps its original implementations. This one nonmoving owner is
// the typed injection boundary for their same-World construction graph. It may
// only be returned after all its members and private one-time links exist.
// No constructor calls the public World getter or validates pending results.
class NativeCompleteWorldSourceOwner:public CheckpointAllocatedObject {
public:
 NativeCompleteWorldSourceOwner()=default;
 virtual ~NativeCompleteWorldSourceOwner()=default;
 NativeCompleteWorldSourceOwner(const NativeCompleteWorldSourceOwner&)=delete;
 NativeCompleteWorldSourceOwner& operator=(const NativeCompleteWorldSourceOwner&)=delete;
 NativeCompleteWorldSourceOwner(NativeCompleteWorldSourceOwner&&)=delete;
 NativeCompleteWorldSourceOwner& operator=(NativeCompleteWorldSourceOwner&&)=delete;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual Status validate_bindings(const WorldBundle&,bool mutable_candidate)const=0;
 virtual const CompleteExactWorldReadBindings& reads()const=0;
 virtual Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>> read_sources(NativeCompleteWorldRoute,const WorldMoment&)const=0;
 virtual Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>> write_sources(NativeCompleteWorldRoute,const WorldMoment&,ExactPhaseWriteContext*,const DynamicStepInput*)=0;
 virtual NativeCompleteWorldWriterExports writers()noexcept=0;
};
struct NativeCompleteWorldFactoryRegistry {
 // The source owner is constructed first and destroyed last. Constructors
 // below never reacquire the World getter and cannot select by table emptiness.
 // The passed Complete owner is already constructed at a stable address, but
 // unpublished. Sources may retain it for existing in-owner callbacks, never
 // call its reads/acquisitions until construction and root validation finish.
 Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>(*sources_read)(const WorldBundle&,const CompleteExactWorldBindingOwner&)=nullptr;
 Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>(*sources_write)(WorldBundle&,CompleteExactWorldBindingOwner&)=nullptr;
 Outcome<std::shared_ptr<const EconomicExactReadLease>>(*economic_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<EconomicExactWriteLease>>(*economic_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&,const DynamicStepInput*)=nullptr;
 Outcome<std::shared_ptr<const CraftExactReadLease>>(*craft_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<CraftExactWriteLease>>(*craft_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
 Outcome<std::shared_ptr<const RepairExactReadLease>>(*repair_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<RepairExactWriteLease>>(*repair_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
 Outcome<std::shared_ptr<const CraftFacilityReadLease>>(*craft_facility_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<CraftFacilityWriteLease>>(*craft_facility_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
 Outcome<std::shared_ptr<const HouseholdServiceReadLease>>(*household_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<HouseholdServiceWriteLease>>(*household_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<const MotionExactReadLease>>(*motion_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<MotionExactWriteLease>>(*motion_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,CompleteExactWorldBindingOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<const StateNeedReadLease>>(*state_need_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<StateNeedWriteLease>>(*state_need_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<const PoliticalExactReadLease>>(*political_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<PoliticalExactWriteLease>>(*political_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<const TaskWorkReadLease>>(*task_work_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const CompleteExactWorldBindingOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<TaskWorkWriteLease>>(*task_work_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,CompleteExactWorldBindingOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<const MilitaryExactReadLease>>(*military_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<MilitaryExactWriteLease>>(*military_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
 Outcome<std::shared_ptr<const DirectorExactReadLease>>(*director_read)(const WorldBundle&,const NativeCompleteWorldSourceOwner&,const CompleteExactWorldBindingOwner&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<DirectorExactWriteLease>>(*director_write)(WorldBundle&,NativeCompleteWorldSourceOwner&,CompleteExactWorldBindingOwner&,const WorldMoment&)=nullptr;
 // Complete-only composition of the original Family provider. Until the true
 // Born and all guardianship legs exist this stays absent, not an empty stage.
 Outcome<ExactPhaseRegistration>(*family_complete)(LifeAgeExactWorldFactory)=nullptr;
 // Finite remaining original provider constructors. Their chosen profile and
 // stage algorithms remain with the domain, not this central dispatcher.
 Outcome<ExactPhaseRegistration>(*estate_phase)(const PreparedCatalog&)=nullptr;
 Outcome<ExactPhaseRegistration>(*commerce_phase)(const PreparedCatalog&)=nullptr;
 Outcome<ExactPhaseRegistration>(*medicine_phase)(const PreparedCatalog&)=nullptr;
 Outcome<ExactPhaseRegistration>(*study_phase)(const PreparedCatalog&)=nullptr;
 Outcome<ExactPhaseRegistration>(*arcana_phase)(const PreparedCatalog&)=nullptr;
};
struct NativeCompleteWorldRouteStatus {
 NativeCompleteWorldRoute route;
 const char* name;
 bool read_route=false,write_route=false;
 // A linked route is not a source implementation or an admitted World.
 const char* implementation_owner;
 const char* missing_source;
};
const NativeCompleteWorldFactoryRegistry& native_complete_world_factory_registry()noexcept;
std::array<NativeCompleteWorldRouteStatus,19> read_native_complete_world_route_status()noexcept;
Status validate_native_complete_world_factory_routes(const NativeCompleteWorldFactoryRegistry&);
// Install only a genuinely supplied compiled registration. Unknown/missing
// original providers still fail; no stage or effect key is fabricated here.
Status append_native_complete_selected_phase_routes(const PreparedCatalog&,std::vector<ExactPhaseRegistration>&);
// A selected Complete Family never enters the phase graph with only life_age.
Outcome<ExactPhaseRegistration> make_native_complete_family_phase_registration(LifeAgeExactWorldFactory);
}
