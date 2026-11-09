"""从明确的青川原作者索引生成独立任务小包；不更改依赖包、不创建世界。"""
from copy import deepcopy
from pathlib import Path
import argparse
import hashlib
import json

BASE_SHA256 = 'a8b9094bbeae19e638a5d305abf44132720b6f21982185b27ba33cff0cd0c4e9'
BASE_SCENARIO = 'society::scenario.qingchuan_spring_reserve'


def build_task_contacts(index):
    """复用原生情景与真实成年人物，只追加独立书信、同行和回访配置。"""
    data = index['catalog']
    scenario = deepcopy(data['scenarios'][BASE_SCENARIO])
    if scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1':
        raise ValueError('原生青川情景来源不匹配')
    capabilities = list(data['required_capabilities'])
    for capability in ['tasks.message', 'tasks.escort', 'companions']:
        if capability not in capabilities:
            capabilities.append(capability)
    source = {'format': 'HM-CONTENT', 'schema_version': 1, 'id': 'task_contacts',
              'version': '1.0.0', 'required_capabilities': capabilities, 'texts': {}, 'flags': {}}
    for table, rows in data.items():
        if isinstance(rows, dict) and table not in ['texts', 'flags']:
            source[table] = []
    texts = source['texts']

    def text(key, value):
        """文案使用本包稳定键，不改依赖包原有台词。"""
        texts[key] = value
        return key

    def say(ident, words, following='menu', scope='sender'):
        """说话只推进原对白节点，不授予任务、物品或人物意愿。"""
        return {'id': ident, 'kind': 'say', 'speaker': '$provider',
                'text_key': text('line.' + scope + '.' + ident, words), 'next': following}

    def option(ident, words, following, conditions=None):
        """重要决定仍进入具名action及原确认；普通退出不会生成永久拒绝。"""
        return {'id': ident, 'text_key': text('option.' + ident, words),
                'conditions': conditions or [], 'next': following}

    def action(ident, kind, success, offer=None):
        """只引用已有业务命令，不允许作者台词任意改世界字段。"""
        command = {'kind': kind}
        if offer:
            command['offer_id'] = offer
        return {'id': ident, 'kind': 'action', 'command': command,
                'on_success': success, 'on_failure': 'unavailable'}

    source['roles'] = [
        {'id': 'role.sender', 'name_key': text('name.role.sender', '学师的私事'),
         'organization_id': None, 'initial_actor': 'society::actor.tutor',
         'place_id': 'society::place.school', 'services': ['information', 'accept_offer', 'report_offer'],
         'dialogue_id': 'dialog.sender'},
        {'id': 'role.recipient', 'name_key': text('name.role.recipient', '剑院收信人'),
         'organization_id': None, 'initial_actor': 'society::actor.sect_steward',
         'place_id': 'society::place.sword_gate', 'services': ['deliver_message'],
         'dialogue_id': 'dialog.recipient'},
        {'id': 'role.companion', 'name_key': text('name.role.companion', '赴剑院的卫简先生'),
         'organization_id': None, 'initial_actor': 'society::actor.study_scholar',
         'place_id': 'society::place.school', 'services': ['information', 'join_party'],
         'companion_cost_per_tick': 6, 'companion_required_trust': 0, 'dialogue_id': 'dialog.companion'}]
    source['items'] = [{'id': 'item.letter', 'name_key': text('name.item.letter', '封好的问安书'),
                        'kind': 'message', 'base_price': 0, 'weight': 1}]
    source['personal_objectives'] = [{'id': 'objective.escort',
        'name_key': text('name.objective.escort', '保护卫简先生'), 'kind': 'escort',
        'max_rounds': 24, 'injury_on_defeat': 1, 'allow_withdrawal': True}]
    for ident, name, template, reward, deadline, goal in [
        ('offer.letter', '剑院问安书', 'message', 120, 16,
         {'kind': 'deliver_message', 'message_id': 'item.letter', 'receiver_actor': 'society::actor.sect_steward'}),
        ('offer.escort', '卫简赴剑院', 'escort', 180, 40,
         {'kind': 'escort', 'actor_id': 'society::actor.study_scholar',
          'destination_place_id': 'society::place.sword_gate', 'objective_id': 'objective.escort'})]:
        source['offers'].append({'id': ident, 'name_key': text('name.' + ident, name),
            'template': template, 'issuer': {'kind': 'actor', 'id': 'society::actor.tutor'},
            'discovery_sources': [{'kind': 'actor', 'id': 'society::actor.tutor'}],
            'accept_role': 'role.sender', 'report_role': 'role.sender',
            'funding_wallet': 'society::wallet.tutor', 'budget': 0, 'reward': reward,
            'deadline_ticks': deadline, 'accept_ticks': 1, 'goal': goal, 'on_unavailable': 'wait_or_leave'})
    nodes = [say('welcome', '你来得正好。我有一封信要送到剑院，卫简先生也想走这一程。两件事各有各的约，办哪一件，由你自己应承。'),
             {'id': 'menu', 'kind': 'choice', 'text_key': text('line.menu', '学师把封好的信放在案边，等你说明来意。'), 'options': [
                 option('ask_letter', '先问清送信的约定。', 'discover_letter'),
                 option('ask_escort', '卫简的行程怎样约定？', 'discover_escort'),
                 option('accept_letter', '这封信我来送。', 'accept_letter', [{'op': 'task_state', 'offer_id': 'offer.letter', 'state': 'known'}]),
                 option('accept_escort', '护送这一程，我应承。', 'accept_escort', [{'op': 'task_state', 'offer_id': 'offer.escort', 'state': 'known'}]),
                 option('leave', '先告辞，想清楚再来。', 'end')]},
             action('discover_letter', 'discover_offer', 'letter_terms', 'offer.letter'),
             action('discover_escort', 'discover_offer', 'escort_terms', 'offer.escort'),
             say('letter_terms', '亲手把信交给剑院执事，再回来找我回报。受领办完后十六个半日内回来，报酬一百二十钱；没交到本人手里，不能算送达。'),
             say('escort_terms', '陪卫简先生到剑院门前，人须平安抵达；再回来找我回报。受领办完后四十个半日内办妥，报酬一百八十钱。他是否同行，还须你当面同他说定。'),
             action('accept_letter', 'accept_offer', 'accepted_letter', 'offer.letter'),
             action('accept_escort', 'accept_offer', 'accepted_escort', 'offer.escort'),
             say('accepted_letter', '信已交托，认准封口和收信人。护送的事若还想问，我们再谈。'),
             say('accepted_escort', '护送的约已记下，动身前请同卫简本人说定同行。那封信若还没定，也可以继续问。'),
             say('unavailable', '眼下还办不成，请核清本人、凭据和实际条件；没有办成的事不算履约。'), {'id': 'end', 'kind': 'end'}]
    source['dialogues'].append({'id': 'dialog.sender', 'context_role': 'role.sender', 'entry': 'welcome', 'nodes': nodes})
    source['dialogues'].append({'id': 'dialog.recipient', 'context_role': 'role.recipient', 'entry': 'recipient_welcome', 'nodes': [
        say('recipient_welcome', '是学师托你送来的信么？请当面交给我。', 'recipient_menu'),
        {'id': 'recipient_menu', 'kind': 'choice', 'text_key': text('line.recipient_menu', '执事等你取出实在携着的信件。'), 'options': [
            option('deliver', '把受托书信亲手交付。', 'deliver', [{'op': 'task_state', 'offer_id': 'offer.letter', 'state': 'active'}]),
            option('recipient_leave', '先不打扰。', 'end')]},
        action('deliver', 'deliver_message', 'recipient_done', 'offer.letter'),
        say('recipient_done', '信已收下。请回去告诉学师，不必挂心。', 'end'),
        say('unavailable', '眼下还不能接下，请核清实际信件和约定。', 'end', 'recipient'), {'id': 'end', 'kind': 'end'}]})
    source['dialogues'].append({'id': 'dialog.companion', 'context_role': 'role.companion', 'entry': 'companion_welcome', 'nodes': [
        say('companion_welcome', '我愿与你走这一程。路上每半日六钱吃用，停下歇脚不收路费。先说清楚，再一道动身。', 'companion_menu'),
        {'id': 'companion_menu', 'kind': 'choice', 'text_key': text('line.companion_menu', '卫简拎着包袱，等你确认同行安排。'), 'options': [
            option('join', '按约同行，路上吃用由我照应。', 'join', [{'op': 'party_member', 'actor_id': 'society::actor.study_scholar', 'equals': False}]),
            option('companion_leave', '这次先不同行。', 'end')]},
        action('join', 'join_party', 'joined'), say('joined', '同行的约定已说定；走到安全的地方，再商量分开。', 'end'),
        say('unavailable', '此时还不能动身，等人手和安排都核清再说。', 'end', 'companion'), {'id': 'end', 'kind': 'end'}]})
    scenario['id'] = 'scenario.qingchuan_contacts'
    scenario['name_key'] = text('name.scenario', '青川书信与同行')
    scenario['seed_text'] = 'qingchuan-task-contacts-v1'
    # 只激活依赖包已有的成年教习；保其原身份、技能、所在与零余额，宅地居民不被搬离。
    scholar = data['actors']['society::actor.study_scholar']
    initial_scholar = {'actor_id': scholar['id'], 'alive': True, 'available': True,
                       'place_id': scholar['initial_place'], 'age_years': scholar['initial_age_years'],
                       'attributes': deepcopy(scholar['attributes']), 'skills': deepcopy(scholar['skills']),
                       'combat': deepcopy(scholar.get('combat', {})),
                       'initial_goods': deepcopy(scholar.get('initial_goods', [])),
                       'appearance_definition': deepcopy(scholar['appearance_definition'])}
    scenario['enabled']['actors'].append(scholar['id'])
    scenario['dynamic_world']['local_economy']['named_actors_excluded'].append(scholar['id'])
    scenario['initial']['actors'].append(initial_scholar)
    scholar_wallet = deepcopy(data['wallets']['society::wallet.study_scholar'])
    scenario['enabled']['wallets'].append(scholar_wallet['id'])
    scenario['initial']['wallets'].append(scholar_wallet)
    for table in ['roles', 'items', 'offers', 'dialogues', 'personal_objectives']:
        scenario['enabled'].setdefault(table, []).extend(row['id'] for row in source[table])
    scenario['initial']['offer_ids'].extend(row['id'] for row in source['offers'])
    scenario['initial']['roles'].extend({'role_id': row['id'], 'actor_id': row['initial_actor'], 'enabled': True} for row in source['roles'])
    source['scenarios'] = [scenario]
    manifest = {'format': 'HM-PACK', 'format_version': 1, 'id': 'hanmo.task_contacts', 'version': '1.0.0',
                'namespace': 'contacts', 'rules_api': {'id': 'hanmo.rules', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'},
                'dependencies': [{'id': 'hanmo.local_society', 'min_version': '1.11.0', 'max_version_exclusive': '1.12.0'}],
                'conflicts': [], 'required_capabilities': capabilities, 'content_entries': ['content.json'],
                'presentation_entries': [], 'resources': [], 'author': 'Hanmo project authors', 'license': 'NOASSERTION'}
    return source, manifest


def main():
    """生成前核原索引字节；输出仅限新包目录，正式compose与运行另行核验。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.base_catalog.read_bytes()
    if hashlib.sha256(raw).hexdigest() != BASE_SHA256:
        raise ValueError('原青川索引字节已变，须重新核定作者来源')
    source, manifest = build_task_contacts(json.loads(raw))
    args.output.mkdir(parents=True, exist_ok=True)
    for name, value in [('content.json', source), ('pack.json', manifest)]:
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'AUTHOR_SOURCE_ONLY', 'runtime_verified': False,
                      'base_sha256': BASE_SHA256, 'output': str(args.output)}))


if __name__ == '__main__':
    main()
