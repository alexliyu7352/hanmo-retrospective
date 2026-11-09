"""用实际四包作者原件核独立人物卡增量；公共作者注册尚未接通。"""
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('actor_cards', ROOT / 'tools/content/actor_cards.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--catalog',type=Path,required=True);args=parser.parse_args()
    raw=args.catalog.read_bytes();source=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()
    fragment=json.loads((ROOT/'docs/native/system-character-life/actor-cards/author-slice.json').read_text())
    assert source==fragment['base_catalog_git_oid']
    original=json.loads(raw)['catalog'];working=deepcopy(original);working['texts'].update(fragment['texts'])
    scenario=deepcopy(working['scenarios'][fragment['base_scenario_id']]);scenario['id']=fragment['scenario_id'];scenario['actor_card_unlocks']=fragment['actor_card_unlocks']
    pack={'actor_cards':fragment['actor_cards'],'scenarios':[scenario]};checks=0
    def check(value,message):
        nonlocal checks
        checks+=1;assert value,message
    def errors(value):return [row[0] for row in MODULE.check_actor_cards(value,working)]
    check(not errors(pack),'actual three-person author scenario accepts explicit two-procurement policy')
    bad=deepcopy(pack);bad['scenarios'][0]['dynamic_world'].pop('founder_policy');check('ACTOR_CARDS_ROSTER_POLICY' in errors(bad),'collection cannot create a roster policy implicitly')
    bad=deepcopy(pack);bad['actor_cards'][0]['acquire_condition']['kind']='two_messages_without_embezzlement';check('ACTOR_CARD_CONDITION' in errors(bad),'unsupported innocence cannot be invented')
    bad=deepcopy(pack);bad['actor_cards'][0]['effect_params']['target_actor_id']='absent';check('ACTOR_CARD_TARGET' in errors(bad),'target must be real authored actor')
    bad=deepcopy(pack);bad['scenarios'][0]['actor_card_unlocks']['initially_available_actor_ids'].remove(scenario['player_actor']);check('ACTOR_CARD_INITIAL' in errors(bad),'default player remains available')
    bad=deepcopy(pack);bad['scenarios'][0]['actor_card_unlocks']['initially_available_actor_ids']=[scenario['player_actor']];check('ACTOR_CARD_COVERAGE' in errors(bad),'every noninitial roster actor needs actual card')
    bad=deepcopy(pack);bad['scenarios'][0]['actor_card_unlocks']['card_ids']*=2;check('ACTOR_CARD_IDS' in errors(bad),'duplicate card selection refused')
    bad=deepcopy(pack);bad['scenarios'][0]['actor_card_unlocks']['free_unlock_all']=True;check('ACTOR_CARDS_PROFILE' in errors(bad),'no loose author override of qualification')
    bad=deepcopy(pack);bad['actor_cards'][0]['prerequisites']={'kind':'always'};check('ACTOR_CARD_PREREQUISITE' in errors(bad),'author cannot bypass real task completion')
    bad=deepcopy(pack);bad['scenarios'][0]['enabled']['actors'].remove('society::actor.city_steward');check('ACTOR_CARD_TARGET' in errors(bad),'disabled target cannot acquire new start qualification')
    bad=deepcopy(pack);bad['scenarios'][0]['actor_card_unlocks']['card_ids']=[];check('ACTOR_CARD_IDS' in errors(bad),'empty selected catalog rejected')
    legacy=deepcopy(pack);legacy['scenarios'][0].pop('actor_card_unlocks');check(not errors(legacy),'passive definitions do not enable old profile')
    check(original==json.loads(raw)['catalog'] and raw==args.catalog.read_bytes(),'author identity assets and duties are unchanged')
    print(f'ACTOR_CARDS_AUTHOR {checks} checks 0 failures; base {source}; PIPELINE_NOT_REGISTERED')
if __name__=='__main__':main()
