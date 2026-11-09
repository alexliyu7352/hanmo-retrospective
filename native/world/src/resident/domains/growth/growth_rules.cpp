#include "resident/storage/money_transfer_recipe.hpp"
#include "resident/kernel/common_action_occupation.hpp"
#include "study_lesson_capability.hpp"
#include "growth_recovery_services.hpp"
// 课程、武学训练与装备沿原报价和半日规则；失败只返回原因，由世界候选统一回滚。
#include "growth_internal.hpp"
#include "study_service_sources.hpp"
#include "study_service_termination.hpp"
#include "growth_condition_services.hpp"
#include "growth_fixed_execution.hpp"
#include "growth_fixed_command.hpp"
#include "growth_started_participation.hpp"
#include "growth_equal.hpp"
#include "resident/domains/social/social_relation_execution.hpp"
#include <algorithm>
namespace hanmo::resident {
    namespace {
        template<class T> Outcome<T> fail(const StableId& code,const StableId& path={},ErrorKind kind=ErrorKind::rule){return {false,{},common_failure(code,path,kind).error};
        }
        template<class T> Outcome<T> error(const RuleError& e){return {false,{},e};
        }
        constexpr Integer thresholds[]={0,100,300,650,1200};
        Integer mastery(const ActorRecord& a,const StableId& id){auto* value=a.combat.learned.find(id);
            return value?*value:-1;
        }
        Integer earned(const StudyFact& f){Integer amount=0;
            const auto bp=f.challenge&&f.challenge->result?f.challenge->result->performance_bp:10000;
            for(const auto& lesson:f.lessons)amount+=lesson.xp*bp/10000;
            return amount;
        }
        GrowthEffect study_effect(const StudyFact& f){GrowthEffect r;
            r.kind=f.terms.study_teaching_agreement?"teach_study":"study_skill";
            r.success=f.success;
            r.code=f.code;
            r.skill_id=f.terms.skill_id;
            r.xp_gained=growth::study_xp_gained(f);
            r.completed_lessons=f.lessons.size();
            r.fee_paid=growth::original_study_lesson_cost(f.terms.xp_before)*r.completed_lessons;
            r.unprovided_released=f.terms.fee-r.fee_paid;
            r.challenge=f.challenge;
            return r;
        }
        // 迟答仅补已授课时的原表现差额，基础教学不可改写；候选事务失败由共同层整体回滚。
        Status settle_study_adjustments(CommonStore& common,StudyFact& f,const StudyAdjustmentWriteContext* exact){if(!growth::study_segmented(f)||!f.challenge||!f.challenge->result)return common_success();
            const auto bp=f.challenge->result->performance_bp;
            if(bp==10000)return common_success();
            auto* student=common.state().actors.find(f.terms.student_id);
            if(!student)return common_failure("StudyGrowthChanged");
            auto current=growth_skill_xp(student->skills,f.terms.skill_id);
            Integer application_ordinal=0;
            for(auto& lesson:f.lessons){if(!lesson.segment)return common_failure("InvalidStudySegment");
                auto& segment=*lesson.segment;
                if(!segment.performance_pending||segment.adjustment)continue;
                const auto delta=lesson.xp*bp/10000-lesson.xp;
                const auto gained=std::max<Integer>(0,std::min<Integer>(delta,f.terms.xp_limit-current));
                if(f.study_service_binding&&!exact)return common_failure("StudyExactAdjustmentWriterRequired",{},ErrorKind::unsupported_capability);
                segment.adjustment=StudyPerformanceAdjustment{exact?exact->actual_moment.half_day_tick:common.tick(),current,bp,gained,{}};
                if(exact){StudyAdjustmentExactBinding binding;binding.settled_at=exact->actual_moment;binding.application_ordinal=++application_ordinal;segment.adjustment->exact_binding=std::move(binding);}
                segment.field_order.push_back("adjustment");
                current+=gained;
            }
            auto skills=student->skills;auto r=growth_set_skill(skills,f.terms.skill_id,current);if(!r.ok)return r;
            ActorMutation mutation;mutation.actor_id=f.terms.student_id;mutation.skills=std::move(skills);return common.apply_actor(mutation);
        }
        // This is the already-adopted original FSL input, after the owning
        // permit's source check. Published/cold validation independently replays
        // its complete contribution refs; it is not a source-validation bypass.
        Outcome<Integer> adopted_study_xp(const GrowthState& state,const StudyFact& fact,const StableId& actor){
            const LifeFixedServiceBoundary* last=nullptr;const StableId* id=nullptr;
            for(const auto& row:state.facts.rows())if(std::get_if<StudyFact>(&row.second)==&fact){if(id)return fail<Integer>("StudyOriginalFactAmbiguous");id=&row.first;}
            if(!id||!fact.study_service_binding)return fail<Integer>("StudyActualSkillInputRequired");
            for(const auto& row:state.fixed_service_boundaries.rows())if(row.second.action_ref==*id&&(!last||row.second.boundary_ordinal>last->boundary_ordinal))last=&row.second;
            const GrowthSkillQualificationV2* selected=nullptr;if(last)for(const auto& person:last->input_snapshot.actors)if(person.actor_id==actor)for(const auto& skill:person.skill_qualification_views)if(skill.skill_id==fact.terms.skill_id){if(selected)return fail<Integer>("StudyActualSkillInputAmbiguous",*id);selected=&skill;}
            if(!selected||selected->actor_id!=actor||selected->coverage_kind!=GrowthSkillQualificationV2::Coverage::complete||selected->minimum_xp<0||selected->minimum_xp>1200||selected->actual_boundary_ref.source_schema!=1||selected->actual_boundary_ref.original_source_ref.domain!="growth"||selected->actual_boundary_ref.original_source_ref.kind!="growth_fixed_service_boundary"||selected->actual_boundary_ref.original_source_ref.id!=last->boundary_id)return fail<Integer>("StudyActualSkillInputRequired",*id);
            return {true,selected->minimum_xp,{}};
        }
        Status settle_study(CommonStore& common,const GrowthCatalog& catalog,StudyFact& f,bool preserve_segmented_challenge=false,const GrowthState* exact_state=nullptr,const GrowthFixedStepPermit* exact_gate=nullptr){if(f.challenge&&!(preserve_segmented_challenge&&growth::study_segmented(f)&&growth::challenge_pending(*f.challenge))){auto r=growth::challenge_close(catalog,*f.challenge);
                if(!r.ok)return r;
                f.challenge->consumed=true;
            }auto* student=common.state().actors.find(f.terms.student_id);
            if(!student)return common_failure("StudyGrowthChanged");
            const auto current=growth_skill_xp(student->skills,f.terms.skill_id),baseline=growth::study_xp_baseline(f);
            if(f.study_service_binding){
                if(!exact_state||!catalog.complete_exact_period_sources)return common_failure("StudyActualSkillInputRequired");auto adopted=adopted_study_xp(*exact_state,f,f.terms.student_id);if(!adopted.ok)return {false,{},adopted.error};Integer expected=adopted.value;
                // The final lesson has just applied its original XP, before its
                // owner appends the corresponding FSL InputChange/terminal.
                if(exact_gate){if(exact_gate->ordinal()!=static_cast<Integer>(f.lessons.size())||f.lessons.empty()||!f.lessons.back().segment)return common_failure("StudyActualFinalLessonRequired");const auto& segment=*f.lessons.back().segment;if(segment.xp_before!=expected||segment.ordinal!=exact_gate->ordinal()||segment.settled_tick!=exact_gate->at().half_day_tick||segment.adjustment)return common_failure("StudyActualFinalLessonInputMismatch");expected=segment.xp_before+segment.xp_gained;}
                if(current!=expected||f.xp_settlement_before)return common_failure("StudyGrowthChanged");
            }else if((growth::study_paused(f)?current<baseline:current!=baseline)||f.xp_settlement_before)return common_failure("StudyGrowthChanged");
            if(growth::study_segmented(f))return common_success();
            if(f.pauses){f.xp_settlement_before=current;
                f.field_order.push_back("xp_settlement_before");
            }
            auto skills=student->skills;
            auto r=growth_set_skill(skills,f.terms.skill_id,current+growth::study_xp_gained(f));
            if(!r.ok)return r;
            ActorMutation m;
            m.actor_id=student->id;
            m.skills=std::move(skills);
            return common.apply_actor(m);
        }
        Status validate_study_action(const CommonStore& common,const GrowthState& state,const ActionCoreView& a,const GrowthFixedStepPermit* exact=nullptr,const GrowthFixedCommandPermit* command_time=nullptr){auto* p=state.facts.find(a.id);
            auto* f=p?std::get_if<StudyFact>(p):nullptr;
            if(!f)return common_failure("MissingStudyFact");
            const auto& t=f->terms;
            if(f->phase!="started"||!f->code.empty()||a.remaining_ticks!=t.duration-Integer(f->lessons.size())||a.remaining_ticks<1||(!exact&&!command_time&&common.tick()-f->start_tick-Integer(f->lessons.size())!=growth::study_paused_ticks(*f,common.tick())))return common_failure("InvalidStudyProgress");
            auto* student=common.state().actors.find(t.student_id);
            if(!student)return common_failure("StudyGrowthChanged");
            const auto current=growth_skill_xp(student->skills,t.skill_id),baseline=growth::study_xp_baseline(*f);
            if(f->study_service_binding){if(!exact&&!command_time)return common_failure("StudyActualSkillInputRequired");auto adopted=adopted_study_xp(state,*f,t.student_id);if(!adopted.ok)return {false,{},adopted.error};if(current!=adopted.value)return common_failure("StudyGrowthChanged");}
            else if(growth::study_paused(*f)?current<baseline:current!=baseline)return common_failure("StudyGrowthChanged");
            if(growth::reserved(common.wallet(t.payer_wallet_id),a.id)!=t.fee-growth::original_study_lesson_cost(t.xp_before)*Integer(f->lessons.size()))return common_failure("StudyReservationMismatch");
            const bool paused=growth::study_paused(*f);
            const auto* payload=state.actions.find(a.id);
            const auto* terms=payload?std::get_if<StudyTerms>(&payload->quote.data):nullptr;
            const auto participants=paused?std::vector<StableId>{}:growth::study_participants(t);
            if(!terms||!growth::study_equal(t,*terms)||a.participants!=participants||a.actor_id!=growth::study_initiator(t)||a.start_tick!=f->start_tick)return common_failure("StudyFactMismatch");
            if(paused){auto* teacher=common.actor(t.teacher_id);
                if(!growth::study_pause_supported(*f)||!teacher||student->action_id==a.id||teacher->action_id==a.id)return common_failure("StudyOccupationMissing");
            }
            for(const auto& person:a.participants){auto* actor=common.actor(person);
                if(!actor||actor->action_id!=a.id)return common_failure("StudyOccupationMissing");
            }return common_success();
        }
        Status continue_study(const CommonStore& common,const GrowthAccess& access,const ActionCoreView& action,const StudyFact& f,bool exact_capability=false,const GrowthState* exact_state=nullptr){const auto& t=f.terms;
            for(const auto& person:action.participants){auto ready=growth::ready(common,access,person,t.skill_id,action.id,true);
                if(!ready.ok)return ready;
            }auto contact=access.contact(t.student_id,t.context,"study_skill",action.id);
            if(!contact.ok)return {false,{},contact.error};
            auto* teacher=common.state().actors.find(t.teacher_id);
            auto* student=common.state().actors.find(t.student_id);
            const auto teacher_xp=growth_skill_xp(teacher->skills,t.skill_id);
            Integer qualification_xp=t.xp_before;
            if(exact_capability){if(!exact_state||!f.study_service_binding)return common_failure("StudyActualSkillInputRequired");auto actual_teacher=adopted_study_xp(*exact_state,f,t.teacher_id),actual_student=adopted_study_xp(*exact_state,f,t.student_id);if(!actual_teacher.ok)return {false,{},actual_teacher.error};if(!actual_student.ok)return {false,{},actual_student.error};if(teacher_xp!=actual_teacher.value||growth_skill_xp(student->skills,t.skill_id)!=actual_student.value)return common_failure("StudyUnadoptedSkillChange");if(f.pauses)for(const auto& pause:*f.pauses)if(pause.end_tick>=0)qualification_xp=pause.student_xp;}
            else {if(!f.pauses&&teacher_xp!=t.teacher_xp)return common_failure("StudyTeacherChanged");if(f.pauses)for(const auto& pause:*f.pauses)if(pause.end_tick>=0)qualification_xp=pause.student_xp;}
            if((exact_capability||f.pauses)&&t.mode=="teacher"&&growth_skill_level(teacher_xp)<=growth_skill_level(qualification_xp))return common_failure("StudyTeacherNotQualified");
            auto* recipient=common.wallet(t.recipient_wallet_id);
            if(!recipient||recipient->balance>2147483647-growth::original_study_lesson_cost(t.xp_before))return common_failure("StudyPaymentBlocked");
            if(!exact_capability&&(student->fatigue!=growth::study_fatigue(f,f.lessons.size())||teacher->fatigue!=growth::study_fatigue(f,f.lessons.size(),true)))return common_failure("StudyConditionChanged");
            return common_success();
        }
    }
    // 完整目录按总体6.3分组；旧目录顺序保持原样，不改变旧页面与冷字段。
    const std::vector<StableId>& growth_skill_ids(bool extended){
        static const std::vector<StableId> legacy={"infantry","cavalry","archery","martial","tactics","debate","negotiation","rites","commerce","transport"};
        static const std::vector<StableId> complete={"infantry","cavalry","archery","naval","martial","tactics","espionage","fortification","agriculture","debate","negotiation","rites","commerce","transport","medicine","forging"};
        return extended?complete:legacy;
    }
    // 目录准入和人物实际拥有的块分开，未知ID及旧profile的新六项都不能报名。
    bool growth_skill_defined(const StableId& id,bool extended){const auto& ids=growth_skill_ids(extended);return std::find(ids.begin(),ids.end(),id)!=ids.end();}
    Integer growth_skill_xp(const ActorSkills& s,const StableId& id){
#define HM_SKILL(name) if(id==#name)return s.name
        HM_SKILL(infantry);
        HM_SKILL(cavalry);
        HM_SKILL(archery);
        HM_SKILL(martial);
        HM_SKILL(tactics);
        HM_SKILL(debate);
        HM_SKILL(negotiation);
        HM_SKILL(rites);
        HM_SKILL(commerce);
        HM_SKILL(transport);
#undef HM_SKILL
        if(s.extended){
#define HM_EXTENDED_SKILL(name) if(id==#name)return s.extended->name
            HM_EXTENDED_SKILL(naval);
            HM_EXTENDED_SKILL(espionage);
            HM_EXTENDED_SKILL(fortification);
            HM_EXTENDED_SKILL(agriculture);
            HM_EXTENDED_SKILL(medicine);
            HM_EXTENDED_SKILL(forging);
#undef HM_EXTENDED_SKILL
        }
        return -1;
    }
    Status growth_set_skill(ActorSkills& s,const StableId& id,Integer xp){if(xp<0||xp>1200)return common_failure("InvalidSkillExperience");
#define HM_SKILL(name) if(id==#name){s.name=xp;return common_success();}
        HM_SKILL(infantry);
        HM_SKILL(cavalry);
        HM_SKILL(archery);
        HM_SKILL(martial);
        HM_SKILL(tactics);
        HM_SKILL(debate);
        HM_SKILL(negotiation);
        HM_SKILL(rites);
        HM_SKILL(commerce);
        HM_SKILL(transport);
#undef HM_SKILL
        if(growth_skill_defined(id,true)){
            if(!s.extended)return common_failure("SkillProfileRequired");
#define HM_EXTENDED_SKILL(name) if(id==#name){s.extended->name=xp;return common_success();}
            HM_EXTENDED_SKILL(naval);
            HM_EXTENDED_SKILL(espionage);
            HM_EXTENDED_SKILL(fortification);
            HM_EXTENDED_SKILL(agriculture);
            HM_EXTENDED_SKILL(medicine);
            HM_EXTENDED_SKILL(forging);
#undef HM_EXTENDED_SKILL
        }
        return common_failure("InvalidSkillExperience");
    }
    Integer growth_skill_level(Integer xp){if(xp<0||xp>1200)return -1;
        for(Integer i=4;
        i>=0;
        --i)if(xp>=thresholds[i])return i;
        return 0;
    }
    Integer growth_lesson_xp(Integer fatigue,Integer injury){
        return 20*growth::original_study_efficiency(fatigue,injury).efficiency_bp/10000;
    }
    bool growth_action_kind(const StableId& k){return k=="study_skill"||k=="teach_study"||k=="learn_skill"||k=="practice_skill"||k=="teach_skill"||k=="service"||k=="trade"||k=="long_rest";
    }
    bool growth_busy_command(const StableId& k){return k=="long_rest"||k=="recovery_cancel"||k=="rest_cancel"||k=="study_cancel"||k=="study_pause"||k=="study_resume"||k=="study_etiquette_choose"||k=="study_etiquette_exit"||k=="study_challenge_choose"||k=="study_challenge_exit";
    }
    bool growth_command_kind(const StableId& k){return k=="trade_bargain_open"||k=="trade_bargain_choose"||k=="trade_bargain_exit"||growth_action_kind(k)||growth_busy_command(k)||k=="equip_weapon"||k=="equip_armor"||k=="configure_heart";
    }
    namespace growth {
        Integer original_skill_threshold(Integer level)noexcept{return level<0||level>4?-1:thresholds[level];}
        OriginalStudyEfficiency original_study_efficiency(Integer fatigue,Integer injury)noexcept{
            if(injury<0||injury>3)return {};
            const Integer fatigue_bp=fatigue<40?10000:fatigue<70?9000:fatigue<90?7500:6000;
            constexpr Integer injury_bp[]={10000,9000,7500,6000};
            return {fatigue_bp,injury_bp[injury],std::min(fatigue_bp,injury_bp[injury])};
        }
        Integer original_study_lesson_cost(Integer accepted_xp_before){return 20+10*(growth_skill_level(accepted_xp_before)+1);}
        // 仅最后一段允许未闭合；完整合法性由事实验证器逐段核对。
        bool study_paused(const StudyFact& f){return f.pauses&&!f.pauses->empty()&&f.pauses->back().end_tick==-1;
        }
        Status pause_study_in_candidate(CommonStore& common,GrowthState& state,const StableId& id,const WorldMoment& at){
            auto* raw=state.facts.find(id);auto* fact=raw?std::get_if<StudyFact>(raw):nullptr;const auto* current=common.action(id);
            if(!current||!fact||fact->phase!="started"||study_paused(*fact)||!study_pause_supported(*fact)||fact->lessons.size()>=static_cast<std::size_t>(fact->terms.duration)||at.half_day_tick<common.tick())return common_failure("StudyActualPauseUnavailable",id);
            auto canonical=encode_world_moment(at);if(!canonical.ok)return {false,{},canonical.error};
            const auto participants=study_participants(fact->terms);
            if(current->participants!=participants)return common_failure("StudyActualPauseOccupationMismatch",id);
            for(const auto& person:participants){const auto* actor=common.actor(person);if(!actor||actor->action_id!=id)return common_failure("StudyActualPauseOccupationMismatch",person);}
            auto changed=pause_action_participation_in_candidate(common,id);if(!changed.ok)return changed;
            if(!fact->pauses){fact->pauses.emplace();if(fact->field_order.empty()){fact->field_order={"kind","phase","actor_id","start_tick","completed_tick","success","terms","lessons","code"};if(fact->challenge)fact->field_order.push_back(fact->challenge->frozen.field);}fact->field_order.push_back("pauses");}
            fact->pauses->push_back(StudyPause{at.half_day_tick,-1,Integer(fact->lessons.size()),-1,-1,-1,{},std::nullopt});return common_success();
        }
        // 新政策允许原手动课程暂停；旧政策保持既有占用边界。
        bool study_pause_supported(const StudyFact& f){return study_segmented(f)||!f.terms.challenge||f.terms.challenge->mode=="automatic";
        }
        // 显式条款决定成长时点，旧事实缺标记时永不补发历史课时。
        bool study_segmented(const StudyFact& f){return f.terms.settlement_policy&&(*f.terms.settlement_policy=="per_lesson_v1"||*f.terms.settlement_policy=="innate_per_lesson_v1");
        }
        // 待答仅供有限展示使用；普通半日仍按原正常基线授课。
        bool study_waiting_challenge(const StudyFact& f){return study_segmented(f)&&f.challenge&&challenge_pending(*f.challenge);
        }
        // 开放段截至当前时刻；终态调用者传完成时刻，绝不继续累计历史课程。
        Integer study_paused_ticks(const StudyFact& f,Integer now){Integer total=0;
            if(f.pauses)for(const auto& pause:*f.pauses)total+=(pause.end_tick<0?now:pause.end_tick)-pause.start_tick;
            return total;
        }
        // 同一课时可多次恢复，以最近闭合段的实时疲劳为唯一新基线。
        Integer study_fatigue(const StudyFact& f,Integer index,bool teacher){Integer fatigue=teacher?f.terms.teacher_fatigue:f.terms.student_fatigue,after=0;
            if(f.pauses)for(const auto& pause:*f.pauses)if(pause.end_tick>=0&&pause.after_lessons<=index){fatigue=teacher?pause.teacher_fatigue:pause.student_fatigue;after=pause.after_lessons;}
            return std::min<Integer>(100,fatigue+(teacher?2:6)*(index-after));
        }
        // 暂停期间可从别处成长，恢复后仅接受已记录的新基线，课程原条款不变。
        Integer study_xp_baseline(const StudyFact& f){Integer value=f.terms.xp_before,credited=0;
            if(f.pauses)for(const auto& pause:*f.pauses)if(pause.end_tick>=0){value=pause.student_xp;credited=pause.credited_xp.value_or(0);}
            return study_segmented(f)?value+study_xp_gained(f)-credited:value;
        }
        // 实际课程贡献以终态前经验计算，超过原上限时绝不扣回外部经验。
        Integer study_xp_gained(const StudyFact& f){if(study_segmented(f)){Integer total=0;for(const auto& lesson:f.lessons)if(lesson.segment){total+=lesson.segment->xp_gained;if(lesson.segment->adjustment)total+=lesson.segment->adjustment->xp_gained;}return total;}
            const auto baseline=f.xp_settlement_before.value_or(f.terms.xp_before);
            return std::max<Integer>(0,std::min<Integer>(earned(f),f.terms.xp_limit-baseline));
        }
        Integer reserved(const WalletView* wallet,const StableId& id){if(wallet)for(const auto& r:wallet->reserved)if(r.action_id==id)return r.amount;
            return -1;
        }
        const GrowthCourse* course(const GrowthCatalog& c,const StableId& role,const StableId& id){auto* r=c.roles.find(role);
            if(!r)return nullptr;
            for(const auto& entry:r->courses)if(entry.skill_id==id)return &entry;
            return nullptr;
        }
        Status ready(const CommonStore& common,const GrowthAccess& access,const StableId& id,const StableId& skill,const StableId& own,bool study){auto* a=common.state().actors.find(id);
            if(!a||!a->alive||!a->available||a->hp<=0)return common_failure("ActorUnavailable");
            if(study){auto xp=growth_skill_xp(a->skills,skill);
                if(xp<0||xp>1200||a->fatigue<0||a->fatigue>100||a->injury<0||a->injury>3)return common_failure("InvalidStudyActor");
                if(a->injury==3&&(skill=="infantry"||skill=="cavalry"||skill=="archery"||skill=="martial"))return common_failure("StudyNeedsRecovery");
            }if(!a->action_id.empty()&&a->action_id!=own)return common_failure("ActorBusy");
            if(access.battle_occupied&&access.battle_occupied(id))return common_failure("BattlePending");
            return common_success();
        }
    }
// 首错依原preview先参数、可用性、接触、资格、支付；取消在公共ActorBusy之前接入。
    static Outcome<GrowthQuote> preview_growth_impl(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& id,const GrowthCommand& command,const StableId& own,const GrowthTeachingAgreement* admitted,const GrowthStudyTeachingAgreement* study_admitted=nullptr,const GrowthFixedCommandPermit* command_time=nullptr){
        const auto& kind=command.kind;
        GrowthQuote q;
        q.command=command;
        auto* actor=common.state().actors.find(id);
        if(!actor)return fail<GrowthQuote>("UnknownActor");
        if(kind=="recovery_cancel")return access.personal_recovery?access.personal_recovery->preview_cancel(common,state,id,command):fail<GrowthQuote>("PersonalInjuryRecoveryUnavailable",{},ErrorKind::unsupported_capability);
        if(kind.rfind("trade_bargain_",0)==0&&!catalog.economy)return fail<GrowthQuote>("EconomyUnavailable");
        if(kind.rfind("trade_bargain_",0)==0||(kind=="trade"&&command.bargain_quote_id))return growth::preview_bargain(common,state,catalog,shared,access,id,command,own);
        if((kind=="long_rest"||kind=="rest_cancel")&&!catalog.family)return fail<GrowthQuote>("FamilyUnavailable");
        if(kind=="long_rest"||kind=="rest_cancel")return growth::preview_rest(common,state,access,id,command,own);
        if(kind=="service"||kind=="trade")return growth::preview_service_trade(common,catalog,shared,access,id,command,own);
        if(!catalog.cultivation)return fail<GrowthQuote>("CultivationUnavailable");
        if(kind=="teach_study"){
            if(!catalog.study_teaching_requests.enabled)return fail<GrowthQuote>("StudyTeachingUnavailable");
            if(!command.context||command.context->kind||command.context->encounter_id||command.ticks<2||command.ticks>6||command.challenge_mode!="automatic"||!growth_skill_defined(command.skill_id,shared.extended_skills))return fail<GrowthQuote>("InvalidStudyTeachingRequest");
            GrowthStudyTeachingAgreement agreement;
            if(study_admitted)agreement=*study_admitted;
            else{
                if(!access.study_teaching_request)return fail<GrowthQuote>("StudyTeachingSourceRequired",{},ErrorKind::unsupported_capability);
                auto source=access.study_teaching_request(id,command.context->provider_actor_id,command.skill_id,command.ticks);
                if(!source.ok)return error<GrowthQuote>(source.error);
                agreement=std::move(source.value.agreement);
            }
            if(agreement.teacher_actor_id!=id||agreement.student_actor_id!=command.context->provider_actor_id||agreement.skill_id!=command.skill_id||agreement.duration!=command.ticks||agreement.accepted_tick!=common.tick()||agreement.accepted_revision!=common.state().identity.revision||!growth::context_equal(agreement.request_context,*command.context))return fail<GrowthQuote>("StudyTeachingAgreementBindingMismatch");
            for(const auto& row:state.facts.rows())if(const auto* prior=std::get_if<StudyFact>(&row.second);prior&&prior->terms.study_teaching_agreement){
                const auto& used=*prior->terms.study_teaching_agreement;
                if(std::tie(used.request_id,used.session_id,used.teacher_actor_id,used.student_actor_id)==std::tie(agreement.request_id,agreement.session_id,agreement.teacher_actor_id,agreement.student_actor_id))return fail<GrowthQuote>("StudyTeachingRequestConsumed");
            }
            const auto* role=shared.roles.find(agreement.course_role_id);if(!role)return fail<GrowthQuote>("StudyTeachingTeacherCourseRequired");
            GrowthCommand study;study.kind="study_skill";study.skill_id=command.skill_id;study.ticks=command.ticks;study.context=InteractionContext{role->place_id,id,role->id};
            auto original=preview_growth_impl(common,state,catalog,shared,social,access,agreement.student_actor_id,study,own,nullptr,nullptr);
            if(!original.ok)return original;
            auto* terms=std::get_if<StudyTerms>(&original.value.data);if(!terms||!access.validate_study_teaching_agreement)return fail<GrowthQuote>("StudyTeachingSourceRequired",{},ErrorKind::unsupported_capability);
            auto valid=access.validate_study_teaching_agreement(agreement,*terms);if(!valid.ok)return error<GrowthQuote>(valid.error);
            terms->study_teaching_agreement=agreement;terms->field_order.push_back("study_teaching_agreement");
            original.value.command=command;original.value.context=agreement.request_context;return original;
        }
        if(growth_busy_command(kind)){
            if(!actor->alive||!actor->available)return fail<GrowthQuote>("ActorUnavailable");
            auto* action=common.action(actor->action_id);
            if(kind=="study_pause"||kind=="study_resume"){
                action=common.action(command.action_id);
                if(!action||!growth::study_action_kind(action->kind)||action->actor_id!=id)return fail<GrowthQuote>("StaleStudyAction");
            }else if(kind=="study_cancel"){auto* requested=common.action(command.action_id);
                const auto* value=requested?state.facts.find(requested->id):nullptr;
                const auto* fact=value?std::get_if<StudyFact>(value):nullptr;
                if(requested&&growth::study_action_kind(requested->kind)&&requested->actor_id==id&&fact&&growth::study_paused(*fact))action=requested;
            }
            if(!action&&kind!="study_cancel"&&kind!="study_pause"&&kind!="study_resume"){
                const auto* row=state.facts.find(command.action_id);const auto* retained=row?std::get_if<StudyFact>(row):nullptr;
                if(retained&&retained->study_service_terminal&&retained->phase=="completed"&&growth::study_segmented(*retained)&&retained->terms.student_id==id&&retained->challenge&&!retained->challenge->consumed){
                    auto next=growth::challenge_transition(catalog,*retained->challenge,command);if(!next.ok)return error<GrowthQuote>(next.error);q.challenge_before=retained->challenge;q.data=std::move(next.value);return {true,std::move(q),{}};
                }
            }
            if(!action||!growth::study_action_kind(action->kind)||action->actor_id!=id)return fail<GrowthQuote>("NotStudying");
            if(command.action_id!=action->id)return fail<GrowthQuote>("StaleStudyAction");
            if(catalog.complete_exact_period_sources&&!command_time)return fail<GrowthQuote>("GrowthFixedCommandPermitRequired",action->id,ErrorKind::unsupported_capability);
            auto valid=validate_study_action(common,state,*action,nullptr,command_time);
            if(!valid.ok)return error<GrowthQuote>(valid.error);
            const auto& fact=std::get<StudyFact>(*state.facts.find(action->id));
            if(kind=="study_cancel"){auto n=Integer(fact.lessons.size()),paid=growth::original_study_lesson_cost(fact.terms.xp_before)*n;
                q.data=StudyCancellation{action->id,n,paid,fact.terms.fee-paid};
                return {true,std::move(q),{}};
            }
            if(kind=="study_pause"||kind=="study_resume"){
                if(!growth::study_pause_supported(fact))return fail<GrowthQuote>("StudyPauseUnsupported");
                const bool paused=growth::study_paused(fact);
                if(kind=="study_pause"&&paused)return fail<GrowthQuote>("StudyAlreadyPaused");
                if(kind=="study_resume"&&!paused)return fail<GrowthQuote>("StudyNotPaused");
                if(kind=="study_resume"){for(const auto& person:{fact.terms.student_id,fact.terms.teacher_id}){auto ready=growth::ready(common,access,person,fact.terms.skill_id,{},true);
                        if(!ready.ok)return error<GrowthQuote>(ready.error);
                    }auto contact=access.contact(fact.terms.student_id,fact.terms.context,"study_skill",{});
                    if(!contact.ok)return error<GrowthQuote>(contact.error);
                    const auto student_xp=growth_skill_xp(common.state().actors.find(fact.terms.student_id)->skills,fact.terms.skill_id);
                    if(student_xp>=fact.terms.xp_limit)return fail<GrowthQuote>("StudyResumeLevelLimit");
                    auto* teacher=common.state().actors.find(fact.terms.teacher_id);
                    if(fact.terms.mode=="teacher"&&growth_skill_level(growth_skill_xp(teacher->skills,fact.terms.skill_id))<=growth_skill_level(student_xp))return fail<GrowthQuote>("StudyTeacherNotQualified");
                    auto* recipient=common.wallet(fact.terms.recipient_wallet_id);
                    if(!recipient||recipient->balance>2147483647-growth::original_study_lesson_cost(fact.terms.xp_before))return fail<GrowthQuote>("StudyPaymentBlocked");
                }
                q.data=StudyPauseChange{action->id,kind=="study_pause",growth::study_paused_ticks(fact,command_time?command_time->at().half_day_tick:common.tick()),Integer(fact.lessons.size()),fact.terms.fee-growth::original_study_lesson_cost(fact.terms.xp_before)*Integer(fact.lessons.size())};
                return {true,std::move(q),{}};
            }
            if(growth::study_paused(fact))return fail<GrowthQuote>("StudyPaused");
            if(!fact.challenge)return fail<GrowthQuote>("StudyChallengeNotOffered");
            auto next=growth::challenge_transition(catalog,*fact.challenge,command);
            if(!next.ok)return error<GrowthQuote>(next.error);
            q.challenge_before=fact.challenge;
            q.data=std::move(next.value);
            return {true,std::move(q),{}};
        }
        if(kind=="study_skill"){if(!growth_skill_defined(command.skill_id,shared.extended_skills)||command.ticks<2||command.ticks>6||!command.context)return fail<GrowthQuote>("InvalidStudyRequest");if(command.context->kind||command.context->encounter_id)return fail<GrowthQuote>("InvalidStudyContext");
        }
        auto ready=growth::ready(common,access,id,command.skill_id,own,kind=="study_skill");
        if(!ready.ok)return error<GrowthQuote>(ready.error);
        if(kind=="equip_weapon"||kind=="equip_armor"){
            const auto& selected=kind=="equip_weapon"?command.weapon_id:command.armor_id;
            EquipmentChoice choice{selected,0};
            if(!selected.empty()){auto* item=shared.items.find(selected);
                if(!item||(kind=="equip_weapon"?!item->weapon:!item->armor))return fail<GrowthQuote>(kind=="equip_weapon"?"NotAWeapon":"NotAnArmor");
                if(!common.inventory(actor->inventory_id))return fail<GrowthQuote>("MissingPersonalInventory");
                auto count=common.count(actor->inventory_id,selected,{Owner{Owner::Kind::actor,id,{}, {}}});
                if(!count.ok)return error<GrowthQuote>(count.error);
                if(count.value<1)return fail<GrowthQuote>(kind=="equip_weapon"?"WeaponNotOwned":"ArmorNotOwned");
                if(kind=="equip_armor"){auto defense=access.armor_defense?access.armor_defense(selected):std::nullopt;
                    if(!defense)return fail<GrowthQuote>("NotAnArmor");
                    choice.defense=*defense;
            }}
            q.data=std::move(choice);
            return {true,std::move(q),{}};
        }
        if(kind=="configure_heart"&&command.skill_id.empty()){q.data=EquipmentChoice{};
            return {true,std::move(q),{}};
        }
        if(kind=="study_skill"){
            if(command.ticks<2||command.ticks>6||!command.context||command.context->kind)return fail<GrowthQuote>("InvalidStudyRequest");
            auto contact=access.contact(id,*command.context,kind,own);
            if(!contact.ok)return error<GrowthQuote>(contact.error);
            for(const auto& row:state.facts.rows())if(auto* f=std::get_if<StudyFact>(&row.second))if(f->phase=="started"&&f->actor_id==id&&row.first!=own)return fail<GrowthQuote>("StudyAlreadyPending");
            auto* teacher=common.state().actors.find(contact.value.provider_actor_id);
            ready=growth::ready(common,access,contact.value.provider_actor_id,command.skill_id,own,true);
            if(!ready.ok)return error<GrowthQuote>(ready.error);
            auto* course=growth::course(catalog,contact.value.role_id,command.skill_id);
            if(!course)return fail<GrowthQuote>("StudyCourseNotOffered");
            const auto xp=growth_skill_xp(actor->skills,command.skill_id),teacher_xp=growth_skill_xp(teacher->skills,command.skill_id),target=growth_skill_level(xp)+1;
            Integer limit=300;
            if(target>4)return fail<GrowthQuote>("StudyAlreadyMastered");
            if(course->mode=="teacher"){if(growth_skill_level(teacher_xp)<=growth_skill_level(xp))return fail<GrowthQuote>("StudyTeacherNotQualified");
                limit=growth::original_skill_threshold(growth_skill_level(teacher_xp));
            }if(xp>=limit)return fail<GrowthQuote>("StudyCourseLevelLimit");
            const auto* role=shared.roles.find(contact.value.role_id);
            auto* payer=common.wallet(actor->wallet_id);
            auto* recipient=common.wallet(course->wallet_id);
            const Owner expected=role&&role->organization_id?Owner{Owner::Kind::organization,*role->organization_id,{}, {}}:Owner{Owner::Kind::actor,teacher->id,{}, {}};
            if(!payer||!owner_equal(payer->owner,Owner{Owner::Kind::actor,id,{}, {}}))return fail<GrowthQuote>("PersonalPaymentRequired");
            if(!recipient||!owner_equal(recipient->owner,expected))return fail<GrowthQuote>("StudyPaymentAuthority");
            if(payer->id==recipient->id)return fail<GrowthQuote>("SelfPayment");
            const auto fee=growth::original_study_lesson_cost(xp)*command.ticks;
            auto money=common.spendable(payer->id);
            if(!money.ok)return error<GrowthQuote>(money.error);
            if(money.value<fee)return fail<GrowthQuote>("InsufficientFunds");
            if(recipient->balance>2147483647-fee)return fail<GrowthQuote>("MoneyOverflow");
            StudyTerms t;
            t.skill_id=command.skill_id;
            t.mode=course->mode;
            t.student_id=id;
            t.teacher_id=teacher->id;
            t.context=contact.value;
            t.duration=command.ticks;
            t.xp_before=xp;
            t.teacher_xp=teacher_xp;
            t.student_fatigue=actor->fatigue;
            t.teacher_fatigue=teacher->fatigue;
            t.xp_limit=limit;
            t.fee=fee;
            t.payer_wallet_id=payer->id;
            t.recipient_wallet_id=recipient->id;
            if(course->settlement_policy){t.settlement_policy=course->settlement_policy;t.field_order={"skill_id","mode","student_id","teacher_id","context","duration","xp_before","teacher_xp","student_fatigue","teacher_fatigue","xp_limit","fee","payer_wallet_id","recipient_wallet_id","settlement_policy"};}
            if(t.settlement_policy&&*t.settlement_policy=="innate_per_lesson_v1"){
                if(!catalog.innate_learning||!access.innate_study_source)return fail<GrowthQuote>("InnateStudySourceRequired");
                auto innate=access.innate_study_source(id,command.skill_id);if(!innate.ok)return error<GrowthQuote>(innate.error);
                const auto& stamp=innate.value.source_identity;const auto& current=common.state().identity;const auto& source=innate.value.source;
                if(std::tie(stamp.world_id,stamp.execution_epoch,stamp.revision,stamp.tick)!=std::tie(current.world_id,current.execution_epoch,current.revision,current.tick)||source.actor_id!=id||source.skill_id!=command.skill_id||source.observed_tick!=common.tick())return fail<GrowthQuote>("StaleInnateStudySource");
                auto valid=innate_study_lesson_xp(0,source.bonus_bp);if(!valid.ok)return error<GrowthQuote>(valid.error);
                t.innate_source=source;t.field_order.push_back("innate_source");
            }
            auto challenge=growth::challenge_quote(catalog,*course,*actor,command.challenge_mode);
            if(!challenge.ok)return error<GrowthQuote>(challenge.error);
            if(!challenge.value.field.empty()){t.challenge=std::move(challenge.value);if(t.settlement_policy)t.field_order.push_back(t.challenge->field);}
            Integer estimate=0;
            for(Integer n=0;
            n<command.ticks;
            ++n){auto base=growth_lesson_xp(std::min<Integer>(100,actor->fatigue+6*n),actor->injury);auto gain=innate_study_lesson_xp(base,t.innate_source?t.innate_source->bonus_bp:0);if(!gain.ok)return error<GrowthQuote>(gain.error);estimate+=gain.value;}
            q.duration=command.ticks;
            q.cost=fee;
            q.expected_xp=std::min(limit,xp+estimate)-xp;
            q.target_level=target;
            q.context=contact.value;
            q.data=std::move(t);
            return {true,std::move(q),{}};
        }
        auto skill=access.personal_skill?access.personal_skill(command.skill_id):std::nullopt;
        if(!skill)return fail<GrowthQuote>("UnknownPersonalSkill");
        if(kind=="configure_heart"){if(skill->kind!="heart"||mastery(*actor,command.skill_id)<0)return fail<GrowthQuote>("HeartNotLearned");
            q.data=EquipmentChoice{command.skill_id,0};
            return {true,std::move(q),{}};
        }
        if(actor->injury>=3||actor->fatigue>=90)return fail<GrowthQuote>("TrainingNeedsRest");
        auto location=common.resolved_location(id);
        if(!location.ok)return error<GrowthQuote>(location.error);
        auto* place=std::get_if<SettlementLocation>(&location.value);
        if(!place||!shared.places.find(place->place_id))return fail<GrowthQuote>("TrainingPlaceRequired");
        TrainingTerms t;
        t.skill_id=command.skill_id;
        t.student_id=id;
        t.place_id=place->place_id;
        if(kind=="practice_skill"){if(command.ticks<1||command.ticks>8)return fail<GrowthQuote>("InvalidPracticeDuration");
            t.duration=command.ticks;
        }
        else {if(command.context&&command.context->kind)return fail<GrowthQuote>("TrainingPlaceRequired");
            if(!command.context)return fail<GrowthQuote>("InvalidInteractionContext");
            auto contact=access.contact(id,*command.context,kind=="learn_skill"?"learn_skill":"",own);
            if(!contact.ok)return error<GrowthQuote>(contact.error);
            q.context=contact.value;
            auto provider=contact.value.provider_actor_id;
            ready=growth::ready(common,access,provider,{},own,false);
            if(!ready.ok)return error<GrowthQuote>(ready.error);
            auto* p=common.state().actors.find(provider);
            if(p->injury>=3||p->fatigue>=90)return fail<GrowthQuote>("TeacherNeedsRest");
            t.teacher_id=kind=="learn_skill"?provider:id;
            t.student_id=kind=="learn_skill"?id:provider;
            auto* teacher=common.state().actors.find(t.teacher_id);
            if(mastery(*teacher,t.skill_id)<80)return fail<GrowthQuote>("TeacherNotQualified");
            if(kind=="learn_skill"){auto* role=catalog.roles.find(contact.value.role_id);
                const GrowthLesson* lesson=nullptr;
                if(role)for(const auto& entry:role->lessons)if(entry.skill_id==t.skill_id){lesson=&entry;
                    break;
                }if(!lesson)return fail<GrowthQuote>("LessonNotOffered");
                if(social_trust(social,t.teacher_id,t.student_id)<lesson->required_trust)return fail<GrowthQuote>("TeacherTrustRequired");
                t.duration=lesson->study_ticks;
                t.fee=lesson->fee;
                t.recipient_wallet_id=lesson->wallet_id;
                auto* definition=shared.roles.find(contact.value.role_id);
                Owner owner=definition&&definition->organization_id?Owner{Owner::Kind::organization,*definition->organization_id,{}, {}}:Owner{Owner::Kind::actor,t.teacher_id,{}, {}};
                auto* wallet=common.wallet(t.recipient_wallet_id);
                if(!wallet||!owner_equal(wallet->owner,owner))return fail<GrowthQuote>("LessonPaymentAuthority");
            }else t.duration=2;
        }
        auto* student=common.state().actors.find(t.student_id);
        if(access.skill_eligible){auto qualified=access.skill_eligible(*student,t.skill_id);
            if(!qualified.ok)return error<GrowthQuote>(qualified.error);
        }
        if(kind=="teach_skill"&&catalog.teaching_requests.enabled){
            if(admitted)t.teaching_agreement=*admitted;
            else {
                if(!access.teaching_request)return fail<GrowthQuote>("TeachingRequestSourceRequired");
                auto offered=access.teaching_request(t.teacher_id,t.student_id,t.skill_id);if(!offered.ok)return error<GrowthQuote>(offered.error);
                t.teaching_agreement=std::move(offered.value.agreement);
            }
            const auto& agreement=*t.teaching_agreement;
            if(agreement.teacher_actor_id!=t.teacher_id||agreement.student_actor_id!=t.student_id||agreement.skill_id!=t.skill_id||!q.context||!growth::context_equal(agreement.context,*q.context)||agreement.accepted_tick!=common.tick()||agreement.accepted_revision!=common.state().identity.revision)return fail<GrowthQuote>("TeachingAgreementBindingMismatch");
            if(!access.validate_teaching_agreement)return fail<GrowthQuote>("TeachingRequestSourceRequired");
            const auto valid=access.validate_teaching_agreement(agreement);if(!valid.ok)return error<GrowthQuote>(valid.error);
        }
        t.mastery_before=mastery(*student,t.skill_id);
        if(kind=="learn_skill"){if(t.mastery_before>=0)return fail<GrowthQuote>("SkillAlreadyLearned");
            t.mastery_after=0;
        }
        else if(kind=="practice_skill"){if(t.mastery_before<0)return fail<GrowthQuote>("SkillNotLearned");
            if(t.mastery_before>=100)return fail<GrowthQuote>("SkillAlreadyMastered");
            if(skill->practice_ticks<1)return fail<GrowthQuote>("InvalidTrainingGrowth");
            Integer gain=((100+skill->practice_ticks-1)/skill->practice_ticks)*t.duration;
            if(student->fatigue>=60)gain=std::max<Integer>(1,gain/2);
            t.mastery_after=std::min<Integer>(100,t.mastery_before+gain);
        }
        else {auto teacher_mastery=mastery(*common.state().actors.find(t.teacher_id),t.skill_id);
            if(t.mastery_before>=teacher_mastery)return fail<GrowthQuote>("StudentAlreadyQualified");
            t.mastery_after=std::min(teacher_mastery,std::max<Integer>(0,t.mastery_before)+10);
        }
        if(kind=="learn_skill"){t.payer_wallet_id=actor->wallet_id;
            auto* payer=common.wallet(t.payer_wallet_id);
            if(!payer||!owner_equal(payer->owner,Owner{Owner::Kind::actor,id,{}, {}}))return fail<GrowthQuote>("PersonalPaymentRequired");
            if(t.payer_wallet_id==t.recipient_wallet_id)return fail<GrowthQuote>("SelfPayment");
            auto money=common.spendable(t.payer_wallet_id);
            if(!money.ok)return error<GrowthQuote>(money.error);
            if(money.value<t.fee)return fail<GrowthQuote>("InsufficientFunds");
            if(common.wallet(t.recipient_wallet_id)->balance>2147483647-t.fee)return fail<GrowthQuote>("MoneyOverflow");
        }
        t.student_fatigue=t.duration*(kind=="practice_skill"?6:4);
        t.teacher_fatigue=t.teacher_id.empty()?0:t.duration*2;
        q.duration=t.duration;
        q.cost=t.fee;
        q.data=std::move(t);
        return {true,std::move(q),{}};
    }
// 普通报价只能从当前真实邀学产生来源；只有开工内部复核可接占用前准入。
    Outcome<GrowthQuote> preview_growth(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& id,const GrowthCommand& command,const StableId& own){return preview_growth_impl(common,state,catalog,shared,social,access,id,command,own,nullptr);}
// 共同kernel须在分配行动ID/占用人物之前调用；旧模式返回空准入，不增加保存字段。
    Outcome<std::optional<GrowthTeachingStartAdmission>> prepare_growth_start(const CommonStore& common,const GrowthCatalog& catalog,const GrowthAccess& access,const StableId& actor,const GrowthQuote& quote){
        if(quote.command.kind=="teach_study"){
            const auto* terms=std::get_if<StudyTerms>(&quote.data);
            if(!catalog.study_teaching_requests.enabled||!access.study_teaching_request||!access.revalidate_study_teaching_request)return {false,{},common_failure("StudyTeachingSourceRequired",{},ErrorKind::unsupported_capability).error};
            if(!terms||!terms->study_teaching_agreement||terms->teacher_id!=actor)return {false,{},common_failure("StudyTeachingQuoteSourceMismatch",{},ErrorKind::internal).error};
            auto source=access.study_teaching_request(actor,terms->student_id,terms->skill_id,terms->duration);if(!source.ok)return {false,{},source.error};
            auto valid=access.revalidate_study_teaching_request(source.value);if(!valid.ok)return {false,{},valid.error};
            if(!growth_study_teaching_agreement_equal(*terms->study_teaching_agreement,source.value.agreement))return {false,{},common_failure("StaleStudyTeachingQuote").error};
            GrowthTeachingStartAdmission admission;admission.source_identity=common.state().identity;admission.study_agreement=std::move(source.value.agreement);return {true,std::move(admission),{}};
        }
        if(quote.command.kind!="teach_skill"||!catalog.teaching_requests.enabled)return {true,std::nullopt,{}};
        const auto* terms=std::get_if<TrainingTerms>(&quote.data);
        if(!access.teaching_request||!access.revalidate_teaching_request)return {false,{},common_failure("TeachingRequestSourceRequired",{},ErrorKind::unsupported_capability).error};
        if(!terms||!terms->teaching_agreement||terms->teacher_id!=actor)return {false,{},common_failure("TeachingRequestQuoteSourceMismatch",{},ErrorKind::internal).error};
        auto current=access.teaching_request(actor,terms->student_id,terms->skill_id);if(!current.ok)return {false,{},current.error};
        auto checked=access.revalidate_teaching_request(current.value);if(!checked.ok)return {false,{},checked.error};
        if(!growth_teaching_agreement_equal(*terms->teaching_agreement,current.value.agreement))return {false,{},common_failure("StaleTeachingRequest").error};
        return {true,GrowthTeachingStartAdmission{common.state().identity,std::move(current.value.agreement)},{}};
    }
// 行动头及占用先由共同kernel建立，领域复核条款后才支付、预留、登记永久事实。
    static Status start_growth_impl(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const ActionCoreView& action,const GrowthQuote& quote,const GrowthTeachingStartAdmission* admission,const GrowthExactStartContext* exact){
        if(exact){auto participation=validate_growth_start_participation_writer(common,state,exact->actor_foundation,exact->participation);if(!participation.ok)return participation;}
        if(catalog.complete_study_sources && growth::study_action_kind(action.kind) && !exact)return common_failure("StudyExactStartContextRequired",{},ErrorKind::unsupported_capability);
        if(exact){auto at=encode_world_moment(exact->actual_moment);if(!at.ok)return {false,{},at.error};if(exact->actual_moment.half_day_tick<common.tick())return common_failure("GrowthStartClockBeforeWorld");}
        if(state.actions.find(action.id)||state.facts.find(action.id))return common_failure("GrowthAlreadyStarted");
        const bool needs_admission=(action.kind=="teach_skill"&&catalog.teaching_requests.enabled)||action.kind=="teach_study";
        if(needs_admission!=bool(admission))return common_failure("TeachingStartAdmissionRequired");
        if(admission&&bool(admission->study_agreement)!=(action.kind=="teach_study"))return common_failure("InvalidTeachingStartAdmission");
        if(admission){const auto& a=admission->source_identity;const auto& b=common.state().identity;if(a.world_id!=b.world_id||a.execution_epoch!=b.execution_epoch||a.revision!=b.revision||a.tick!=b.tick)return common_failure("StaleTeachingStartAdmission");}
        auto current=preview_growth_impl(common,state,catalog,shared,social,access,action.actor_id,quote.command,action.id,admission&&!admission->study_agreement?&admission->agreement:nullptr,admission&&admission->study_agreement?&*admission->study_agreement:nullptr);
        if(!current.ok)return {false,{},current.error};
        if(!growth::quote_equal(quote,current.value))return common_failure(action.kind=="study_skill"?"StaleStudyQuote":"StaleTrainingQuote");
        std::vector<StableId> participants{action.actor_id};
        if(quote.context&&quote.context->provider_actor_id!=action.actor_id)participants.push_back(quote.context->provider_actor_id);
        if(action.participants!=participants||action.start_tick!=(exact?exact->actual_moment.half_day_tick:common.tick())||action.remaining_ticks!=quote.duration)return common_failure("InvalidGrowthEnvelope");
        for(const auto& person:participants){auto* a=common.actor(person);
            if(!a||a->action_id!=action.id)return common_failure("GrowthOccupationMissing");
        }
        if(action.kind=="service"||action.kind=="trade"){auto started=growth::start_service_trade(common,state,catalog,action,quote,access.market_capture,exact?std::optional<Integer>{exact->actual_moment.half_day_tick}:std::nullopt,exact);if(!started.ok)return started;return action.kind=="service"&&access.personal_recovery?access.personal_recovery->started(common,state,action):started;}
        if(action.kind=="long_rest")return growth::start_rest(common,state,action,quote,exact?std::optional<Integer>{exact->actual_moment.half_day_tick}:std::nullopt);
        ResourceRecipe recipe;
        GrowthFact fact;
        if(auto* t=std::get_if<StudyTerms>(&quote.data)){recipe.operations.push_back(SetReservation{t->payer_wallet_id,action.id,t->fee,false});
            StudyFact f;
            f.actor_id=t->student_id;
            f.start_tick=action.start_tick;
            f.terms=*t;
            if(t->challenge){auto instance=growth::challenge_start(common,catalog,*t->challenge,t->student_id,action.id);
                if(!instance.ok)return {false,{},instance.error};
                f.challenge=std::move(instance.value);
            }fact=std::move(f);
        }
        else if(auto* t=std::get_if<TrainingTerms>(&quote.data)){if(t->fee>0){auto payment=resource_recipe::append_money_transfer(recipe,t->payer_wallet_id,t->recipient_wallet_id,t->fee,resource_recipe::MoneyTransferPolicy::spendable);if(!payment.ok)return payment;
            }TrainingFact f;
            f.actor_id=action.actor_id;
            f.action_kind=action.kind;
            f.start_tick=action.start_tick;
            f.training=*t;
            f.context=quote.context;
            fact=std::move(f);
        }
        else return common_failure("InvalidGrowthQuote");
        auto applied=common.apply_resources(recipe);
        if(!applied.ok)return applied;
        state.actions.insert(action.id,GrowthAction{quote.command,quote,{}});
        state.facts.insert(action.id,std::move(fact));
        auto published=common.append_fact({action.id,growth::study_action_kind(action.kind)?"skill_study":"training","cultivation"});
        if(!published.ok)return published;
        return exact&&growth::study_action_kind(action.kind)?bind_study_service_after_actual_start(common,state,catalog,action,*exact):published;
    }
    Status settle_original_study_termination_in_candidate(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const StableId& id,const WorldMoment& at){
        auto* row=state.facts.find(id);auto* fact=row?std::get_if<StudyFact>(row):nullptr;
        if(!fact||!fact->study_service_binding||fact->study_service_terminal||fact->phase!="started"||fact->lessons.size()>=static_cast<std::size_t>(fact->terms.duration))return common_failure("StudyActualUnprovidedScopeRequired",id);
        // 保原开放暂停的外部成长基线：先原结算，再记录实际关闭前值。
        auto settled=settle_study(common,catalog,*fact,true,&state);if(!settled.ok)return settled;
        if(growth::study_paused(*fact)){auto& pause=fact->pauses->back();pause.end_tick=at.half_day_tick;const auto* student=common.state().actors.find(fact->terms.student_id);const auto* teacher=common.actor(fact->terms.teacher_id);if(!student||!teacher)return common_failure("StudyOriginalParticipantMissing",id);pause.student_fatigue=student->fatigue;pause.teacher_fatigue=teacher->fatigue;pause.student_xp=growth_skill_xp(student->skills,fact->terms.skill_id);if(growth::study_segmented(*fact)){pause.credited_xp=growth::study_xp_gained(*fact);pause.field_order={"start_tick","end_tick","after_lessons","student_fatigue","teacher_fatigue","student_xp","credited_xp"};}}
        fact->phase="completed";fact->completed_tick=at.half_day_tick;fact->success=false;fact->code="StudyActualServiceTermination";return common_success();
    }
    Status start_growth(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const ActionCoreView& action,const GrowthQuote& quote,const GrowthTeachingStartAdmission* admission){return start_growth_impl(common,state,catalog,shared,social,access,action,quote,admission,nullptr);}
    Status start_growth_at_moment(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const ActionCoreView& action,const GrowthQuote& quote,const GrowthTeachingStartAdmission* admission,const GrowthExactStartContext& exact){return start_growth_impl(common,state,catalog,shared,social,access,action,quote,admission,&exact);}
// 每半日只走一次实际课时或到期结算；接触中断正常结课，返回由kernel统一释放占用与余款。
    static Outcome<GrowthStep> step_growth_impl(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionCoreView& action,const GrowthFixedStepPermit* exact,const PreparedStudyLessonCapability* lesson_capability=nullptr){
        if(access.terminal_recovery && (!exact||!exact->final_gate()||action.kind!="service"||access.personal_recovery))
            return fail<GrowthStep>("TerminalRecoveryOriginalGateRequired");

        if(exact){auto valid=validate_growth_fixed_step_permit(*exact,common,state,action);if(!valid.ok)return error<GrowthStep>(valid.error);if(!common.actor_condition_capture_identity())return fail<GrowthStep>("GrowthExactConditionCaptureRequired");if(access.condition_writer)return fail<GrowthStep>("LegacyGrowthConditionStepOnExactGate");}
        const Integer actual_tick=exact?exact->at().half_day_tick:common.tick()+1;
        if(catalog.cards.enabled!=state.cards.has_value())return fail<GrowthStep>(catalog.cards.enabled?"MissingGrowthCards":"CardsCapabilityRequired");
        if(action.kind=="long_rest"){if(catalog.complete_exact_period_sources)return fail<GrowthStep>("CompleteLongRestConditionWriterRequired");return exact?growth::step_rest_at_fixed_gate(common,state,access,action,*exact):growth::step_rest(common,state,access,action);}
        auto* payload=state.actions.find(action.id);
        if(!payload)return fail<GrowthStep>("MissingGrowthAction");
        if(social.original_relation_version==2&&action.kind=="service"&&payload->request.activity=="work"){
            if(!exact||!access.original_service_relation_writer||!access.original_service_relation_writer->bound_to(common,social,state))return fail<GrowthStep>("OriginalGrowthServiceRelationWriterRequired",action.id,ErrorKind::unsupported_capability);
            auto source=access.original_service_relation_writer->validate_completion_candidate(action.id,exact->at());if(!source.ok)return error<GrowthStep>(source.error);
        }
        GrowthStep result;
        result.effect.kind=action.kind;
        ActionMutation mutation;
        mutation.action=action;
        mutation.remaining_ticks=exact?(growth::study_action_kind(action.kind)?exact->required_half_days()-exact->ordinal():0):action.remaining_ticks-1;
        if(growth::study_action_kind(action.kind)){
            auto valid=validate_study_action(common,state,action,exact);
            if(!valid.ok)return error<GrowthStep>(valid.error);
            auto& fact=std::get<StudyFact>(*state.facts.find(action.id));
            if(growth::study_paused(fact))return {true,std::move(result),{}};
            const auto terms=fact.terms;
            std::optional<GrowthConditionWriteBoundary> condition_boundary;
            Outcome<Integer> selected_xp{true,0,{}};
            if(exact){if(!lesson_capability)return fail<GrowthStep>("CompleteStudyLessonCapabilityRequired");selected_xp=read_prepared_study_lesson_xp(*lesson_capability,common,state,action,*exact);if(!selected_xp.ok&&selected_xp.error.original.code!="StudyNeedsRecovery")return error<GrowthStep>(selected_xp.error);}
            auto continued=selected_xp.ok?continue_study(common,access,action,fact,exact!=nullptr,exact?&state:nullptr):Status{false,{},selected_xp.error};
            if(!continued.ok){if(exact&&fact.study_service_binding)return fail<GrowthStep>("StudyActualPauseOrServiceTerminationRequired",action.id);if(continued.error.kind!=ErrorKind::rule)return error<GrowthStep>(continued.error);
                fact.code=continued.error.original.code;
                mutation.remaining_ticks=0;
            }
            else {auto* student=common.state().actors.find(terms.student_id);
                auto* teacher=common.state().actors.find(terms.teacher_id);
                StudyLesson lesson{student->fatigue,student->injury,exact?selected_xp.value:growth_lesson_xp(student->fatigue,student->injury),{},std::nullopt};
                if(terms.innate_source){if(!catalog.innate_learning||!access.validate_innate_study_source)return fail<GrowthStep>("InnateStudySourceRequired");auto source=access.validate_innate_study_source(*terms.innate_source,action.id);if(!source.ok)return error<GrowthStep>(source.error);auto xp=innate_study_lesson_xp(lesson.xp,terms.innate_source->bonus_bp);if(!xp.ok)return error<GrowthStep>(xp.error);lesson.xp=xp.value;}
                if(growth::study_segmented(fact)){const auto current=growth_skill_xp(student->skills,terms.skill_id);const auto bp=fact.challenge&&fact.challenge->result?fact.challenge->result->performance_bp:10000;
                    lesson.segment=StudySegment{Integer(fact.lessons.size()+1),actual_tick,current,bp,std::max<Integer>(0,std::min<Integer>(growth::challenge_lesson_xp(fact,lesson.xp),terms.xp_limit-current)),fact.challenge&&growth::challenge_pending(*fact.challenge),std::nullopt,{"ordinal","settled_tick","xp_before","performance_bp","xp_gained","performance_pending"}};
                    lesson.field_order={"fatigue_before","injury","xp","segment"};
                }
                const auto cost=growth::original_study_lesson_cost(terms.xp_before);
                ResourceRecipe recipe;
                auto payment=resource_recipe::append_reserved_money_payment(recipe,SetReservation{terms.payer_wallet_id,action.id,terms.fee-cost*Integer(fact.lessons.size()+1),false},terms.recipient_wallet_id,cost,resource_recipe::ReservedMoneyPaymentOrder::debit_then_release);if(!payment.ok)return error<GrowthStep>(payment.error);
                if(access.condition_writer){const auto& actual_step=access.condition_writer->condition_step();auto opened=begin_growth_condition_write_in_candidate(common,{GrowthPersonalConditionBusiness::study_lesson,action.id,actual_step.to_tick,Integer(fact.lessons.size()+1)},actual_step);if(!opened.ok)return error<GrowthStep>(opened.error);condition_boundary=std::move(opened.value);}
                auto r=common.apply_resources(recipe);
                if(!r.ok)return error<GrowthStep>(r.error);
                ActorMutation sm;
                sm.actor_id=terms.student_id;
                sm.fatigue=std::min<Integer>(100,lesson.fatigue_before+6);
                if(lesson.segment){auto skills=student->skills;auto adjusted=growth_set_skill(skills,terms.skill_id,lesson.segment->xp_before+lesson.segment->xp_gained);if(!adjusted.ok)return error<GrowthStep>(adjusted.error);sm.skills=std::move(skills);}
                r=common.apply_actor(sm);
                if(!r.ok)return error<GrowthStep>(r.error);
                ActorMutation tm;
                tm.actor_id=terms.teacher_id;
                tm.fatigue=std::min<Integer>(100,teacher->fatigue+2);
                r=common.apply_actor(tm);
                if(!r.ok)return error<GrowthStep>(r.error);
                fact.lessons.push_back(lesson);
            }
            auto r=common.apply_action(mutation);
            if(!r.ok)return error<GrowthStep>(r.error);
            result.completed=*mutation.remaining_ticks==0;
            if(result.completed){r=settle_study(common,catalog,fact,false,exact?&state:nullptr,exact);
                if(!r.ok)return error<GrowthStep>(r.error);
                fact.phase="completed";
                fact.completed_tick=actual_tick;
                fact.success=fact.code.empty();
                result.effect=study_effect(fact);
            }
            if(condition_boundary){auto bound=bind_growth_study_condition_in_candidate(common,state,*condition_boundary);if(!bound.ok)return error<GrowthStep>(bound.error);auto accepted=access.condition_writer->accept_bound_condition(std::move(bound.value));if(!accepted.ok)return error<GrowthStep>(accepted.error);}
            return {true,std::move(result),{}};
        }
        // 原Action尚未扣时前记一次实际服务；失败由外层整候选回滚，不支付第二次费用。
        if(exact&&action.kind=="service"&&access.personal_recovery)return fail<GrowthStep>("GrowthExactRecoveryPeriodSourceRequired");
        if(action.kind=="service"&&access.personal_recovery){auto provided=access.personal_recovery->provided(common,state,action);if(!provided.ok)return error<GrowthStep>(provided.error);}
        auto r=common.apply_action(mutation);
        if(!r.ok)return error<GrowthStep>(r.error);
        result.completed=*mutation.remaining_ticks==0;
        if(!result.completed)return {true,std::move(result),{}};
        if(action.kind=="service"||action.kind=="trade"){auto finished=growth::finish_service_trade(common,state,catalog,social,interaction,action,access.personal_recovery,exact?std::optional<Integer>{actual_tick}:std::nullopt,access.original_service_relation_writer,access.terminal_recovery);
            if(!finished.ok)return error<GrowthStep>(finished.error);
            if(action.kind=="service"&&access.personal_recovery){auto closed=access.personal_recovery->completed(common,state,action);if(!closed.ok)return error<GrowthStep>(closed.error);}
            result.effect=std::move(finished.value);
            return {true,std::move(result),{}};
        }
        auto* f=state.facts.find(action.id);
        auto* fact=f?std::get_if<TrainingFact>(f):nullptr;
        if(!fact)return fail<GrowthStep>("MissingTrainingFact");
        const auto t=fact->training;
        if(fact->phase=="completed"){result.effect.success=fact->success;
            result.effect.already_completed=true;
            result.effect.code=fact->success?"":"TrainingInterrupted";
            return {true,std::move(result),{}};
        }
        bool success=true;
        for(const auto& person:action.participants){auto* a=common.state().actors.find(person);
            auto location=common.resolved_location(person);
            const auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
            if(!growth::ready(common,access,person,{},action.id,false).ok||!a||a->injury>=3||!place||place->place_id!=t.place_id)success=false;
        }
        if(action.kind!="practice_skill"){if(!fact->context||!access.contact(action.actor_id,*fact->context,action.kind=="learn_skill"?"learn_skill":"",action.id).ok||mastery(*common.state().actors.find(t.teacher_id),t.skill_id)<80)success=false;
        }
        std::optional<GrowthConditionWriteBoundary> condition_boundary;
        if(success){
            if(access.condition_writer){const auto& actual_step=access.condition_writer->condition_step();auto opened=begin_growth_condition_write_in_candidate(common,{GrowthPersonalConditionBusiness::training_completion,action.id,actual_step.to_tick,1},actual_step);if(!opened.ok)return error<GrowthStep>(opened.error);condition_boundary=std::move(opened.value);}
            auto* student=common.state().actors.find(t.student_id);
            auto combat=student->combat;
            combat.learned.set(t.skill_id,t.mastery_after);
            ActorMutation sm;
            sm.actor_id=t.student_id;
            sm.combat=std::move(combat);
            sm.fatigue=std::min<Integer>(100,student->fatigue+t.student_fatigue);
            r=common.apply_actor(sm);
            if(!r.ok)return error<GrowthStep>(r.error);
            if(!t.teacher_id.empty()){auto* teacher=common.actor(t.teacher_id);
                ActorMutation tm;
                tm.actor_id=t.teacher_id;
                tm.fatigue=std::min<Integer>(100,teacher->fatigue+t.teacher_fatigue);
                r=common.apply_actor(tm);
                if(!r.ok)return error<GrowthStep>(r.error);
        }}
        fact->phase="completed";
        fact->completed_tick=actual_tick;
        fact->success=success;
        // 先有真实成功能力结算，再登记学生首次来源；练习只沿原掌握度结算。
        if(success&&catalog.cards.enabled&&!catalog.complete_study_sources&&(action.kind=="learn_skill"||action.kind=="teach_skill")){
            if(!state.cards)return fail<GrowthStep>("MissingGrowthCards");
            auto acquired=record_growth_training_cards(common.state(),state,*state.cards,catalog.cards,action.id);
            if(!acquired.ok)return error<GrowthStep>(acquired.error);
        }
        if(condition_boundary){auto bound=bind_growth_training_condition_in_candidate(common,state,*condition_boundary);if(!bound.ok)return error<GrowthStep>(bound.error);auto accepted=access.condition_writer->accept_bound_condition(std::move(bound.value));if(!accepted.ok)return error<GrowthStep>(accepted.error);}
        result.effect.success=success;
        result.effect.code=success?"":"TrainingInterrupted";
        result.effect.skill_id=t.skill_id;
        result.effect.mastery=success?t.mastery_after:t.mastery_before;
        (void)shared;
        return {true,std::move(result),{}};
    }
// 两入口共享原费用/成长/身体数学，精确入口只消费原完整服务门，不调用虚构整数step。
    Outcome<GrowthStep> step_growth(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionCoreView& action){return step_growth_impl(common,state,catalog,shared,social,interaction,access,action,nullptr);}
    Outcome<GrowthStep> step_growth_at_fixed_gate(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionCoreView& action,const GrowthFixedStepPermit& exact){return step_growth_impl(common,state,catalog,shared,social,interaction,access,action,&exact);}
    Outcome<GrowthStep> step_growth_study_at_fixed_gate(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionCoreView& action,const GrowthFixedStepPermit& exact,const PreparedStudyLessonCapability& capability){if(!growth::study_action_kind(action.kind))return fail<GrowthStep>("StudyCapabilityActionMismatch");return step_growth_impl(common,state,catalog,shared,social,interaction,access,action,&exact,&capability);}
// 即时配置不分配行动；课程取消仅结算已授课时，最后释放交由共同生命周期执行。
    static Outcome<GrowthEffect> apply_growth_instant_impl(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& id,const GrowthQuote& q,const StudyAdjustmentWriteContext* exact,const GrowthFixedCommandPermit* command_time=nullptr){auto checked=command_time?preview_growth_at_fixed_command(common,state,catalog,shared,social,access,id,q.command,*command_time):preview_growth(common,state,catalog,shared,social,access,id,q.command);
        if(!checked.ok)return error<GrowthEffect>(checked.error);
        if(q.command.kind=="recovery_cancel")return access.personal_recovery?access.personal_recovery->cancel(common,state,id,q):fail<GrowthEffect>("PersonalInjuryRecoveryUnavailable");
        if(q.command.kind.rfind("trade_bargain_",0)==0)return growth::apply_bargain(common,state,catalog,shared,access,id,q);
        GrowthEffect effect;
        effect.kind=q.command.kind;
        if(q.command.kind=="rest_cancel"){const auto& cancellation=std::get<RestCancellation>(checked.value.data);
            auto& fact=std::get<RestFact>(*state.facts.find(cancellation.action_id));
            fact.phase="completed";
            fact.completed_tick=command_time?command_time->at().half_day_tick:common.tick();
            fact.stop_reason="cancelled";
            effect.action_id=cancellation.action_id;
            effect.elapsed=fact.elapsed;
            return {true,std::move(effect),{}};
        }if(q.command.kind=="study_pause"||q.command.kind=="study_resume"){
            if(!growth::quote_equal(q,checked.value))return fail<GrowthEffect>("StaleStudyQuote");
            const auto& change=std::get<StudyPauseChange>(checked.value.data);
            auto& fact=std::get<StudyFact>(*state.facts.find(change.action_id));
            auto* current=common.action(change.action_id);
            if(!current)return fail<GrowthEffect>("MissingStudyAction");
            if(change.paused){auto result=growth::pause_study_in_candidate(common,state,change.action_id,command_time?command_time->at():WorldMoment{common.tick(),{}});if(!result.ok)return error<GrowthEffect>(result.error);}
            else {
                auto result=resume_action_participation_in_candidate(common,change.action_id,growth::study_participants(fact.terms));if(!result.ok)return error<GrowthEffect>(result.error);
auto& pause=fact.pauses->back();
                pause.end_tick=command_time?command_time->at().half_day_tick:common.tick();
                pause.student_fatigue=common.actor(fact.terms.student_id)->fatigue;
                pause.teacher_fatigue=common.actor(fact.terms.teacher_id)->fatigue;
                pause.student_xp=growth_skill_xp(common.state().actors.find(fact.terms.student_id)->skills,fact.terms.skill_id);
                if(growth::study_segmented(fact)){pause.credited_xp=growth::study_xp_gained(fact);pause.field_order={"start_tick","end_tick","after_lessons","student_fatigue","teacher_fatigue","student_xp","credited_xp"};}
            }
            effect.action_id=change.action_id;
            effect.paused=change.paused;
            effect.paused_ticks=change.paused_ticks;
            effect.completed_lessons=change.completed_lessons;
            effect.reserved=change.reserved;
            return {true,std::move(effect),{}};
        }if(q.command.kind=="study_cancel"){auto& fact=std::get<StudyFact>(*state.facts.find(q.command.action_id));
            if(fact.study_service_binding)return fail<GrowthEffect>("AcceptedStudyServiceCannotSelfCancel",q.command.action_id);
            if(growth::study_paused(fact)){auto& pause=fact.pauses->back();
                pause.end_tick=command_time?command_time->at().half_day_tick:common.tick();
                pause.student_fatigue=common.actor(fact.terms.student_id)->fatigue;
                pause.teacher_fatigue=common.actor(fact.terms.teacher_id)->fatigue;
                pause.student_xp=growth_skill_xp(common.state().actors.find(fact.terms.student_id)->skills,fact.terms.skill_id);
                if(growth::study_segmented(fact)){pause.credited_xp=growth::study_xp_gained(fact);pause.field_order={"start_tick","end_tick","after_lessons","student_fatigue","teacher_fatigue","student_xp","credited_xp"};}
            }
            auto r=settle_study(common,catalog,fact,false,command_time?&state:nullptr);
            if(!r.ok)return error<GrowthEffect>(r.error);
            fact.phase="completed";
            fact.completed_tick=command_time?command_time->at().half_day_tick:common.tick();
            fact.success=false;
            fact.code="StudyCancelled";
            effect=study_effect(fact);
            effect.kind="study_cancel";
            effect.action_id=q.command.action_id;
            return {true,std::move(effect),{}};
        }if(growth_busy_command(q.command.kind)){auto& fact=std::get<StudyFact>(*state.facts.find(q.command.action_id));
            fact.challenge=std::get<GrowthChallengeInstance>(checked.value.data);
            auto adjusted=settle_study_adjustments(common,fact,exact);if(!adjusted.ok)return error<GrowthEffect>(adjusted.error);
            if(exact&&fact.study_service_terminal&&fact.challenge&&fact.challenge->result)fact.challenge->consumed=true;
            effect.action_id=q.command.action_id;
            effect.challenge=fact.challenge;
            effect.public_challenge=growth::public_challenge(catalog,shared,*fact.challenge);
            return {true,std::move(effect),{}};
        }auto* actor=common.state().actors.find(id);
        auto combat=actor->combat;
        const auto& choice=std::get<EquipmentChoice>(checked.value.data);
        if(q.command.kind=="equip_weapon")combat.weapon_id=choice.id;
        else if(q.command.kind=="equip_armor")combat.armor_id=choice.id;
        else combat.heart_id=choice.id;
        ActorMutation mutation;
        mutation.actor_id=id;
        mutation.combat=std::move(combat);
        auto r=common.apply_actor(mutation);
        if(!r.ok)return error<GrowthEffect>(r.error);
        return {true,std::move(effect),{}};
    }
    Outcome<GrowthEffect> apply_growth_instant(CommonStore& c,GrowthState& s,const GrowthCatalog& g,const CommonCatalog& k,const SocialState& social,const GrowthAccess& access,const StableId& actor,const GrowthQuote& quote){return apply_growth_instant_impl(c,s,g,k,social,access,actor,quote,nullptr);}
    Outcome<GrowthEffect> apply_growth_instant_at_moment(CommonStore& c,GrowthState& s,const GrowthCatalog& g,const CommonCatalog& k,const SocialState& social,const GrowthAccess& access,const StableId& actor,const GrowthQuote& quote,const StudyAdjustmentWriteContext& exact){
        const auto& kind=quote.command.kind;if(!g.complete_study_sources||(kind!="study_challenge_choose"&&kind!="study_challenge_exit"&&kind!="study_etiquette_choose"&&kind!="study_etiquette_exit"))return fail<GrowthEffect>("StudyExactAdjustmentCommandRequired");
        auto at=encode_world_moment(exact.actual_moment);if(!at.ok)return error<GrowthEffect>(at.error);if(exact.actual_moment.half_day_tick<c.tick())return fail<GrowthEffect>("StudyExactAdjustmentClockInvalid");
        return apply_growth_instant_impl(c,s,g,k,social,access,actor,quote,&exact);
    }

    Outcome<GrowthQuote> preview_growth_at_fixed_command(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& actor,const GrowthCommand& command,const GrowthFixedCommandPermit& permit){
        if(!catalog.complete_exact_period_sources)return fail<GrowthQuote>("GrowthFixedCommandProfileRequired");auto valid=validate_growth_fixed_command_permit(permit,common,state,actor,command);if(!valid.ok)return error<GrowthQuote>(valid.error);
        return preview_growth_impl(common,state,catalog,shared,social,access,actor,command,{},nullptr,nullptr,&permit);
    }
    Outcome<GrowthEffect> apply_growth_instant_at_fixed_command(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& actor,const GrowthQuote& quote,const GrowthFixedCommandPermit& permit){
        if(!catalog.complete_exact_period_sources)return fail<GrowthEffect>("GrowthFixedCommandProfileRequired");auto valid=validate_growth_fixed_command_permit(permit,common,state,actor,quote.command);if(!valid.ok)return error<GrowthEffect>(valid.error);
        StudyAdjustmentWriteContext exact{permit.at()};return apply_growth_instant_impl(common,state,catalog,shared,social,access,actor,quote,&exact,&permit);
    }

} // 新政策逐课入账、迟答只补原差额；旧政策保留末尾结算，查询和重放均不发奖励。
