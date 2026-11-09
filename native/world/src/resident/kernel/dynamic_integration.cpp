#include "resident/domains/military/military_supply_lookup.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "political_subordinate_appointment_director_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "political_office_resignation_response_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "political_governance_agency_intent_world.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_world.hpp"
#include "resident/domains/tasks/task_responsibility_director_world.hpp"
#include "political_takeover_execution_world.hpp"
#include "occupation_fiscal_integration.hpp"
#include "treaty_passage_integration.hpp"
#include "resident/domains/politics/treaty/treaty_passage_director.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_delegation_director.hpp"
// 宏观阶段接线和提案采用共用同一候选；领域效果、消息和计划不能分开发布。
#include "dynamic_integration.hpp"
#include "political_governance_director_world.hpp"
#include "political_shop_registry_director_integration.hpp"
#include "political_supply_renewal_integration.hpp"
#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
#include "task_advance_integration.hpp"
#include "regional_integration.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_response_director.hpp"
#include "resident/domains/politics/politics_director.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
#include "resident/domains/director/director_membership.hpp"
#include <algorithm>
#include <set>
#include "resident/domains/military/training_rules.hpp"
namespace hanmo::resident {
namespace {
// Callback borrows only this candidate. Each actor preview rebuilds its legal index.
DirectorConversationResponse diplomatic_conversation_response(WorldBundle& w){
    return [&w](const StableId& actor,const DirectorPolicy& policy)->Outcome<bool>{
        if(w.catalog->director->visit_reception&&std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::visit_reception)!=policy.templates.end()){auto visit=visit_reception_conversation_world(w,actor);if(!visit.ok||visit.value)return visit;}
        if(!w.diplomacy_enabled())return {true,false,{}};
        auto access=w.services.interaction_access();
        DiplomaticPresentRead present{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,access};
        DirectorDiplomaticContext context{present,*w.catalog->common,w.services};
        if(w.treaty_passage_enabled()&&std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::treaty_ratification)!=policy.templates.end()){auto treaty=TreatyPassageWorldAccess::create(w);if(!treaty.ok)return {false,false,treaty.error};auto candidates=propose_treaty_passage_npc(treaty.value->context(),actor);if(!candidates.ok)return {false,false,candidates.error};for(const auto& command:candidates.value)if(command.authority.organization_id==policy.organization_id)return {true,true,{}};}
        if(w.catalog->director->diplomatic_response&&policy.diplomatic_response){auto reply=diplomatic_response_available(context,actor,*policy.diplomatic_response,w.catalog->director->candidates_per_subject);if(!reply.ok||reply.value)return reply;}if(w.catalog->director->diplomatic_delegation&&policy.diplomatic_delegation_policy_id)return diplomatic_delegation_available(context,actor,*policy.diplomatic_delegation_policy_id);return {true,false,{}};
    };
}
// NPC提案可经任务/服务/对白跨写多域；完整typed写集配合共同savepoint，拒绝时一起还原。
struct ProposalWriteSet {
    TravelState travel;MilitaryState military;MilitaryTacticLearningState military_tactic_learning;NavalState naval;InteractionState interaction;SocialState social;EstateState estate;TaskState tasks;FamilyState family;PersonalState personal;GrowthState growth;GrowthRecoveryState personal_recovery;GrowthTacticLessonState growth_tactic_lessons;
    EstateEmploymentState estate_employment;
    PrivateShopLiquidationState private_shop_liquidations;
    PrivateShopDailyRentState private_shop_daily_rent;
    PrivateShopCommandResultsState private_shop_command_results;
    PrivateShopState private_shops;PoliticalShopLicensingState shop_licensing;PoliticalShopRegistrySuccessionState shop_registry;GrowthMarketHistoryState market_history;GrowthMarketCaptureState market_capture;
    EconomicSupplyPolicyState economic_supply_policies;PoliticalSupplyAdoptionState political_supply_adoptions;
    TreatyPassageState treaty_passage;
    PoliticalMilitaryAppointmentState political_military_appointments;
    PoliticalFieldAppointmentState political_field_appointments;
    DiplomacyState diplomacy;PoliticalTakeoverState political_takeover;EconomicTakeoverExecutionState economic_takeover_execution;
    OccupationFiscalState occupation_fiscal;OccupationFiscalSettlementState occupation_fiscal_settlement;
    std::optional<PoliticalGovernanceAgencyState> governance_agencies;
    std::optional<PoliticalOfficeResignationState> office_resignations;
    std::optional<PoliticalSubordinateAppointmentState> subordinate_appointments;
    PoliticsState politics;PoliticalFiscalState political_fiscal;PoliticsGovernanceState politics_governance;GovernanceTaskState governance_tasks;EnvoyTaskState envoy_tasks;FiscalSettlementState fiscal_settlement;PublicReliefState public_relief;
    LocalEconomyState economy;OrganizationState organization;AffairsState affairs;DirectorState director;PopulationMilitaryState population;
    explicit ProposalWriteSet(const WorldBundle& w):subordinate_appointments(w.political_subordinate_appointments),office_resignations(w.political_office_resignations),governance_agencies(w.political_governance_agencies),private_shop_liquidations(w.private_shop_liquidations),private_shops(w.private_shops),private_shop_daily_rent(w.private_shop_daily_rent),private_shop_command_results(w.private_shop_command_results),shop_licensing(w.political_shop_licensing),shop_registry(w.political_shop_registry_successions),market_history(w.growth_market_history),market_capture(w.growth_market_capture),travel(w.travel),military(w.military),military_tactic_learning(w.military_tactic_learning),naval(w.naval),interaction(w.interaction),social(w.social),estate(w.estate),estate_employment(w.estate_employment),economic_supply_policies(w.economic_supply_policies),political_supply_adoptions(w.political_supply_adoptions),tasks(w.tasks),family(w.family),personal(w.personal),growth(w.growth),personal_recovery(w.personal_recovery),growth_tactic_lessons(w.growth_tactic_lessons),diplomacy(w.diplomacy),treaty_passage(w.treaty_passage),political_military_appointments(w.political_military_appointments),political_field_appointments(w.political_field_appointments),political_takeover(w.political_takeover),economic_takeover_execution(w.economic_takeover_execution),politics(w.politics),political_fiscal(w.political_fiscal),occupation_fiscal(w.occupation_fiscal),occupation_fiscal_settlement(w.occupation_fiscal_settlement),politics_governance(w.politics_governance),governance_tasks(w.governance_tasks),envoy_tasks(w.envoy_tasks),fiscal_settlement(w.fiscal_settlement),public_relief(w.public_relief),economy(w.local_economy),organization(w.organization),affairs(w.affairs),director(w.director),population(w.population_military){}
    void restore(WorldBundle& w){w.political_subordinate_appointments=std::move(subordinate_appointments);w.political_office_resignations=std::move(office_resignations);w.political_governance_agencies=std::move(governance_agencies);w.treaty_passage=std::move(treaty_passage);w.political_military_appointments=std::move(political_military_appointments);w.political_field_appointments=std::move(political_field_appointments);w.political_takeover=std::move(political_takeover);w.economic_takeover_execution=std::move(economic_takeover_execution);w.diplomacy=std::move(diplomacy);w.politics=std::move(politics);w.political_fiscal=std::move(political_fiscal);w.occupation_fiscal=std::move(occupation_fiscal);w.occupation_fiscal_settlement=std::move(occupation_fiscal_settlement);w.politics_governance=std::move(politics_governance);w.governance_tasks=std::move(governance_tasks);w.envoy_tasks=std::move(envoy_tasks);w.fiscal_settlement=std::move(fiscal_settlement);w.public_relief=std::move(public_relief);w.travel=std::move(travel);invalidate_military_supply_lookup(w.military);w.military=std::move(military);w.military_tactic_learning=std::move(military_tactic_learning);w.naval=std::move(naval);w.interaction=std::move(interaction);w.social=std::move(social);w.estate=std::move(estate);w.estate_employment=std::move(estate_employment);w.economic_supply_policies=std::move(economic_supply_policies);w.political_supply_adoptions=std::move(political_supply_adoptions);w.tasks=std::move(tasks);w.family=std::move(family);w.personal=std::move(personal);w.growth=std::move(growth);w.personal_recovery=std::move(personal_recovery);w.growth_tactic_lessons=std::move(growth_tactic_lessons);w.local_economy=std::move(economy);w.organization=std::move(organization);w.affairs=std::move(affairs);w.director=std::move(director);w.population_military=std::move(population);w.private_shops=std::move(private_shops);w.private_shop_daily_rent=std::move(private_shop_daily_rent);w.private_shop_command_results=std::move(private_shop_command_results);w.private_shop_liquidations=std::move(private_shop_liquidations);w.political_shop_licensing=std::move(shop_licensing);w.political_shop_registry_successions=std::move(shop_registry);w.growth_market_history=std::move(market_history);w.growth_market_capture=market_capture;}
};
// 观察、真实命令和计划进度在一个子事务中；普通拒绝只在还原后记录一次复评。
Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){
    if(proposal.command){auto allowed=task_personal_acceptance_npc_command_allowed(proposal.command->kind);if(!allowed.ok)return allowed;}
    if(proposal.command){auto allowed=task_personal_cancellation_npc_command_allowed(proposal.command->kind);if(!allowed.ok)return allowed;}
    if(proposal.command){auto allowed=task_personal_report_npc_command_allowed(proposal.command->kind);if(!allowed.ok)return allowed;}
    if(proposal.command){auto allowed=task_personal_message_npc_command_allowed(proposal.command->kind);if(!allowed.ok)return allowed;}
    if(proposal.command){auto allowed=task_message_return_npc_command_allowed(w,proposal.command->kind,std::get_if<TaskCommand>(&proposal.command->data));if(!allowed.ok)return allowed;}
    if(w.catalog->task_shop_order_contacts_enabled&&proposal.command&&(proposal.command->kind=="task_shop_order_deliver"||(std::get_if<TaskShopOrderCommand>(&proposal.command->data)&&std::get<TaskShopOrderCommand>(proposal.command->data).kind==TaskShopOrderCommand::Kind::deliver)))return common_failure("TaskShopOrderContactOuterResultRequired");
    if(w.catalog->visit_message_receipts_enabled&&proposal.command&&proposal.subject_id==w.common.state().metadata.player_actor_id&&(std::holds_alternative<VisitMessageCommand>(proposal.command->data)||visit_message_command_kind(proposal.command->kind)))return common_failure("VisitMessageNpcActorRequired");
    if(proposal.command&&(std::holds_alternative<OpenKnownPersonalDialogue>(proposal.command->data)||proposal.command->kind=="open_known_personal_dialogue"))return common_failure("KnownPersonalDialoguePlayerRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalHeadHandoverCommand>(proposal.command->data)||is_political_head_handover_command(proposal.command->kind)))return common_failure("PoliticalHeadHandoverPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalSuccessorRoundCommand>(proposal.command->data)||is_political_successor_round_command(proposal.command->kind)))return common_failure("PoliticalSuccessorRoundPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalTermHandoverCommand>(proposal.command->data)||is_political_term_handover_command(proposal.command->kind)))return common_failure("PoliticalTermHandoverPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalOfficeReceptionCommand>(proposal.command->data)||is_political_office_reception_command(proposal.command->kind)))return common_failure("PoliticalOfficeReceptionPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalOfficeResignationCommand>(proposal.command->data)||(is_political_office_resignation_command(proposal.command->kind)&&!std::holds_alternative<AuthoredOfficeResignationCommand>(proposal.command->data))))return common_failure("PoliticalOfficeResignationPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalSubordinateAppointmentCommand>(proposal.command->data)||(is_political_subordinate_appointment_command(proposal.command->kind)&&!std::holds_alternative<AuthoredSubordinateAppointmentCommand>(proposal.command->data))))return common_failure("PoliticalSubordinateAppointmentPlayerCommandRequired");
    if(w.catalog->political_dismissal_sources&&proposal.command&&(proposal.command->kind=="political_dismiss"||(std::get_if<PoliticsCommand>(&proposal.command->data)&&std::holds_alternative<PoliticalDismissCommand>(*std::get_if<PoliticsCommand>(&proposal.command->data)))))return common_failure("PoliticalDismissalPlayerOnly");
    if(proposal.command&&(std::holds_alternative<PoliticalGovernanceAgencyCommand>(proposal.command->data)||(is_political_governance_agency_command(proposal.command->kind)&&!std::holds_alternative<AuthoredGovernanceAgencyCommand>(proposal.command->data))))return common_failure("PoliticalGovernanceAgencyPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PoliticalSuccessionCommand>(proposal.command->data)||is_political_succession_command(proposal.command->kind)))return common_failure("PoliticalSuccessionPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<VisitAppointmentCommand>(proposal.command->data)||std::holds_alternative<VisitAppointmentMeetingCommand>(proposal.command->data)||is_visit_appointment_command(proposal.command->kind)||is_visit_appointment_meeting_command(proposal.command->kind)))return common_failure("VisitAppointmentPlayerCommandRequired");
    // 同本人热入口的写前拒绝；不靠后续身份失败来恢复缺失的控制账保存集。
    if(proposal.command&&(std::holds_alternative<GrowthTacticLessonCommand>(proposal.command->data)||proposal.command->kind.rfind("tactic_lesson_",0)==0))return common_failure("TacticLessonPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<VoluntaryPathCommand>(proposal.command->data)||proposal.command->kind.rfind("family_voluntary_path_",0)==0||(w.family_voluntary_paths_enabled()&&(proposal.command->kind=="family_agree_succession"||proposal.command->kind=="family_cancel_succession"||proposal.command->kind=="family_continue_as"||proposal.command->kind=="family_retire"))))return common_failure("VoluntaryPathPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<CollateralControlCommand>(proposal.command->data)||proposal.command->kind.rfind("family_collateral_",0)==0))return common_failure("CollateralControlPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<BiologicalControlCommand>(proposal.command->data)||proposal.command->kind=="family_biological_accept"||proposal.command->kind=="family_biological_revoke"))return common_failure("BiologicalControlPlayerCommandRequired");
    if(proposal.command&&(w.family_biological_legacy_enabled()||w.family_collateral_legacy_enabled())&&(std::holds_alternative<FamilyLegacyCommand>(proposal.command->data)||proposal.command->kind.rfind("family_legacy_",0)==0))return common_failure("BiologicalControlPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<PrivateShopDailyRentCommand>(proposal.command->data)||private_shop_daily_rent_command_kind(proposal.command->kind)))return common_failure("PrivateShopDailyRentPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<GovernanceInvitationReplyCommand>(proposal.command->data)||is_governance_invitation_reply_command(proposal.command->kind)))return common_failure("GovernanceInvitationPlayerCommandRequired");
    if(proposal.command&&(std::holds_alternative<OpenBornPersonalDialogue>(proposal.command->data)||proposal.command->kind=="open_born_personal_dialogue"))return common_failure("BornPersonalDialoguePlayerRequired");
    if(proposal.transition&&proposal.transition->expected_revision!=w.common.revision())return common_failure("StaleDirectorRevision",proposal.subject_id);
    if(w.catalog->director->dynamic_subjects){const auto* registration=w.director.registrations.find(proposal.subject_id);if(!registration||!proposal.transition||registration->lifecycle!=DirectorLifecycle::active||registration->epoch!=proposal.transition->registration_epoch||proposal.subject_id==w.common.state().metadata.player_actor_id)return common_failure("StaleDirectorRegistration",proposal.subject_id);}
    auto mark=w.common.begin_savepoint();if(!mark.ok)return {false,{},mark.error};ProposalWriteSet saved(w);DecisionOutcome outcome;Status status=common_success();
    for(const auto& expected:proposal.expected_versions)if(w.common.read_version(expected.resource).version!=expected.version){status=common_failure("StaleResourceRecipe",expected.resource.id);break;}
    if(status.ok&&proposal.transition)for(const auto& id:proposal.transition->known_report_ids){status=observe_dynamic_report(w.common,w.social,id,{SubjectKind::actor,proposal.subject_id},tick);if(!status.ok)break;}
    if(status.ok&&proposal.command){auto prepared=prepare_command(w,proposal.subject_id,*proposal.command);if(!prepared.ok)status={false,{},prepared.error};else{auto applied=apply_command(w,proposal.subject_id,*prepared.value->operation);if(!applied.ok)status={false,{},applied.error};else{
        if(const auto* action=std::get_if<ActionStarted>(&applied.value.data))outcome.action_id=action->action_id;
        if(const auto* effects=std::get_if<OrganizationEffects>(&applied.value.data)){outcome.receipt_ids=effects->receipt_ids;for(const auto& id:effects->fact_ids)if(const auto* fact=w.organization.facts.find(id))if(!fact->activity_id.empty())outcome.org_activity_id=fact->activity_id;}
    }}}
    if(status.ok&&(w.growth_market_capture.open_scopes!=0||w.growth_market_capture.unbound_business))status=common_failure("GrowthMarketUnboundBusiness",{},ErrorKind::internal);
    if(status.ok){outcome.kind=DecisionOutcome::Kind::adopted;auto bound=bind_visit_reception_world(w,proposal);if(!bound.ok)status={false,{},bound.error};else status=record_director_result(w.director,bound.value,outcome,tick);if(status.ok){auto committed=w.common.commit_savepoint(mark.value);return committed;}}
    auto rollback=w.common.rollback_savepoint(mark.value);saved.restore(w);if(!rollback.ok)return rollback;
    if(status.error.kind!=ErrorKind::rule)return status;
    outcome={};outcome.kind=DecisionOutcome::Kind::rejected;outcome.code=status.error.original.code;
    auto recorded=record_director_result(w.director,proposal,outcome,tick);if(!recorded.ok){recorded.error.kind=ErrorKind::internal;return recorded;}return common_success();
}
}
Status reconcile_dynamic_director_registry(WorldBundle& w){if(!w.dynamic_enabled())return common_success();auto authority=w.services.organization_access();auto result=reconcile_director_registry(w.common,w.director,*w.catalog->director,authority,w.tasks,w.services,diplomatic_conversation_response(w));if(!result.ok)return result;auto attendance=wake_personal_appointment_attendance_world(w);if(!attendance.ok)return attendance;return wake_visit_appointment_attendance_world(w);}
// 旧供养仍由旧领域执行；宏观绑定不能同时把同一真人算进第二份真实口粮。
Status validate_dynamic_provision_owners(const WorldBundle& w,Integer tick){
    if(!w.dynamic_enabled())return common_success();
    std::set<StableId> fed;
    auto local=[&](const StableId& actor,const StableId& place){auto where=w.common.resolved_location(actor);auto* at=where.ok?std::get_if<SettlementLocation>(&where.value):nullptr;return at&&at->place_id==place;};
    for(const auto& row:w.estate.estates.rows()){
        const auto* definition=w.catalog->estate->estates.find(row.first);if(!definition)return common_failure("UnknownEstate",row.first);
        for(const auto& actor_id:row.second.resident_actor_ids){const auto* actor=w.common.state().actors.find(actor_id);const auto* duty=row.second.duties.find(actor_id);
            if(!actor||!actor->alive||!actor->available||actor->injury>0||!actor->action_id.empty()||row.second.detached.find(actor_id)||!local(actor_id,definition->place_id)||w.services.in_open_battle(actor_id)||w.services.in_interaction(actor_id,{}))continue;
            if(actor_id==row.second.manager_actor_id||(duty&&(*duty=="labor"||*duty=="guard")))fed.insert(actor_id);
        }
    }
    auto interaction=w.services.interaction_access();auto access=w.services.military_access();access.interaction_access=&interaction;
    for(const auto& army:w.military.armies)if(army.status=="active"){
        for(const auto& id:army.member_actor_ids){const auto* actor=w.common.actor(id);if(actor&&actor->alive)fed.insert(id);}
        fed.insert(army.prisoner_ids.begin(),army.prisoner_ids.end());
        for(const auto& id:army.unit_ids)for(const auto& unit:w.military.military_units)if(unit.id==id){fed.insert(unit.fit.begin(),unit.fit.end());fed.insert(unit.wounded.begin(),unit.wounded.end());}
    }
    for(const auto& camp:w.military.garrisons){const auto definition=std::find_if(w.catalog->military->garrisons.begin(),w.catalog->military->garrisons.end(),[&](const auto& d){return d.id==camp.id;});if(definition==w.catalog->military->garrisons.end())return common_failure("UnknownGarrison",camp.id);
        fed.insert(camp.prisoner_ids.begin(),camp.prisoner_ids.end());
        for(const auto& unit:w.military.military_units)if(unit.garrison_id==camp.id&&unit.army_id.empty()){
            fed.insert(unit.fit.begin(),unit.fit.end());fed.insert(unit.wounded.begin(),unit.wounded.end());
            const auto* actor=w.common.actor(unit.commander_actor_id);if(actor&&actor->alive&&actor->available&&local(actor->id,definition->place_id)){auto active=w.services.active_army(actor->id);if(!active.ok)return {false,{},active.error};if(!active.value)fed.insert(actor->id);}
            if(unit.training_action_id&&!unit.training_action_id->empty()){auto people=military_training_mouths(w.common,w.military,*w.catalog->military,access,unit,camp.id);if(!people.ok)return {false,{},people.error};fed.insert(people.value.begin(),people.value.end());}
        }
    }
    if(w.population_military_enabled()){auto context=w.population_read(tick>w.common.tick()?std::optional<Integer>{tick}:std::optional<Integer>{});auto continuity=population_military_continuity(context);if(!continuity.ok)return {false,{},continuity.error};for(const auto& person:continuity.value.provisions)if((person.status==PopulationSoldierStatus::fit||person.status==PopulationSoldierStatus::wounded||person.status==PopulationSoldierStatus::captured)&&!fed.count(person.member_id))return common_failure("PopulationMilitaryProvisionMissing",person.member_id);}
    for(const auto& row:w.local_economy.provision_bindings.rows()){const auto& binding=row.second;if(binding.policy_owner!=ProvisionOwner::macro||binding.subject.kind!=SubjectKind::actor||tick>binding.through_tick||!fed.count(binding.subject.id))continue;
        const auto* obligation=w.local_economy.obligations.find(binding.obligation_id);if(!obligation||!obligation->active||obligation->kind!=ObligationKind::ration)continue;
        if(obligation->period_ticks<=0||obligation->next_due_tick<0)return common_failure("InvalidRationSchedule",binding.obligation_id);
        Integer due=obligation->next_due_tick;const auto start=std::max(tick,binding.from_tick);
        if(due<start){const auto gap=start-due;const auto count=gap/obligation->period_ticks+(gap%obligation->period_ticks?1:0);Integer delta=0;if(__builtin_mul_overflow(count,obligation->period_ticks,&delta)||__builtin_add_overflow(due,delta,&due))return common_failure("EconomyTickOverflow",binding.obligation_id);}
        if(due<=binding.through_tick)return common_failure("ConflictingProvisionOwner",row.first);}
    for(const auto& row:w.local_economy.production_bindings.rows())if(row.second.policy_owner==ProvisionOwner::macro&&w.estate.estates.find(row.first))return common_failure("ConflictingProductionOwner",row.first);
    return common_success();
}
// 新局已有的实物缺口在零tick即可观察；只建真实来源/需求，不结算周期、不执行NPC、不投递消息。
Status bootstrap_dynamic_world(WorldBundle& w){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w);if(!takeover.ok)return {false,{},takeover.error};
    if(!w.dynamic_enabled())return common_failure("DynamicProfileRequired");
    if(w.common.tick()!=0||w.common.revision()!=0||!w.social.dynamic||w.social.dynamic->last_collected_tick!=-1||!w.affairs.needs.empty()||!w.tasks.offer_instances.empty())return common_failure("DynamicWorldAlreadyInitialized");
    auto initialized_fiscal=initialize_occupation_fiscal_world(w);if(!initialized_fiscal.ok)return initialized_fiscal;
    auto registered=reconcile_dynamic_director_registry(w);if(!registered.ok)return registered;
    auto supply_suspensions=takeover.value->supply_suspensions();if(!supply_suspensions.ok)return {false,{},supply_suspensions.error};
    auto observed=refresh_local_supply_observations(w.common,w.local_economy,*w.catalog->local_economy,0,takeover.value->economy());if(!observed.ok)return {false,{},observed.error};
    auto issued_observed=refresh_economic_supply_world(w);if(!issued_observed.ok)return {false,{},issued_observed.error};
    auto affairs=takeover.value->issued_supply()?reconcile_affairs_with_issued_supply(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,observed.value,w.services,0,*takeover.value->issued_supply(),takeover.value->tasks(),takeover.value->economy()?&supply_suspensions.value:nullptr):reconcile_affairs(w.common,w.affairs,*w.catalog->affairs,w.tasks,observed.value,w.services,0,takeover.value->tasks(),takeover.value->economy()?&supply_suspensions.value:nullptr);if(!affairs.ok)return affairs;
    auto published=takeover.value->issued_supply()?publish_need_offers_with_issued_supply(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,0,*takeover.value->issued_supply()):publish_need_offers(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,0);if(!published.ok)return published;
    // 显式官方订阅只排到下一半日；远方人物此刻仍没有这些知识。
    auto reports=collect_dynamic_reports(w.common,w.social,w.services,0,w.catalog->director.get(),&w.director);if(!reports.ok)return reports;if(w.catalog->director->dynamic_subjects)w.observation_index=build_world_observation_index(w.common,w.social,w.tasks);return common_success();
}
DynamicStepInput capture_dynamic_step(const WorldBundle& w,Integer to_tick){
    DynamicStepInput input;input.from_tick=w.common.tick();input.to_tick=to_tick;input.existing_action_ids=w.common.action_order();
    for(const auto& row:w.organization.activities.rows())if(row.second.status==OrgActivityStatus::approved||row.second.status==OrgActivityStatus::active||row.second.status==OrgActivityStatus::waiting)input.existing_activity_ids.push_back(row.first);
    if(w.social.dynamic)for(const auto& row:w.social.dynamic->pending_messages.rows())if(row.second.status==MessageStatus::pending&&row.second.created_tick<=input.from_tick&&row.second.due_tick<=to_tick)input.due_message_ids.push_back(row.first);
    return input;
}
Status finish_dynamic_boundary(WorldBundle& w,const DynamicStepInput& input){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w);if(!takeover.ok)return {false,{},takeover.error};
    if(!w.dynamic_enabled())return common_success();const auto tick=w.common.tick();if(tick!=input.to_tick)return common_failure("DynamicBoundaryTickMismatch",{},ErrorKind::internal);
    auto contracts=reconcile_task_contracts(w.common,w.tasks,w.affairs,tick,takeover.value->tasks());if(!contracts.ok)return contracts;
    auto advances=settle_task_advances_after_clock(w,takeover.value->tasks());if(!advances.ok)return advances;
    auto supply_suspensions=takeover.value->supply_suspensions();if(!supply_suspensions.ok)return {false,{},supply_suspensions.error};
    auto observed=refresh_local_supply_observations(w.common,w.local_economy,*w.catalog->local_economy,tick,takeover.value->economy());if(!observed.ok)return {false,{},observed.error};
    auto issued_observed=refresh_economic_supply_world(w);if(!issued_observed.ok)return {false,{},issued_observed.error};
    auto affairs=takeover.value->issued_supply()?reconcile_affairs_with_issued_supply(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,observed.value,w.services,tick,*takeover.value->issued_supply(),takeover.value->tasks(),takeover.value->economy()?&supply_suspensions.value:nullptr):reconcile_affairs(w.common,w.affairs,*w.catalog->affairs,w.tasks,observed.value,w.services,tick,takeover.value->tasks(),takeover.value->economy()?&supply_suspensions.value:nullptr);if(!affairs.ok)return affairs;
    auto published=takeover.value->issued_supply()?publish_need_offers_with_issued_supply(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,tick,*takeover.value->issued_supply()):publish_need_offers(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,tick);if(!published.ok)return published;
    auto members=reconcile_organization(w.common,w.organization,*w.catalog->organization,w.local_economy,w.services,tick);if(!members.ok)return {false,{},members.error};
    if(w.named_membership_enabled()){auto expired=expire_membership_offers(w.common,w.organization,*w.catalog->organization,tick);if(!expired.ok)return {false,{},expired.error};auto released=reconcile_named_membership(w.common,w.organization,*w.catalog->organization,w.local_economy,*w.catalog->local_economy,w.services,w.services,tick);if(!released.ok)return {false,{},released.error};}
    if(w.politics_enabled()){auto context=w.politics_write();auto reconciled=finish_political_business(context);if(!reconciled.ok)return {false,{},reconciled.error};}
    auto registered=reconcile_dynamic_director_registry(w);if(!registered.ok)return registered;
    auto supply_projected=project_economic_supply_world_in_candidate(w);if(!supply_projected.ok)return supply_projected;
    auto reports=collect_dynamic_reports(w.common,w.social,w.services,tick,w.catalog->director.get(),&w.director);if(!reports.ok)return reports;
    auto delivered=deliver_dynamic_reports(w.common,w.social,input);if(!delivered.ok)return delivered;return prune_regional_life(w);
}
bool director_routes_actor(const WorldBundle& w,const StableId& actor){
    if(!w.dynamic_enabled()||actor==w.common.state().metadata.player_actor_id||!director_subject_active(w.director,actor))return false;
    // 已承担军旅的真人继续军令责任；不会因为普通生活策略已登记而废弃现役返回链。
    for(const auto& army:w.military.armies)if(army.status=="active"&&(army.commander_actor_id==actor||std::find(army.member_actor_ids.begin(),army.member_actor_ids.end(),actor)!=army.member_actor_ids.end()))return false;
    return true;
}
// 同一提案事务供真实调度与必要故障注入共用，不建立测试专用提交路径。
Status apply_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){return adopt_director_proposal(w,proposal,tick);}
Status run_dynamic_decisions(WorldBundle& w,Integer tick){
    if(!w.dynamic_enabled())return common_success();auto registered=reconcile_dynamic_director_registry(w);if(!registered.ok)return registered;auto due=director_due_subjects(w.common,w.director,*w.catalog->director,w.tasks,tick);if(!due.ok)return {false,{},due.error};
    for(const auto& actor:due.value){if(!director_routes_actor(w,actor))continue;auto authority=w.services.organization_access();auto known=known_subject_view(w.common,w.social,authority,actor,&w.interaction);
        DirectorMembershipContext membership{w.social,w.local_economy,*w.catalog->local_economy,w.interaction,w.services,w.services};
        PoliticalNavigationRead navigation{w.travel,*w.catalog->travel,w.services};std::optional<DirectorPoliticalContext> politics;if(w.politics_enabled())politics.emplace(DirectorPoliticalContext{w.politics,*w.catalog->politics,w.local_economy,w.social,w.services,navigation});
        if(w.political_military_appointments_enabled()&&politics)politics->military_appointment=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_military_appointment_world(w,known,policy,base);};
        if(w.political_field_appointments_enabled()&&politics)politics->field_appointment=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_field_appointment_world(w,known,policy,base);};
        if(w.catalog->director->supply_renewals&&politics)politics->supply_renewal=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_supply_renewal_world(w,known,policy,base);};
        if(w.catalog->director->shop_registry_intents&&politics)politics->shop_registry_acceptance=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_shop_registry_director_world(w,known,policy,base);};
        if(w.catalog->director->office_resignation_responses&&politics)politics->office_resignation_response=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_office_resignation_response_world(w,known,policy,base);};
        if(w.catalog->political_subordinate_appointment_intents&&politics)politics->subordinate_appointment=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_subordinate_appointment_world(w,known,policy,base);};
        if(w.catalog->director->governance_agency_intents&&politics)politics->governance_agency=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_governance_agency_director_world(w,known,policy,base);};
        if(w.catalog->director->governance_intents&&politics)politics->governance_commission=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_governance_director_world(w,known,policy,base);};
        auto diplomatic_access=w.services.interaction_access();
        std::optional<DiplomaticPresentRead> diplomatic_present;std::optional<DirectorDiplomaticContext> diplomatic;
        if(w.diplomacy_enabled()&&(w.catalog->director->diplomatic_response||w.catalog->director->diplomatic_delegation||w.catalog->director->treaty_passage)){diplomatic_present.emplace(DiplomaticPresentRead{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,diplomatic_access});diplomatic.emplace(DirectorDiplomaticContext{*diplomatic_present,*w.catalog->common,w.services});}
        std::unique_ptr<TreatyPassageWorldAccess> treaty;if(w.treaty_passage_enabled()){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};treaty=std::move(access.value);if(diplomatic)diplomatic->treaty=&treaty->context();}
        std::unique_ptr<TaskResponsibilityDirectorWorldAccess> task_responsibility;if(w.catalog->director->authored_task_continuation&&w.catalog->director->authored_task_continuation->version==2){auto read=TaskResponsibilityDirectorWorldAccess::create(w,authority);if(!read.ok)return {false,{},read.error};task_responsibility=std::move(read.value);}
        auto issued=PoliticalTakeoverExecutionWorldAccess::create(w);if(!issued.ok)return {false,{},issued.error};
        std::unique_ptr<VisitReceptionWorldAccess> reception;if(w.catalog->director->visit_reception){auto access=VisitReceptionWorldAccess::create(w);if(!access.ok)return {false,{},access.error};reception=std::move(access.value);}
        auto written_attendance=visit_appointment_attendance_read_world(w);if(!written_attendance.ok)return {false,{},written_attendance.error};std::optional<PersonalAppointmentAttendanceRead> appointment_attendance;if(w.catalog->director->personal_appointment_attendance){if(!w.catalog->personal_appointments||!w.interaction.personal_appointments)return common_failure("AppointmentAttendanceProfileMismatch");appointment_attendance.emplace(PersonalAppointmentAttendanceRead{w.common,w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments,written_attendance.value?&*written_attendance.value:nullptr,w.catalog->visit_appointment_attendance.get()});}
        auto growth_access=w.services.growth_access();auto proposed=propose_director(w.common,w.director,*w.catalog->director,known,authority,w.affairs,w.tasks,*w.catalog->tasks,*w.catalog->common,*w.catalog->growth,*w.catalog->travel,w.organization,*w.catalog->organization,tick,w.regional_life_enabled()?&growth_access:nullptr,w.regional_life_enabled()?&w.services:nullptr,w.named_membership_enabled()?&membership:nullptr,politics?&*politics:nullptr,diplomatic?&*diplomatic:nullptr,issued.value->issued_supply(),task_responsibility?&task_responsibility->context():nullptr,reception?&reception->context():nullptr,appointment_attendance?&*appointment_attendance:nullptr);if(!proposed.ok)return {false,{},proposed.error};
        if(proposed.value.empty())continue;auto result=apply_director_proposal(w,proposed.value.front(),tick);if(!result.ok)return result;
    }
    return common_success();
}
} // 普通拒绝不免费重抽，内部错误使外层半日候选整体失败。
