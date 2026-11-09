#include "director_actor_read.hpp"
#include "director_actor_proposal2.hpp"
// 主体登记只派生共同人物和真实组织身份，不生成第二份人物、资格或可花资产。
#include "director_model.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace hanmo::resident {
namespace {
// 旧责任是否仍需本人处理由冻结合同与实际托管货物决定，欠薪债权不会强迫继续工作。
bool has_frozen_responsibility(const CommonStore& common,const TaskState& tasks,const StableId& actor) {
    for(const auto& row:tasks.tasks.rows()) {
        const auto& task=row.second;
        if(task.assignee!=actor)continue;
        if(task.status=="active"||task.status=="accepted"||task.status=="in_progress"||task.status=="ready")return true;
        if(task.status!="settling")continue;
        const auto* person=common.actor(actor);const auto* bag=person?common.inventory(person->inventory_id):nullptr;
        if(!bag)continue;
        for(const auto& lot:bag->lots)if(lot.quantity>0&&lot.owner.kind==Owner::Kind::task&&lot.owner.id==task.id)return true;
    }
    return false;
}

// 有序准入只选一份政策；组织成员的失效历史不能重新授予新职责。
const std::pair<StableId,DirectorAdmission>* admission_for(const DirectorCatalog& catalog,const OrganizationRead& organizations,const StableId& actor) {
    for(const auto& row:catalog.admissions.rows()) {
        const auto& rule=row.second;
        if(std::find(rule.excluded_actor_ids.begin(),rule.excluded_actor_ids.end(),actor)!=rule.excluded_actor_ids.end())continue;
        if(rule.kind==DirectorAdmissionKind::all_actors)return &row;
        if(rule.kind!=DirectorAdmissionKind::organization_members)continue;
        const auto member=organizations.member_view(actor,rule.organization_id);
        if(organizations.organization_active(rule.organization_id)&&member&&member->response!="departed"&&member->response!="awaiting_release")return &row;
    }
    return nullptr;
}

// 目录字段组合闭合；准入不代替后续具名命令对年龄、权限、资源及占用的检查。
Status validate_admissions(const CommonStore& common,const DirectorCatalog& catalog,bool actor_branch_only=false) {
    if(!catalog.dynamic_subjects)return common_success();
    if(!catalog.subjects.empty()||catalog.policies.empty()||catalog.policies.size()>64||catalog.admissions.empty()||catalog.admissions.size()>64)return common_failure("InvalidDirectorRegistryCatalog");
    for(const auto& row:catalog.admissions.rows()) {
        const auto& rule=row.second;
        if(row.first.empty()||!catalog.policies.find(rule.policy_id))return common_failure("UnknownDirectorPolicy",row.first);
        if(rule.kind!=DirectorAdmissionKind::all_actors&&rule.kind!=DirectorAdmissionKind::organization_members){if(actor_branch_only&&catalog.subjects2&&(rule.kind==DirectorAdmissionKind::household_cohorts||rule.kind==DirectorAdmissionKind::approved_organizations))continue;return common_failure("UnsupportedDirectorAdmission",row.first,ErrorKind::unsupported_capability);}
        if((rule.kind==DirectorAdmissionKind::all_actors&&!rule.organization_id.empty())||(rule.kind==DirectorAdmissionKind::organization_members&&rule.organization_id.empty()))return common_failure("InvalidDirectorAdmissionScope",row.first);
        if(rule.excluded_actor_ids.size()>4096)return common_failure("DirectorExclusionLimit",row.first);
        std::set<StableId> seen;
        for(const auto& id:rule.excluded_actor_ids)if(id.empty()||!common.actor(id)||!seen.insert(id).second)return common_failure("InvalidDirectorExcludedActor",id);
        const auto* policy=catalog.policies.find(rule.policy_id);
        const bool adult_business=policy->diplomatic_delegation_policy_id.has_value()||policy->diplomatic_response.has_value()||policy->political_career.has_value()||policy->named_membership_policy_id.has_value()||!policy->recruitment_posts.empty()||std::any_of(policy->templates.begin(),policy->templates.end(),[](DirectorTemplate kind){return kind==DirectorTemplate::office_resignation_response||kind==DirectorTemplate::governance_agency||kind==DirectorTemplate::governance_commission||kind==DirectorTemplate::treaty_ratification||kind==DirectorTemplate::private_livelihood||kind==DirectorTemplate::replenish_supply||kind==DirectorTemplate::fulfill_contract||kind==DirectorTemplate::organize_activity;});
        if(rule.minimum_age_years<0||rule.minimum_age_years>120||(adult_business&&rule.minimum_age_years<18))return common_failure("DirectorAdultPolicyAgeRequired",row.first);
        if(rule.kind==DirectorAdmissionKind::organization_members&&!policy->organization_id.empty()&&policy->organization_id!=rule.organization_id)return common_failure("DirectorPolicyOrganizationMismatch",row.first);
    }
    return common_success();
}
// 两种布局复用原人物生命周期；读取只提案，登记采用及世代变化仍由原写者负责。
template <class Registration>
Status evaluate_actor_lifecycle(
    const CommonStore &common, const DirectorCatalog &catalog,
    const ActivityRead &activity, const StableId &actor,
    const std::pair<StableId, DirectorAdmission> *selected, bool responsibility,
    const DirectorConversationResponse &response, Registration &registration,
    const std::function<Outcome<std::optional<Integer>>(const StableId &)>
        &original_age = {}) {
  const auto *person = common.actor(actor);
  if (!person)
    return common_failure("InvalidDirectorSubject", actor);
  registration.lifecycle = DirectorLifecycle::active;
  registration.reason =
      registration.obligations_only ? "ExistingResponsibilityOnly" : "Admitted";
  if (!person->alive) {
    registration.lifecycle = DirectorLifecycle::retired;
    registration.reason = "ActorDead";
  } else if (!selected && !responsibility) {
    registration.lifecycle = DirectorLifecycle::retired;
    registration.reason = "AdmissionWithdrawn";
  } else if (actor == common.state().metadata.player_actor_id) {
    registration.lifecycle = DirectorLifecycle::paused;
    registration.reason = "PlayerControlled";
  } else if (!person->available) {
    registration.lifecycle = DirectorLifecycle::paused;
    registration.reason = "ActorUnavailable";
  } else if (selected && selected->second.minimum_age_years > 0) {
    // Complete 只在原年龄门内借已验声明年龄；旧调用者仍逐字沿真实 birth_tick。
    std::optional<Integer> age;
    if (original_age) {
      auto read = original_age(actor);
      if (!read.ok)
        return {false, {}, std::move(read.error)};
      age = read.value;
    }
    const auto *identity = common.state().actors.find(actor);
    if (!age &&
        (!identity->birth_tick || *identity->birth_tick > common.tick())) {
      registration.lifecycle = DirectorLifecycle::paused;
      registration.reason = "ActorAgeUnknown";
    } else if (age ? *age < selected->second.minimum_age_years
                   : static_cast<__int128>(common.tick()) -
                             static_cast<__int128>(*identity->birth_tick) <
                         static_cast<__int128>(
                             selected->second.minimum_age_years) *
                             720) {
      registration.lifecycle = DirectorLifecycle::paused;
      registration.reason = "ActorBelowPolicyAge";
    } else if (!person->action_id.empty() || activity.in_open_battle(actor) ||
               activity.in_interaction(actor, {})) {
      registration.lifecycle = DirectorLifecycle::paused;
      registration.reason = "ActorOccupied";
    }
  } else if (!person->action_id.empty() || activity.in_open_battle(actor) ||
             activity.in_interaction(actor, {})) {
    registration.lifecycle = DirectorLifecycle::paused;
    registration.reason = "ActorOccupied";
  }
  if (registration.lifecycle == DirectorLifecycle::paused &&
      registration.reason == "ActorOccupied" && selected &&
      !registration.obligations_only && person->action_id.empty() &&
      !activity.in_open_battle(actor) && activity.in_interaction(actor, {}) &&
      (catalog.diplomatic_response || catalog.diplomatic_delegation ||
       catalog.treaty_passage || catalog.visit_reception) &&
      response) {
    const auto *policy = catalog.policies.find(registration.policy_id);
    if (policy && (policy->diplomatic_response ||
                   policy->diplomatic_delegation_policy_id ||
                   std::find(policy->templates.begin(), policy->templates.end(),
                             DirectorTemplate::treaty_ratification) !=
                       policy->templates.end() ||
                   std::find(policy->templates.begin(), policy->templates.end(),
                             DirectorTemplate::visit_reception) !=
                       policy->templates.end())) {
      auto allowed = response(actor, *policy);
      if (!allowed.ok)
        return {false, {}, allowed.error};
      if (allowed.value) {
        registration.lifecycle = DirectorLifecycle::active;
        registration.reason = catalog.visit_reception
                                  ? "AuthoredConversationReady"
                                  : "DiplomaticResponseReady";
      }
    }
  }
  return common_success();
}

} // namespace

// 仅 Complete
// 原读口传入延迟年龄来源；复用下方唯一生命周期，不新增登记或资格状态。
Outcome<std::optional<SubjectAdmissionRead2>>
read_native_director_actor_admission2(
    const CommonStore &c, const DirectorState &s,
    const DirectorCatalog &catalog, const OrganizationRead &organizations,
    const TaskState &tasks, const ActivityRead &activity, const StableId &actor,
    const DirectorConversationResponse &response,
    const std::function<Outcome<std::optional<Integer>>(const StableId &)>
        &original_age) {
  if (!s.subject2 || !catalog.subjects2 || !catalog.dynamic_subjects)
    return {false,
            {},
            common_failure("DirectorActorSubject2Required", actor).error};
  auto valid = validate_admissions(c, catalog, true);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *person = c.actor(actor);
  if (!person)
    return {false, {}, common_failure("InvalidDirectorSubject", actor).error};
  const auto *selected = admission_for(catalog, organizations, actor);
  const auto *prior = s.subject2->registrations.find(
      director_subject_key2({SubjectKind::actor, actor}));
  if (!selected && !prior)
    return {true, std::nullopt, {}};
  const FinitePlan *plan = nullptr;
  if (prior) {
    auto actual_plan = read_director_actor_plan(s, actor);
    if (!actual_plan.ok)
      return {false, {}, actual_plan.error};
    plan = actual_plan.value;
  }
  const bool responsibility =
      has_frozen_responsibility(c, tasks, actor) ||
      (plan && plan->pending_life_action_id.has_value());
  SubjectAdmissionRead2 out;
  auto &r = out.registration;
  if (prior)
    r = *prior;
  r.subject = {SubjectKind::actor, actor};
  if (!prior) {
    r.entered_tick = r.changed_tick = c.tick();
    r.epoch = 1;
  }
  r.obligations_only =
      responsibility &&
      (!selected || (prior && selected->second.policy_id != prior->policy_id));
  if (selected && !r.obligations_only) {
    r.policy_id = selected->second.policy_id;
    r.admission_id = selected->first;
  }
  r.admission_source = ActorDirectorAdmission{actor, r.admission_id};
  auto lifecycle =
      evaluate_actor_lifecycle(c, catalog, activity, actor, selected,
                               responsibility, response, r, original_age);
  if (!lifecycle.ok)
    return {false, {}, lifecycle.error};
  const auto *policy = catalog.policies.find(r.policy_id);
  if (!policy || policy->templates.empty())
    return {
        false, {}, common_failure("DirectorActorPolicyRequired2", actor).error};
  out.route = DirectorDecisionRoute2::actor;
  out.source_active = selected != nullptr;
  out.has_frozen_responsibility = responsibility;
  const auto fallback =
      std::find(policy->templates.begin(), policy->templates.end(),
                DirectorTemplate::await_adjustment);
  const auto *name = director_template_id(fallback != policy->templates.end()
                                              ? *fallback
                                              : policy->templates.front());
  if (!name)
    return {false,
            {},
            common_failure("DirectorActorTemplateRequired2", actor).error};
  out.template_id = name;
  return {true, std::optional<SubjectAdmissionRead2>{std::move(out)}, {}};
}

// 保留原公开签名；未提供 Complete 年龄源时，所有旧门与错误优先级保持不变。
Outcome<std::optional<SubjectAdmissionRead2>>
read_native_director_actor_admission2(
    const CommonStore &c, const DirectorState &s,
    const DirectorCatalog &catalog, const OrganizationRead &organizations,
    const TaskState &tasks, const ActivityRead &activity, const StableId &actor,
    const DirectorConversationResponse &response) {
  return read_native_director_actor_admission2(
      c, s, catalog, organizations, tasks, activity, actor, response, {});
}

// 有版本登记解析政策；旧profile仍按原actor键读取，不转换旧存档。
const DirectorPolicy* director_policy(const DirectorState& state,const DirectorCatalog& catalog,const StableId& actor) {
    if(!catalog.dynamic_subjects)return catalog.subjects.find(actor);
    const auto registration=director_actor_registration(state,actor);
    return registration&&registration->policy_id?catalog.policies.find(*registration->policy_id):nullptr;
}

// 生命周期只控制新的自主评估，已创建行动由其原领域继续推进。
bool director_subject_active(const DirectorState& state,const StableId& actor) {
    if(!state.dynamic_subjects)return state.schedules.find(actor)!=nullptr;
    const auto registration=director_actor_registration(state,actor);
    return registration&&registration->lifecycle==DirectorLifecycle::active;
}

// 登记变更先写域内候选；任何非法输入或epoch溢出均保留原登记、计划和复评游标。
Status reconcile_director_registry(const CommonStore& c,DirectorState& s,const DirectorCatalog& catalog,const OrganizationRead& organizations,const TaskState& tasks,const ActivityRead& activity){return reconcile_director_registry(c,s,catalog,organizations,tasks,activity,{}); }
Status reconcile_director_registry(const CommonStore& common,DirectorState& state,const DirectorCatalog& catalog,const OrganizationRead& organizations,const TaskState& tasks,const ActivityRead& activity,const DirectorConversationResponse& response) {
    if(state.subject2)return common_failure("DirectorSubject2ReconcileWriterRequired");
    if(!catalog.dynamic_subjects)return common_success();
    if(!state.dynamic_subjects)return common_failure("DirectorRegistryVersionMismatch");
    auto valid=validate_admissions(common,catalog);if(!valid.ok)return valid;
    if(common.actor_order().size()>4096)return common_failure("DirectorSubjectLimit");
    DirectorState candidate=state;
    for(const auto& actor_id:common.actor_order()) {
        const auto* person=common.actor(actor_id);if(!person)return common_failure("InvalidDirectorSubject",actor_id);
        const auto* selected=admission_for(catalog,organizations,actor_id);
        const auto* prior=candidate.registrations.find(actor_id);
        if(!selected&&!prior)continue;
        DirectorRegistration next;
        if(prior)next=*prior;
        else {next.actor_id=actor_id;next.policy_id=selected->second.policy_id;next.admission_id=selected->first;next.entered_tick=common.tick();next.changed_tick=common.tick();}
        const auto* plan=candidate.plans.find(actor_id);
        const bool awaiting_life=plan&&plan->pending_life_action_id.has_value();
        const bool responsibility=has_frozen_responsibility(common,tasks,actor_id)||awaiting_life;
        const bool binding_ends=prior&&(!selected||selected->second.policy_id!=prior->policy_id);
        if(binding_ends&&!responsibility&&plan){
            // 归档仅保已结束历史，不再执行、钉住临时活动或消费同一结算来源。
            if(plan->current_action_id&&common.action(*plan->current_action_id))return common_failure("DirectorPlanStillActive",actor_id);
            FinitePlan archived=*plan;archived.policy_id=prior->policy_id;archived.current_action_id.reset();archived.pending_life_action_id.reset();
            if(candidate.archived_plans.find(archived.id))return common_failure("DuplicateArchivedDirectorPlan",archived.id);
            const auto archive_id=archived.id;candidate.archived_plans.insert(archive_id,std::move(archived));candidate.plans.erase(actor_id);
            std::vector<StableId> history;for(const auto& row:candidate.archived_plans.rows())if(row.second.subject.kind==SubjectKind::actor&&row.second.subject.id==actor_id)history.push_back(row.first);
            while(history.size()>16){candidate.archived_plans.erase(history.front());history.erase(history.begin());}
        }
        // 冻结责任期间保持旧政策来源；新政策不能篡改已选履约计划及其历史。
        next.obligations_only=responsibility&&(!selected||(prior&&selected->second.policy_id!=prior->policy_id));
        if(selected&&!next.obligations_only){next.policy_id=selected->second.policy_id;next.admission_id=selected->first;}
        auto lifecycle=evaluate_actor_lifecycle(common,catalog,activity,actor_id,selected,responsibility,response,next);if(!lifecycle.ok)return lifecycle;
        if(prior&&(next.policy_id!=prior->policy_id||next.admission_id!=prior->admission_id||next.lifecycle!=prior->lifecycle||next.reason!=prior->reason||next.obligations_only!=prior->obligations_only)) {
            if(prior->epoch==std::numeric_limits<Integer>::max())return common_failure("DirectorRegistrationEpochOverflow",actor_id);
            next.epoch=prior->epoch+1;next.changed_tick=common.tick();
        }
        candidate.registrations.set(actor_id,next);
        if(!candidate.schedules.find(actor_id))candidate.schedules.insert(actor_id,{{SubjectKind::actor,actor_id},common.tick(),0,common.tick(),-1,{}, {}});
    }
    valid=validate_director_registry(common,candidate,catalog);if(!valid.ok)return valid;
    state=std::move(candidate);return common_success();
}

// 冷读校验只核已保存引用与时间，不重新选择政策、不重抽身份、不重放责任。
Status validate_director_registry(const CommonStore& common,const DirectorState& state,const DirectorCatalog& catalog) {
    if(state.dynamic_subjects!=catalog.dynamic_subjects)return common_failure("DirectorRegistryVersionMismatch");
    if(!catalog.dynamic_subjects)return state.registrations.empty()&&state.archived_plans.empty()?common_success():common_failure("DirectorRegistryCapabilityRequired");
    auto valid=validate_admissions(common,catalog);if(!valid.ok)return valid;
    if(state.registrations.size()!=state.schedules.size()||state.registrations.size()>4096)return common_failure("InvalidDirectorRegistryState");
    for(const auto& row:state.registrations.rows()) {
        const auto& registration=row.second;const auto* actor=common.actor(row.first);if(registration.lifecycle==DirectorLifecycle::active&&(!actor||!actor->alive||!actor->available||row.first==common.state().metadata.player_actor_id))return common_failure("InvalidActiveDirectorRegistration",row.first);
        if(registration.actor_id!=row.first||!common.actor(row.first)||!catalog.policies.find(registration.policy_id)||!catalog.admissions.find(registration.admission_id)||!state.schedules.find(row.first)||registration.epoch<1||registration.entered_tick<0||registration.changed_tick<registration.entered_tick||registration.changed_tick>common.tick()||registration.reason.empty()||static_cast<Integer>(registration.lifecycle)<0||static_cast<Integer>(registration.lifecycle)>2)return common_failure("InvalidDirectorRegistration",row.first);
    }
    std::map<StableId,Integer> archives;
    for(const auto& row:state.archived_plans.rows()){const auto& plan=row.second;if(row.first!=plan.id||plan.policy_id.empty()||!catalog.policies.find(plan.policy_id)||plan.subject.kind!=SubjectKind::actor||!common.actor(plan.subject.id)||plan.current_action_id||plan.pending_life_action_id||++archives[plan.subject.id]>16)return common_failure("InvalidArchivedDirectorPlan",row.first);}
    return common_success();
}
} // 退出只保存生命周期，债、货、事实、计划与共同身份始终由原所有者保留。
