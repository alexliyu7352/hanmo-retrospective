"""日租World候选按固定原共享叶变换；只生成隔离后像，不直接注册不完整配对。"""
import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.content.private_shop_daily_rent_links import with_private_shop_daily_rent_schema

BASE = '5566d061522a51cf9bf596a4ac0a2ca7b7ac2e94'
PATHS = [
    'native/world/include/hanmo/resident/model_contract.hpp',
    'native/world/src/resident/model/common_model.hpp',
    'native/world/src/resident/kernel/world_bundle.hpp',
    'native/world/src/resident/kernel/world_bundle.cpp',
    'native/world/src/resident/kernel/dynamic_integration.cpp',
    'native/world/src/resident/kernel/phase_plan.cpp',
    'native/world/src/resident/kernel/political_takeover_integration.cpp',
    'native/world/src/resident/kernel/resident_world.cpp',
    'native/world/src/resident/kernel/command_types.hpp',
    'native/world/src/resident/kernel/dispatcher.cpp',
    'native/world/src/resident/kernel/live_view_queries.cpp',
    'native/world/src/resident/checkpoint/command_boundary.cpp',
    'native/world/src/resident/storage/common_codec.cpp',
    'native/world/resident.cmake',
    'schemas/content.schema.json',
    'game/data/rule_capabilities.json',
    'tools/content/links.py',
    'tools/verify.py',
    'game/ui/resident_page_requests.gd',
    'game/ui/screen_composer.gd',
    'native/world/src/resident/kernel/private_shop_integration.hpp',
    'native/world/src/resident/kernel/private_shop_integration.cpp',
    'native/world/src/resident/domains/local_economy/local_economy_model.hpp',
    'native/world/src/resident/domains/local_economy/local_economy_rules.cpp',
]

PRODUCT_CPP = ['src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_authority.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_business.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_calendar.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_financial.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_sources.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_view.cpp', 'src/resident/domains/commerce/daily_rent/private_shop_rent_period.cpp', 'src/resident/kernel/private_shop_daily_rent_integration.cpp']
PRODUCT_HPP = ['src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_business.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_financial.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_internal.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_daily_rent_view.hpp', 'src/resident/domains/commerce/daily_rent/private_shop_rent_period.hpp', 'src/resident/kernel/private_shop_daily_rent_integration.hpp']
NATIVE_TESTS = ['tests/resident/economic_world/private_shop_daily_rent_cached_receipt_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_command_boundary_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_le_phase_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_phase_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_sources_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_transaction_test.cpp', 'tests/resident/economic_world/private_shop_daily_rent_view_test.cpp', 'tests/resident/economic_world/private_shop_rent_period_test.cpp']

def once(text, old, new):
    """每个真实接点只允许恰一次命中，错基准不得静默省略生命周期字段。"""
    if text.count(old) != 1:
        raise ValueError(f'Expected one anchor, found {text.count(old)}: {old[:160]}')
    return text.replace(old, new, 1)


def transform(path, text):
    """同栈权源须在完整Shop Context前自持，原Family技能借口和任务借口均保持。"""
    if path == 'native/world/resident.cmake':
        text = once(text, 'add_library(hanmo_resident STATIC\n', 'add_library(hanmo_resident STATIC\n' + ''.join('    ' + p + '\n' for p in PRODUCT_CPP))
        text = once(text, 'set(HANMO_RESIDENT_IDENTITY_FILES\n', 'set(HANMO_RESIDENT_IDENTITY_FILES\n' + ''.join('    ' + p + '\n' for p in PRODUCT_CPP + PRODUCT_HPP))
        text += '\n# 日租仅绑定真实原LE付款；格式/暂停/原phase风险用例仍须同代库另行运行。\n'
        for path in NATIVE_TESTS:
            name = Path(path).stem.removesuffix('_test')
            target = 'test_resident_' + name
            text += f'add_executable({target} {path})\ntarget_link_libraries({target} PRIVATE hanmo_resident)\ntarget_include_directories({target} PRIVATE src)\ntarget_compile_options({target} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_{name} COMMAND {target})\n\n'
        return text
    if path == 'schemas/content.schema.json':
        return json.dumps(with_private_shop_daily_rent_schema(json.loads(text)), indent=2, ensure_ascii=False) + '\n'
    if path == 'game/data/rule_capabilities.json':
        value = json.loads(text)
        if any(row['capability_id'] == 'private_shop_daily_rent_v1' for row in value['capabilities']):
            raise ValueError('Daily rent capability already declared')
        value['capabilities'].append({'capability_id':'private_shop_daily_rent_v1','rule_version':'opening_anchored_two_halfdays_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.private_shop_daily_rent.version','equals':1}], 'supported_semantics':[], 'test_ids':['test_private_shop_daily_rent_panel_guard'], 'evidence':[]})
        return json.dumps(value, indent=2, ensure_ascii=False) + '\n'
    if path == 'tools/content/links.py':
        text = once(text, '    from .private_shop_links import check_private_shops', '    from .private_shop_daily_rent_links import check_private_shop_daily_rent\n    from .private_shop_links import check_private_shops')
        return once(text, ' + check_private_shops(pack,index)', ' + check_private_shop_daily_rent(pack,index) + check_private_shops(pack,index)')
    if path == 'tools/verify.py':
        return once(text, 'TESTS = [', "TESTS = ['test_private_shop_daily_rent_panel_guard', ")
    if path in ('game/ui/resident_page_requests.gd', 'game/ui/screen_composer.gd'):
        text = once(text, 'const PrivateShopPanel=preload("res://ui/private_shop_panel.gd")', 'const PrivateShopPanel=preload("res://ui/private_shop_panel.gd")\nconst PrivateShopDailyRentPanel=preload("res://ui/private_shop_daily_rent_panel.gd")')
        if path.endswith('resident_page_requests.gd'):
            return once(text, '\tif kernel.has_method("private_shops_enabled")', '\tif kernel.has_method("private_shop_daily_rent_enabled") and kernel.private_shop_daily_rent_enabled():\n\t\tif not PrivateShopDailyRentPanel.prepare(host): return false\n\tif kernel.has_method("private_shops_enabled")')
        return once(text, '\tif kernel.has_method("private_shops_enabled")', '\tif kernel.has_method("private_shop_daily_rent_enabled") and kernel.private_shop_daily_rent_enabled() and not PrivateShopDailyRentPanel._current(m).is_empty():\n\t\tPrivateShopDailyRentPanel.build(m,_sidebar_section(m,stage,"PrivateShopDailyRentArea","原租期与自愿付款"))\n\tif kernel.has_method("private_shops_enabled")')
    if path.endswith('command_types.hpp'):
        text = once(text, '#include "resident/domains/commerce/liquidation/private_shop_liquidation_boundary.hpp"', '#include "resident/domains/commerce/liquidation/private_shop_liquidation_boundary.hpp"\n#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.hpp"')
        text = once(text, 'ContactInquiryRequest,VisitReplyCommand>;', 'ContactInquiryRequest,VisitReplyCommand,PrivateShopDailyRentCommand>;')
        return once(text, 'ContactInquiryQuote,VisitReplyQuote>;', 'ContactInquiryQuote,VisitReplyQuote,PrivateShopDailyRentQuote>;')
    if path.endswith('dispatcher.cpp'):
        text = once(text, '#include "private_shop_integration.hpp"', '#include "private_shop_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        text = once(text, '    if(private_shop_command_kind(kind)', '    if(private_shop_daily_rent_command_kind(kind)&&!w.private_shop_daily_rent_enabled())return common_failure("PrivateShopDailyRentProfileRequired");\n    if(private_shop_command_kind(kind)')
        text = once(text, '    const auto* shop=std::get_if<PrivateShopCommand>', '    const auto* rent=std::get_if<PrivateShopDailyRentCommand>(&command.data);if(private_shop_daily_rent_command_kind(command.kind)!=(rent!=nullptr)||(rent&&private_shop_daily_rent_command_kind(*rent)!=command.kind))return reject("InvalidPrivateShopDailyRentCommand");\n    const auto* shop=std::get_if<PrivateShopCommand>')
        text = once(text, '    if(shop){auto q=', '    if(rent){auto q=prepare_private_shop_daily_rent_world(w,id,*rent);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n    if(shop){auto q=')
        text = once(text, '    if(const auto* c=std::get_if<PrivateShopCommand>(&op.command.data))', '    if(const auto* c=std::get_if<PrivateShopDailyRentCommand>(&op.command.data)){if(private_shop_daily_rent_command_kind(*c)!=op.command.kind||!std::holds_alternative<PrivateShopDailyRentQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidPrivateShopDailyRentCommand"));}else if(private_shop_daily_rent_command_kind(op.command.kind))return failed<ReceiptDetails>(common_failure("InvalidPrivateShopDailyRentCommand"));\n    if(const auto* c=std::get_if<PrivateShopCommand>(&op.command.data))')
        return once(text, '    if(const auto* c=std::get_if<PrivateShopCommand>(&command.data))', '    if(const auto* c=std::get_if<PrivateShopDailyRentCommand>(&command.data)){auto r=commit_private_shop_daily_rent_world(w,id,*c,std::get<PrivateShopDailyRentQuote>(op.quote.data));if(!r.ok)return {false,{},r.error};InteractionEffect effect;effect.settlement_result=r.value.source_fact_id;return {true,ReceiptDetails{std::move(effect),{}},{}};}\n    if(const auto* c=std::get_if<PrivateShopCommand>(&command.data))')
    if path.endswith('live_view_queries.cpp'):
        text = once(text, '#include "private_shop_integration.hpp"', '#include "private_shop_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        return once(text, '    if(query=="private_shop_business")', '    if(query=="private_shop_daily_rent"){auto parsed=decode_private_shop_daily_rent_query(args);if(!parsed.ok)return {false,{},parsed.error};return query_private_shop_daily_rent_world(world,parsed.value); }\n    if(query=="private_shop_business")')
    if path.endswith('command_boundary.cpp'):
        text = once(text, '    if(kind==political_takeover_command_kind){boundary::fields(value,{},{}', '    if(private_shop_daily_rent_command_kind(kind)){boundary::fields(value,{"settlement_result"},{},"receipt/details");InteractionEffect effect;effect.settlement_result=boundary::get_string(value,"settlement_result","receipt/details");result.data=std::move(effect);auto valid=validate_private_shop_daily_rent_receipt(kind,result);if(!valid.ok)throw MarshalFailure{valid.error};return result;}\n    if(kind==political_takeover_command_kind){boundary::fields(value,{},{}')
        text = once(text, 'c.field("variant",tag);if(', 'c.field("variant",tag);if(private_shop_daily_rent_command_kind(kind)&&(!(c.schema_features&private_shop_daily_rent_checkpoint_feature)||tag!=4))return c.reject("InvalidPrivateShopRentReceipt");if(')
        # typed visitor在数据读取后验精确字段，旧InteractionEffect布局一字节不扩。
        line = next(x for x in text.splitlines() if 'else if constexpr(std::is_same_v<T,InteractionEffect>){c.field' in x)
        text = once(text, line, line.replace('return c.status();}', 'if(private_shop_daily_rent_command_kind(kind)){auto checked=validate_private_shop_daily_rent_receipt(kind,x);if(!checked.ok)return c.reject("InvalidPrivateShopRentReceipt");}return c.status();}'))
        text = once(text, 'const auto& r=*input.value;\n', 'const auto& r=*input.value;\n        if(private_shop_daily_rent_command_kind(r.kind)){if(!r.details)failed("InvalidPrivateShopRentReceipt","receipt/details");auto valid=validate_private_shop_daily_rent_receipt(r.kind,*r.details);if(!valid.ok)throw MarshalFailure{valid.error};}\n')
        text = once(text, '    bool has_details=static_cast<bool>(value.details);', '    if(private_shop_daily_rent_command_kind(value.kind)&&!(c.schema_features&private_shop_daily_rent_checkpoint_feature))return c.reject("PrivateShopDailyRentProfileRequired");\n    bool has_details=static_cast<bool>(value.details);')
        text = once(text, 'c.field("has_details",has_details);', 'c.field("has_details",has_details);if(private_shop_daily_rent_command_kind(value.kind)&&!has_details)return c.reject("InvalidPrivateShopRentReceipt");')
        text = once(text, '    if(private_shop_command_kind(kind)){auto r=decode_private_shop_command', '    if(private_shop_daily_rent_command_kind(kind)){auto r=decode_private_shop_daily_rent_command(kind,params);return r.ok?Outcome<CommandPayload>{true,{kind,std::move(r.value)},{}}:Outcome<CommandPayload>{false,{},r.error};}\n    if(private_shop_command_kind(kind)){auto r=decode_private_shop_command')
        return once(text, 'else if constexpr(std::is_same_v<T,PrivateShopCommandQuote>)', 'else if constexpr(std::is_same_v<T,PrivateShopDailyRentQuote>)return encode_private_shop_daily_rent_quote(quote);\n        else if constexpr(std::is_same_v<T,PrivateShopCommandQuote>)')
    if path.endswith('common_codec.cpp'):
        text = once(text, 'const bool apprenticeship=kind==', 'const bool daily_rent=kind=="private_shop_rent_assessed"||kind=="private_shop_rent_paid"||kind=="private_shop_rent_resumed";if(daily_rent)fields(row.second,{"kind","domain"},{},row.first);const bool apprenticeship=kind==')
        text = once(text, 'std::string domain=', 'std::string domain=daily_rent?"private_shop_daily_rent":')
        text = once(text, 'auto named=string(*declared,row.first);', 'auto named=string(*declared,row.first);if(daily_rent!=(named=="private_shop_daily_rent"))fail("InvalidFactDomain",row.first);')
        return once(text, 'if(!(named=="political_governance_continuations"', 'if(!(named=="private_shop_daily_rent"&&daily_rent)&&!(named=="political_governance_continuations"')
    if path.endswith('world_bundle.cpp'):
        text = once(text, '#include "private_shop_liquidation_integration.hpp"', '#include "private_shop_liquidation_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"\n#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.hpp"')
        text = once(text, 'private_shops_seed_complete(bool(seed.private_shops)),', 'private_shops_seed_complete(bool(seed.private_shops)),private_shop_daily_rent(seed.private_shop_daily_rent?*seed.private_shop_daily_rent:PrivateShopDailyRentState{}),private_shop_daily_rent_seed_complete(bool(seed.private_shop_daily_rent)),')
        text = once(text, 'private_shops_seed_complete(other.private_shops_seed_complete),', 'private_shops_seed_complete(other.private_shops_seed_complete),private_shop_daily_rent(other.private_shop_daily_rent),private_shop_daily_rent_seed_complete(other.private_shop_daily_rent_seed_complete),')
        text = once(text, 'Status WorldBundle::validate() const {', 'Status WorldBundle::validate() const {\n    auto daily_pair=validate_private_shop_daily_rent_pair(private_shop_daily_rent_enabled(),catalog->identity.module_versions);if(!daily_pair.ok)return daily_pair;if(private_shop_daily_rent_enabled()!=private_shop_daily_rent_seed_complete)return common_failure("PrivateShopDailyRentSeedRequired");')
        text = once(text, 'private_shops,*catalog->private_shops,nullptr});', 'private_shops,*catalog->private_shops,nullptr,catalog->private_shop_daily_rent.get(),private_shop_daily_rent_enabled()?&private_shop_daily_rent:nullptr});')
        text = once(text, '    valid=validate_private_shop_liquidation_world(*this);if(!valid.ok)return valid;', '    valid=validate_private_shop_liquidation_world(*this);if(!valid.ok)return valid;\n    valid=validate_private_shop_daily_rent_world(*this);if(!valid.ok)return valid;')
        text = once(text, '    valid=validate_family_disciple_legacy_world(*this);if(!valid.ok)return valid;', '    valid=validate_family_disciple_legacy_world(*this);if(!valid.ok)return valid;\n    valid=validate_private_shop_daily_rent_player_history_world(*this);if(!valid.ok)return valid;')
        old = 'row.second.domain!="private_shop_liquidations"'
        new = old + '&&!(row.second.domain==private_shop_daily_rent_module&&(row.second.kind==private_shop_rent_assessed_kind||row.second.kind==private_shop_rent_paid_kind||row.second.kind==private_shop_rent_resumed_kind))'
        text = once(text, old, new)
        anchor = '    if(facts.size()!=w.common.state().facts.size())'
        block = '    for(const auto& row:w.common.state().facts.rows())if(row.second.domain==private_shop_daily_rent_module){if(!w.private_shop_daily_rent_enabled())return common_failure("PrivateShopDailyRentProfileRequired",row.first);auto payload=encode_private_shop_daily_rent_fact_payload(w.common,w.private_shop_daily_rent,row.second);if(!payload.ok)return {false,{},payload.error};auto checked=fact(row.first,row.second.kind,private_shop_daily_rent_module);if(!checked.ok)return checked;}\n'
        return once(text, anchor, block + anchor)
    if path.endswith('dynamic_integration.cpp'):
        text = once(text, 'Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){', 'Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){\n    if(proposal.command&&(std::holds_alternative<PrivateShopDailyRentCommand>(proposal.command->data)||private_shop_daily_rent_command_kind(proposal.command->kind)))return common_failure("PrivateShopDailyRentPlayerCommandRequired");')
        text = once(text, '    PrivateShopState private_shops;', '    PrivateShopDailyRentState private_shop_daily_rent;\n    PrivateShopState private_shops;')
        text = once(text, 'private_shops(w.private_shops),', 'private_shops(w.private_shops),private_shop_daily_rent(w.private_shop_daily_rent),')
        return once(text, 'w.private_shops=std::move(private_shops);', 'w.private_shops=std::move(private_shops);w.private_shop_daily_rent=std::move(private_shop_daily_rent);')
    if path.endswith('phase_plan.cpp'):
        text = once(text, '#include "private_shop_integration.hpp"', '#include "private_shop_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        text = once(text, '    applied=false;', '    applied=false;if(std::holds_alternative<PrivateShopDailyRentCommand>(command.data)||private_shop_daily_rent_command_kind(command.kind))return common_failure("PrivateShopDailyRentPlayerCommandRequired");')
        text = once(text, '    auto shops=shop_command?std::optional<PrivateShopState>{w.private_shops}:std::nullopt;', '    auto shops=shop_command?std::optional<PrivateShopState>{w.private_shops}:std::nullopt;\n    auto daily_rent=shop_command&&w.private_shop_daily_rent_enabled()?std::optional<PrivateShopDailyRentState>{w.private_shop_daily_rent}:std::nullopt;')
        text = once(text, 'if(shops)w.private_shops=std::move(*shops);', 'if(shops)w.private_shops=std::move(*shops);if(daily_rent)w.private_shop_daily_rent=std::move(*daily_rent);')
        text = once(text, 'auto economy=step_local_economy_in_candidate(', 'std::optional<PrivateShopResponsibilitySourceContext> shop_sources;if(w.private_shops_enabled())shop_sources.emplace(PrivateShopResponsibilitySourceContext{w.common,w.local_economy,*w.catalog->local_economy,w.private_shops,*w.catalog->private_shops,&dynamic_input,w.catalog->private_shop_daily_rent.get(),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr});auto economy=step_local_economy_in_candidate(')
        text = once(text, 'employment?&employment->context():nullptr,w.services.market_history());', 'employment?&employment->context():nullptr,w.services.market_history(),shop_sources?&*shop_sources:nullptr);')
        return once(text, 'case Phase::clock_commit:w.common.state().identity.tick=to_tick;break;', 'case Phase::clock_commit:w.common.state().identity.tick=to_tick;r=reconcile_private_shop_daily_rent_world_in_candidate(w,dynamic_input);break;')
    if path.endswith('political_takeover_integration.cpp'):
        old = 'w.private_shops,*w.catalog->private_shops,phase}'
        if text.count(old) != 2:
            raise ValueError('Expected two original private obligation contexts')
        return text.replace(old, 'w.private_shops,*w.catalog->private_shops,phase,w.catalog->private_shop_daily_rent.get(),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr}')
    if path.endswith('resident_world.cpp'):
        text = once(text, '#include "private_shop_integration.hpp"', '#include "private_shop_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        text = once(text, 'const DiscipleControlState* disciple_bindings_before){', 'const DiscipleControlState* disciple_bindings_before,const PrivateShopDailyRentState* daily_rent_before){')
        text = once(text, 'current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr);', 'current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr,current.private_shop_daily_rent_enabled()?&current.private_shop_daily_rent:nullptr);')
        anchor = '    auto estate_orders_bound=bind_task_shop_order_estate_result_world'
        block = '    // 仅真实提交前后日租账差分可绑定，原Common结果和history已分配后才写永久来源。\n    if(w.private_shop_daily_rent_enabled()&&!daily_rent_before)return common_failure("PrivateShopDailyRentBeforeStateRequired");const PrivateShopDailyRentState empty_daily_rent;auto daily_rent_bound=bind_private_shop_daily_rent_result_world(w,daily_rent_before?*daily_rent_before:empty_daily_rent,actor,sequence,*receipt);if(!daily_rent_bound.ok)return daily_rent_bound;\n'
        return once(text, anchor, block + anchor)
    if path.endswith('local_economy_model.hpp'):
        line = next(x for x in text.splitlines() if x.startswith('Outcome<LocalEconomyEffects> step_local_economy_in_candidate('))
        extended = line[:-2] + ',const PrivateShopResponsibilitySourceContext*);'
        return once(text, line, line + '\n// 私约仅原业务应计/支付；同候选具名来源证明后宏观LE才跳过这些精确责任。\nstruct PrivateShopResponsibilitySourceContext;\n' + extended)
    if path.endswith('local_economy_rules.cpp'):
        text = once(text, '#include "local_economy_internal.hpp"', '#include "local_economy_internal.hpp"\n#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"\n#include <set>')
        signature = next(x for x in text.splitlines() if x.startswith('Outcome<LocalEconomyEffects> step_body('))
        text = once(text, signature, signature.replace('MarketBusinessCaptureServices* market){', 'MarketBusinessCaptureServices* market,const std::set<StableId>& private_obligations){'))
        text = once(text, 'auto& obligation=row.second;if(employment&&', 'auto& obligation=row.second;if(private_obligations.count(row.first))continue;if(employment&&')
        signature = next(x for x in text.splitlines() if x.startswith('Outcome<LocalEconomyEffects> step_with_market('))
        text = once(text, signature, signature.replace('bool own_savepoint){', 'bool own_savepoint,const PrivateShopResponsibilitySourceContext* shops){') + '\n    std::set<StableId> private_obligations;if(shops){if(&shops->common!=&c||&shops->economy!=&s||&shops->economy_catalog!=&catalog||shops->phase!=&input)return {false,{},common_failure("PrivateShopEconomyPhaseContextMismatch").error};auto source=read_private_shop_economy_source_ids(*shops);if(!source.ok)return {false,{},source.error};private_obligations.insert(source.value.obligation_ids.begin(),source.value.obligation_ids.end());}')
        text = once(text, 'if(!catalog.enabled){if(disruption||takeover)', 'if(!catalog.enabled){if(disruption||takeover||shops)')
        text = once(text, 'execution_context?&*execution_context:nullptr,employment,market);', 'execution_context?&*execution_context:nullptr,employment,market,private_obligations);')
        text = once(text, 'takeover,employment,nullptr,true);', 'takeover,employment,nullptr,true,nullptr);')
        line = next(x for x in text.splitlines() if x.startswith('Outcome<LocalEconomyEffects> step_local_economy_in_candidate('))
        original_signature = line[:line.index('{')]
        extended = line.replace('MarketBusinessCaptureServices* market){', 'MarketBusinessCaptureServices* market,const PrivateShopResponsibilitySourceContext* shops){').replace('takeover,employment,market,false);', 'takeover,employment,market,false,shops);')
        wrapper = original_signature + '{return step_local_economy_in_candidate(c,s,catalog,organizations,activity,input,household_management,disruption,takeover,employment,market,nullptr);}'
        return once(text, line, wrapper + '\n' + extended)
    if path.endswith('model_contract.hpp'):
        text = once(text, 'struct PrivateShopCatalog;',
                    'struct PrivateShopDailyRentCatalog;struct PrivateShopDailyRentState;struct PrivateShopCatalog;')
        anchor = '    std::shared_ptr<const PrivateShopCatalog> private_shops;'
        return once(text, anchor, anchor + '\n    std::shared_ptr<const PrivateShopDailyRentCatalog> private_shop_daily_rent;')
    if path.endswith('common_model.hpp'):
        anchor = '    std::shared_ptr<const PrivateShopState> private_shops;'
        return once(text, anchor, anchor + '\n    std::shared_ptr<const PrivateShopDailyRentState> private_shop_daily_rent;')
    if path.endswith('world_bundle.hpp'):
        text = once(text, '#include "private_shop_liquidation_integration.hpp"',
                    '#include "private_shop_liquidation_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        anchor = '    PrivateShopState private_shops;bool private_shops_seed_complete=false;'
        text = once(text, anchor, anchor + '\n    PrivateShopDailyRentState private_shop_daily_rent;bool private_shop_daily_rent_seed_complete=false;\n    bool private_shop_daily_rent_enabled()const{return bool(catalog->private_shop_daily_rent);}')
        return once(text, 'const DiscipleControlState* disciple_bindings_before=nullptr);', 'const DiscipleControlState* disciple_bindings_before=nullptr,const PrivateShopDailyRentState* daily_rent_before=nullptr);')
    if path.endswith('private_shop_integration.hpp'):
        text = once(text, 'struct WorldBundle;', 'struct WorldBundle;class PrivateShopRentAuthorityWorldAccess;')
        text = once(text, ' PrivateShopWorldAccess(const PrivateShopWorldAccess&)=delete;',
                    ' ~PrivateShopWorldAccess();\n PrivateShopWorldAccess(const PrivateShopWorldAccess&)=delete;')
        text = once(text, 'PrivateShopWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalShopLicensingWorldAccess>,const DynamicStepInput*);',
                    'PrivateShopWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalShopLicensingWorldAccess>,std::unique_ptr<PrivateShopRentAuthorityWorldAccess>,const DynamicStepInput*);')
        return once(text, ' GrowthSkillOriginContext skill_origins_;',
                    ' GrowthSkillOriginContext skill_origins_;\n std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent_authority_;')
    if path.endswith('private_shop_integration.cpp'):
        text = once(text, '#include "private_shop_integration.hpp"',
                    '#include "private_shop_integration.hpp"\n#include "private_shop_daily_rent_integration.hpp"')
        text = once(text, 'std::unique_ptr<PoliticalShopLicensingWorldAccess> law,const DynamicStepInput* phase)',
                    'std::unique_ptr<PoliticalShopLicensingWorldAccess> law,std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent,const DynamicStepInput* phase)')
        text = once(text, 'w.family_birth_enabled()?&w.family_birth:nullptr},',
                    'w.family_birth_enabled()?&w.family_birth:nullptr},rent_authority_(std::move(rent)),')
        text = once(text, 'w.catalog->estate.get(),&skill_origins_},meeting_',
                    'w.catalog->estate.get(),&skill_origins_,w.catalog->private_shop_daily_rent.get(),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr,rent_authority_?rent_authority_->executors():nullptr,rent_authority_?rent_authority_->death_origin():FamilyDeathOriginRead{}},meeting_')
        text = once(text, 'Outcome<std::unique_ptr<PrivateShopWorldAccess>> PrivateShopWorldAccess::create',
                    'PrivateShopWorldAccess::~PrivateShopWorldAccess()=default;\nOutcome<std::unique_ptr<PrivateShopWorldAccess>> PrivateShopWorldAccess::create')
        text = once(text, 'return {true,std::unique_ptr<PrivateShopWorldAccess>{new PrivateShopWorldAccess(w,std::move(law.value),phase)},{}};',
                    'std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent;if(w.private_shop_daily_rent_enabled()){auto read=PrivateShopRentAuthorityWorldAccess::create(w,phase);if(!read.ok)return {false,{},read.error};rent=std::move(read.value);}return {true,std::unique_ptr<PrivateShopWorldAccess>{new PrivateShopWorldAccess(w,std::move(law.value),std::move(rent),phase)},{}};')
        text = once(text, 'std::get<PrivateShopOpeningQuote>(current.value.data));',
                    'std::get<PrivateShopOpeningQuote>(current.value.data),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr);')
        return once(text, 'w.tasks.shop_orders.emplace();}return common_success();}',
                    'w.tasks.shop_orders.emplace();}return initialize_private_shop_daily_rent_world_in_candidate(w);}')
    raise ValueError(path)


def blob(text):
    """Git blob OID用于共享前后像核对，不把WIP候选冒原提交字节。"""
    raw = text.encode()
    return hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()


def main():
    """生成器输出目录必须新建；三生产者/Codec另由中央持有。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', required=True)
    parser.add_argument('--base', default=BASE)
    parser.add_argument('--source')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    repo, output = Path(args.repo), Path(args.output)
    output.mkdir(parents=True, exist_ok=False)
    rows, patch = [], []
    for path in PATHS:
        before = subprocess.check_output(['git', '-C', str(repo), 'show', args.base + ':' + path], text=True)
        source = Path(args.source) / path if args.source else None
        text = source.read_text() if source and source.exists() else before
        after = transform(path, text)
        target = output / 'candidate' / path
        target.parent.mkdir(parents=True, exist_ok=True); target.write_text(after)
        rows.append({'path': path, 'base_oid': blob(before), 'input_oid': blob(text), 'after_oid': blob(after)})
        patch.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True), 'a/' + path, 'b/' + path))
    (output / 'candidate.patch').write_text(''.join(patch))
    (output / 'manifest.json').write_text(json.dumps({'base': args.base, 'status': 'COMPLETE_S5_SOURCE_CANDIDATE_EXTERNAL_THREE_PRODUCERS_PENDING',
        'shared': rows, 'missing': ['central three-producer/codec external closure', 'same-generation behavior gates']}, indent=2) + '\n')
    print(output / 'manifest.json')


if __name__ == '__main__':
    main()
