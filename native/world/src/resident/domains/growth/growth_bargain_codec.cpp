#include "growth_state_projection.hpp"
// 正式议价的冷合同及既有排序摘要；摘要只消费固定报价/题面字段，不保存或重建世界字典。
#include "growth_internal.hpp"
#include "resident/checkpoint/canonical_digest.hpp"
#include <algorithm>
namespace hanmo::resident::growth {
    namespace {
        using namespace boundary;
        BoundaryValue v(const StableId& x){return BoundaryValue{x};
        }BoundaryValue v(Integer x){return BoundaryValue{x};
        }BoundaryValue v(bool x){return BoundaryValue{x};
        }
        BoundaryValue selected_texts(const GrowthChallengeDefinition& d,const CommonCatalog& shared){BoundaryValue::Dict rows;
            auto put=[&](const StableId& key){if(key.empty())return;
                if(std::none_of(rows.begin(),rows.end(),[&](const auto& r){return r.first==key;
                })){auto* text=shared.texts.find(key);
                    if(!text)fail("MissingBargainText",key);
                    rows.emplace_back(key,v(*text));
            }};
            for(const auto& c:d.conditions)put(c.text_key);
            for(const auto& n:d.nodes){put(n.topic_key);
                put(n.stance_key);
                put(n.prompt_key);
                for(const auto& o:n.options)put(o.text_key);
            }return BoundaryValue{std::move(rows)};
        }
        BoundaryValue encode_policy(const BargainPolicy& policy){BoundaryValue::Array sources;
            for(const auto& source:policy.condition_sources){auto value=obj({{"condition_id",v(source.condition_id)},{"source",v(source.source)}});
                if(source.value)std::get<BoundaryValue::Dict>(value.data).emplace_back("value",v(*source.value));
                sources.push_back(ordered(std::move(value),source.field_order));
            }return ordered(obj({{"challenge_id",v(policy.challenge_id)},{"condition_sources",BoundaryValue{std::move(sources)}}}),policy.field_order);
        }
        BoundaryValue encode_context(const StudyChallengeContext& c){return ordered(obj({{"budget",v(c.budget)},{"quantity",v(c.quantity)},{"relation_discount_bp",v(c.relation_discount_bp)},{"quote",c.quote?ordered(obj({{"quote_id",v(c.quote->quote_id)},{"quote_sha256",v(c.quote->quote_sha256)},{"market_revision",v(c.quote->market_revision)},{"valid_tick",v(c.quote->valid_tick)}}),c.quote->field_order):BoundaryValue{std::monostate{}}}}),c.field_order);
        }
        StudyChallengeContext decode_context(const BoundaryValue& value,const StableId& path){fields(value,{"budget","quantity","relation_discount_bp","quote"},{},path);
            StudyChallengeContext c;
            c.field_order=order(value);
            c.budget=get_int(value,"budget",path);
            c.quantity=get_int(value,"quantity",path);
            c.relation_discount_bp=get_int(value,"relation_discount_bp",path);
            const auto& quote=field(value,"quote",path);
            fields(quote,{"quote_id","quote_sha256","market_revision","valid_tick"},{},path);
            c.quote=BargainQuoteContext{get_string(quote,"quote_id",path),get_string(quote,"quote_sha256",path),get_int(quote,"market_revision",path),get_int(quote,"valid_tick",path),order(quote)};
            return c;
        }
    }
    // 兼容本域旧调用名；排序、JSON转义与SHA算法只有共同边界一个实现。
    StableId canonical_digest(BoundaryValue value){return boundary_canonical_digest(value);}
    // 公开题面逐层保留作者目录的字典序；摘要调用仍对独立副本排序。
    BoundaryValue encode_definition(const GrowthChallengeDefinition& d){BoundaryValue::Array nodes;
        for(const auto& n:d.nodes){BoundaryValue::Array options;
            for(const auto& o:n.options){if(d.kind=="etiquette"){BoundaryValue::Array requirements;
                    for(const auto& r:o.requires_choices)requirements.push_back(ordered(obj({{"node_id",v(r.node_id)},{"option_id",v(r.option_id)}}),r.field_order));
                    options.push_back(ordered(obj({{"id",v(o.id)},{"text_key",v(o.text_key)},{"points",v(o.points)},{"requires_choices",BoundaryValue{std::move(requirements)}}}),o.field_order));
                }else options.push_back(ordered(obj({{"id",v(o.id)},{"kind",v(o.kind)},{"method",v(o.method)},{"text_key",v(o.text_key)},{"supply_condition_id",o.supply_condition_id?v(*o.supply_condition_id):BoundaryValue{std::monostate{}}},{"terms_condition_id",o.terms_condition_id?v(*o.terms_condition_id):BoundaryValue{std::monostate{}}}}),o.field_order));
            }if(d.kind=="etiquette")nodes.push_back(ordered(obj({{"id",v(n.id)},{"topic_key",v(n.topic_key)},{"stance_key",v(n.stance_key)},{"options",BoundaryValue{std::move(options)}}}),n.field_order));
            else nodes.push_back(ordered(obj({{"id",v(n.id)},{"prompt_key",v(n.prompt_key)},{"options",BoundaryValue{std::move(options)}}}),n.field_order));
        }auto result=obj({{"id",v(d.id)},{"difficulty",v(d.difficulty)}});
        auto& rows=std::get<BoundaryValue::Dict>(result.data);
        // 旧definition_version为0时字节完全不变；新Family真实正版本参与同一SHA。
        if (d.kind == "etiquette" && d.definition_version > 0)
          rows.emplace_back("definition_version", v(d.definition_version));
        if(d.kind=="bargain"){BoundaryValue::Array conditions;
            for(const auto& c:d.conditions)conditions.push_back(ordered(obj({{"id",v(c.id)},{"kind",v(c.kind)},{"text_key",v(c.text_key)},{"matched",v(c.matched)}}),c.field_order));
            rows.emplace_back("conditions",BoundaryValue{std::move(conditions)});
        }rows.emplace_back(d.kind=="etiquette"?"nodes":"rounds",BoundaryValue{std::move(nodes)});
        return ordered(std::move(result),d.field_order);
    }
    StableId challenge_digest(const GrowthChallengeDefinition& d,const CommonCatalog& shared){return canonical_digest(obj({{"definition",encode_definition(d)},{"texts",selected_texts(d,shared)}}));
    }
    StableId bargain_source_digest(const GrowthChallengeDefinition& d,const BargainPolicy& policy,const CommonCatalog& shared){return canonical_digest(obj({{"definition",encode_definition(d)},{"policy",encode_policy(policy)},{"texts",selected_texts(d,shared)}}));
    }
Status emit_bargain_market_fields(ProjectionWriter& out,const BargainMarketBasis& m){
#define F(x) out.field(#x,m.x)
 F(base_price);F(stock);F(elasticity_bp);F(target_stock);F(event_bp);F(shop_wallet_id);F(shop_inventory_id);F(actor_inventory_id);F(available_quantity);
#undef F
 return out.order_fields(m.field_order);}
Status emit_bargain_terms_fields(ProjectionWriter& out,const BargainTerms& t){out.object("request",[&](auto& w){return emit_growth_command_fields(w,t.request);});out.object("base_quote",[&](auto& w){return emit_growth_trade_terms_fields(w,t.base_quote);});out.object("market_basis",[&](auto& w){return emit_bargain_market_fields(w,t.market_basis);});out.field("source_sha256",t.source_sha256);out.field("challenge_id",t.challenge_id);out.object("actor_snapshot",t.snapshot_order,[&](auto& w){w.field("skill_level",t.skill_level);return w.field("auxiliary_attribute",t.auxiliary_attribute);});out.object("quote_context",[&](auto& w){return emit_growth_challenge_context_fields(w,t.quote_context);});return out.order_fields(t.field_order);}
Status emit_growth_bargain_fact_fields(ProjectionWriter& out,const BargainFact& f){
 // Original encode_challenge validated its complete wrapper before extracting
 // the two adopted fields. Both sinks preserve that detached failure gate.
 out.validate_detached([&](auto& w){return emit_growth_challenge_fields(w,f.instance);});out.text("kind","trade_bargain");out.field("actor_id",f.actor_id);out.field("basis_sha256",f.basis_sha256);out.object("terms",[&](auto& w){return emit_bargain_terms_fields(w,f.terms);});out.object("seed_origin",[&](auto& w){return emit_growth_challenge_seed_fields(w,f.instance);});out.object("challenge",[&](auto& w){return emit_growth_challenge_state_fields(w,f.instance);});out.field("consumed_action_id",f.consumed_action_id);out.object("receipt",[&](auto& w){return f.receipt?emit_growth_trade_terms_fields(w,*f.receipt):w.status();});return out.order_fields(f.field_order);}

    BoundaryValue encode_bargain_market(const BargainMarketBasis& m){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_bargain_market_fields(w,m);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    BoundaryValue encode_bargain_terms(const BargainTerms& t){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_bargain_terms_fields(w,t);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    BargainTerms decode_bargain_terms(const BoundaryValue& value,const StableId& path){fields(value,{"request","base_quote","market_basis","source_sha256","challenge_id","actor_snapshot","quote_context"},{},path);
        BargainTerms t;
        t.field_order=order(value);
        auto decoded=decode_growth_command("trade",field(value,"request",path));
        if(!decoded.ok)fail(decoded.error.original.code,path);
        t.request=std::move(decoded.value);
        t.base_quote=decode_trade_terms(field(value,"base_quote",path),path);
        t.source_sha256=get_string(value,"source_sha256",path);
        t.challenge_id=get_string(value,"challenge_id",path);
        const auto& snapshot=field(value,"actor_snapshot",path);
        fields(snapshot,{"skill_level","auxiliary_attribute"},{},path);
        t.snapshot_order=order(snapshot);
        t.skill_level=get_int(snapshot,"skill_level",path);
        t.auxiliary_attribute=get_int(snapshot,"auxiliary_attribute",path);
        t.quote_context=decode_context(field(value,"quote_context",path),path);
        const auto& market=field(value,"market_basis",path);
        fields(market,{"base_price","stock","elasticity_bp","target_stock","event_bp","shop_wallet_id","shop_inventory_id","actor_inventory_id","available_quantity"},{},path);
        auto& m=t.market_basis;
        m.field_order=order(market);
#define HM_INT(name) m.name=get_int(market,#name,path)
        HM_INT(base_price);
        HM_INT(stock);
        HM_INT(elasticity_bp);
        HM_INT(target_stock);
        HM_INT(event_bp);
        HM_INT(available_quantity);
#undef HM_INT
        m.shop_wallet_id=get_string(market,"shop_wallet_id",path);
        m.shop_inventory_id=get_string(market,"shop_inventory_id",path);
        m.actor_inventory_id=get_string(market,"actor_inventory_id",path);
        return t;
    }
    BoundaryValue encode_bargain_fact(const BargainFact& f){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_bargain_fact_fields(w,f);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    BargainFact decode_bargain_fact(const BoundaryValue& value,const StableId& path){fields(value,{"kind","actor_id","basis_sha256","terms","seed_origin","challenge","consumed_action_id","receipt"},{},path);
        BargainFact f;
        f.field_order=order(value);
        f.actor_id=get_string(value,"actor_id",path);
        f.basis_sha256=get_string(value,"basis_sha256",path);
        f.consumed_action_id=get_string(value,"consumed_action_id",path);
        f.terms=decode_bargain_terms(field(value,"terms",path),path);
        const auto& challenge=field(value,"challenge",path);
        ChallengeFrozen frozen;
        frozen.field="challenge";
        frozen.kind="bargain";
        frozen.challenge_id=f.terms.challenge_id;
        frozen.mode=get_string(challenge,"mode",path);
        frozen.source_sha256=get_string(challenge,"source_sha256",path);
        frozen.skill_level=f.terms.skill_level;
        frozen.auxiliary_attribute=f.terms.auxiliary_attribute;
        frozen.context=f.terms.quote_context;
        auto instance=obj({{"seed_origin",field(value,"seed_origin",path)},{"challenge",challenge},{"consumed",v(false)}});
        f.instance=decode_challenge(instance,frozen,path);
        f.instance.field_order.clear();
        const auto& receipt=field(value,"receipt",path);
        if(!object(receipt,path).empty())f.receipt=decode_trade_terms(receipt,path);
        return f;
    }
    BoundaryValue encode_bargain_prepared(const BargainPrepared& prepared){if(prepared.next){auto before=encode_challenge(*prepared.before),next=encode_challenge(*prepared.next);
            return obj({{"ok",v(true)},{"duration",v(Integer(0))},{"quote_id",v(prepared.quote_id)},{"before",field(before,"challenge","")},{"next",field(next,"challenge","")}});
        }auto basis=obj({{"ok",v(true)},{"request",encode_growth_command(prepared.terms.request)},{"base_quote",encode_trade_terms(prepared.terms.base_quote)},{"market_basis",encode_bargain_market(prepared.terms.market_basis)},{"source_sha256",v(prepared.terms.source_sha256)},{"challenge_id",v(prepared.terms.challenge_id)},{"definition",encode_definition(prepared.definition)},{"basis_sha256",v(prepared.basis_sha256)}});
        return obj({{"ok",v(true)},{"duration",v(Integer(0))},{"mode",v(prepared.mode)},{"basis",std::move(basis)}});
    }
} // 原基准摘要供重开/时效/一次消费绑定，绝不替代当前资金、在场或库存校验。
