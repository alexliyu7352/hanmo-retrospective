// 本人课程、经验和货单的有限投影；不导出世界，也不把老师精确经验与支付私有条款当页面数据。
#include "growth_internal.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/views/common_presentation.hpp"
#include "resident/views/character_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
    namespace {
        using namespace boundary;
        BoundaryValue text(const StableId& s){return BoundaryValue{s};
        }
        BoundaryValue number(Integer n){return BoundaryValue{n};
        }
        void add(BoundaryValue& value,const StableId& key,BoundaryValue v){std::get<BoundaryValue::Dict>(value.data).emplace_back(key,std::move(v));
        }
        StableId person(const CommonStore& common,const CommonCatalog& catalog,const StableId& id){auto* a=common.state().actors.find(id);
            if(a&&a->display_name)return *a->display_name;
            auto* n=catalog.actor_names.find(id);
            if(n){auto* value=catalog.texts.find(n->name_key);
                if(value)return *value;
            }return "姓名未记";
        }
        BoundaryValue active_study(const CommonStore& common,const GrowthState& state,const StableId& actor_id){auto* actor=common.actor(actor_id);
            auto* action=actor?common.action(actor->action_id):nullptr;
            if(!action||!growth::study_action_kind(action->kind)){
                for(const auto& row:state.facts.rows())if(auto* fact=std::get_if<StudyFact>(&row.second))if(fact->phase=="started"&&growth::study_initiator(fact->terms)==actor_id&&growth::study_paused(*fact)){auto* pending=common.action(row.first);
                    if(pending&&growth::study_action_kind(pending->kind)){action=pending;break;}
                }
            }
            if(!action||!growth::study_action_kind(action->kind))return obj({{"active",BoundaryValue{false}}});
            auto* value=state.facts.find(action->id);
            auto* f=value?std::get_if<StudyFact>(value):nullptr;
            if(!f)boundary::fail("MissingStudyFact",action->id);
            auto reserve=growth::reserved(common.wallet(f->terms.payer_wallet_id),action->id);
            auto result=obj({{"active",BoundaryValue{true}},{"actor_id",text(actor_id)},{"action_id",text(action->id)},{"kind",text(action->kind)},{"remaining_ticks",number(action->remaining_ticks)},{"completed_lessons",number(f->lessons.size())},{"paid",number(f->terms.fee-reserve)},{"reserved",number(reserve)},{"challenge_scopes",string_array(f->challenge?std::vector<StableId>{"study"}:std::vector<StableId>{})},{"paused",BoundaryValue{growth::study_paused(*f)}},{"pause_supported",BoundaryValue{growth::study_pause_supported(*f)}},{"paused_ticks",number(growth::study_paused_ticks(*f,common.tick()))}});
            // 新账只显示已实际入账贡献；旧终态课程保持原有限字段和解释。
            if(growth::study_segmented(*f)){add(result,"settlement_policy",text(*f->terms.settlement_policy));
                add(result,"xp_gained",number(growth::study_xp_gained(*f)));
                if(f->terms.innate_source)add(result,"learning_bonus_bp",number(f->terms.innate_source->bonus_bp));
                add(result,"challenge_pending",BoundaryValue{f->challenge&&growth::challenge_pending(*f->challenge)});
            }return result;
        }
    }
    Outcome<BoundaryValue> growth_courses_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& actor_id,const InteractionContext& context,Integer ticks){if(ticks<2||ticks>6||context.kind||context.encounter_id)return {false,{},common_failure("InvalidStudyRequest").error};
        if(!catalog.cultivation)return {false,{},common_failure("CultivationUnavailable").error};
        auto contact=access.contact(actor_id,context,"study_skill",{});
        if(!contact.ok)return {false,{},contact.error};
        auto* actor=common.state().actors.find(actor_id);
        if(!actor)return {false,{},common_failure("UnknownActor").error};
        BoundaryValue::Array rows;
        for(const auto& id:growth_skill_ids(shared.extended_skills)){auto* course=growth::course(catalog,contact.value.role_id,id);
            if(!course)continue;
            const auto xp=growth_skill_xp(actor->skills,id),level=growth_skill_level(xp);
            if(level<0)return {false,{},common_failure("InvalidStudyActor").error};
            GrowthCommand command;
            command.kind="study_skill";
            command.skill_id=id;
            command.ticks=ticks;
            command.context=contact.value;
            auto quote=preview_growth(common,state,catalog,shared,social,access,actor_id,command);
            auto row=obj({{"skill_id",text(id)},{"mode",text(course->mode)},{"xp",number(xp)},{"level",number(level)},{"next_threshold",level<4?number(growth::original_skill_threshold(level+1)):BoundaryValue{std::monostate{}}},{"eligible",BoundaryValue{quote.ok}},{"code",text(quote.ok?"":quote.error.original.code)},{"params",encode_growth_command(command)}});
            if(quote.ok){auto& t=std::get<StudyTerms>(quote.value.data);
                auto offer=obj({{"duration",number(quote.value.duration)},{"cost",number(quote.value.cost)},{"expected_xp",number(quote.value.expected_xp)},{"target_level",number(quote.value.target_level)},{"xp_limit",number(t.xp_limit)},{"payment_policy",text("reserve_then_pay_per_lesson")},{"cancel_supported",BoundaryValue{true}}});
                if(t.settlement_policy)add(offer,"settlement_policy",text(*t.settlement_policy));
                if(t.innate_source)add(offer,"learning_bonus_bp",number(t.innate_source->bonus_bp));
                if(t.challenge){const auto& frozen=*t.challenge;
                    auto challenge=obj({{"challenge_id",text(frozen.challenge_id)},{"modes",string_array({"automatic","manual"})}});
                    if(frozen.field=="challenge"){auto context_value=frozen.context?obj({{"budget",number(frozen.context->budget)},{"quantity",number(frozen.context->quantity)},{"relation_discount_bp",number(frozen.context->relation_discount_bp)},{"quote",BoundaryValue{std::monostate{}}}}):BoundaryValue{std::monostate{}};
                        challenge=obj({{"kind",text(frozen.kind)},{"challenge_id",text(frozen.challenge_id)},{"modes",string_array({"automatic","manual"})},{"context",std::move(context_value)}});
                    }add(offer,frozen.field,std::move(challenge));
                }add(row,"offer",std::move(offer));
            }rows.push_back(std::move(row));
        }return {true,obj({{"ok",BoundaryValue{true}},{"actor_id",text(actor_id)},{"context",interaction_codec::encode_context(contact.value)},{"ticks",number(ticks)},{"courses",BoundaryValue{std::move(rows)}}}),{}};
    }
    Outcome<BoundaryValue> growth_skills_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& actor_id,const StableId& requested){auto* actor=common.state().actors.find(actor_id);
        if(!actor)return {false,{},common_failure("UnknownActor").error};
        const auto& skill_ids=growth_skill_ids(shared.extended_skills);
        auto selected=std::find(skill_ids.begin(),skill_ids.end(),requested)!=skill_ids.end()?requested:skill_ids.front();
        BoundaryValue::Array skills,records;
        for(const auto& id:skill_ids){auto xp=growth_skill_xp(actor->skills,id),level=growth_skill_level(xp);
            skills.push_back(obj({{"skill_id",text(id)},{"xp",level>=0?number(xp):BoundaryValue{std::monostate{}}},{"level",number(level)},{"next_threshold",level>=0&&level<4?number(growth::original_skill_threshold(level+1)):BoundaryValue{std::monostate{}}}}));
        }std::vector<std::pair<StableId,const StudyFact*>> history;
        for(const auto& row:state.facts.rows())if(auto* fact=std::get_if<StudyFact>(&row.second))if(fact->phase=="completed"&&fact->actor_id==actor_id&&fact->terms.student_id==actor_id&&fact->terms.skill_id==selected)history.emplace_back(row.first,fact);
        std::sort(history.begin(),history.end(),[](const auto& a,const auto& b){return a.second->completed_tick!=b.second->completed_tick?a.second->completed_tick>b.second->completed_tick:a.first<b.first;
        });
        for(const auto& row:history){const auto& f=*row.second;
            const auto& t=f.terms;
            // 开课总经验与本课贡献分开；结束总经验包含暂停期间正常发生的其他成长。
            const auto baseline=growth::study_segmented(f)?t.xp_before:f.xp_settlement_before.value_or(t.xp_before);
            const auto gain=growth::study_xp_gained(f);
            const auto end_xp=growth::study_segmented(f)?growth::study_xp_baseline(f):baseline+gain;
            auto paid=(growth::original_study_lesson_cost(t.xp_before))*Integer(f.lessons.size());
            auto* place=catalog.place_names.find(t.context.place_id);
            auto record=obj({{"action_id",text(row.first)},{"skill_id",text(selected)},{"completed_tick",number(f.completed_tick)},{"success",BoundaryValue{f.success}},{"code",text(f.code)},{"xp_before",number(baseline)},{"xp_gained",number(gain)},{"xp_after",number(end_xp)},{"completed_lessons",number(f.lessons.size())},{"duration",number(t.duration)},{"fee_paid",number(paid)},{"unprovided_released",number(t.fee-paid)},{"teacher_name",text(person(common,shared,t.teacher_id))},{"place_name",text(place?*place:"场所未记")}});
            if(growth::study_segmented(f))add(record,"settlement_policy",text(*f.terms.settlement_policy));
            if(t.innate_source)add(record,"learning_bonus_bp",number(t.innate_source->bonus_bp));
            records.push_back(std::move(record));
        }auto result=obj({{"actor_id",text(actor_id)},{"display_name",text(person(common,shared,actor_id))},{"skills",BoundaryValue{std::move(skills)}},{"selected_skill_id",text(selected)},{"course_records",BoundaryValue{std::move(records)}}});
        if(catalog.study_segments)add(result,"settlement_policy",text("per_lesson_v1"));
        return {true,std::move(result),{}};
    }
    Outcome<BoundaryValue> growth_challenge_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& actor_id,const StableId& action_id,bool etiquette_only){auto* actor=common.actor(actor_id);
        if(!actor||!actor->alive||!actor->available)return {false,{},common_failure("ActorUnavailable").error};
        auto* action=common.action(actor->action_id);
        if(!action||action->kind!="study_skill"||action->actor_id!=actor_id)return {false,{},common_failure("NotStudying").error};
        if(action->id!=action_id)return {false,{},common_failure("StaleStudyAction").error};
        auto* value=state.facts.find(action_id);
        auto* f=value?std::get_if<StudyFact>(value):nullptr;
        if(!f||!f->challenge||(etiquette_only&&f->challenge->frozen.kind!="etiquette"))return {false,{},common_failure("StudyChallengeNotOffered").error};
        auto valid=growth::challenge_validate(catalog,*f->challenge);
        if(!valid.ok)return {false,{},valid.error};
        auto view=growth::challenge_view(catalog,shared,*f->challenge);
        if(!etiquette_only){add(view,"challenge_kind",text(f->challenge->frozen.kind));
            add(view,"price_applied",BoundaryValue{false});
        }return {true,std::move(view),{}};
    }
// 手动模式在题面答完后仍阻止UI偷走首半日，只随整个真实课程结束而关闭。
    bool growth_manual_window(const CommonStore& common,const GrowthState& state,const StableId& id){auto* actor=common.actor(id);
        auto* action=actor?common.action(actor->action_id):nullptr;
        if(!action||action->kind!="study_skill"||action->actor_id!=id)return false;
        auto* value=state.facts.find(action->id);
        auto* fact=value?std::get_if<StudyFact>(value):nullptr;
        return fact&&fact->phase=="started"&&fact->actor_id==id&&fact->terms.challenge&&fact->terms.challenge->mode=="manual";
    }
// 家事休整页只看本人私庄及原累积账目，保持活动优先、历史同tick先出现者规则。
    Outcome<BoundaryValue> growth_rest_rows(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const GrowthAccess& access,const StableId& id){BoundaryValue::Array rows;
        auto ids=catalog.estate_ids;
        std::sort(ids.begin(),ids.end());
        auto* actor=common.actor(id);
        auto* action=actor?common.action(actor->action_id):nullptr;
        for(const auto& estate_id:ids){auto estate=access.rest_estate?access.rest_estate(estate_id):std::nullopt;
            if(!estate||estate->owner_actor_id!=id)continue;
            auto row=obj({{"estate_id",text(estate_id)},{"active_period",BoundaryValue{BoundaryValue::Dict{}}},{"active_ledger",BoundaryValue{BoundaryValue::Dict{}}},{"latest_completed",BoundaryValue{BoundaryValue::Dict{}}}});
            auto* payload=action?state.actions.find(action->id):nullptr;
            if(action&&action->kind=="long_rest"&&payload&&payload->request.estate_id==estate_id){*row.find("active_period")=obj({{"duration",number(payload->request.duration)},{"remaining_ticks",number(action->remaining_ticks)}});
                auto* value=state.facts.find(action->id);
                if(auto* fact=value?std::get_if<RestFact>(value):nullptr)*row.find("active_ledger")=obj({{"elapsed",number(fact->elapsed)},{"consumed",number(fact->consumed)},{"food_item_id",text(fact->food_item_id)}});
                rows.push_back(std::move(row));
                continue;
            }const RestFact* latest=nullptr;
            for(const auto& entry:state.facts.rows())if(auto* f=std::get_if<RestFact>(&entry.second))if(f->phase=="completed"&&f->actor_id==id&&f->estate_id==estate_id&&(!latest||f->completed_tick>latest->completed_tick))latest=f;
            if(latest)*row.find("latest_completed")=obj({{"stop_reason",text(latest->stop_reason)},{"elapsed",number(latest->elapsed)},{"consumed",number(latest->consumed)},{"food_item_id",text(latest->food_item_id)}});
            rows.push_back(std::move(row));
        }return {true,BoundaryValue{std::move(rows)},{}};
    }
// 武学和行囊页只复制本人有限输入；所有装备选项仍需正式preview检查当前自有未预留lot。
    static Outcome<BoundaryValue> character_summary(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& id,const StableId& page){auto* actor=common.state().actors.find(id);
        if(!actor)return {false,{},common_failure("UnknownActor").error};
        auto location=common.resolved_location(id);
        auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
        BoundaryValue action_value{BoundaryValue::Dict{}};
        if(auto* action=common.action(actor->action_id)){StableId skill;
            auto* payload=state.actions.find(action->id);
            auto* terms=payload?std::get_if<TrainingTerms>(&payload->quote.data):nullptr;
            if(terms)skill=terms->skill_id;
            action_value=obj({{"kind",text(action->kind)},{"skill_id",text(skill)},{"remaining_ticks",number(action->remaining_ticks)}});
        }auto view=obj({{"actor_id",text(id)},{"display_name",text(person(common,shared,id))},{"place_id",text(place?place->place_id:"")},{"fatigue",number(actor->fatigue)},{"injury",number(actor->injury)},{"action",std::move(action_value)}});
        CommonCharacterPresentation presentation(common.state(),shared);
        auto character=presentation.character(id);
        if(!character.ok)return {false,{},character.error};
        auto portrait=encode_character_presentation(character.value);
        if(!portrait.ok)return portrait;
        add(view,"portrait_input",std::move(portrait.value));
        auto* inventory=common.inventory(actor->inventory_id);
        if(!inventory)return {false,{},common_failure("MissingPersonalInventory").error};
        std::map<StableId,Integer> weapons,armors;
        for(const auto& lot:inventory->lots){auto* item=shared.items.find(lot.item_id);
            if(item&&lot.quantity>0&&owner_equal(lot.owner,Owner{Owner::Kind::actor,id,{}, {}})){if(item->weapon)weapons[lot.item_id]+=lot.quantity;
                if(item->armor)armors[lot.item_id]+=lot.quantity;
        }}BoundaryValue::Array weapon_rows,armor_rows;
        for(const auto& row:weapons)weapon_rows.push_back(obj({{"item_id",text(row.first)},{"quantity",number(row.second)}}));
        for(const auto& row:armors)armor_rows.push_back(obj({{"item_id",text(row.first)},{"quantity",number(row.second)}}));
        StableId selected=actor->combat.armor_id.value_or("");
        Integer defense=0;
        if(!selected.empty()){auto count=common.count(actor->inventory_id,selected,{Owner{Owner::Kind::actor,id,{}, {}}});
            auto amount=access.armor_defense?access.armor_defense(selected):std::nullopt;
            if(count.ok&&count.value>0&&amount)defense=*amount;
            else selected.clear();
        }auto effective=obj({{"ok",BoundaryValue{true}},{"duration",number(0)},{"armor_id",text(selected)},{"armor_defense",number(defense)}});
        add(view,"equipment",obj({{"weapon_id",text(actor->combat.weapon_id)},{"heart_id",text(actor->combat.heart_id)},{"weapon_options",BoundaryValue{std::move(weapon_rows)}},{"armor_id",text(actor->combat.armor_id.value_or(""))},{"armor_options",BoundaryValue{std::move(armor_rows)}},{"armor_effective",std::move(effective)}}));
        if(page=="inventory"){BoundaryValue::Array lots;
            for(const auto& lot:inventory->lots){StableId ownership="本人财物";
                if(lot.owner.kind==Owner::Kind::task){if(!access.task_name)return {false,{},common_failure("UnsupportedNativeCapability","task_inventory_label",ErrorKind::unsupported_capability).error};
                    ownership="委托公物 · "+access.task_name(lot.owner.id);
                }else if(lot.owner.kind==Owner::Kind::organization){auto* name=catalog.organization_names.find(lot.owner.id);
                    ownership="机构公物 · "+(name?*name:StableId{"所属机构"});
                }else if(lot.owner.id!=id)ownership="代管财物 · "+person(common,shared,lot.owner.id);
                StableId reservation="未预留";
                if(!lot.reserved_by.empty()){auto* action=common.action(lot.reserved_by);
                    StableId name="进行中的行动";
                    if(action){if(action->kind=="report_offer")name="委托交接";
                        else if(action->kind=="trade")name="交易交割";
                        else if(action->kind=="army_form")name="军团编组";
                        else if(action->kind=="army_supply")name="军需补给";
                        else if(action->kind=="estate_build")name="庄园营造";
                    }reservation="已预留 · "+name;
                }lots.push_back(obj({{"lot_id",text(lot.id)},{"item_id",text(lot.item_id)},{"quantity",number(lot.quantity)},{"ownership_text",text(ownership)},{"reservation_text",text(reservation)}}));
            }auto weight=common.inventory_weight(actor->inventory_id);
            if(!weight.ok)return {false,{},weight.error};
            add(view,"inventory",obj({{"weight",number(weight.value)},{"capacity",number(inventory->capacity)},{"lots",BoundaryValue{std::move(lots)}}}));
            return {true,std::move(view),{}};
        }
        std::vector<StableId> ids;
        for(const auto& row:actor->combat.learned.rows())if(shared.personal_skill_kinds.find(row.first))ids.push_back(row.first);
        std::sort(ids.begin(),ids.end());
        BoundaryValue::Array skills,students;
        for(const auto& skill:ids){const TrainingFact* origin=nullptr;
            const TrainingFact* taught=nullptr;
            for(const auto& row:state.facts.rows())if(auto* fact=std::get_if<TrainingFact>(&row.second))if(fact->phase=="completed"&&fact->success&&fact->training.skill_id==skill&&!fact->training.teacher_id.empty()){if(fact->training.student_id==id&&(!origin||fact->completed_tick<origin->completed_tick))origin=fact;
                if(fact->training.teacher_id==id&&(!taught||fact->completed_tick>taught->completed_tick))taught=fact;
            }StableId provenance="来源：未见传承记录";
            auto name=[&](const StableId& place){auto* value=catalog.place_names.find(place);
                return value?*value:StableId{"地点未记"};
            };
            if(origin)provenance="师承："+person(common,shared,origin->training.teacher_id)+" · "+name(origin->training.place_id);
            if(taught)provenance+="\n已传授："+person(common,shared,taught->training.student_id)+" · "+name(taught->training.place_id);
            skills.push_back(obj({{"skill_id",text(skill)},{"mastery",number(*actor->combat.learned.find(skill))},{"provenance_text",text(provenance)}}));
        }if(!ids.empty()&&place){auto roles=shared.roles.order();
            std::sort(roles.begin(),roles.end());
            std::vector<StableId> seen;
            for(const auto& role_id:roles){auto* role=shared.roles.find(role_id);
                auto* assignment=common.state().roles.find(role_id);
                if(role->place_id!=place->place_id||!assignment||!assignment->enabled||assignment->actor_id==id||std::find(seen.begin(),seen.end(),assignment->actor_id)!=seen.end())continue;
                auto* person_record=common.actor(assignment->actor_id);
                auto resolved=common.resolved_location(assignment->actor_id);
                auto* actual=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;
                if(!person_record||!person_record->alive||!actual||actual->place_id!=place->place_id)continue;
                InteractionContext context;
                context.place_id=place->place_id;
                context.role_id=role_id;
                context.provider_actor_id=assignment->actor_id;
                students.push_back(obj({{"display_name",text(person(common,shared,assignment->actor_id))},{"context",interaction_codec::encode_context(context)}}));
                seen.push_back(assignment->actor_id);
        }}add(view,"skills",BoundaryValue{std::move(skills)});
        add(view,"students",BoundaryValue{std::move(students)});
        return {true,std::move(view),{}};
    }
// 中央仅将固定query_id路由到此，观察者仍由当前world.player决定，参数不能读取其他人物账本。
    Outcome<BoundaryValue> query_growth_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const InteractionState& interaction,const GrowthAccess& access,const StableId& actor_id,const StableId& query_id,const BoundaryValue& args){try{auto* actor=common.actor(actor_id);
            if(!actor)return {false,{},common_failure("UnknownActor").error};
            if(query_id=="growth_cards"){
                fields(args,{}, {"actor_id","discovered","offset","limit"},query_id);
                if(auto* requested=args.find("actor_id"))if(string(*requested,query_id)!=actor_id)return {false,{},common_failure("NotPlayerActor").error};
                if(!catalog.cards.enabled)return {false,{},common_failure("CardsUnavailable",{},ErrorKind::unsupported_capability).error};
                if(!state.cards)return {false,{},common_failure("MissingGrowthCards").error};
                return growth_cards_view(common.state(),*state.cards,catalog.cards,shared,actor_id,args.find("discovered")?strings(*args.find("discovered"),query_id):std::vector<StableId>{},args.find("offset")?get_int(args,"offset",query_id):0,args.find("limit")?get_int(args,"limit",query_id):32);
            }
            if(query_id=="trade_bargain_source"){const auto sha=get_string(args,"basis_sha256",query_id);
                StableId quote_id;
                for(const auto& row:state.facts.rows())if(auto* f=std::get_if<BargainFact>(&row.second))if(f->actor_id==actor_id&&f->basis_sha256==sha){if(!quote_id.empty())return {true,obj({{"quote_id",text("")},{"duplicate",BoundaryValue{true}}}),{}};
                    quote_id=row.first;
                }return {true,obj({{"quote_id",text(quote_id)},{"duplicate",BoundaryValue{false}}}),{}};
            }if(query_id=="trade_bargain"||query_id=="trade_bargain_view")return growth::bargain_view(common,state,catalog,shared,access,actor_id,get_string(args,"quote_id",query_id));
            if(query_id=="action_manual_window")return {true,obj({{"ok",BoundaryValue{true}},{"manual",BoundaryValue{growth_manual_window(common,state,actor_id)}}}),{}};
            if(query_id=="character_summary")return character_summary(common,state,catalog,shared,access,actor_id,args.find("page")?get_string(args,"page",query_id):"skills");
            if(query_id=="ordinary_skills")return growth_skills_view(common,state,catalog,shared,actor_id,args.find("skill_id")?get_string(args,"skill_id",query_id):"");
            if(query_id=="study_activity")return {true,active_study(common,state,actor_id),{}};
            if(query_id=="study_courses"){if(auto* id=args.find("actor_id"))if(string(*id,query_id)!=actor_id)return {false,{},common_failure("NotPlayerActor").error};
                auto context=interaction_codec::decode_context(field(args,"context",query_id),query_id);
                return growth_courses_view(common,state,catalog,shared,social,access,actor_id,context,args.find("ticks")?get_int(args,"ticks",query_id):2);
            }if(query_id=="study_challenge"||query_id=="study_etiquette")return growth_challenge_view(common,state,catalog,shared,actor_id,get_string(args,"action_id",query_id),query_id=="study_etiquette");
            if(query_id=="study_course_contexts"){BoundaryValue::Array contexts;
                auto* location=std::get_if<SettlementLocation>(&actor->location);
                if(location)for(const auto& row:catalog.roles.rows())if(!row.second.courses.empty()){auto* role=shared.roles.find(row.first);
                    auto* assignment=common.state().roles.find(row.first);
                    if(role&&assignment&&role->place_id==location->place_id){InteractionContext context;
                        context.place_id=role->place_id;
                        context.provider_actor_id=assignment->actor_id;
                        context.role_id=row.first;
                        contexts.push_back(obj({{"role_id",text(row.first)},{"context",interaction_codec::encode_context(context)}}));
                }}return {true,obj({{"ok",BoundaryValue{true}},{"rows",BoundaryValue{std::move(contexts)}}}),{}};
            }
            if(query_id=="trade_cache_context"){bool trading=false;
                for(const auto& session:interaction.interactions)if(session.actor_id==actor_id&&session.phase=="trade")trading=true;
                return {true,obj({{"actor_id",text(actor_id)},{"is_trade",BoundaryValue{trading}}}),{}};
            }
            if(query_id=="trade_action_receipt"){const auto id=get_string(args,"action_id",query_id);
                auto* payload=state.actions.find(id);
                auto* action=common.action(id);
                if(!payload||!action||action->kind!="trade"||action->actor_id!=actor_id)return {true,BoundaryValue{BoundaryValue::Dict{}},{}};
                auto* t=std::get_if<TradeTerms>(&payload->quote.data);
                if(!t)return {false,{},common_failure("InvalidTradeReceipt").error};
                return {true,obj({{"action_id",text(id)},{"quote",obj({{"owner",encode_owner(t->owner)},{"item_id",text(t->item_id)},{"quantity",number(t->quantity)},{"total",number(t->total)},{"side",text(t->side)}})}}),{}};
            }
            if(query_id=="trade_view"){const auto shop_id=get_string(args,"shop_id",query_id);
                auto* shop=catalog.shops.find(shop_id);
                if(!shop)return {false,{},common_failure("UnknownShop").error};
                const auto provider=get_string(args,"provider_actor_id",query_id);
                const InteractionSession* session=nullptr;
                for(const auto& current:interaction.interactions)if(current.actor_id==actor_id&&current.phase=="trade"&&current.context.provider_actor_id==provider){session=&current;break;}
                if(!session)return {false,{},common_failure("TradeContextRequired").error};
                if(!access.contact)return {false,{},common_failure("UnsupportedNativeCapability","interaction_access",ErrorKind::unsupported_capability).error};
                // 正在装货的本人交易仍可读当前收据；不能把其他忙碌行动借作接触豁免。
                StableId own;
                if(const auto* action=common.action(actor->action_id);action&&action->actor_id==actor_id&&action->kind=="trade"){
                    const auto* payload=state.actions.find(action->id);
                    const auto* terms=payload?std::get_if<TradeTerms>(&payload->quote.data):nullptr;
                    if(terms&&terms->shop_id==shop_id&&terms->context.provider_actor_id==provider&&terms->context.place_id==session->context.place_id&&terms->context.role_id==session->context.role_id)own=action->id;
                }
                auto contact=access.contact(actor_id,session->context,"trade",own);
                if(!contact.ok)return {false,{},contact.error};
                if(contact.value.place_id!=shop->place_id||std::find(shop->representative_roles.begin(),shop->representative_roles.end(),contact.value.role_id)==shop->representative_roles.end())return {false,{},common_failure("WrongShopRepresentative").error};
                auto* inventory=common.inventory(shop->inventory_id);
                if(!inventory)return {false,{},common_failure("UnknownInventory").error};
                BoundaryValue::Array items,payers;
                for(const auto& id:catalog.tradeable_items){auto count=common.count(inventory->id,id,{inventory->owner});
                    if(!count.ok)return {false,{},count.error};
                    items.push_back(obj({{"item_id",text(id)},{"stock",number(count.value)}}));
                }if(access.procurement_payers)for(const auto& payer:access.procurement_payers(actor_id))payers.push_back(obj({{"offer_id",text(payer.first)},{"wallet_id",text(payer.second)}}));
                const auto pending=args.find("receipt_action_id")?get_string(args,"receipt_action_id",query_id):"";
                const auto* receipt=common.action(pending);
                return {true,obj({{"actor_id",text(actor_id)},{"personal_wallet_id",text(actor->wallet_id)},{"provider_display_name",text(person(common,shared,provider))},{"items",BoundaryValue{std::move(items)}},{"task_payers",BoundaryValue{std::move(payers)}},{"receipt_action_pending",BoundaryValue{receipt&&receipt->actor_id==actor_id&&receipt->kind=="trade"&&receipt->id==own}}}),{}};
            }
            return {false,{},common_failure("UnsupportedLiveQuery",query_id,ErrorKind::unsupported_capability).error};
        }catch(const boundary::DecodeFailure& e){return {false,{},e.error};
    }}
} // 查询只返回当前有限页，所有读值与正式命令使用同一候选、同一内容和同一资产来源。
