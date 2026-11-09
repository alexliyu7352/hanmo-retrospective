#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_trade_terminal_codec.hpp"
#include "growth_capability_application_codec.hpp"
#include "study_adjustment_time2_codec.hpp"
#include "long_rest_period.hpp"
#include "study_service_codec.hpp"
// 冷检查点逐字段遍历真实类型；不压入JSON黑箱，恢复后继续全域校验。
#include "growth_internal.hpp"
#include "growth_fixed_service_state_codec.hpp"
#include "growth_fixed_service_codec.hpp"
#include "resident/domains/family/innate_study_source.hpp"
#include "growth_teaching_requests_codec.hpp"
#include "growth_study_teaching_requests_codec.hpp"
#include "growth_life_results.hpp"
#include <algorithm>
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
namespace hanmo::resident {
    namespace {
        using namespace common_checkpoint;
// BargainQuoteContext的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status quote_context(CodecVisitor& c,BargainQuoteContext& v){c.field("quote_id",v.quote_id);
            c.field("quote_sha256",v.quote_sha256);
            c.field("market_revision",v.market_revision);
            c.field("valid_tick",v.valid_tick);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// StudyChallengeContext的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status context(CodecVisitor& c,StudyChallengeContext& v){c.field("budget",v.budget);
            c.field("quantity",v.quantity);
            c.field("relation_discount_bp",v.relation_discount_bp);
            strings(c,"field_order",v.field_order);
            c.optional("quote",v.quote,quote_context);
            return c.status();
        }
// GrowthChoice的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status choice(CodecVisitor& c,GrowthChoice& v){c.field("step_id",v.step_id);
            c.field("option_id",v.option_id);
            c.field("points",v.points);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// GrowthChallengeResult的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status result(CodecVisitor& c,GrowthChallengeResult& v){c.field("completion",v.completion);
            c.field("score",v.score);
            c.field("auto_score",v.auto_score);
            c.field("performance_bp",v.performance_bp);
            c.field("request_delta",v.request_delta);
            c.field("discount_bp",v.discount_bp);
            c.field("relation_discount_bp",v.relation_discount_bp);
            c.field("challenge_discount_bp",v.challenge_discount_bp);
            strings(c,"field_order",v.field_order);
            c.optional("raw_score",v.raw_score,[](auto& c,auto& x){return c.field("value",x);
            });
            c.sequence("choices",v.choices,choice);
            return c.status();
        }
// ChallengeFrozen的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status frozen(CodecVisitor& c,ChallengeFrozen& v){c.field("field",v.field);
            c.field("kind",v.kind);
            c.field("challenge_id",v.challenge_id);
            c.field("mode",v.mode);
            c.field("source_sha256",v.source_sha256);
            c.field("skill_level",v.skill_level);
            c.field("auxiliary_attribute",v.auxiliary_attribute);
            strings(c,"field_order",v.field_order);
            strings(c,"snapshot_order",v.snapshot_order);
            c.optional("context",v.context,context);
            return c.status();
        }
// GrowthChallengeInstance的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge(CodecVisitor& c,GrowthChallengeInstance& v){c.field("seed",v.seed);
            c.field("actor_id",v.actor_id);
            c.field("source_id",v.source_id);
            c.field("phase",v.phase);
            c.field("consumed",v.consumed);
            strings(c,"field_order",v.field_order);
            strings(c,"state_order",v.state_order);
            strings(c,"binding_order",v.binding_order);
            c.object("seed_origin",[&](auto& x){x.field("seed",v.seed_origin.seed);
                x.field("state",v.seed_origin.state);
                return strings(x,"field_order",v.seed_origin.field_order);
            });
            c.object("frozen",[&](auto& x){return frozen(x,v.frozen);
            });
            c.sequence("choices",v.choices,choice);
            c.optional("result",v.result,result);
            return c.status();
        }
// GrowthCommand的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status command(CodecVisitor& c,GrowthCommand& v){c.field("kind",v.kind);
            c.field("skill_id",v.skill_id);
            c.field("item_id",v.item_id);
            c.field("weapon_id",v.weapon_id);
            c.field("armor_id",v.armor_id);
            c.field("activity",v.activity);
            c.field("shop_id",v.shop_id);
            c.field("side",v.side);
            c.field("payer_wallet_id",v.payer_wallet_id);
            c.field("action_id",v.action_id);
            c.field("step_id",v.step_id);
            c.field("option_id",v.option_id);
            c.field("estate_id",v.estate_id);
            c.field("challenge_mode",v.challenge_mode);
            c.field("ticks",v.ticks);
            c.field("quantity",v.quantity);
            c.field("duration",v.duration);
            strings(c,"field_order",v.field_order);
            strings(c,"trade_field_order",v.trade_field_order);
            c.optional("quoted_total",v.quoted_total,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("market_revision",v.market_revision,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("context",v.context,interaction_codec::visit_context);
            c.optional("bargain_quote_id",v.bargain_quote_id,[](auto& c,auto& x){return c.field("value",x);
            });
            return c.status();
        }
// TrainingTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status training(CodecVisitor& c,TrainingTerms& v){c.field("skill_id",v.skill_id);
            c.field("student_id",v.student_id);
            c.field("teacher_id",v.teacher_id);
            c.field("place_id",v.place_id);
            c.field("payer_wallet_id",v.payer_wallet_id);
            c.field("recipient_wallet_id",v.recipient_wallet_id);
            c.field("duration",v.duration);
            c.field("mastery_before",v.mastery_before);
            c.field("mastery_after",v.mastery_after);
            c.field("student_fatigue",v.student_fatigue);
            c.field("teacher_fatigue",v.teacher_fatigue);
            c.field("fee",v.fee);
            strings(c,"field_order",v.field_order);
            if(c.schema_features&common_checkpoint::teaching_request_sources)c.optional("teaching_agreement",v.teaching_agreement,visit_growth_teaching_agreement);
            else if(c.reading())v.teaching_agreement.reset();
            else if(v.teaching_agreement)return c.reject("TeachingRequestsCapabilityRequired");
            return c.status();
        }
// StudyTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status study(CodecVisitor& c,StudyTerms& v){c.field("skill_id",v.skill_id);
            c.field("mode",v.mode);
            c.field("student_id",v.student_id);
            c.field("teacher_id",v.teacher_id);
            c.field("payer_wallet_id",v.payer_wallet_id);
            c.field("recipient_wallet_id",v.recipient_wallet_id);
            c.field("duration",v.duration);
            c.field("xp_before",v.xp_before);
            c.field("teacher_xp",v.teacher_xp);
            c.field("student_fatigue",v.student_fatigue);
            c.field("teacher_fatigue",v.teacher_fatigue);
            c.field("xp_limit",v.xp_limit);
            c.field("fee",v.fee);
            strings(c,"field_order",v.field_order);
            c.object("context",[&](auto& x){return interaction_codec::visit_context(x,v.context);
            });
            c.optional("challenge",v.challenge,frozen);
            const bool segmented=std::find(v.field_order.begin(),v.field_order.end(),"settlement_policy")!=v.field_order.end();
            if(c.reading()){if(segmented)v.settlement_policy.emplace();else v.settlement_policy.reset();}
            if(bool(v.settlement_policy)!=segmented)return c.reject("InvalidStudySettlementPolicy");
            if(v.settlement_policy)c.field("settlement_policy",*v.settlement_policy);
            if(c.schema_features&common_checkpoint::study_teaching_request_sources)c.optional("study_teaching_agreement",v.study_teaching_agreement,visit_growth_study_teaching_agreement);
            else if(c.reading())v.study_teaching_agreement.reset();
            else if(v.study_teaching_agreement)return c.reject("StudyTeachingCapabilityRequired");
            if(c.schema_features&innate_study_checkpoint_feature)c.optional("innate_source",v.innate_source,visit_innate_study_source);
            else{if(v.innate_source||(v.settlement_policy&&*v.settlement_policy=="innate_per_lesson_v1"))return c.reject("InnateStudyCapabilityRequired");if(c.reading())v.innate_source.reset();}
            if(bool(v.innate_source)!=(v.settlement_policy&&*v.settlement_policy=="innate_per_lesson_v1"))return c.reject("InnateStudyPolicySourceMismatch");
            return c.status();
        }
// ServiceTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status service(CodecVisitor& c,ServiceTerms& v){c.field("duration",v.duration);
            c.field("cost",v.cost);
            c.field("wage",v.wage);
            c.field("hp_gain",v.hp_gain);
            c.field("fatigue_delta",v.fatigue_delta);
            c.field("wallet_id",v.wallet_id);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// TradeTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status trade(CodecVisitor& c,TradeTerms& v){c.field("shop_id",v.shop_id);
            c.field("item_id",v.item_id);
            c.field("side",v.side);
            c.field("payer_wallet_id",v.payer_wallet_id);
            c.field("quantity",v.quantity);
            c.field("total",v.total);
            c.field("market_revision",v.market_revision);
            c.field("valid_tick",v.valid_tick);
            strings(c,"field_order",v.field_order);
            c.optional("bargain_quote_id",v.bargain_quote_id,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("bargain_basis_sha256",v.bargain_basis_sha256,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("discount_bp",v.discount_bp,[](auto& c,auto& x){return c.field("value",x);
            });
            c.object("owner",[&](auto& x){return owner(x,v.owner);
            });
            c.object("context",[&](auto& x){return interaction_codec::visit_context(x,v.context);
            });
            return c.status();
        }
// EquipmentChoice的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status equipment(CodecVisitor& c,EquipmentChoice& v){c.field("id",v.id);
            c.field("defense",v.defense);
            return c.status();
        }
// StudyCancellation的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status cancellation(CodecVisitor& c,StudyCancellation& v){c.field("action_id",v.action_id);
            c.field("completed_lessons",v.completed_lessons);
            c.field("fee_paid",v.fee_paid);
            c.field("unprovided_released",v.unprovided_released);
            return c.status();
        }
// GrowthChallengeCondition的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge_condition(CodecVisitor& c,GrowthChallengeCondition& v){c.field("id",v.id);
            c.field("kind",v.kind);
            c.field("text_key",v.text_key);
            c.field("matched",v.matched);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// GrowthChallengeRequirement的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge_requirement(CodecVisitor& c,GrowthChallengeRequirement& v){c.field("node_id",v.node_id);
            c.field("option_id",v.option_id);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// GrowthChallengeOption的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge_option(CodecVisitor& c,GrowthChallengeOption& v){c.field("id",v.id);
            c.field("text_key",v.text_key);
            c.field("kind",v.kind);
            c.field("method",v.method);
            c.field("points",v.points);
            c.sequence("requires_choices",v.requires_choices,challenge_requirement);
            c.optional("supply_condition_id",v.supply_condition_id,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("terms_condition_id",v.terms_condition_id,[](auto& c,auto& x){return c.field("value",x);
            });
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// GrowthChallengeNode的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge_node(CodecVisitor& c,GrowthChallengeNode& v){c.field("id",v.id);
            c.field("topic_key",v.topic_key);
            c.field("stance_key",v.stance_key);
            c.field("prompt_key",v.prompt_key);
            c.sequence("options",v.options,challenge_option);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// GrowthChallengeDefinition的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status challenge_definition(CodecVisitor& c,GrowthChallengeDefinition& v){c.field("id",v.id);
            c.field("kind",v.kind);
            c.field("source_sha256",v.source_sha256);
            c.field("difficulty",v.difficulty);
            c.sequence("conditions",v.conditions,challenge_condition);
            c.sequence("nodes",v.nodes,challenge_node);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// BargainMarketBasis的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status market_basis(CodecVisitor& c,BargainMarketBasis& v){c.field("base_price",v.base_price);
            c.field("stock",v.stock);
            c.field("elasticity_bp",v.elasticity_bp);
            c.field("target_stock",v.target_stock);
            c.field("event_bp",v.event_bp);
            c.field("available_quantity",v.available_quantity);
            c.field("shop_wallet_id",v.shop_wallet_id);
            c.field("shop_inventory_id",v.shop_inventory_id);
            c.field("actor_inventory_id",v.actor_inventory_id);
            strings(c,"field_order",v.field_order);
            return c.status();
        }
// BargainTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status bargain_terms(CodecVisitor& c,BargainTerms& v){c.field("source_sha256",v.source_sha256);
            c.field("challenge_id",v.challenge_id);
            c.field("skill_level",v.skill_level);
            c.field("auxiliary_attribute",v.auxiliary_attribute);
            strings(c,"field_order",v.field_order);
            strings(c,"snapshot_order",v.snapshot_order);
            c.object("request",[&](auto& c){return command(c,v.request);
            });
            c.object("base_quote",[&](auto& c){return trade(c,v.base_quote);
            });
            c.object("market_basis",[&](auto& c){return market_basis(c,v.market_basis);
            });
            c.object("quote_context",[&](auto& c){return context(c,v.quote_context);
            });
            return c.status();
        }
// BargainPrepared的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status bargain_prepared(CodecVisitor& c,BargainPrepared& v){c.field("mode",v.mode);
            c.field("quote_id",v.quote_id);
            c.field("basis_sha256",v.basis_sha256);
            c.object("terms",[&](auto& c){return bargain_terms(c,v.terms);
            });
            c.object("definition",[&](auto& c){return challenge_definition(c,v.definition);
            });
            c.optional("before",v.before,challenge);
            c.optional("next",v.next,challenge);
            return c.status();
        }
// BargainFact的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status bargain_fact(CodecVisitor& c,BargainFact& v){c.field("actor_id",v.actor_id);
            c.field("basis_sha256",v.basis_sha256);
            c.field("consumed_action_id",v.consumed_action_id);
            strings(c,"field_order",v.field_order);
            c.object("terms",[&](auto& c){return bargain_terms(c,v.terms);
            });
            c.object("instance",[&](auto& c){return challenge(c,v.instance);
            });
            c.optional("receipt",v.receipt,trade);
            return c.status();
        }
// RestTerms的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status rest_terms(CodecVisitor& c,RestTerms& v){c.field("estate_id",v.estate_id);
            c.field("food_item_id",v.food_item_id);
            c.field("duration",v.duration);
            c.field("ration_per_person",v.ration_per_person);
            strings(c,"mouths",v.mouths);
            return c.status();
        }
// 暂停控制报价只保存占用结果，不复用取消的费用字段。
        Status pause_change(CodecVisitor& c,StudyPauseChange& v){c.field("action_id",v.action_id);
            c.field("paused",v.paused);
            c.field("paused_ticks",v.paused_ticks);
            c.field("completed_lessons",v.completed_lessons);
            c.field("reserved",v.reserved);
            return c.status();
        }
// RestCancellation的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status rest_cancel(CodecVisitor& c,RestCancellation& v){c.field("action_id",v.action_id);
            return c.status();
        }
// RestFact的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status rest_fact(CodecVisitor& c,RestFact& v,bool exact_periods,bool exact_rest_mouths){c.field("actor_id",v.actor_id);
            c.field("estate_id",v.estate_id);
            c.field("phase",v.phase);
            c.field("food_item_id",v.food_item_id);
            c.field("stop_reason",v.stop_reason);
            c.field("start_tick",v.start_tick);
            c.field("duration",v.duration);
            c.field("elapsed",v.elapsed);
            c.field("completed_tick",v.completed_tick);
            c.field("ration_per_person",v.ration_per_person);
            c.field("consumed",v.consumed);
            strings(c,"last_mouths",v.last_mouths);
            strings(c,"field_order",v.field_order);
            if(exact_periods)c.sequence("actual_periods",v.actual_periods,[&](auto& x,auto& period){return visit_long_rest_actual_period(x,period,exact_rest_mouths);});
            else if(c.reading())v.actual_periods.clear();else if(!v.actual_periods.empty())return c.reject("GrowthExactPeriodsPhysicalSelectionRequired");
            return c.status();
        }
// GrowthQuote的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status quote(CodecVisitor& c,GrowthQuote& v){c.field("duration",v.duration);
            c.field("cost",v.cost);
            c.field("wage",v.wage);
            c.field("expected_xp",v.expected_xp);
            c.field("target_level",v.target_level);
            c.object("command",[&](auto& x){return command(x,v.command);
            });
            c.optional("context",v.context,interaction_codec::visit_context);
            c.optional("challenge_before",v.challenge_before,challenge);
            Integer tag=v.data.index();
            c.field("data_kind",tag);
            if(tag<0||tag>10)return c.reject("InvalidGrowthQuote");
            if(c.reading()){switch(tag){case 0:v.data=EquipmentChoice{};
                    break;
                    case 1:v.data=TrainingTerms{};
                    break;
                    case 2:v.data=StudyTerms{};
                    break;
                    case 3:v.data=ServiceTerms{};
                    break;
                    case 4:v.data=TradeTerms{};
                    break;
                    case 5:v.data=StudyCancellation{};
                    break;
                    case 6:v.data=GrowthChallengeInstance{};
                    break;
                    case 7:v.data=RestTerms{};
                    break;
                    case 8:v.data=RestCancellation{};
                    break;
                    case 9:v.data=BargainPrepared{};
                    break;
                    case 10:v.data=StudyPauseChange{};
                    break;
            }}auto status=std::visit([&](auto& value){using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,EquipmentChoice>)return equipment(c,value);
                else if constexpr(std::is_same_v<T,TrainingTerms>)return training(c,value);
                else if constexpr(std::is_same_v<T,StudyTerms>)return study(c,value);
                else if constexpr(std::is_same_v<T,ServiceTerms>)return service(c,value);
                else if constexpr(std::is_same_v<T,TradeTerms>)return trade(c,value);
                else if constexpr(std::is_same_v<T,StudyCancellation>)return cancellation(c,value);
                else if constexpr(std::is_same_v<T,StudyPauseChange>)return pause_change(c,value);
                else if constexpr(std::is_same_v<T,RestTerms>)return rest_terms(c,value);
                else if constexpr(std::is_same_v<T,RestCancellation>)return rest_cancel(c,value);
                else if constexpr(std::is_same_v<T,BargainPrepared>)return bargain_prepared(c,value);
                else return challenge(c,value);
            },v.data);
            if(!status.ok)return status;
            return c.status();
        }
// GrowthAction的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status action(CodecVisitor& c,GrowthAction& v){strings(c,"field_order",v.field_order);
            c.object("request",[&](auto& x){return command(x,v.request);
            });
            c.object("quote",[&](auto& x){return quote(x,v.quote);
            });
            return c.status();
        }
// TrainingFact的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status training_fact(CodecVisitor& c,TrainingFact& v,bool capability_chain,GrowthContributionWireVersion wire){c.field("phase",v.phase);
            c.field("action_kind",v.action_kind);
            c.field("actor_id",v.actor_id);
            c.field("start_tick",v.start_tick);
            c.field("completed_tick",v.completed_tick);
            c.field("success",v.success);
            strings(c,"field_order",v.field_order);
            c.object("training",[&](auto& x){return training(x,v.training);
            });
            c.optional("context",v.context,interaction_codec::visit_context);
            if(capability_chain){c.optional("capability_application",v.capability_application,[&](auto& x,auto& a){return growth_capability_codec::application(x,a,wire);});if(c.status().ok&&bool(v.capability_application)!=(v.phase=="completed"&&v.success&&v.training.mastery_after!=v.training.mastery_before))return c.reject("TrainingCapabilityApplicationMismatch");}else if(v.capability_application)return c.reject("GrowthCapabilityPhysicalSelectionRequired");
            return c.status();
        }
// 教学与迟答收据分开检查点化；每个课时最多一个差额，读档不执行成长。
        Status study_adjustment(CodecVisitor& c,StudyPerformanceAdjustment& v){if(!c.reading()&&v.exact_binding)return c.reject("StudyAdjustmentTime2PhysicalSelectionRequired");if(c.reading())v.exact_binding.reset();c.field("settled_tick",v.settled_tick);c.field("xp_before",v.xp_before);c.field("performance_bp",v.performance_bp);c.field("xp_gained",v.xp_gained);strings(c,"field_order",v.field_order);return c.status();}
// 新政策独有的课时证据保留完整字段与顺序，旧课时没有此字段。
        Status study_segment(CodecVisitor& c,StudySegment& v,bool exact_periods,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){c.field("ordinal",v.ordinal);c.field("settled_tick",v.settled_tick);c.field("xp_before",v.xp_before);c.field("performance_bp",v.performance_bp);c.field("xp_gained",v.xp_gained);c.field("performance_pending",v.performance_pending);c.optional("adjustment",v.adjustment,[&](auto& x,auto& a){return exact_periods?visit_study_adjustment_time2(x,a,exact_command_cuts,capability_chain,wire):study_adjustment(x,a);});strings(c,"field_order",v.field_order);if(capability_chain){c.optional("capability_application",v.capability_application,[&](auto& x,auto& a){return growth_capability_codec::application(x,a,wire);});if(c.status().ok&&bool(v.capability_application)!=(v.xp_gained>0))return c.reject("StudyLessonCapabilityApplicationMismatch");}else if(v.capability_application)return c.reject("GrowthCapabilityPhysicalSelectionRequired");return c.status();}
// StudyLesson的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status lesson(CodecVisitor& c,StudyLesson& v,bool exact_periods,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){c.field("fatigue_before",v.fatigue_before);
            c.field("injury",v.injury);
            c.field("xp",v.xp);
            strings(c,"field_order",v.field_order);
            const bool segmented=std::find(v.field_order.begin(),v.field_order.end(),"segment")!=v.field_order.end();
            if(c.reading()){if(segmented)v.segment.emplace();else v.segment.reset();}
            if(bool(v.segment)!=segmented)return c.reject("InvalidStudySegment");
            if(v.segment)c.object("segment",[&](auto& x){return study_segment(x,*v.segment,exact_periods,exact_command_cuts,capability_chain,wire);});
            return c.status();
        }
// 暂停段逐字段保存冷边界顺序，哨兵合法性留给事实验证器。
        Status study_pause(CodecVisitor& c,StudyPause& v){c.field("start_tick",v.start_tick);
            c.field("end_tick",v.end_tick);
            c.field("after_lessons",v.after_lessons);
            c.field("student_fatigue",v.student_fatigue);
            c.field("teacher_fatigue",v.teacher_fatigue);
            c.field("student_xp",v.student_xp);
            strings(c,"field_order",v.field_order);
            const bool credited=std::find(v.field_order.begin(),v.field_order.end(),"credited_xp")!=v.field_order.end();
            if(c.reading()){if(credited)v.credited_xp.emplace();else v.credited_xp.reset();}
            if(bool(v.credited_xp)!=credited)return c.reject("InvalidStudyPause");
            if(v.credited_xp)c.field("credited_xp",*v.credited_xp);
            return c.status();
        }
// StudyFact的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status study_fact(CodecVisitor& c,StudyFact& v,bool complete,bool exact_periods,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){c.field("phase",v.phase);
            c.field("actor_id",v.actor_id);
            c.field("code",v.code);
            c.field("start_tick",v.start_tick);
            c.field("completed_tick",v.completed_tick);
            c.field("success",v.success);
            strings(c,"field_order",v.field_order);
            c.object("terms",[&](auto& x){return study(x,v.terms);
            });
            c.sequence("lessons",v.lessons,[&](auto& x,auto& l){return lesson(x,l,exact_periods,exact_command_cuts,capability_chain,wire);});
            c.optional("challenge",v.challenge,challenge);
            // 访客optional要求字段头；既有字段序显式区分未暂停事实，版本迁移由中央决定。
            const bool has_pauses=std::find(v.field_order.begin(),v.field_order.end(),"pauses")!=v.field_order.end();
            if(c.reading()){if(has_pauses)v.pauses.emplace();else v.pauses.reset();}
            if(bool(v.pauses)!=has_pauses)return c.reject("InvalidStudyPause");
            if(v.pauses)c.sequence("pauses",*v.pauses,study_pause);
            const bool has_settlement=std::find(v.field_order.begin(),v.field_order.end(),"xp_settlement_before")!=v.field_order.end();
            if(c.reading()){if(has_settlement)v.xp_settlement_before.emplace();else v.xp_settlement_before.reset();}
            if(bool(v.xp_settlement_before)!=has_settlement)return c.reject("InvalidStudyGrowth");
            if(v.xp_settlement_before)c.field("xp_settlement_before",*v.xp_settlement_before);
            if(complete){c.optional("study_service_binding",v.study_service_binding,visit_study_service_binding_v2);c.optional("study_service_terminal",v.study_service_terminal,visit_study_service_terminal_v2);}
            else if(v.study_service_binding||v.study_service_terminal)return c.reject("CompleteStudyServiceCodecRequired");
            return c.status();
        }
// GrowthPublicOption的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status public_option(CodecVisitor& c,GrowthPublicOption& v){c.field("id",v.id);
            c.field("text",v.text);
            c.field("kind",v.kind);
            c.field("method",v.method);
            c.optional("supply_condition_id",v.supply_condition_id,[](auto& c,auto& x){return c.field("value",x);
            });
            c.optional("terms_condition_id",v.terms_condition_id,[](auto& c,auto& x){return c.field("value",x);
            });
            return c.status();
        }
// GrowthPublicCondition的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status public_condition(CodecVisitor& c,GrowthPublicCondition& v){c.field("id",v.id);
            c.field("kind",v.kind);
            c.field("text",v.text);
            c.field("matched",v.matched);
            return c.status();
        }
// GrowthPublicChallenge的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status public_view(CodecVisitor& c,GrowthPublicChallenge& v){c.field("kind",v.kind);
            c.field("phase",v.phase);
            c.field("step_id",v.step_id);
            c.field("topic",v.topic);
            c.field("stance",v.stance);
            c.field("prompt",v.prompt);
            c.field("round",v.round);
            c.field("budget",v.budget);
            c.field("quantity",v.quantity);
            c.sequence("options",v.options,public_option);
            c.sequence("conditions",v.conditions,public_condition);
            c.optional("result",v.result,result);
            return c.status();
        }
// GrowthEffect的完整持久字段；每项读取错误沿visitor短路状态保留。
        Status effect(CodecVisitor& c,GrowthEffect& v){c.field("kind",v.kind);
            c.field("code",v.code);
            c.field("skill_id",v.skill_id);
            c.field("action_id",v.action_id);
            c.field("quote_id",v.quote_id);
            c.field("success",v.success);
            c.field("already_completed",v.already_completed);
            c.field("reopened",v.reopened);
            c.field("mastery",v.mastery);
            c.field("xp_gained",v.xp_gained);
            c.field("fee_paid",v.fee_paid);
            c.field("unprovided_released",v.unprovided_released);
            c.field("completed_lessons",v.completed_lessons);
            c.field("elapsed",v.elapsed);
            strings(c,"field_order",v.field_order);
            c.optional("challenge",v.challenge,challenge);
            c.optional("public_challenge",v.public_challenge,public_view);
            // 只有新增即时控制回执附加字段，旧课程完成回执的检查点布局不变。
            if(v.kind=="study_pause"||v.kind=="study_resume"){c.field("paused",v.paused);
                c.field("paused_ticks",v.paused_ticks);
                c.field("reserved",v.reserved);
            }
            return c.status();
        }
    }
    static Status fixed_unsupported(const char* code){RuleError e;e.original={code,"growth_fixed_state"};e.kind=ErrorKind::unsupported_capability;return {false,{},std::move(e)};}
    static Status visit_growth_checkpoint_impl(CodecVisitor& c,GrowthState& s,bool regional_life,bool cards,bool fixed_service,bool complete=false,bool exact_periods=false,bool exact_command_cuts=false,bool exact_rest_mouths=false,bool capability_chain=false,GrowthContributionWireVersion wire=GrowthContributionWireVersion::original){
        if(wire!=GrowthContributionWireVersion::original&&wire!=GrowthContributionWireVersion::trade_grants)return c.reject("GrowthContributionWireVersionUnsupported");
        if(wire==GrowthContributionWireVersion::trade_grants&&!capability_chain)return c.reject("GrowthTradeGrantPhysicalSelectionRequired");
        if(fixed_service&&!complete&&(!regional_life||cards||(c.schema_features&(common_checkpoint::teaching_request_sources|common_checkpoint::study_teaching_request_sources|innate_study_checkpoint_feature))))return fixed_unsupported("UnsupportedGrowthFixedSchema");
        if(!c.reading()){
            if(!fixed_service&&!s.fixed_service_boundaries.rows().empty())return fixed_unsupported("GrowthFixedServiceCapabilityRequired");
            for(const auto& row:s.life_activities.rows()){
                if(row.second.selected_trade_settlement)return fixed_unsupported("UnsupportedGrowthFixedTradeSource");
                auto shape=growth_trade_terminal_codec::validate_activity(row.second,fixed_service&&complete);
                if(!shape.ok)return shape;
            }
        }
        if(c.reading()&&!fixed_service)s.fixed_service_boundaries=OrderedTable<LifeFixedServiceBoundary>{};
        if(!c.reading()&&cards!=s.cards.has_value())return c.reject(cards?"MissingGrowthCards":"CardsCapabilityRequired","growth_cards");
        if(c.reading()){if(cards)s.cards.emplace();else s.cards.reset();}
        common_checkpoint::table(c,"actions",s.actions,action);
        common_checkpoint::table(c,"facts",s.facts,[&](CodecVisitor& v,GrowthFact& f){Integer tag=f.index();
            v.field("kind",tag);
            if(tag<0||tag>3)return v.reject("InvalidGrowthFact");
            if(v.reading()){if(tag==0)f=TrainingFact{};
                else if(tag==1)f=StudyFact{};
                else if(tag==2)f=RestFact{};
                else f=BargainFact{};
            }return std::visit([&](auto& value){using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,TrainingFact>)return training_fact(v,value,capability_chain,wire);
                else if constexpr(std::is_same_v<T,StudyFact>)return study_fact(v,value,complete,exact_periods,exact_command_cuts,capability_chain,wire);
                else if constexpr(std::is_same_v<T,RestFact>)return rest_fact(v,value,exact_periods,exact_rest_mouths);
                else return bargain_fact(v,value);
            },f);
        });
        if(regional_life)common_checkpoint::table(c,"life_activities",s.life_activities,[&](CodecVisitor& v,LifeActivityRecord& record){
            v.object("view",[&](auto& child){return visit_life_activity_view(child,record.view);});
            v.object("context",[&](auto& child){return interaction_codec::visit_context(child,record.context);});
            Integer terms_tag=static_cast<Integer>(record.terms.index());
            if(fixed_service){v.field("terms_kind",terms_tag);if(!v.status().ok)return v.status();if(terms_tag<0||terms_tag>2)return v.reject("InvalidLifeActivityTermsKind");if(terms_tag==2||(!complete&&terms_tag!=0))return fixed_unsupported("UnsupportedGrowthFixedTerms");if(!complete&&(record.view.kind!=LifeActivityKind::service||record.view.activity!="rest"))return fixed_unsupported("UnsupportedGrowthFixedActivity");}
            if(v.reading())record.terms=record.view.kind==LifeActivityKind::service?std::variant<ServiceTerms,TradeTerms>{ServiceTerms{}}:std::variant<ServiceTerms,TradeTerms>{TradeTerms{}};
            v.object("terms",[&](auto& child){if(auto* terms=std::get_if<ServiceTerms>(&record.terms))return service(child,*terms);return trade(child,std::get<TradeTerms>(record.terms));});
            if(fixed_service&&terms_tag!=static_cast<Integer>(record.terms.index()))return v.reject("LifeActivityTermsKindMismatch");
            v.field("cost_paid",record.cost_paid);v.field("wage_paid",record.wage_paid);strings(v,"field_order",record.field_order);strings(v,"view_field_order",record.view_field_order);
            if(fixed_service){
                auto unsupported=[](auto&,auto&){return fixed_unsupported("UnsupportedGrowthFixedTradeSource");};
                v.optional("selected_trade_settlement",record.selected_trade_settlement,unsupported);
                v.optional("trade_terminal_time",record.trade_terminal_time,growth_trade_terminal_codec::visit);
            }else if(v.reading()){record.selected_trade_settlement.reset();record.trade_terminal_time.reset();}
            if(!v.status().ok)return v.status();
            return growth_trade_terminal_codec::validate_activity(record,fixed_service&&complete);
        });
        else if(!s.life_activities.rows().empty())return c.reject("RegionalLifeUnavailable");
        // 显式新module才增加独立对象，旧编码没有optional存在位。
        if(cards)c.object("growth_cards",[&](auto& visitor){return visit_growth_cards_checkpoint(visitor,*s.cards);});
        if(fixed_service){common_checkpoint::table(c,"fixed_service_boundaries",s.fixed_service_boundaries,[&](auto& x,auto& value){return complete?visit_growth_complete_fixed_service_boundary(x,value,exact_periods,wire):visit_growth_fixed_service_boundary(x,value);});if(c.status().ok)for(const auto& row:s.fixed_service_boundaries.rows())if(row.first!=row.second.boundary_id)return c.reject("GrowthFixedBoundaryKeyMismatch");}
        return c.status();
    }
    Status visit_growth_checkpoint(CodecVisitor& c,GrowthState& s,bool regional_life,bool cards){return visit_growth_checkpoint_impl(c,s,regional_life,cards,false);}
    Status visit_growth_fixed_service_checkpoint(CodecVisitor& c,GrowthState& s,bool regional_life,bool cards){try{
        if(!c.reading())return visit_growth_checkpoint_impl(c,s,regional_life,cards,true);
        GrowthState candidate;auto status=visit_growth_checkpoint_impl(c,candidate,regional_life,cards,true);
        if(status.ok){static_assert(std::is_nothrow_move_assignable_v<GrowthState>);s=std::move(candidate);}return status;
    }catch(const std::bad_alloc&){RuleError e;e.original={"OutOfMemory","growth_fixed_state"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"SizeExhausted","growth_fixed_state"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
    Status visit_growth_complete_fixed_checkpoint(CodecVisitor& c,GrowthState& state,const GrowthCatalog& catalog){
        if(!catalog.complete_study_sources)return c.reject("CompleteGrowthCatalogRequired");
        if(bool(c.schema_features&common_checkpoint::teaching_request_sources)!=catalog.teaching_requests.enabled||bool(c.schema_features&common_checkpoint::study_teaching_request_sources)!=catalog.study_teaching_requests.enabled||bool(c.schema_features&innate_study_checkpoint_feature)!=catalog.innate_learning)return c.reject("GrowthCatalogCheckpointFeaturesMismatch");
        if(!c.reading())return visit_growth_checkpoint_impl(c,state,catalog.regional_life,catalog.cards.enabled,true,true);
        GrowthState candidate;auto status=visit_growth_checkpoint_impl(c,candidate,catalog.regional_life,catalog.cards.enabled,true,true);if(status.ok){static_assert(std::is_nothrow_move_assignable_v<GrowthState>);state=std::move(candidate);}return status;
    }
    Status visit_growth_exact_period_checkpoint(CodecVisitor& c,GrowthState& state,const GrowthCatalog& catalog){
        if(!catalog.complete_study_sources||!catalog.complete_exact_period_sources)return c.reject("GrowthExactPeriodsCatalogRequired");
        if(bool(c.schema_features&common_checkpoint::teaching_request_sources)!=catalog.teaching_requests.enabled||bool(c.schema_features&common_checkpoint::study_teaching_request_sources)!=catalog.study_teaching_requests.enabled||bool(c.schema_features&innate_study_checkpoint_feature)!=catalog.innate_learning)return c.reject("GrowthCatalogCheckpointFeaturesMismatch");
        if(!c.reading())return visit_growth_checkpoint_impl(c,state,catalog.regional_life,catalog.cards.enabled,true,true,true);
        GrowthState candidate;auto status=visit_growth_checkpoint_impl(c,candidate,catalog.regional_life,catalog.cards.enabled,true,true,true);if(status.ok){static_assert(std::is_nothrow_move_assignable_v<GrowthState>);state=std::move(candidate);}return status;
    }
    Status visit_growth_exact_command_cut_checkpoint(CodecVisitor& c,GrowthState& state,const GrowthCatalog& catalog){
        if(!catalog.complete_study_sources||!catalog.complete_exact_period_sources||!catalog.complete_exact_command_cuts)return c.reject("GrowthExactCommandCutsCatalogRequired");
        if(bool(c.schema_features&common_checkpoint::teaching_request_sources)!=catalog.teaching_requests.enabled||bool(c.schema_features&common_checkpoint::study_teaching_request_sources)!=catalog.study_teaching_requests.enabled||bool(c.schema_features&innate_study_checkpoint_feature)!=catalog.innate_learning)return c.reject("GrowthCatalogCheckpointFeaturesMismatch");
        if(!c.reading())return visit_growth_checkpoint_impl(c,state,catalog.regional_life,catalog.cards.enabled,true,true,true,true);
        GrowthState candidate;auto status=visit_growth_checkpoint_impl(c,candidate,catalog.regional_life,catalog.cards.enabled,true,true,true,true);if(status.ok){static_assert(std::is_nothrow_move_assignable_v<GrowthState>);state=std::move(candidate);}return status;
    }
    Status visit_growth_exact_rest_mouth_checkpoint(CodecVisitor& c,GrowthState& state,const GrowthCatalog& catalog){
        if(!catalog.complete_study_sources||!catalog.complete_exact_period_sources||!catalog.complete_exact_command_cuts||!catalog.complete_exact_rest_mouth_sources)return c.reject("GrowthExactRestMouthCatalogRequired");
        if(bool(c.schema_features&common_checkpoint::teaching_request_sources)!=catalog.teaching_requests.enabled||bool(c.schema_features&common_checkpoint::study_teaching_request_sources)!=catalog.study_teaching_requests.enabled||bool(c.schema_features&innate_study_checkpoint_feature)!=catalog.innate_learning)return c.reject("GrowthCatalogCheckpointFeaturesMismatch");
        if(!c.reading())return visit_growth_checkpoint_impl(c,state,catalog.regional_life,catalog.cards.enabled,true,true,true,true,true);
        GrowthState candidate;auto status=visit_growth_checkpoint_impl(c,candidate,catalog.regional_life,catalog.cards.enabled,true,true,true,true,true);if(status.ok){static_assert(std::is_nothrow_move_assignable_v<GrowthState>);state=std::move(candidate);}return status;
    }
    Status visit_growth_effect(CodecVisitor& c,GrowthEffect& e){return effect(c,e);
    }
    namespace growth {
        Status visit_challenge(CodecVisitor& c,GrowthChallengeInstance& v){return challenge(c,v);
        }
        Status visit_frozen(CodecVisitor& c,ChallengeFrozen& v){return frozen(c,v);
        }
    }
} // 新域section由中央组合者登记，既有旧存档迁移不在此处暗改。

namespace hanmo::resident {
Status visit_growth_capability_chain_checkpoint(CodecVisitor& c,GrowthState& value,const GrowthCatalog& catalog){if(!catalog.complete_study_sources||!catalog.complete_exact_period_sources||!catalog.complete_exact_command_cuts||!catalog.complete_exact_rest_mouth_sources||!catalog.complete_capability_application_chain)return c.reject("GrowthCapabilityChainCatalogRequired");return visit_growth_checkpoint_impl(c,value,catalog.regional_life,catalog.cards.enabled,true,true,true,true,true,true);}
}

// 显式 base+192：原 phase4 字段表，仅有限来源格式后继。
namespace hanmo::resident {
Status visit_growth_trade_grant_checkpoint(CodecVisitor& c,GrowthState& value,const GrowthCatalog& catalog){if(!catalog.complete_study_sources||!catalog.complete_exact_period_sources||!catalog.complete_exact_command_cuts||!catalog.complete_exact_rest_mouth_sources||!catalog.complete_capability_application_chain)return c.reject("GrowthCapabilityChainCatalogRequired");return visit_growth_checkpoint_impl(c,value,catalog.regional_life,catalog.cards.enabled,true,true,true,true,true,true,GrowthContributionWireVersion::trade_grants);}
}
