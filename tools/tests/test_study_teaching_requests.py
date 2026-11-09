"""只读原Installed人物/真实普通课程，检查本域显式学生条款作者叶。"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from content.study_teaching_requests import PROFILE, check_study_teaching_requests

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--catalog',type=Path,required=True);args=parser.parse_args()
    raw=args.catalog.read_bytes();assert hashlib.sha256(raw).hexdigest()=='8f02ed2028bce4cb2659e04d300b5440eb02bcf9a8bdb106923b27f7abe9c62e'
    index=json.loads(raw)['data'];scenario=deepcopy(index['scenarios']['actorlife::scenario.qingchuan_procurement_life'])
    teacher='society::actor.sword_master';student='society::actor.apprentice';course='society::role.sword_master'
    role='lessons::role.student';dialogue='lessons::dialogue.request';request='lessons::request.martial'
    scenario['study_teaching_requests']={'profile':PROFILE,'request_ids':[request]}
    scenario['enabled']['roles'].append(role);scenario['enabled']['dialogues'].append(dialogue)
    row={'id':request,'teacher_actor_id':teacher,'student_actor_id':student,'skill_id':'martial','course_role_id':course,'dialogue_id':dialogue,'node_id':'ask','duration':2,'payment_policy':'student_personal_tuition_v1','tariff':'ordinary_next_level_v1'}
    pack={'study_teaching_requests':[row],'roles':[{'id':role,'initial_actor':student,'place_id':index['roles'][course]['place_id']}],'dialogues':[{'id':dialogue,'context_role':role,'nodes':[{'id':'ask','kind':'say'}]}],'scenarios':[scenario]}
    checks=0
    def check(ok,why):
        nonlocal checks
        checks+=1;assert ok,why
    def errors(value,source=index):return [r[0] for r in check_study_teaching_requests(value,source)]
    check(not errors(pack),f'original actual teacher course and named student finite payment request accepted: {errors(pack)}')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['duration']=True;check('STUDY_TEACHING_FIELDS' in errors(bad),'bool duration not integer')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['duration']=7;check('STUDY_TEACHING_DURATION' in errors(bad),'course duration bounded by original rule')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['tariff']='any_price';check('STUDY_TEACHING_PAYMENT' in errors(bad),'arbitrary tuition not accepted')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['accept_automatically']=True;check('STUDY_TEACHING_FIELDS' in errors(bad),'no automatic acceptance toggle')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['course_role_id']='society::role.tutor';check('STUDY_TEACHING_COURSE' in errors(bad),'facility source cannot impersonate real teacher course')
    bad=deepcopy(pack);bad['roles'][0]['place_id']='society::place.grain';check('STUDY_TEACHING_PLACE' in errors(bad),'student request location and actual course location agree')
    bad=deepcopy(pack);bad['dialogues'][0]['nodes'][0]['kind']='action';check('STUDY_TEACHING_NODE' in errors(bad),'first explicit ordinary request profile does not infer duration from action without typed ticks')
    bad=deepcopy(pack);bad['scenarios'][0]['enabled']['actors'].remove(student);check('STUDY_TEACHING_ACTORS' in errors(bad),'selected actual student required')
    bad=deepcopy(pack);bad['scenarios'][0]['enabled']['roles'].remove(course);check('STUDY_TEACHING_ROLE' in errors(bad),'actual teacher course must be enabled')
    bad=deepcopy(pack);bad['study_teaching_requests'].append(dict(row,id=request+'.duplicate'));bad['scenarios'][0]['study_teaching_requests']['request_ids'].append(request+'.duplicate');check('STUDY_TEACHING_BINDING' in errors(bad),'duplicate same node course binding rejected')
    bad=deepcopy(pack);bad['scenarios'][0]['study_teaching_requests']['profile']='authored_teaching_requests_v1';check('STUDY_TEACHING_PROFILE' in errors(bad),'old personal skill request profile cannot authorize tuition')
    bad=deepcopy(pack);bad['study_teaching_requests'][0]['skill_id']='medicine';check('STUDY_TEACHING_SKILL_PROFILE' in errors(bad),'old active skill profile cannot activate extended medicine')
    passive=deepcopy(index);passive['study_teaching_requests']={'passive.old':False};check(not errors(pack,passive),'unselected passive definition does not activate')
    legacy=deepcopy(pack);legacy['scenarios'][0].pop('study_teaching_requests');check(not errors(legacy),'passive directory does not upgrade old scenario')
    check(raw==args.catalog.read_bytes() and index==json.loads(raw)['data'],'original actor/course/person money source unchanged')
    print(f'STUDY_TEACHING_AUTHOR {checks} checks 0 failures; AUTHOR_LEAF_ONLY')
if __name__=='__main__':main()
