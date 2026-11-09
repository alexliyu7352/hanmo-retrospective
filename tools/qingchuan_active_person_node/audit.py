"""从真实Main回执、原生全态和有限资产快照审计本人归属及结算；不生成可运行世界。"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

ACTOR = 'society::actor.study_swordswoman'
CHENG = 'society::actor.apprentice'
SWORD = 'society::item.practice_sword'


def read(path: Path):
    """原件只读，缺文件直接失败，不能补默认空数据制造通过。"""
    return json.loads(path.read_text())


def sha(path: Path) -> str:
    """报告只引用大原件摘要，避免重复保全完整world。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Audit:
    """独立只读检查收集可定位失败，报告同时保留成功证据与未通过项。"""
    def __init__(self):
        self.checks = []

    def check(self, value: bool, label: str) -> None:
        """每项检查保留真实布尔值，失败不能通过后续覆盖消失。"""
        self.checks.append({'check': label, 'ok': bool(value)})

    def ownership(self, state: dict, initial: dict, label: str) -> None:
        """控制者、财物和旧身份归属沿持久ID核对，不把目前玩家解释成新人物。"""
        person = state['actors'][ACTOR]
        self.check(state['player_actor_id'] == ACTOR, label+': current actor')
        self.check(ACTOR not in state['director']['schedules'] and ACTOR not in state['director']['plans'], label+': no NPC scheduling for player')
        for key in ['id', 'birth_tick', 'created_tick', 'appearance', 'wallet_id', 'inventory_id', 'attributes']:
            self.check(person[key] == initial['actors'][ACTOR][key], label+': Shen original '+key)
            self.check(state['actors'][CHENG][key] == initial['actors'][CHENG][key], label+': Cheng original '+key)
        for who in [ACTOR, CHENG]:
            actor = state['actors'][who]
            self.check(state['wallets'][actor['wallet_id']]['owner'] == {'kind':'actor','id':who}, label+': wallet owner '+who)
            self.check(state['inventories'][actor['inventory_id']]['owner'] == {'kind':'actor','id':who}, label+': inventory owner '+who)
        for estate_id, estate in initial['estates'].items():
            for key in ['owner_actor_id', 'manager_actor_id']:
                if key in estate:
                    self.check(state['estates'][estate_id][key] == estate[key], label+': existing estate responsibility '+estate_id+'/'+key)

    def commands(self, rows: list, state: dict, label: str, prior_states: list | None = None) -> None:
        """每条实际成功回执同时与持久results和history匹配，actor由权威历史证明。"""
        snapshots = (prior_states or []) + [state]
        by_id = {result['result_id']:(key,result) for snapshot in snapshots for key,result in snapshot['results'].items()}
        history = {row['result_id']:row for snapshot in snapshots for row in snapshot['history']}
        seen = set()
        for index, row in enumerate(rows):
            receipt = row['result']
            self.check(receipt.get('ok') is True, f'{label}: command {index} accepted')
            result_id = receipt.get('result_id')
            self.check(result_id not in seen, f'{label}: command {index} unique result')
            seen.add(result_id)
            recorded = by_id.get(result_id)
            self.check(recorded is not None and recorded[0].startswith(ACTOR+':') and recorded[1] == receipt,
                       f'{label}: command {index} exact persistent receipt and actor sequence')
            item = history.get(result_id, {})
            self.check(item.get('actor_id') == ACTOR and item.get('kind') == row['kind'],
                       f'{label}: command {index} authoritative history actor')
            self.check(row['after']['revision'] == receipt.get('revision') and row['after']['tick'] == receipt.get('tick'),
                       f'{label}: command {index} actual source receipt')


def audit_payments(audit: Audit, steps: list, commands: list, saved: dict, catalog: dict) -> list:
    """每笔资金从当前活报价、正式回执与双方有限快照核实，另列同tick原有公共俸禄。"""
    actor_wallet = saved['actors'][ACTOR]['wallet_id']
    ledger = []
    player_change = 0
    for step in steps:
        before, after = step.get('assets'), step.get('after', {}).get('assets')
        if not before or not after:
            continue
        delta = after['wallets'][actor_wallet]['balance']-before['wallets'][actor_wallet]['balance']
        if not delta:
            continue
        receipts = [row['result'] for row in commands[step['command_count']:step['after']['command_count']]]
        receipt_ids = [row.get('result_id') for row in receipts]
        quote = next((row['metadata']['regional_quote'] for row in step['visible']
                      if row.get('metadata', {}).get('command_kind') == step['kind'] and 'regional_quote' in row['metadata']), {})
        command = step['interaction'].get('command', {})
        counterparty = ''
        evidence = {}
        expected = None
        if command.get('kind') == 'service' and command.get('activity') == 'work':
            expected = quote['wage']-quote['cost']; counterparty = quote['terms']['wallet_id']
            evidence = {'quote':quote, 'kind':'actual_work'}
        elif step['kind'] == 'trade':
            trade_receipts = [fact for fact in saved['facts'].values() if fact.get('kind')=='life_activity'
                and fact['view']['actor_id']==ACTOR and fact['view']['activity']=='buy'
                and fact['view']['start_tick']==before['tick']]
            audit.check(len(trade_receipts)==1, 'one real purchase source for step '+str(step['step']))
            if len(trade_receipts)==1:
                fact=trade_receipts[0]; expected=-fact['view']['amount']; counterparty=fact['view']['payee_wallet_id']
                evidence={'kind':'actual_purchase','receipt':fact}
                item_id=fact['view']['item_id']; quantity=fact['view']['quantity']
                for inventory_id, change in [(fact['view']['to_inventory_id'],quantity),(fact['view']['from_inventory_id'],-quantity)]:
                    prior=sum(lot['quantity'] for lot in before['inventories'][inventory_id]['lots'] if lot['item_id']==item_id)
                    later=sum(lot['quantity'] for lot in after['inventories'][inventory_id]['lots'] if lot['item_id']==item_id)
                    audit.check(later-prior==change,'actual bilateral item movement '+str(step['step'])+'/'+inventory_id)
        elif command.get('kind') == 'learn_skill':
            terms=quote.get('training',{}); expected=-terms.get('fee',0); counterparty=terms.get('recipient_wallet_id','')
            evidence={'kind':'actual_training_fee','quote':quote}
            audit.check(terms.get('student_id')==ACTOR and terms.get('teacher_id')==step['interaction']['context']['provider_actor_id'], 'training actual teacher and student '+str(step['step']))
        elif command.get('kind') == 'duel_start':
            expected=-quote.get('fee',0); counterparty=quote.get('wallet_id','')
            evidence={'kind':'actual_sparring_fee','quote':quote}
            audit.check(quote.get('controller_id')==ACTOR, 'sparring quote actual controller '+str(step['step']))
        elif step['kind']=='advance' and before['actor']['location']['kind']=='OnRoute':
            edge=before['actor']['location']['edge_id']; route=catalog['routes'][edge]
            prior_map=next(row for row in reversed(steps[:steps.index(step)]) if row['kind']=='map_confirm')
            quote_text=next(row['text'] for row in prior_map['visible'] if '余程费用' in row.get('text',''))
            match=re.search(r'余程费用 ([0-9]+)钱',quote_text)
            audit.check(bool(match) and route['duration_ticks']==1 and after['tick']-before['tick']==1,'actual one-half-day route and visible finite cost')
            expected=-int(match.group(1)) if match else None
            evidence={'kind':'actual_route_tick','route_id':edge,'authored_route':route,'actual_visible_quote':quote_text}
        audit.check(expected is not None and delta==expected,'player delta grounded in actual action terms step '+str(step['step']))
        audit.check(bool(receipts) and all(row.get('ok') is True for row in receipts),'payment has actual accepted receipts step '+str(step['step']))
        peers=[]
        if counterparty:
            other_delta=after['wallets'][counterparty]['balance']-before['wallets'][counterparty]['balance']
            unrelated=0
            for payment in saved['local_economy']['payments'].values():
                if before['tick']<payment['tick']<=after['tick']:
                    amount=(payment['amount'] if payment['to_wallet_id']==counterparty else 0)-(payment['amount'] if payment['from_wallet_id']==counterparty else 0)
                    if amount: unrelated+=amount; peers.append(payment)
            audit.check(other_delta == -delta+unrelated,'counterparty delta with separate original obligations step '+str(step['step']))
            evidence.update(counterparty_wallet=counterparty,counterparty_delta=other_delta,other_obligation_receipts=peers)
        player_change+=delta
        ledger.append({'step':step['step'],'player_wallet':actor_wallet,'before':before['wallets'][actor_wallet]['balance'],
                       'after':after['wallets'][actor_wallet]['balance'],'delta':delta,'result_ids':receipt_ids,'evidence':evidence})
    audit.check(player_change==saved['wallets'][actor_wallet]['balance']-8,'all actual private wallet changes accounted by distinct observed payments')
    return ledger


def main() -> None:
    """仅审计本片producer和独立consumer；不读取旧tick0存档或历史矩阵结论。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--consumer', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    producer, consumer = args.producer.resolve(), args.consumer.resolve()
    audit = Audit()
    initial, saved, final = (read(path) for path in [producer/'initial-world.json',producer/'saved-world.json',consumer/'final-world.json'])
    catalog = read(producer/'expanded-catalog.json')['data']
    authored = catalog['scenarios'][initial['scenario_id']]['initial']
    people = {person['actor_id']:person for person in authored['actors']}
    for who in [ACTOR, CHENG]:
        for key in ['attributes', 'skills', 'alive', 'available']:
            audit.check(initial['actors'][who][key] == people[who][key], 'fresh actual person has authored '+who+'/'+key)
        audit.check(initial['actors'][who]['birth_tick'] == -720*people[who]['age_years'], 'fresh actual authored age '+who)
    audit.check(initial['tick'] == 0 and saved['tick'] > 0, 'actual fresh start progressed before saved bank')
    audit.check(initial['wallets'][initial['actors'][ACTOR]['wallet_id']]['balance'] == 8, 'original eight money observed only as initial state')
    for label, state in [('initial',initial),('saved',saved),('continued',final)]: audit.ownership(state,initial,label)
    producer_commands, consumer_commands = read(producer/'commands.json'), read(consumer/'commands.json')
    prior_states=[read(path) for path in sorted(producer.glob('work-*-state.json'))]
    audit.commands(producer_commands,saved,'producer',prior_states)
    audit.commands(consumer_commands,final,'consumer')
    audit.check(len(producer_commands) == saved['request_sequences'][ACTOR], 'all producer player request sequences observed')
    audit.check(len(consumer_commands) == final['request_sequences'][ACTOR]-saved['request_sequences'][ACTOR], 'all continuation request sequences observed')
    life = []
    for fact_id, fact in saved['facts'].items():
        if fact.get('kind') != 'life_activity' or fact['view']['actor_id'] != ACTOR: continue
        view, terms = fact['view'], fact['terms']
        audit.check(view['fact_id'] == fact_id == view['action_id'] and view['provider_actor_id'] != ACTOR,
                    'actual distinct counterparty and fact/action '+fact_id)
        audit.check(view['participant_actor_ids'] == [ACTOR,view['provider_actor_id']], 'actual participants '+fact_id)
        audit.check(view['payer_wallet_id'] in saved['wallets'] and view['payee_wallet_id'] in saved['wallets'] and view['payer_wallet_id'] != view['payee_wallet_id'], 'actual separate wallets '+fact_id)
        if view['activity'] == 'work':
            audit.check(view['success'] and fact['wage_paid'] == terms['wage'] == view['amount'] and view['settlement_tick'] == view['completed_tick'], 'completed wage backed by frozen terms '+fact_id)
            audit.check(view['payee_wallet_id'] == initial['actors'][ACTOR]['wallet_id'] and view['payer_wallet_id'] == terms['wallet_id'], 'wage correct ownership '+fact_id)
        elif view['activity'] == 'buy':
            audit.check(terms['owner'] == {'kind':'actor','id':ACTOR} and view['to_inventory_id'] == initial['actors'][ACTOR]['inventory_id'], 'purchase private inventory owner '+fact_id)
            audit.check(view['item_id'] == terms['item_id'] and view['quantity'] == terms['quantity'] and view['amount'] == terms['total'], 'purchase exact item quantity amount '+fact_id)
        life.append({'fact_id':fact_id, 'view':view, 'terms':terms, 'cost_paid':fact['cost_paid'], 'wage_paid':fact['wage_paid']})
        audit.check(final['facts'].get(fact_id) == fact, 'continuation does not rewrite settled receipt '+fact_id)
    audit.check(any(row['view']['activity']=='work' and row['wage_paid']>0 for row in life), 'real completed wage exists')
    audit.check(any(row['view']['activity']=='buy' and row['view']['item_id']==SWORD for row in life), 'real sword purchase exists')
    actor = saved['actors'][ACTOR]
    audit.check(actor['combat']['weapon_id']==SWORD and final['actors'][ACTOR]['combat']['weapon_id']==SWORD, 'purchased weapon equipped on same person through result')
    owned = sum(lot['quantity'] for lot in saved['inventories'][actor['inventory_id']]['lots'] if lot['item_id']==SWORD and lot['owner']=={'kind':'actor','id':ACTOR})
    audit.check(owned>0, 'equipped sword actually owned in same personal inventory')
    audit.check(any(value>initial['actors'][ACTOR]['combat']['learned'].get(skill,-1) for skill,value in actor['combat']['learned'].items()), 'actual personal martial learning persisted')
    audit.check(saved['wallets']==final['wallets'] and saved['inventories']==final['inventories'], 'battle continuation pays no repeated wage fee or purchase')
    audit.check(saved['tick']==final['tick'], 'personal battle continuation does not secretly advance world half days')
    audit.check(sum(row['kind']=='battle_close' for row in consumer_commands)==1, 'actual battle result closed exactly once')
    final_battles = [battle for battle in final['battles'].values() if battle['controller_id']==ACTOR]
    audit.check(len(final_battles)==1 and final_battles[0]['phase']=='closed' and final_battles[0]['applied'], 'one real battle outcome applied and closed')
    payments = audit_payments(audit,read(producer/'steps.json'),producer_commands,saved,catalog)
    report = {'format':'HM-QINGCHUAN-ACTIVE-AUDIT/1','ok':all(row['ok'] for row in audit.checks), 'checks':audit.checks,
              'failures':[row['check'] for row in audit.checks if not row['ok']], 'life_receipts':life, 'payments':payments,
              'battle_outcomes':[{'id':battle['id'],'outcome':battle['outcome'],'round':battle['round'],'result_id':battle['result_id']} for battle in final_battles],
              'initial_money':initial['wallets'][actor['wallet_id']]['balance'], 'saved_money':saved['wallets'][actor['wallet_id']]['balance'],
              'source_files':{str(path):sha(path) for path in [producer/'initial-world.json',producer/'saved-world.json',producer/'commands.json',consumer/'final-world.json',consumer/'commands.json']},
              'limits':['GUI and exported package excluded']}
    with args.output.open('x') as file: json.dump(report,file,ensure_ascii=False,indent=2); file.write('\n')
    print(json.dumps({'ok':report['ok'],'checks':len(audit.checks),'failures':report['failures'],'report':str(args.output)},ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__': main()
