"""无素材导出、无未来人物或成功凭据的纯作者合同门。"""
import copy
import unittest
from tools.content.born_personal_dialogue_links import check_born_personal_dialogues,born_personal_dialogue_bindings

def fixture():
    dialogue={'id':'child.talk','context_role':'','entry':'hello','nodes':[{'id':'hello','kind':'say','speaker':'$provider','next':'menu'},{'id':'menu','kind':'choice','options':[{'id':'accept','next':'accept','conditions':[]},{'id':'revoke','next':'revoke','conditions':[]}]},{'id':'accept','kind':'say','speaker':'$provider','next':'end'},{'id':'revoke','kind':'say','speaker':'$provider','next':'end'},{'id':'end','kind':'end'}]}
    block={'version':1,'templates':[{'id':'child','prototype_id':'child.prototype','dialogue_id':'child.talk'}]}
    scene={'dynamic_world':{'profile':'local_society_v1','born_personal_dialogues':block},'family_birth':{'arrangements':{'a':{'terms':{'child_prototype_id':'child.prototype'}}}},'family_lineage':{},'family_life':{},'enabled':{'actors':[],'dialogues':['child.talk']}}
    return {'required_capabilities':['born_personal_dialogues_v1','family_birth_v1','dialogue','multi_actor','native_dynamic_world'],'scenarios':[scene]}, {'actors':{'child.prototype':{}},'dialogues':{'child.talk':dialogue}}

class BornPersonalAuthor(unittest.TestCase):
    def test_explicit_source(self):
        p,i=fixture();self.assertEqual(check_born_personal_dialogues(p,i),[]);self.assertEqual(born_personal_dialogue_bindings(p),{'child.talk'})
    def test_legacy_unchanged(self):
        self.assertEqual(check_born_personal_dialogues({'scenarios':[{}]},{}),[])
    def test_actual_provider(self):
        p,i=fixture();i['dialogues']['child.talk']['nodes'][2]['speaker']='child.prototype';self.assertTrue(check_born_personal_dialogues(p,i))
    def test_no_automatic_command(self):
        p,i=fixture();i['dialogues']['child.talk']['nodes'][2]['command']={'kind':'family_legacy_accept'};self.assertTrue(check_born_personal_dialogues(p,i))
    def test_actual_birth_policy(self):
        p,i=fixture();p['scenarios'][0]['family_birth']['arrangements']={};self.assertTrue(check_born_personal_dialogues(p,i))
    def test_enabled_dialogue(self):
        p,i=fixture();p['scenarios'][0]['enabled']['dialogues']=[];self.assertTrue(check_born_personal_dialogues(p,i))
    def test_unreachable_consent(self):
        p,i=fixture();i['dialogues']['child.talk']['nodes'].append({'id':'hidden','kind':'say','speaker':'$provider','next':'end'});self.assertTrue(check_born_personal_dialogues(p,i))
    def test_wrong_profile_type(self):
        p,i=fixture();p['scenarios'][0]['dynamic_world']['born_personal_dialogues']['version']=True;self.assertTrue(check_born_personal_dialogues(p,i))
    def test_required_caps(self):
        p,i=fixture();p['required_capabilities'].remove('family_birth_v1');self.assertTrue(check_born_personal_dialogues(p,i))

if __name__=='__main__':unittest.main()
