#include "growth_state_projection.hpp"
// 课程礼议/议价沿原三轮纯规则；seed只在开课从共同minigame流抽取一次，奖励由课程owner结算。
#include "growth_internal.hpp"
#include "growth_effect_projection.hpp"
#include "growth_equal.hpp"
#include "resident/model/common_rng.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::growth {
    namespace {
        const GrowthChallengeDefinition* definition(const GrowthCatalog& catalog,const ChallengeFrozen& frozen){return (frozen.kind=="etiquette"?catalog.etiquette:catalog.bargain).find(frozen.challenge_id);
        }
        Integer automatic(const GrowthChallengeDefinition& d,const ChallengeFrozen& f){return std::clamp<Integer>(45+8*f.skill_level+f.auxiliary_attribute/10-8*d.difficulty,30,90);
        }
        Integer points(const GrowthChallengeDefinition& d,const GrowthChallengeOption& option,const std::vector<GrowthChoice>& choices){if(d.kind=="etiquette"){for(const auto& required:option.requires_choices){bool found=false;
                    for(const auto& choice:choices)if(choice.step_id==required.node_id&&choice.option_id==required.option_id)found=true;
                    if(!found)return 0;
                }return option.points;
            }Integer value=0;
            for(const auto& condition:d.conditions){if(option.supply_condition_id&&condition.id==*option.supply_condition_id&&condition.matched)value+=20;
                if(option.terms_condition_id&&condition.id==*option.terms_condition_id&&condition.matched)value+=10;
            }return value;
        }
        GrowthChallengeResult result(const GrowthChallengeDefinition& d,const GrowthChallengeInstance& instance,const StableId& completion){GrowthChallengeResult r;
            r.completion=completion;
            r.auto_score=automatic(d,instance.frozen);
            r.score=r.auto_score;
            r.choices=instance.choices;
            if(completion=="manual"){Integer raw=d.kind=="etiquette"?25:20;
                for(const auto& choice:instance.choices)raw+=choice.points;
                if(d.kind=="bargain")raw=std::min<Integer>(100,raw);
                r.raw_score=raw;
                r.score=std::max(raw,r.auto_score-10);
            }r.performance_bp=std::min<Integer>(12000,10000+std::max<Integer>(0,r.score-r.auto_score)*200);
            auto value=r.score-60;
            auto quotient=value/8;
            if(value<0&&value%8)--quotient;
            r.request_delta=std::clamp<Integer>(quotient,-5,5);
            if(instance.frozen.context)r.relation_discount_bp=instance.frozen.context->relation_discount_bp;
            r.challenge_discount_bp=r.score*5;
            r.discount_bp=std::min<Integer>(800,r.relation_discount_bp+r.challenge_discount_bp);
            return r;
        }
        bool choices_equal(const std::vector<GrowthChoice>& a,const std::vector<GrowthChoice>& b){if(a.size()!=b.size())return false;
            for(std::size_t n=0;
            n<a.size();
            ++n)if(std::tie(a[n].step_id,a[n].option_id,a[n].points)!=std::tie(b[n].step_id,b[n].option_id,b[n].points))return false;
            return true;
        }
        bool results_equal(const GrowthChallengeResult& a,const GrowthChallengeResult& b,const StableId& kind){if(std::tie(a.completion,a.score,a.auto_score,a.performance_bp,a.raw_score)!=std::tie(b.completion,b.score,b.auto_score,b.performance_bp,b.raw_score)||!choices_equal(a.choices,b.choices))return false;
            return kind=="etiquette"?a.request_delta==b.request_delta:std::tie(a.discount_bp,a.relation_discount_bp,a.challenge_discount_bp)==std::tie(b.discount_bp,b.relation_discount_bp,b.challenge_discount_bp);
        }
        const StableId prefix(const StableId& kind){return kind=="etiquette"?"Etiquette":"Bargain";
        }
        BoundaryValue scalar(const StableId& s){return BoundaryValue{s};
        }
        BoundaryValue scalar(Integer i){return BoundaryValue{i};
        }
        BoundaryValue scalar(bool b){return BoundaryValue{b};
        }
        BoundaryValue choices_value(const std::vector<GrowthChoice>& choices,const StableId& kind){BoundaryValue::Array values;
            for(const auto& c:choices)values.push_back(boundary::ordered(boundary::obj({{kind=="etiquette"?"node_id":"round_id",scalar(c.step_id)},{"option_id",scalar(c.option_id)},{"points",scalar(c.points)}}),c.field_order));
            return BoundaryValue{std::move(values)};
        }
        std::vector<GrowthChoice> read_choices(const BoundaryValue& v,const StableId& kind,const StableId& path){using namespace boundary;
            std::vector<GrowthChoice> out;
            const auto key=kind=="etiquette"?"node_id":"round_id";
            for(const auto& value:array(v,path)){if(kind=="etiquette")fields(value,{"node_id","option_id","points"},{},path);
                else fields(value,{"round_id","option_id","points"},{},path);
                out.push_back({get_string(value,key,path),get_string(value,"option_id",path),get_int(value,"points",path),order(value)});
            }return out;
        }
        BoundaryValue result_value(const GrowthChallengeResult& r,const StableId& kind){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_growth_challenge_result_fields(out,r,kind);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
        GrowthChallengeResult read_result(const BoundaryValue& v,const StableId& kind,const StableId& path){using namespace boundary;
            GrowthChallengeResult r;
            r.field_order=order(v);
            if(kind=="etiquette")fields(v,{"completion","score","raw_score","auto_score","performance_bp","request_delta","choices"},{},path);
            else fields(v,{"completion","score","raw_score","auto_score","performance_bp","discount_bp","relation_discount_bp","challenge_discount_bp","choices"},{},path);
            r.completion=get_string(v,"completion",path);
            r.score=get_int(v,"score",path);
            r.auto_score=get_int(v,"auto_score",path);
            r.performance_bp=get_int(v,"performance_bp",path);
            auto& raw=field(v,"raw_score",path);
            if(!raw.as<std::monostate>())r.raw_score=integer(raw,path);
            if(kind=="etiquette")r.request_delta=get_int(v,"request_delta",path);
            else{r.discount_bp=get_int(v,"discount_bp",path);
                r.relation_discount_bp=get_int(v,"relation_discount_bp",path);
                r.challenge_discount_bp=get_int(v,"challenge_discount_bp",path);
            }r.choices=read_choices(field(v,"choices",path),kind,path);
            return r;
        }
        StudyChallengeContext read_context(const BoundaryValue& v,const StableId& path){using namespace boundary;
            fields(v,{"budget","quantity","relation_discount_bp","quote"},{},path);
            StudyChallengeContext c{get_int(v,"budget",path),get_int(v,"quantity",path),get_int(v,"relation_discount_bp",path),order(v),{}};
            const auto& quote=field(v,"quote",path);
            if(!quote.as<std::monostate>()){fields(quote,{"quote_id","quote_sha256","market_revision","valid_tick"},{},path);
                c.quote=BargainQuoteContext{get_string(quote,"quote_id",path),get_string(quote,"quote_sha256",path),get_int(quote,"market_revision",path),get_int(quote,"valid_tick",path),order(quote)};
            }if(c.budget<0||c.budget>2147483647||c.quantity<1||c.quantity>500||c.relation_discount_bp<0||c.relation_discount_bp>800)fail("InvalidStudyChallengeContext",path);
            return c;
        }
        BoundaryValue context_value(const StudyChallengeContext& c){return boundary::ordered(boundary::obj({{"budget",scalar(c.budget)},{"quantity",scalar(c.quantity)},{"relation_discount_bp",scalar(c.relation_discount_bp)},{"quote",c.quote?boundary::ordered(boundary::obj({{"quote_id",scalar(c.quote->quote_id)},{"quote_sha256",scalar(c.quote->quote_sha256)},{"market_revision",scalar(c.quote->market_revision)},{"valid_tick",scalar(c.quote->valid_tick)}}),c.quote->field_order):BoundaryValue{std::monostate{}}}}),c.field_order);
        }
    }
    Outcome<ChallengeFrozen> challenge_quote(const GrowthCatalog& catalog,const GrowthCourse& course,const ActorRecord& actor,const StableId& mode){if(!course.challenge){if(mode!="automatic")return {false,{},common_failure("StudyChallengeNotOffered").error};
            return {true,{}, {}};
        }const auto& source=*course.challenge;
        ChallengeFrozen frozen;
        frozen.field=source.field;
        frozen.kind=source.kind;
        frozen.challenge_id=source.id;
        frozen.mode=mode;
        frozen.context=source.context;
        if(frozen.context&&!frozen.context->field_order.empty()&&std::find(frozen.context->field_order.begin(),frozen.context->field_order.end(),"quote")==frozen.context->field_order.end())frozen.context->field_order.push_back("quote");
        auto* def=definition(catalog,frozen);
        if(!def)return {false,{},common_failure("MissingStudyChallenge").error};
        if(mode!="manual"&&mode!="automatic")return {false,{},common_failure(source.kind=="etiquette"?"InvalidEtiquetteStart":"InvalidBargainStart").error};
        frozen.skill_level=growth_skill_level(growth_skill_xp(actor.skills,course.skill_id));
        if(course.skill_id=="rites"||course.skill_id=="commerce")frozen.auxiliary_attribute=actor.attributes.administration;
        else if(course.skill_id=="debate")frozen.auxiliary_attribute=actor.attributes.intellect;
        else if(course.skill_id=="negotiation")frozen.auxiliary_attribute=actor.attributes.charisma;
        else return {false,{},common_failure("InvalidStudyChallengeActor").error};
        if(frozen.skill_level<0||frozen.auxiliary_attribute<1||frozen.auxiliary_attribute>110)return {false,{},common_failure("InvalidStudyChallengeActor").error};
        frozen.source_sha256=def->source_sha256;
        return {true,std::move(frozen),{}};
    }
    Outcome<GrowthChallengeInstance> challenge_start(CommonStore& common,const GrowthCatalog& catalog,const ChallengeFrozen& frozen,const StableId& actor,const StableId& action){auto* stream=common.state().rng.find("minigame");
        if(!stream)return {false,{},common_failure("InvalidMinigameRandomState").error};
        GrowthChallengeInstance instance;
        instance.seed_origin=*stream;
        instance.frozen=frozen;
        instance.actor_id=actor;
        instance.source_id=action;
        auto draw=common.random_bounded("minigame",std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max());
        if(!draw.ok)return {false,{},draw.error};
        instance.seed=std::to_string(draw.value+2147483648LL);
        auto* def=definition(catalog,frozen);
        if(!def||def->source_sha256!=frozen.source_sha256)return {false,{},common_failure("StudyChallengeSourceChanged").error};
        if(frozen.mode=="automatic"){instance.phase="completed";
            instance.result=result(*def,instance,"automatic");
        }return {true,std::move(instance),{}};
    }
    Status challenge_validate(const GrowthCatalog& catalog,const GrowthChallengeInstance& instance){const auto& f=instance.frozen;
        auto* def=definition(catalog,f);
        if(!def)return common_failure("MissingStudyChallenge");
        auto name=prefix(f.kind);
        if(def->source_sha256!=f.source_sha256)return common_failure(name+"SourceChanged");
        if(f.skill_level<0||f.skill_level>4||f.auxiliary_attribute<1||f.auxiliary_attribute>110||(f.mode!="manual"&&f.mode!="automatic")||(instance.phase!="active"&&instance.phase!="completed")||instance.choices.size()>3||instance.actor_id.empty()||instance.source_id.empty())return common_failure("Invalid"+name+"State");
        auto bits=rng_detail::unsigned_bits(instance.seed_origin.state);
        auto raw=rng_detail::raw(bits);
        if(instance.seed!=std::to_string(raw))return common_failure("StudyChallengeBindingMismatch");
        std::vector<GrowthChoice> history;
        for(std::size_t n=0;
        n<instance.choices.size();
        ++n){const auto& choice=instance.choices[n];
            const auto& node=def->nodes[n];
            const GrowthChallengeOption* option=nullptr;
            for(const auto& o:node.options)if(o.id==choice.option_id)option=&o;
            if(choice.step_id!=node.id||!option||choice.points!=points(*def,*option,history))return common_failure("Invalid"+name+"Choice");
            history.push_back(choice);
        }if(instance.phase=="active")return f.mode=="manual"&&instance.choices.size()<3&&!instance.result&&!instance.consumed?common_success():common_failure("Invalid"+name+"Progress");
        if(!instance.result)return common_failure("Invalid"+name+"Result");
        const auto& completion=instance.result->completion;
        if((completion=="automatic"&&(f.mode!="automatic"||!instance.choices.empty()))||(completion=="manual"&&(f.mode!="manual"||instance.choices.size()!=3))||(completion=="exited"&&(f.mode!="manual"||instance.choices.size()>=3))||(completion!="automatic"&&completion!="manual"&&completion!="exited")||!results_equal(*instance.result,result(*def,instance,completion),f.kind))return common_failure("Invalid"+name+"Result");
        return common_success();
    }
// 未完成题面只决定当前是否采用基线；不阻断原课程授课，也不改变随机或已有选择。
    bool challenge_pending(const GrowthChallengeInstance& instance){return instance.frozen.mode=="manual"&&instance.phase=="active";
    }
// 每课沿原倍率逐课取整；晚答的补差由课程账记录，题面本身不授奖。
    Integer challenge_lesson_xp(const StudyFact& fact,Integer base_xp){const auto bp=fact.challenge&&fact.challenge->result?fact.challenge->result->performance_bp:10000;
        return base_xp*bp/10000;
    }
    Outcome<GrowthChallengeInstance> challenge_transition(const GrowthCatalog& catalog,const GrowthChallengeInstance& instance,const GrowthCommand& command){auto valid=challenge_validate(catalog,instance);
        if(!valid.ok)return {false,{},valid.error};
        if(instance.consumed)return {false,{},common_failure("StudyChallengeAlreadyConsumed").error};
        if(command.kind.rfind("study_etiquette_",0)==0&&instance.frozen.kind!="etiquette")return {false,{},common_failure("UnsupportedAction").error};
        auto next=instance;
        auto* def=definition(catalog,instance.frozen);
        bool choose=command.kind=="study_etiquette_choose"||command.kind=="study_challenge_choose";
        auto name=prefix(instance.frozen.kind);
        if(!choose){if(next.phase=="active"){next.phase="completed";
                next.result=result(*def,next,"exited");
            }return {true,std::move(next),{}};
        }if(instance.phase!="active")return {false,{},common_failure(name+"AlreadyCompleted").error};
        const auto& node=def->nodes[instance.choices.size()];
        if(command.step_id!=node.id)return {false,{},common_failure(instance.frozen.kind=="etiquette"?"StaleEtiquetteNode":"StaleBargainRound").error};
        const GrowthChallengeOption* option=nullptr;
        for(const auto& o:node.options)if(o.id==command.option_id)option=&o;
        if(!option)return {false,{},common_failure("Unknown"+name+"Option").error};
        next.choices.push_back({command.step_id,command.option_id,points(*def,*option,instance.choices),{}});
        if(next.choices.size()==3){next.phase="completed";
            next.result=result(*def,next,"manual");
        }return {true,std::move(next),{}};
    }
    Status challenge_close(const GrowthCatalog& catalog,GrowthChallengeInstance& instance){GrowthCommand command;
        command.kind="study_challenge_exit";
        auto next=challenge_transition(catalog,instance,command);
        if(!next.ok)return {false,{},next.error};
        instance=std::move(next.value);
        return common_success();
    }
    BoundaryValue challenge_view(const GrowthCatalog& catalog,const CommonCatalog& common,const GrowthChallengeInstance& instance){return encode_public_challenge(public_challenge(catalog,common,instance));
    }
    GrowthPublicChallenge public_challenge(const GrowthCatalog& catalog,const CommonCatalog& common,const GrowthChallengeInstance& instance){GrowthPublicChallenge v;
        v.kind=instance.frozen.kind;
        v.phase=instance.phase;
        v.result=instance.result;
        if(v.phase=="completed")return v;
        const auto* d=definition(catalog,instance.frozen);
        const auto& node=d->nodes[instance.choices.size()];
        auto text=[&](const StableId& key){auto* value=common.texts.find(key);
            return value?*value:StableId{};
        };
        v.round=instance.choices.size()+1;
        if(instance.frozen.context){v.budget=instance.frozen.context->budget;
            v.quantity=instance.frozen.context->quantity;
        }v.step_id=node.id;
        v.topic=text(node.topic_key);
        v.stance=text(node.stance_key);
        v.prompt=text(node.prompt_key);
        for(const auto& o:node.options)v.options.push_back({o.id,text(o.text_key),o.kind,o.method,o.supply_condition_id,o.terms_condition_id});
        for(const auto& c:d->conditions)v.conditions.push_back({c.id,c.kind,text(c.text_key),c.matched});
        return v;
    }
    Status emit_growth_challenge_result_fields(ProjectionWriter& out,const GrowthChallengeResult& r,const StableId& kind){out.field("completion",r.completion);out.field("score",r.score);if(r.raw_score)out.field("raw_score",*r.raw_score);else out.null("raw_score");out.field("auto_score",r.auto_score);out.field("performance_bp",r.performance_bp);if(kind=="etiquette")out.field("request_delta",r.request_delta);else{out.field("discount_bp",r.discount_bp);out.field("relation_discount_bp",r.relation_discount_bp);out.field("challenge_discount_bp",r.challenge_discount_bp);}out.sequence("choices",r.choices,[&](auto& writer,const auto& c){return writer.object("",c.field_order,[&](auto& v){v.field(kind=="etiquette"?"node_id":"round_id",c.step_id);v.field("option_id",c.option_id);return v.field("points",c.points);});});return out.order_fields(r.field_order);}
    Status emit_growth_public_challenge_fields(ProjectionWriter& out,const GrowthPublicChallenge& v){out.field("ok",true);out.field("phase",v.phase);if(v.phase=="completed"){if(!v.result)return out.reject("MissingGrowthChallengeResult","growth_effect");return out.object("result",[&](auto& child){return emit_growth_challenge_result_fields(child,*v.result,v.kind);});}out.field("round",v.round);out.field(v.kind=="etiquette"?"node_id":"round_id",v.step_id);if(v.kind=="etiquette"){out.field("topic",v.topic);out.field("stance",v.stance);}else{out.field("prompt",v.prompt);out.field("budget",v.budget);out.field("quantity",v.quantity);out.sequence("conditions",v.conditions,[](auto& w,const auto& c){return w.object("",[&](auto& x){x.field("id",c.id);x.field("kind",c.kind);x.field("text",c.text);return x.field("matched",c.matched);});});}return out.sequence("options",v.options,[&](auto& w,const auto& option){return w.object("",[&](auto& x){x.field("id",option.id);if(v.kind=="etiquette")return x.field("text",option.text);x.field("kind",option.kind);x.field("method",option.method);x.field("text",option.text);if(option.supply_condition_id)x.field("supply_condition_id",*option.supply_condition_id);else x.null("supply_condition_id");if(option.terms_condition_id)x.field("terms_condition_id",*option.terms_condition_id);else x.null("terms_condition_id");return x.status();});});}
    BoundaryValue encode_public_challenge(const GrowthPublicChallenge& v){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_growth_public_challenge_fields(out,v);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    GrowthPublicChallenge decode_public_challenge(const BoundaryValue& value,const StableId& kind,const StableId& path){using namespace boundary;
        GrowthPublicChallenge v;
        v.kind=kind;
        v.phase=get_string(value,"phase",path);
        if(v.phase=="completed")v.result=read_result(field(value,"result",path),kind,path);
        else{v.round=get_int(value,"round",path);
            v.step_id=get_string(value,kind=="etiquette"?"node_id":"round_id",path);
            if(kind=="etiquette"){v.topic=get_string(value,"topic",path);
                v.stance=get_string(value,"stance",path);
            }else{v.prompt=get_string(value,"prompt",path);
                v.budget=get_int(value,"budget",path);
                v.quantity=get_int(value,"quantity",path);
                for(const auto& c:array(field(value,"conditions",path),path))v.conditions.push_back({get_string(c,"id",path),get_string(c,"kind",path),get_string(c,"text",path),get_bool(c,"matched",path)});
            }for(const auto& o:array(field(value,"options",path),path)){GrowthPublicOption option;
                option.id=get_string(o,"id",path);
                option.text=get_string(o,"text",path);
                if(kind=="bargain"){option.kind=get_string(o,"kind",path);
                    option.method=get_string(o,"method",path);
                    const auto& supply=field(o,"supply_condition_id",path);
                    if(!supply.as<std::monostate>())option.supply_condition_id=string(supply,path);
                    const auto& terms=field(o,"terms_condition_id",path);
                    if(!terms.as<std::monostate>())option.terms_condition_id=string(terms,path);
                }v.options.push_back(std::move(option));
        }}return v;
    }
    GrowthChallengeResult decode_challenge_result(const BoundaryValue& value,const StableId& kind,const StableId& path){return read_result(value,kind,path);
    }
// 冷codec保留旧terms.etiquette与新terms.challenge的不同字段形状。
Status emit_growth_challenge_context_fields(ProjectionWriter& out,const StudyChallengeContext& c){out.field("budget",c.budget);out.field("quantity",c.quantity);out.field("relation_discount_bp",c.relation_discount_bp);if(c.quote)out.object("quote",c.quote->field_order,[&](auto& q){q.field("quote_id",c.quote->quote_id);q.field("quote_sha256",c.quote->quote_sha256);q.field("market_revision",c.quote->market_revision);return q.field("valid_tick",c.quote->valid_tick);});else out.null("quote");return out.order_fields(c.field_order);}
Status emit_growth_frozen_fields(ProjectionWriter& out,const ChallengeFrozen& f){out.field("challenge_id",f.challenge_id);out.field("mode",f.mode);out.object("actor_snapshot",f.snapshot_order,[&](auto& w){w.field("skill_level",f.skill_level);return w.field("auxiliary_attribute",f.auxiliary_attribute);});out.field("source_sha256",f.source_sha256);if(f.field=="challenge"){out.field("kind",f.kind);if(f.context)out.object("context",[&](auto& w){return emit_growth_challenge_context_fields(w,*f.context);});else out.null("context");}return out.order_fields(f.field_order);}
Status emit_growth_challenge_seed_fields(ProjectionWriter& out,const GrowthChallengeInstance& i){out.field("seed",std::to_string(i.seed_origin.seed));out.field("state",std::to_string(i.seed_origin.state));return out.order_fields(i.seed_origin.field_order);}
Status emit_growth_challenge_state_fields(ProjectionWriter& out,const GrowthChallengeInstance& i){const auto& f=i.frozen;out.text("format",f.kind=="etiquette"?"HM-ETIQUETTE-CHALLENGE/1":"HM-BARGAIN-CHALLENGE/1");out.field("definition_id",f.challenge_id);out.field("source_sha256",f.source_sha256);out.object("binding",i.binding_order,[&](auto& w){w.field("actor_id",i.actor_id);return w.field("source_id",i.source_id);});out.object("actor_snapshot",f.snapshot_order,[&](auto& w){w.field("skill_level",f.skill_level);return w.field("auxiliary_attribute",f.auxiliary_attribute);});if(f.kind=="bargain"){if(!f.context)return out.reject("InvalidStudyChallengeContext");out.object("context",[&](auto& w){return emit_growth_challenge_context_fields(w,*f.context);});}out.field("seed",i.seed);out.field("mode",f.mode);out.field("phase",i.phase);out.sequence("choices",i.choices,[&](auto& w,const auto& c){return w.object("",c.field_order,[&](auto& x){x.field(f.kind=="etiquette"?"node_id":"round_id",c.step_id);x.field("option_id",c.option_id);return x.field("points",c.points);});});out.object("result",[&](auto& w){return i.result?emit_growth_challenge_result_fields(w,*i.result,f.kind):w.status();});return out.order_fields(i.state_order);}
Status emit_growth_challenge_fields(ProjectionWriter& out,const GrowthChallengeInstance& i){out.object("seed_origin",[&](auto& w){return emit_growth_challenge_seed_fields(w,i);});out.object("challenge",[&](auto& w){return emit_growth_challenge_state_fields(w,i);});out.field("consumed",i.consumed);return out.order_fields(i.field_order);}

    BoundaryValue encode_frozen(const ChallengeFrozen& f){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_frozen_fields(w,f);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    ChallengeFrozen decode_frozen(const BoundaryValue& v,const StableId& key,const StableId& path){using namespace boundary;
        if(key=="etiquette")fields(v,{"challenge_id","mode","actor_snapshot","source_sha256"},{},path);
        else fields(v,{"challenge_id","mode","actor_snapshot","source_sha256","kind","context"},{},path);
        ChallengeFrozen f;
        f.field=key;
        f.field_order=order(v);
        f.kind=key=="etiquette"?"etiquette":get_string(v,"kind",path);
        if(f.kind!="etiquette"&&f.kind!="bargain")fail("UnknownStudyChallengeKind",path);
        f.challenge_id=get_string(v,"challenge_id",path);
        f.mode=get_string(v,"mode",path);
        f.source_sha256=get_string(v,"source_sha256",path);
        const auto& snapshot=field(v,"actor_snapshot",path);
        fields(snapshot,{"skill_level","auxiliary_attribute"},{},path);
        f.snapshot_order=order(snapshot);
        f.skill_level=get_int(snapshot,"skill_level",path);
        f.auxiliary_attribute=get_int(snapshot,"auxiliary_attribute",path);
        if(key=="challenge"){const auto& c=field(v,"context",path);
            if(f.kind=="bargain")f.context=read_context(c,path);
            else if(!c.as<std::monostate>())fail("InvalidStudyChallengeContext",path);
        }return f;
    }
    BoundaryValue encode_challenge(const GrowthChallengeInstance& i){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_challenge_fields(w,i);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    GrowthChallengeInstance decode_challenge(const BoundaryValue& v,const ChallengeFrozen& frozen,const StableId& path){using namespace boundary;
        fields(v,{"seed_origin","challenge","consumed"},{},path);
        GrowthChallengeInstance i;
        i.field_order=order(v);
        i.frozen=frozen;
        i.consumed=get_bool(v,"consumed",path);
        const auto& origin=field(v,"seed_origin",path);
        fields(origin,{"seed","state"},{},path);
        i.seed_origin.field_order=order(origin);
        auto parse=[&](const char* key){auto text=get_string(origin,key,path);
            try{std::size_t used=0;
                auto n=std::stoll(text,&used);
                if(used!=text.size()||std::to_string(n)!=text)fail("InvalidMinigameRandomState",path);
                return Integer(n);
            }catch(const std::exception&){fail("InvalidMinigameRandomState",path);
                return Integer(0);
        }};
        i.seed_origin.seed=parse("seed");
        i.seed_origin.state=parse("state");
        const auto& state=field(v,"challenge",path);
        i.state_order=order(state);
        if(frozen.kind=="etiquette")fields(state,{"format","definition_id","source_sha256","binding","actor_snapshot","seed","mode","phase","choices","result"},{},path);
        else fields(state,{"format","definition_id","source_sha256","binding","actor_snapshot","context","seed","mode","phase","choices","result"},{},path);
        if(get_string(state,"format",path)!=(frozen.kind=="etiquette"?"HM-ETIQUETTE-CHALLENGE/1":"HM-BARGAIN-CHALLENGE/1")||get_string(state,"definition_id",path)!=frozen.challenge_id||get_string(state,"source_sha256",path)!=frozen.source_sha256||get_string(state,"mode",path)!=frozen.mode)fail("StudyChallengeBindingMismatch",path);
        const auto& snapshot=field(state,"actor_snapshot",path);
        if(get_int(snapshot,"skill_level",path)!=frozen.skill_level||get_int(snapshot,"auxiliary_attribute",path)!=frozen.auxiliary_attribute)fail("StudyChallengeSnapshotMismatch",path);
        if(frozen.kind=="bargain"){auto context=read_context(field(state,"context",path),path);
            if(!frozen.context||std::tie(context.budget,context.quantity,context.relation_discount_bp)!=std::tie(frozen.context->budget,frozen.context->quantity,frozen.context->relation_discount_bp))fail("StudyChallengeContextMismatch",path);
        }const auto& binding=field(state,"binding",path);
        fields(binding,{"actor_id","source_id"},{},path);
        i.binding_order=order(binding);
        i.actor_id=get_string(binding,"actor_id",path);
        i.source_id=get_string(binding,"source_id",path);
        i.seed=get_string(state,"seed",path);
        i.phase=get_string(state,"phase",path);
        i.choices=read_choices(field(state,"choices",path),frozen.kind,path);
        const auto& outcome=field(state,"result",path);
        if(!object(outcome,path).empty())i.result=read_result(outcome,frozen.kind,path);
        return i;
    }
} // 题面只评分公开作者条件，不拥有课程成长、交易改价、退款或第二个随机流。
