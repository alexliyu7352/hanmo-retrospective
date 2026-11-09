#include "resident/domains/military/military_model.hpp"
// 原驻军训练服务和首课前军阵实例：只计真实供给课时，取消不会补发未提供训练或收走未来钱款。
#include "resident/storage/money_transfer_recipe.hpp"
#include "training_rules.hpp"
#include "training_original_rules.hpp"
#include "training_progress_values.hpp"
#include "training_codec.hpp"
#include "occupation_rules.hpp"
#include "resident/model/common_operations.hpp"
#include "resident/model/common_rng.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
template<class T>T* get(std::vector<T>& rows,const StableId& id){for(auto& x:rows)if(x.id==id)return &x;return nullptr;}
template<class T>const T* get(const std::vector<T>& rows,const StableId& id){for(auto& x:rows)if(x.id==id)return &x;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
Status fail(const StableId& code,const StableId& path={}){return common_failure(code,path);}
template<class T>Outcome<T> error(const StableId& code,const StableId& path={}){return {false,{},fail(code,path).error};}
bool sorted_unique(const std::vector<StableId>& a){StableId previous;for(auto& x:a){if(x.empty()||(!previous.empty()&&x<=previous))return false;previous=x;}return true;}
bool profile(const MilitaryUnitRecord& u){return u.training&&u.fatigue&&u.training_action_id&&*u.training>=0&&*u.training<=100&&*u.fatigue>=0&&*u.fatigue<=100;}
Integer level(Integer xp){if(xp<0||xp>1200)return -1;return xp>=1200?4:xp>=650?3:xp>=300?2:xp>=100?1:0;}
StableId skill(const MilitaryCatalog& cat,const StableId& troop_id){auto troop=get(cat.battle_catalog.troop_types,troop_id);if(!troop)return {};return troop->category=="infantry"?"infantry":troop->category=="archer"?"archery":troop->category=="cavalry"?"cavalry":"";}
Status ready_actor(const CommonRead& c,const MilitaryAccess& a,const StableId& id,const StableId& place,const StableId& own){auto p=c.actor(id);if(!p||!p->alive||!p->available||p->hp<=0||!a.actor_injury||a.actor_injury(id)>=3)return fail("TrainingActorUnavailable");if(!p->action_id.empty()&&p->action_id!=own)return fail("ActorBusy");if(a.in_open_battle&&a.in_open_battle(id))return fail("BattlePending");auto loc=a.resolve_location?a.resolve_location(id):p->location;auto city=std::get_if<SettlementLocation>(&loc);if(!city||city->place_id!=place)return fail("TrainingAssemblyRequired");return common_success();}
const DrillDefinition* drill_definition(const MilitaryCatalog& cat,const MilitaryTrainingTerms& t){auto camp=get(cat.garrisons,t.garrison_id);return camp&&camp->training_drill_id?get(cat.drill_challenges,*camp->training_drill_id):nullptr;}
TrainingGroupSource group_source(const MilitaryTrainingTerms& t,const DrillDefinition& d){TrainingGroupSource g;g.unit_id=t.unit_id;g.troop_type_id=t.troop_type_id;g.roster_sha256=training_codec::roster_digest(t.fit_ids);for(auto& x:d.groups)g.group_ids.push_back(x.id);return g;}
bool same_group(const TrainingGroupSource& a,const TrainingGroupSource& b){return std::tie(a.kind,a.unit_id,a.troop_type_id,a.roster_sha256,a.group_ids)==std::tie(b.kind,b.unit_id,b.troop_type_id,b.roster_sha256,b.group_ids);}
bool same_drill_terms(const TrainingDrillTerms& a,const TrainingDrillTerms& b){return std::tie(a.kind,a.challenge_id,a.mode,a.tactics_xp,a.actor_snapshot.skill_level,a.actor_snapshot.auxiliary_attribute,a.source_sha256)==std::tie(b.kind,b.challenge_id,b.mode,b.tactics_xp,b.actor_snapshot.skill_level,b.actor_snapshot.auxiliary_attribute,b.source_sha256)&&same_group(a.group_source,b.group_source);}
bool same_terms(const MilitaryTrainingTerms& a,const MilitaryTrainingTerms& b,bool continuation=false){if(std::tie(a.actor_id,a.office_id,a.authority_source,a.unit_id,a.garrison_id,a.organization_id,a.ownership_id,a.place_id,a.commander_actor_id,a.troop_type_id,a.fit_ids,a.participants,a.skill_id,a.skill_xp,a.skill_level,a.gain_total,a.fatigue_per_tick,a.payer_wallet_id,a.recipient_wallet_id,a.duration,a.fee)!=std::tie(b.actor_id,b.office_id,b.authority_source,b.unit_id,b.garrison_id,b.organization_id,b.ownership_id,b.place_id,b.commander_actor_id,b.troop_type_id,b.fit_ids,b.participants,b.skill_id,b.skill_xp,b.skill_level,b.gain_total,b.fatigue_per_tick,b.payer_wallet_id,b.recipient_wallet_id,b.duration,b.fee)||a.drill.has_value()!=b.drill.has_value())return false;if(!continuation&&(a.training_before!=b.training_before||a.fatigue_before!=b.fatigue_before))return false;return !a.drill||same_drill_terms(*a.drill,*b.drill);}
Outcome<std::optional<TrainingDrillTerms>> drill_quote(const MilitaryCatalog& cat,const MilitaryAccess& a,const MilitaryTrainingTerms& t,const StableId& mode){auto camp=get(cat.garrisons,t.garrison_id);if(!camp)return error<std::optional<TrainingDrillTerms>>("MissingTrainingDrillGarrison");if(!camp->training_drill_id){if(mode!="automatic")return error<std::optional<TrainingDrillTerms>>("TrainingDrillNotOffered");return {true,std::nullopt,{}};}auto d=drill_definition(cat,t);if(!d)return error<std::optional<TrainingDrillTerms>>("MissingTrainingDrill");auto xp=a.actor_skill?a.actor_skill(t.actor_id,"tactics"):std::nullopt;auto intellect=a.actor_attribute?a.actor_attribute(t.actor_id,"intellect"):std::nullopt;if(!xp||level(*xp)<0||!intellect||*intellect<1||*intellect>110)return error<std::optional<TrainingDrillTerms>>("InvalidTrainingDrillActor");DrillSnapshot snapshot{level(*xp),*intellect,{}};auto created=create_drill(*d,t.actor_id,"preview",snapshot,"0",mode);if(!created.ok)return {false,{},created.error};if(!sorted_unique(t.fit_ids)||t.fit_ids.empty())return error<std::optional<TrainingDrillTerms>>("InvalidTrainingDrillRoster");TrainingDrillTerms terms;terms.challenge_id=d->id;terms.mode=mode;terms.tactics_xp=*xp;terms.actor_snapshot=snapshot;terms.source_sha256=d->source_sha256;terms.group_source=group_source(t,*d);return {true,terms,{}};}
// 所有调用共用原envelope、作者来源、随机起点和消费阶段校验，历史视图不旁路绑定。
Status validate_drill(const MilitaryCatalog& cat,const MilitaryTrainingFact& fact){
    std::size_t id_length=0;for(unsigned char ch:fact.id)if((ch&0xc0)!=0x80)++id_length;
    if(fact.id.empty()||id_length>160||fact.id.find_first_of("\n\r")!=StableId::npos||fact.terms.actor_id!=fact.actor_id||(fact.phase!="started"&&fact.phase!="completed")||fact.start_tick<0||fact.start_tick>2147483647||fact.lessons.size()>4)return fail("InvalidTrainingDrillEnvelope");
    auto camp=get(cat.garrisons,fact.terms.garrison_id);if(!camp)return fail("MissingTrainingDrillGarrison");
    if(!camp->training_drill_id)return !fact.terms.drill&&!fact.drill?common_success():fail("UnexpectedTrainingDrill");
    if(!fact.terms.drill)return fail("InvalidTrainingDrillTerms");
    if(!fact.drill)return fail("InvalidTrainingDrillInstance");
    auto& instance=*fact.drill;
    auto checked=validate_original_training_drill_identity(cat,fact.terms,fact.actor_id,fact.id,instance.origin_seed,instance.origin_state,instance.challenge);
    if(!checked.ok)return checked;
    if((instance.phase!="pending"&&instance.phase!="frozen"&&instance.phase!="closed")||instance.performance_bp.has_value()!=(instance.phase=="frozen"))return fail("InvalidTrainingDrillInstance");
    const auto& challenge=instance.challenge;
    if(instance.phase=="pending"){if(fact.phase!="started"||!fact.lessons.empty()||instance.frozen_tick!=-1)return fail("TrainingDrillConsumptionMismatch");}
    else if(instance.phase=="closed"){if(fact.phase!="completed"||!fact.lessons.empty()||instance.frozen_tick!=-1||challenge.phase!="completed")return fail("TrainingDrillConsumptionMismatch");}
    else if(fact.lessons.empty()||instance.frozen_tick!=fact.start_tick+1||challenge.phase!="completed"||!challenge.result||*instance.performance_bp!=challenge.result->performance_bp)return fail("TrainingDrillConsumptionMismatch");
    return common_success();
}
Integer training_after(const MilitaryTrainingFact& f,Integer count){return military_training_progress::unit_training(f.terms,count,count>0&&f.drill?f.drill->performance_bp.value_or(0):10000);}
Status supply_fact(const MilitarySupplyFact& f,const MilitaryCatalog& cat,const MilitaryTrainingTerms& t,Integer tick){auto camp=get(cat.garrisons,t.garrison_id);if(!camp||f.garrison_id!=t.garrison_id||!f.army_id.empty()||f.organization_id!=t.organization_id||f.ownership_id!=t.ownership_id||f.tick!=tick||!sorted_unique(f.mouth_ids)||f.required<0||f.required>2147483647||f.consumed<0||f.consumed>2147483647||f.shortage<0||f.shortage>2147483647||f.required!=Integer(f.mouth_ids.size())*camp->ration_per_person||f.consumed+f.shortage!=f.required)return fail("InvalidMilitaryTrainingSupply");for(auto& person:t.fit_ids)if(!has(f.mouth_ids,person))return fail("MilitaryTrainingSupplyRosterMismatch");for(auto& person:t.participants)if(!has(f.mouth_ids,person))return fail("MilitaryTrainingSupplyRosterMismatch");return f.shortage>0?fail("MilitaryTrainingUnsupplied"):common_success();}
Outcome<StableId> find_supply(const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryTrainingTerms& t,Integer tick){const MilitarySupplyFact* found=nullptr;for(auto& f:s.supply_facts)if(f.garrison_id==t.garrison_id&&f.army_id.empty()&&f.tick==tick){if(found)return error<StableId>("MilitaryTrainingSupplyAmbiguous");found=&f;}if(!found)return error<StableId>("MilitaryTrainingSupplyRequired");auto checked=supply_fact(*found,cat,t,tick);if(!checked.ok)return {false,{},checked.error};return {true,found->id,{}};}
// 历史课时由固定费用差额、原供给和冻结军阵系数核验，不反向恢复当前部队条件。
Status validate_fact(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryTrainingFact& f,const MilitaryAccess* access=nullptr){auto& t=f.terms;if(f.phase!="started"&&f.phase!="completed")return fail("InvalidMilitaryTrainingFact");if(t.duration!=4||t.fee!=150||!sorted_unique(t.fit_ids)||t.fit_ids.empty()||!sorted_unique(t.participants)||!has(t.participants,t.actor_id)||!has(t.participants,t.commander_actor_id)||t.training_before<0||t.training_before>100||t.fatigue_before<0||t.fatigue_before>100||t.fatigue_per_tick<0||t.fatigue_per_tick>20||level(t.skill_xp)<0||t.skill_level!=level(t.skill_xp)||t.gain_total!=3+t.skill_level)return fail("InvalidMilitaryTrainingTerms");auto u=get(s.military_units,t.unit_id);auto camp=get(cat.garrisons,t.garrison_id);auto office=get(cat.offices,t.office_id);if(!u||!camp||!office||!c.wallet(t.payer_wallet_id)||!c.wallet(t.recipient_wallet_id)||t.payer_wallet_id==t.recipient_wallet_id)return fail("UnknownMilitaryTrainingReference");if(office->garrison_id!=t.garrison_id||office->organization_id!=t.organization_id||u->troop_type_id!=t.troop_type_id)return fail("MilitaryTrainingAuthoritySourceMissing");std::vector<StableId> people{t.actor_id};if(!has(people,t.commander_actor_id))people.push_back(t.commander_actor_id);for(auto& id:t.fit_ids){const auto* original=get(cat.garrisons,u->source_garrison_id);const auto* definition=original?get(original->units,u->id):nullptr;const bool historical=f.phase=="completed"&&definition&&definition->population_source_id&&!c.actor(id);if(historical&&access){if(!access->population_training_member||!access->population_training_member(*definition->population_source_id,u->id,id,f.start_tick))return fail("MilitaryTrainingRosterSourceMissing");}else if(!has(u->member_ids,id))return fail("MilitaryTrainingRosterSourceMissing");if(c.actor(id)&&!has(people,id))people.push_back(id);}std::sort(people.begin(),people.end());if(people!=t.participants)return fail("MilitaryTrainingRosterSourceMissing");for(auto& id:people)if(!c.actor(id))return fail("UnknownMilitaryTrainingReference");if(t.place_id!=camp->place_id||t.payer_wallet_id!=camp->wallet_id||!camp->training_wallet_id||t.recipient_wallet_id!=*camp->training_wallet_id||!camp->training_fatigue_per_tick||t.fatigue_per_tick!=*camp->training_fatigue_per_tick||t.skill_id!=skill(cat,t.troop_type_id))return fail("MilitaryTrainingDefinitionMismatch");auto appointment=get(s.action_facts,t.authority_source);auto terms=appointment?std::get_if<OfficeTerms>(&appointment->terms):nullptr;if(!appointment||appointment->command!="office_accept"||appointment->phase!="completed"||!appointment->success||appointment->actor_id!=t.actor_id||!terms||terms->office_id!=t.office_id||appointment->completed_tick<0||appointment->completed_tick>f.start_tick)return fail("MilitaryTrainingAuthoritySourceMissing");if(f.actor_id!=t.actor_id||f.start_tick<0||f.start_tick>c.tick()||f.lessons.size()>4)return fail("InvalidMilitaryTrainingTime");auto checked=validate_drill(cat,f);if(!checked.ok)return checked;Integer training=t.training_before,fatigue=t.fatigue_before;for(std::size_t i=0;i<f.lessons.size();++i){auto& row=f.lessons[i];Integer n=i+1;if(row.tick!=f.start_tick+n||row.fee_paid!=military_training_progress::lesson_fee(n)||row.training_before!=training||row.fatigue_before!=fatigue||row.training_after!=training_after(f,n)||row.fatigue_after!=military_training_progress::unit_fatigue(t,n))return fail("MilitaryTrainingLessonMismatch");auto supplied=get(s.supply_facts,row.supply_fact_id);if(!supplied)return fail("InvalidMilitaryTrainingSupply");checked=supply_fact(*supplied,cat,t,row.tick);if(!checked.ok)return checked;training=row.training_after;fatigue=row.fatigue_after;}if(f.phase=="started"){if(f.completed_tick!=-1||f.success)return fail("PrematureMilitaryTrainingResult");}else{if((f.code.empty()&&f.lessons.size()!=4)||f.completed_tick<f.start_tick+Integer(f.lessons.size())||f.completed_tick>f.start_tick+Integer(f.lessons.size())+1||f.success!=(f.code.empty()&&f.lessons.size()==4))return fail("InvalidMilitaryTrainingCompletion");}return common_success();}
// 行动外壳只绑定同一条事实；不借另一课程的条款释放资金或授课。
Outcome<const MilitaryTrainingFact*> record(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const ActionCoreView& action){
    const MilitaryTrainingAction* payload=nullptr;for(auto& p:s.training_actions)if(p.action_id==action.id)payload=&p;
    if(action.kind!="military_train"||action.id.empty()||!payload)return error<const MilitaryTrainingFact*>("InvalidMilitaryTrainingAction");
    auto f=get(s.training_facts,action.id);if(!f)return error<const MilitaryTrainingFact*>("MilitaryTrainingFactMissing");
    auto valid=validate_fact(c,s,cat,*f);if(!valid.ok)return {false,{},valid.error};
    if(action.actor_id!=f->actor_id||action.start_tick!=f->start_tick||action.participants!=f->terms.participants||!same_terms(payload->training,f->terms)||action.remaining_ticks<0||action.remaining_ticks>4)return error<const MilitaryTrainingFact*>("MilitaryTrainingFactMismatch");
    return {true,f,{}};
}
// 本人即时命令复用当前精确行动，过期ID先于题面操作报错。
Outcome<const ActionCoreView*> current_action(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const StableId& requested){
    auto person=c.actor(actor);auto action=person?c.action(person->action_id):nullptr;
    if(!action||action->kind!="military_train"||action->actor_id!=actor)return error<const ActionCoreView*>("NotMilitaryTraining");
    if(requested!=action->id)return error<const ActionCoreView*>("StaleMilitaryTrainingAction");
    auto checked=validate_military_training_action(c,s,cat,a,*action);if(!checked.ok)return {false,{},checked.error};return {true,action,{}};
}
// 未实际授课的题面只封闭，首课后只能复用冻结表现，不能补发奖励。
Status close_drill(const MilitaryCatalog& cat,MilitaryTrainingFact& fact,std::optional<Integer> lesson_tick){
    auto checked=validate_drill(cat,fact);if(!checked.ok)return checked;
    if(lesson_tick&&(fact.phase!="started"||fact.lessons.size()>=4||*lesson_tick!=fact.start_tick+Integer(fact.lessons.size())+1))return fail("TrainingDrillClockMismatch");
    if(!fact.drill)return common_success();
    auto& instance=*fact.drill;
    if(lesson_tick&&instance.phase=="frozen")return common_success();
    if(!lesson_tick&&(!fact.lessons.empty()||instance.phase=="closed"))return common_success();
    if(instance.phase!="pending"||!fact.lessons.empty())return fail("TrainingDrillConsumptionMismatch");
    auto definition=drill_definition(cat,fact.terms);auto closed=exit_drill(*definition,instance.challenge);if(!closed.ok)return {false,{},closed.error};
    instance.challenge=std::move(closed.value);instance.phase=lesson_tick?"frozen":"closed";
    if(lesson_tick){instance.frozen_tick=*lesson_tick;instance.performance_bp=instance.challenge.result->performance_bp;}
    return common_success();
}
// 回执只由永久已提供课时推导，未来单位状态不改变旧账。
MilitaryEffect receipt(const MilitaryTrainingFact& fact,bool repeated=false){
    MilitaryEffect out;out.success=fact.success;out.code=fact.code;out.already_completed=repeated;
    MilitaryTrainingReceipt details;details.unit_id=fact.terms.unit_id;details.provided_half_days=fact.lessons.size();details.fee_paid=military_training_progress::cumulative_fee(details.provided_half_days);details.training_gain=(fact.lessons.empty()?fact.terms.training_before:fact.lessons.back().training_after)-fact.terms.training_before;details.released_unprovided_fee=military_training_progress::unprovided_fee(details.provided_half_days);out.training_receipt=details;return out;
}
// 封账释放本部标记和未来预留；人物占用与行动删除仍由唯一runtime负责。
Outcome<MilitaryEffect> end_training(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const ActionCoreView& action,Integer tick,const StableId& code){
    auto fact=get(s.training_facts,action.id);auto closed=close_drill(cat,*fact,std::nullopt);if(!closed.ok)return {false,{},closed.error};
    fact->phase="completed";fact->completed_tick=tick;fact->success=code.empty()&&fact->lessons.size()==4;fact->code=code;
    auto unit=get(s.military_units,fact->terms.unit_id);if(unit->training_action_id&&*unit->training_action_id==action.id)*unit->training_action_id="";
    ResourceRecipe resources;resources.operations.push_back(SetReservation{fact->terms.payer_wallet_id,action.id,0,true});auto applied=c.apply_resources(resources);if(!applied.ok)return {false,{},applied.error};
    ActionMutation mutation;mutation.action.id=action.id;mutation.remaining_ticks=0;applied=c.apply_action(mutation);if(!applied.ok)return {false,{},applied.error};
    return {true,receipt(*fact),{}};
}
// 只读公职与营地归属，保持原ready_office首错次序。
Status office_ready(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const StableId& actor,const StableId& office){
    auto o=get(s.offices,office);auto d=get(cat.offices,office);auto p=c.actor(actor);
    if(!o||!d||!p||!p->alive||!p->available||!o->enabled||o->holder_actor_id!=actor)return fail("OfficeAuthorityRequired");
    auto owner=military_garrison_owner(c,s,cat,access,d->garrison_id);if(!owner.ok)return {false,{},owner.error};
    if(owner.value.organization_id!=d->organization_id)return fail("OfficeAuthorityLost");
    return common_success();
}
// 缺预留和显式零预留必须可区分，以便校验最后半课的付款边界。
std::optional<Integer> reservation(const WalletView& wallet,const StableId& id){for(auto& r:wallet.reserved)if(r.action_id==id)return r.amount;return std::nullopt;}
}

// 正常报价不改变世界；续课只重验服务及实际名册，沿用首课前已经冻结的兵法题面。
Outcome<MilitaryQuote> preview_military_training(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryCommand& command,const StableId& own){
    if(auto cancel=std::get_if<MilitaryTrainCancel>(&command)){
        if(!cancel->input_valid)return error<MilitaryQuote>("InvalidMilitaryTrainingCancellation");
        auto current=current_action(c,s,cat,a,actor,cancel->action_id);if(!current.ok)return {false,{},current.error};MilitaryQuote quote;quote.action_id=current.value->id;quote.provided_half_days=get(s.training_facts,quote.action_id)->lessons.size();return {true,quote,{}};
    }
    if(auto drill=std::get_if<MilitaryDrillCommand>(&command)){
        auto current=current_action(c,s,cat,a,actor,drill->action_id);if(!current.ok)return {false,{},current.error};auto fact=get(s.training_facts,current.value->id);auto checked=validate_drill(cat,*fact);if(!checked.ok)return {false,{},checked.error};if(!fact->drill)return error<MilitaryQuote>("TrainingDrillNotOffered");if(fact->drill->phase!="pending")return error<MilitaryQuote>("TrainingDrillAlreadyFrozen");if(!drill->input_valid)return error<MilitaryQuote>("InvalidTrainingDrillRequest");auto& before=fact->drill->challenge;auto changed=apply_original_training_drill_command(cat,fact->terms,before,*drill);
        if(!changed.ok)return {false,{},changed.error};
        MilitaryQuote quote;quote.action_id=current.value->id;quote.drill_before=before;quote.drill_after=std::move(changed.value);return {true,std::move(quote),{}};
    }
    const auto* running=get(s.training_facts,own);
    if(running){auto valid=validate_drill(cat,*running);if(!valid.ok)return {false,{},valid.error};}
    return preview_original_military_training_terms(c,s,cat,a,actor,command,own,
        running?&running->terms:nullptr,running?Integer(running->lessons.size()):0);
}

// Both legacy and exact owners use this original eligibility/quote algorithm.
// Accepted terms are read from their original owner before this call; this pure
// function never manufactures an acceptance or changes any historical source.
Outcome<MilitaryQuote> preview_original_military_training_terms(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryCommand& command,const StableId& own,const MilitaryTrainingTerms* accepted,Integer completed_lessons){
    if(completed_lessons<0||completed_lessons>=4||(!accepted&&completed_lessons))return error<MilitaryQuote>("InvalidMilitaryTrainingLessonOrdinal");
    auto request=std::get_if<MilitaryTrain>(&command);if(!request||!request->input_valid||(request->challenge_mode&&*request->challenge_mode!="automatic"&&*request->challenge_mode!="manual"))return error<MilitaryQuote>("InvalidMilitaryTrainingRequest");
    auto authority=office_ready(c,s,cat,a,actor,request->office_id);if(!authority.ok)return {false,{},authority.error};auto office=get(cat.offices,request->office_id);auto held=get(s.offices,request->office_id);auto unit=get(s.military_units,request->unit_id);
    if(!unit||unit->garrison_id!=office->garrison_id||unit->organization_id!=office->organization_id||!unit->army_id.empty()||!unit->mobilizing_action_id.empty()||unit->fit.empty())return error<MilitaryQuote>("TrainingUnitUnavailable");
    if(!profile(*unit))return error<MilitaryQuote>("TrainingProfileRequired");
    if(!unit->training_action_id->empty()&&*unit->training_action_id!=own)return error<MilitaryQuote>("TrainingUnitReserved");
    if(*unit->training>=100&&own.empty())return error<MilitaryQuote>("MilitaryTrainingAtLimit");
    for(auto& battle:s.battles)if(battle.phase!="closed")for(auto& enlisted:battle.units)if(enlisted.unit_id==unit->id)return error<MilitaryQuote>("TrainingUnitInBattle");
    auto camp=get(cat.garrisons,office->garrison_id);if(!camp||!camp->training_wallet_id||!camp->training_fatigue_per_tick||*camp->training_fatigue_per_tick<0||*camp->training_fatigue_per_tick>20)return error<MilitaryQuote>("MilitaryTrainingNotOffered");
    std::vector<StableId> participants{actor};if(!has(participants,unit->commander_actor_id))participants.push_back(unit->commander_actor_id);for(auto& id:unit->fit)if(c.actor(id)&&!has(participants,id))participants.push_back(id);std::sort(participants.begin(),participants.end());for(auto& id:participants){auto ready=ready_actor(c,a,id,camp->place_id,own);if(!ready.ok)return {false,{},ready.error};}
    auto skill_id=skill(cat,unit->troop_type_id);if(skill_id.empty())return error<MilitaryQuote>("TrainingTroopUnsupported");auto xp=a.actor_skill?a.actor_skill(actor,skill_id):std::nullopt;if(!xp||level(*xp)<0)return error<MilitaryQuote>("InvalidTrainingSkill");
    auto payer=c.wallet(camp->wallet_id),recipient=c.wallet(*camp->training_wallet_id);auto proper=[&](const WalletView* w){return w&&w->owner.kind==Owner::Kind::organization&&w->owner.id==office->organization_id&&!w->owner.extra;};
    if(camp->wallet_id==*camp->training_wallet_id||!proper(payer)||!proper(recipient)||!a.common_catalog||!a.common_catalog->required_wallets.find(*camp->training_wallet_id))return error<MilitaryQuote>("TrainingPublicPaymentRequired");
    auto reserved=own.empty()?0:reservation(*payer,own).value_or(0);Integer required_space=accepted?military_training_progress::lesson_fee(completed_lessons+1):150;
    if(own.empty()){auto balance=c.spendable(camp->wallet_id);if(!balance.ok)return {false,{},balance.error};if(balance.value<150)return error<MilitaryQuote>("InsufficientFunds");}if(reserved<0||recipient->balance>2147483647-required_space)return error<MilitaryQuote>("TrainingPaymentBlocked");
    auto owner=military_garrison_owner(c,s,cat,a,camp->id);if(!owner.ok)return {false,{},owner.error};
    MilitaryTrainingTerms t;t.actor_id=actor;t.office_id=request->office_id;t.authority_source=held->appointment_id;t.unit_id=unit->id;t.garrison_id=camp->id;t.organization_id=office->organization_id;t.ownership_id=owner.value.occupation_id;t.place_id=camp->place_id;t.commander_actor_id=unit->commander_actor_id;t.troop_type_id=unit->troop_type_id;t.fit_ids=unit->fit;std::sort(t.fit_ids.begin(),t.fit_ids.end());t.participants=participants;t.skill_id=skill_id;t.skill_xp=*xp;t.skill_level=level(*xp);t.gain_total=3+t.skill_level;t.training_before=*unit->training;t.fatigue_before=*unit->fatigue;t.fatigue_per_tick=*camp->training_fatigue_per_tick;t.payer_wallet_id=camp->wallet_id;t.recipient_wallet_id=*camp->training_wallet_id;
    if(accepted){t.drill=accepted->drill;}
    else{auto quoted=drill_quote(cat,a,t,request->challenge_mode.value_or("automatic"));if(!quoted.ok)return {false,{},quoted.error};t.drill=quoted.value;}
    MilitaryQuote out;out.duration=4;out.training_cost=150;out.participants=participants;out.training_terms=t;out.expected_training_gain=std::min<Integer>(100,*unit->training+3+t.skill_level)-*unit->training;return {true,std::move(out),{}};
}

// 共用runtime建立占用后重新报价，再一次性预留公款和单位；题面只抽一次minigame。
Status start_military_training(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action,const MilitaryTrain& request,const MilitaryQuote& offer){
    if(action.kind!="military_train"||action.id.empty())return fail("InvalidMilitaryTrainingAction");
    if(get(s.training_facts,action.id))return fail("MilitaryTrainingAlreadyStarted");
    auto current=preview_military_training(c,s,cat,a,action.actor_id,MilitaryCommand{request},action.id);if(!current.ok)return {false,{},current.error};if(!offer.training_terms||!same_terms(*current.value.training_terms,*offer.training_terms))return fail("StaleMilitaryTrainingQuote");auto terms=*current.value.training_terms;
    if(action.participants!=terms.participants||action.start_tick!=c.tick()||action.remaining_ticks!=4)return fail("InvalidMilitaryTrainingEnvelope");
    for(auto& id:terms.participants)if(!c.actor(id)||c.actor(id)->action_id!=action.id)return fail("TrainingActorOccupationMissing");
    auto spendable=c.spendable(terms.payer_wallet_id);if(!spendable.ok)return {false,{},spendable.error};if(spendable.value<150)return fail("InsufficientFunds");
    MilitaryTrainingFact fact;fact.id=action.id;fact.actor_id=action.actor_id;fact.start_tick=action.start_tick;fact.terms=terms;
    if(terms.drill){auto seeded=create_original_training_drill_in_candidate(c,cat,a,terms,fact.id);if(!seeded.ok)return {false,{},seeded.error};TrainingDrillInstance instance;instance.origin_seed=std::move(seeded.value.origin_seed);instance.origin_state=std::move(seeded.value.origin_state);instance.origin_field_order=std::move(seeded.value.origin_field_order);instance.challenge=std::move(seeded.value.challenge);fact.drill=std::move(instance);}
    ResourceRecipe resources;resources.operations.push_back(SetReservation{terms.payer_wallet_id,action.id,150,false});auto changed=c.apply_resources(resources);if(!changed.ok)return changed;*get(s.military_units,terms.unit_id)->training_action_id=action.id;
    changed=c.append_fact({action.id,"military_training","military"});if(!changed.ok)return changed;s.training_actions.push_back({action.id,terms,{}});s.training_facts.push_back(std::move(fact));return common_success();
}

// 每半日先消费已存在的唯一供给事实，再冻结题面、扣该课差额和改变部队条件。
Outcome<MilitaryTrainingStep> step_military_training(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ActionCoreView& action,Integer finish_tick){
    auto found=record(c,s,cat,action);if(!found.ok)return {false,{},found.error};auto fact=get(s.training_facts,action.id);if(fact->phase=="completed")return {true,{true,false,false},{}};if(!fact->lessons.empty()&&fact->lessons.back().tick==finish_tick)return {true,{action.remaining_ticks==0,false,false},{}};
    auto valid=validate_military_training_action(c,s,cat,a,action);if(!valid.ok)return {false,{},valid.error};if(finish_tick!=c.tick()+1)return error<MilitaryTrainingStep>("MilitaryTrainingClockMismatch");auto& terms=fact->terms;MilitaryTrain request;request.office_id=terms.office_id;request.unit_id=terms.unit_id;auto continuation=preview_military_training(c,s,cat,a,action.actor_id,MilitaryCommand{request},action.id);StableId code;
    if(!continuation.ok)code=continuation.error.original.code;else if(!same_terms(terms,*continuation.value.training_terms,true))code="MilitaryTrainingConditionsChanged";
    Outcome<StableId> supplied;if(code.empty()){supplied=find_supply(s,cat,terms,finish_tick);if(!supplied.ok){if(supplied.error.original.code!="MilitaryTrainingUnsupplied")return {false,{},supplied.error};code=supplied.error.original.code;}}
    if(!code.empty()){fact->code=code;ActionMutation mutation;mutation.action.id=action.id;mutation.remaining_ticks=0;auto applied=c.apply_action(mutation);if(!applied.ok)return {false,{},applied.error};return {true,{true,false,false},{}};}
    auto frozen=close_drill(cat,*fact,finish_tick);if(!frozen.ok)return {false,{},frozen.error};Integer index=fact->lessons.size()+1,cost=military_training_progress::lesson_fee(index);auto unit=get(s.military_units,terms.unit_id);MilitaryTrainingLesson lesson;lesson.tick=finish_tick;lesson.supply_fact_id=supplied.value;lesson.fee_paid=cost;lesson.training_before=*unit->training;lesson.training_after=training_after(*fact,index);lesson.fatigue_before=*unit->fatigue;lesson.fatigue_after=military_training_progress::unit_fatigue(terms,index);
    ResourceRecipe resources;auto money_recipe=resource_recipe::append_reserved_money_payment(resources,SetReservation{terms.payer_wallet_id,action.id,military_training_progress::unprovided_fee(index),false},terms.recipient_wallet_id,cost,resource_recipe::ReservedMoneyPaymentOrder::release_then_original_balance);if(!money_recipe.ok)return {false,{},money_recipe.error};auto applied=c.apply_resources(resources);if(!applied.ok)return {false,{},applied.error};*unit->training=lesson.training_after;*unit->fatigue=lesson.fatigue_after;fact->lessons.push_back(std::move(lesson));ActionMutation mutation;mutation.action.id=action.id;mutation.remaining_ticks=4-index;applied=c.apply_action(mutation);if(!applied.ok)return {false,{},applied.error};return {true,{index==4,false,true},{}};
}

// 最后一步或条件中断只封已提供账，重复finish只读取原回执。
Outcome<MilitaryEffect> finish_military_training(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess&,const ActionCoreView& action,Integer finish_tick){
    auto checked=record(c,s,cat,action);if(!checked.ok)return {false,{},checked.error};auto fact=checked.value;if(fact->phase=="completed")return {true,receipt(*fact,true),{}};if(action.remaining_ticks!=0||finish_tick!=c.tick()+1||(fact->code.empty()&&fact->lessons.size()!=4))return error<MilitaryEffect>("MilitaryTrainingNotReady");return end_training(c,s,cat,action,finish_tick,fact->code);
}

// 取消和布阵都走同一只读报价；中央候选提交前没有第二套题面或钱款状态。
Outcome<MilitaryEffect> apply_military_training(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryCommand& command){
    auto quote=preview_military_training(c,s,cat,a,actor,command);if(!quote.ok)return {false,{},quote.error};if(std::holds_alternative<MilitaryTrainCancel>(command)){auto action=c.action(quote.value.action_id);return end_training(c,s,cat,*action,c.tick(),"MilitaryTrainingCancelled");}if(std::holds_alternative<MilitaryDrillCommand>(command)&&quote.value.drill_after){get(s.training_facts,quote.value.action_id)->drill->challenge=std::move(*quote.value.drill_after);MilitaryEffect out;out.action_id=quote.value.action_id;return {true,out,{}};}return error<MilitaryEffect>("UnsupportedAction");
}

// 活动校验同时核事实、双方占用、公共预留和固定进度，不用人物疲劳代替部队条件。
Status validate_military_training_action(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess&,const ActionCoreView& action){
    auto checked=record(c,s,cat,action);if(!checked.ok)return {false,{},checked.error};auto& f=*checked.value;auto& t=f.terms;Integer count=f.lessons.size();if(f.phase!="started"||!f.code.empty()||count>=4||action.remaining_ticks!=4-count||c.tick()!=f.start_tick+count)return fail("MilitaryTrainingProgressMismatch");auto unit=get(s.military_units,t.unit_id);if(!unit->training_action_id||*unit->training_action_id!=action.id)return fail("TrainingUnitOccupationMissing");if(!unit->training||!unit->fatigue||*unit->training!=training_after(f,count)||*unit->fatigue!=military_training_progress::unit_fatigue(t,count))return fail("MilitaryTrainingConditionMismatch");auto wallet=c.wallet(t.payer_wallet_id);Integer required=military_training_progress::unprovided_fee(count);if(reservation(*wallet,action.id)!=std::optional<Integer>{required}||wallet->balance<required)return fail("MilitaryTrainingReservationMismatch");for(auto& id:t.participants)if(!c.actor(id)||c.actor(id)->action_id!=action.id)return fail("TrainingActorOccupationMissing");return common_success();
}

// 稳定世界核每条历史与当前占用，再交共享条件折叠唯一重放训练和行军。
Status validate_military_training_state(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a){
    for(auto& f:s.training_facts){auto checked=validate_fact(c,s,cat,f,&a);if(!checked.ok)return checked;if(f.phase=="started"){auto action=c.action(f.id);if(!action)return fail("MissingMilitaryTrainingAction");checked=validate_military_training_action(c,s,cat,a,*action);if(!checked.ok)return checked;}else if(c.action(f.id)||f.completed_tick>c.tick()||reservation(*c.wallet(f.terms.payer_wallet_id),f.id))return fail("MilitaryTrainingCompletionMismatch");}
    for(auto& unit:s.military_units){auto source=get(cat.garrisons,unit.source_garrison_id);auto definition=source?get(source->units,unit.id):nullptr;bool history=false;for(auto& f:s.training_facts)if(f.terms.unit_id==unit.id)history=true;if((!definition||!definition->initial_training)&&!unit.training&&!history)continue;if(!definition||!definition->initial_training||!definition->initial_fatigue||*definition->initial_training<0||*definition->initial_training>100||*definition->initial_fatigue<0||*definition->initial_fatigue>100||!profile(unit))return fail("TrainingProfileRequired",unit.id);auto folded=fold_military_condition(s,cat,unit.id,c.tick());if(!folded.ok)return fail(folded.error.original.code=="MilitaryConditionHistoryTimeMismatch"?"DuplicateMilitaryTrainingTick":"MilitaryTrainingHistoryMismatch",unit.id);if(*unit.training!=folded.value.training||*unit.fatigue!=folded.value.fatigue)return fail("MilitaryTrainingHistoryMismatch",unit.id);if(!unit.training_action_id->empty()){auto action=c.action(*unit.training_action_id);const MilitaryTrainingAction* payload=nullptr;for(auto& p:s.training_actions)if(p.action_id==*unit.training_action_id)payload=&p;if(!action||action->kind!="military_train"||!payload||payload->training.unit_id!=unit.id)return fail("TrainingUnitOccupationMissing",unit.id);}}
    return common_success();
}

// 原口粮并集只额外加入仍在场且持有效公职的本课教官，失权由下一课正常中断。
Outcome<std::vector<StableId>> military_training_mouths(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const MilitaryUnitRecord& unit,const StableId& camp_id){
    if(!unit.training_action_id||unit.training_action_id->empty())return {true,{}, {}};
    auto& id=*unit.training_action_id;auto action=c.action(id);auto fact=get(s.training_facts,id);const MilitaryTrainingAction* payload=nullptr;for(auto& p:s.training_actions)if(p.action_id==id)payload=&p;
    if(!action||action->kind!="military_train"||!payload||!fact||fact->phase!="started")return error<std::vector<StableId>>("InvalidTrainingSupplySource");
    auto& t=payload->training;if(!same_terms(fact->terms,t)||t.unit_id!=unit.id||t.garrison_id!=camp_id||t.actor_id!=action->actor_id||t.participants!=action->participants)return error<std::vector<StableId>>("InvalidTrainingSupplySource");auto actor=c.actor(action->actor_id);auto camp=get(cat.garrisons,camp_id);auto place=actor?std::get_if<SettlementLocation>(&actor->location):nullptr;if(!actor||!actor->alive||!actor->available||actor->action_id!=id||!camp||!place||place->place_id!=camp->place_id)return {true,{}, {}};auto authority=office_ready(c,s,cat,a,action->actor_id,t.office_id);auto office=get(cat.offices,t.office_id);auto held=get(s.offices,t.office_id);if(!authority.ok||!office||office->garrison_id!=camp_id||!held||held->appointment_id!=t.authority_source)return {true,{}, {}};return {true,{action->actor_id},{}};
}

// 视图与运行共用同一题面来源和行动绑定校验，不重新创建或抽取实例。
Status validate_training_drill_binding(const MilitaryCatalog& cat,const MilitaryTrainingFact& fact){return validate_drill(cat,fact);}
bool same_original_military_training_terms(const MilitaryTrainingTerms& a,const MilitaryTrainingTerms& b,bool continuation){return same_terms(a,b,continuation);}
Outcome<Integer> original_military_training_fee(Integer ordinal){if(ordinal<1||ordinal>4)return error<Integer>("InvalidMilitaryTrainingLessonOrdinal");return {true,military_training_progress::lesson_fee(ordinal),{}};}
Outcome<Integer> original_military_training_after(const MilitaryTrainingTerms& t,Integer count,std::optional<Integer> performance){
    if(count<0||count>4||t.training_before<0||t.training_before>100||t.gain_total!=3+level(t.skill_xp)||t.skill_level!=level(t.skill_xp)||level(t.skill_xp)<0||bool(t.drill)&&count>0&&!performance||performance&&(*performance<10000||*performance>12000))return error<Integer>("InvalidMilitaryTrainingResultBasis");
    Integer bp=count>0&&t.drill?*performance:10000;
    return {true,military_training_progress::unit_training(t,count,bp),{}};
}
Status validate_original_military_training_supply(const MilitarySupplyFact& f,const MilitaryCatalog& cat,const MilitaryTrainingTerms& t,Integer period_to_tick){return supply_fact(f,cat,t,period_to_tick);}
Status validate_original_training_drill_identity(const MilitaryCatalog& cat,const MilitaryTrainingTerms& t,const StableId& actor,const StableId& source_id,const StableId& origin_seed,const StableId& origin_state,const DrillState& challenge){
    auto camp=get(cat.garrisons,t.garrison_id);if(!camp)return fail("MissingTrainingDrillGarrison");
    if(!camp->training_drill_id)return !t.drill?common_success():fail("UnexpectedTrainingDrill");
    auto definition=drill_definition(cat,t);if(!definition)return fail("MissingTrainingDrill");auto checked=validate_drill_definition(*definition);if(!checked.ok)return checked;
    if(!t.drill)return fail("InvalidTrainingDrillTerms");
    auto& terms=*t.drill;
    if(terms.kind!="drill"||terms.challenge_id!=definition->id||(terms.mode!="manual"&&terms.mode!="automatic")||level(terms.tactics_xp)<0||terms.actor_snapshot.skill_level!=level(terms.tactics_xp)||terms.actor_snapshot.auxiliary_attribute<1||terms.actor_snapshot.auxiliary_attribute>110)return fail("InvalidTrainingDrillTerms");
    if(t.fit_ids.empty()||!sorted_unique(t.fit_ids)||!same_group(terms.group_source,group_source(t,*definition)))return fail("TrainingDrillRosterMismatch");
    auto created=create_drill(*definition,actor,"verify",terms.actor_snapshot,"0",terms.mode);if(!created.ok)return {false,{},created.error};if(created.value.source_sha256!=terms.source_sha256)return fail("TrainingDrillSourceChanged");
    Integer seed=0,state=0;auto a=std::from_chars(origin_seed.data(),origin_seed.data()+origin_seed.size(),seed),b=std::from_chars(origin_state.data(),origin_state.data()+origin_state.size(),state);
    if(a.ec!=std::errc()||a.ptr!=origin_seed.data()+origin_seed.size()||b.ec!=std::errc()||b.ptr!=origin_state.data()+origin_state.size()||std::to_string(seed)!=origin_seed||std::to_string(state)!=origin_state)return fail("InvalidTrainingDrillInstance");
    checked=validate_drill_state(*definition,challenge);if(!checked.ok)return checked;auto draw=godot_pcg32_range(state,-2147483648LL,2147483647);
    if(!draw.ok||challenge.definition_id!=terms.challenge_id||challenge.source_sha256!=terms.source_sha256||challenge.mode!=terms.mode||challenge.actor_snapshot.skill_level!=terms.actor_snapshot.skill_level||challenge.actor_snapshot.auxiliary_attribute!=terms.actor_snapshot.auxiliary_attribute||challenge.actor_id!=actor||challenge.source_id!=source_id||challenge.seed!=std::to_string(draw.value.value+2147483648LL))return fail("TrainingDrillBindingMismatch");
    return common_success();
}
Outcome<OriginalTrainingDrillSeeded> create_original_training_drill_in_candidate(CommonWrite& c,const MilitaryCatalog& cat,const MilitaryAccess& a,const MilitaryTrainingTerms& t,const StableId& source_id){
    if(!t.drill)return error<OriginalTrainingDrillSeeded>("TrainingDrillNotOffered");
    auto quoted=drill_quote(cat,a,t,t.drill->mode);if(!quoted.ok)return {false,{},quoted.error};
    if(!quoted.value||!same_drill_terms(*quoted.value,*t.drill))return error<OriginalTrainingDrillSeeded>("TrainingDrillSnapshotChanged");
    auto origin=a.rng_stream?a.rng_stream("minigame"):std::nullopt;if(!origin)return error<OriginalTrainingDrillSeeded>("InvalidMinigameRandomState");
    auto draw=godot_pcg32_range(origin->state,-2147483648LL,2147483647LL);if(!draw.ok)return {false,{},draw.error};
    auto created=create_drill(*drill_definition(cat,t),t.actor_id,source_id,t.drill->actor_snapshot,std::to_string(draw.value.value+2147483648LL),t.drill->mode);if(!created.ok)return {false,{},created.error};
    if(created.value.source_sha256!=t.drill->source_sha256)return error<OriginalTrainingDrillSeeded>("TrainingDrillSourceChanged");
    OriginalTrainingDrillSeeded out{std::to_string(origin->seed),std::to_string(origin->state),origin->field_order,std::move(created.value)};
    auto consumed=c.random_bounded("minigame",-2147483648LL,2147483647LL);if(!consumed.ok)return {false,{},consumed.error};
    if(consumed.value!=draw.value.value)return error<OriginalTrainingDrillSeeded>("InvalidMinigameRandomState");return {true,std::move(out),{}};
}
Outcome<DrillState> apply_original_training_drill_command(const MilitaryCatalog& cat,const MilitaryTrainingTerms& terms,const DrillState& before,const MilitaryDrillCommand& command){
    const auto* definition=drill_definition(cat,terms);if(!definition)return error<DrillState>("MissingTrainingDrill");
    if(!command.input_valid)return error<DrillState>("InvalidTrainingDrillRequest");
    if(command.kind=="military_drill_draft"){if(!command.expected_step||!command.target)return error<DrillState>("InvalidTrainingDrillRequest");return draft_drill_move(*definition,before,*command.expected_step,command.group_id,*command.target);}
    if(command.kind=="military_drill_confirm"||command.kind=="military_drill_cancel_pending"){if(!command.expected_step||!command.expected_pending)return error<DrillState>("InvalidTrainingDrillRequest");return command.kind=="military_drill_confirm"?confirm_drill_move(*definition,before,*command.expected_step,*command.expected_pending):cancel_drill_pending(*definition,before,*command.expected_step,*command.expected_pending);}
    if(command.kind=="military_drill_exit")return exit_drill(*definition,before);return error<DrillState>("UnsupportedAction");
}
Outcome<DrillState> close_original_training_drill(const MilitaryCatalog& cat,const MilitaryTrainingTerms& terms,const DrillState& state){auto definition=drill_definition(cat,terms);if(!definition)return error<DrillState>("MissingTrainingDrill");return exit_drill(*definition,state);}

}
