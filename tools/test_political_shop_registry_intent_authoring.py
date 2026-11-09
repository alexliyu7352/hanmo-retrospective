"""只核作者源准入，无世界状态、批准或NPC执行注入。"""
import copy
import unittest
from content.political_shop_registry_intent_links import check_political_shop_registry_intents

class RegistryIntentAuthoring(unittest.TestCase):
    def setUp(self):
        self.pack={
            'required_capabilities':['political_shop_registry_intents_v1','political_shop_registry_successions_v1'],
            'actors':[{'id':'judge'}],
            'scenarios':[{'dynamic_world':{
                'profile':'local_society_v1', 'political_takeover':{'enabled':True},
                'political_shop_licensing':{'version':1}, 'private_shops':{'enabled':True},
                'political_shop_registry_successions':{'enabled':True,'version':1,'policies':{'registry':{'court_id':'court','organization_id':'new'}}},
                'director':{
                    'subjects':{'judge':{'organization_id':'new','templates':['shop_registry_acceptance'],'shop_registry_intent_id':'intent'}},
                    'political_shop_registry_intents':{'version':1,'intents':{'intent':{'id':'intent','actor_id':'judge','registry_policy_id':'registry'}}}
                }
            }}]
        }
        self.d=self.pack['scenarios'][0]['dynamic_world'];self.cfg=self.d['director']['political_shop_registry_intents']
    def errors(self):return check_political_shop_registry_intents(self.pack,{})
    def test_explicit(self):self.assertEqual(self.errors(),[])
    def test_legacy_unchanged(self):self.d['director']={};self.assertEqual(self.errors(),[])
    def test_missing_marker(self):del self.d['political_shop_registry_successions'];self.assertTrue(self.errors())
    def test_capability(self):self.pack['required_capabilities'].pop(0);self.assertTrue(self.errors())
    def test_unknown_actor(self):self.cfg['intents']['intent']['actor_id']='unknown';self.assertTrue(self.errors())
    def test_key(self):self.cfg['intents']['intent']['id']='other';self.assertTrue(self.errors())
    def test_boolean_version(self):self.cfg['version']=True;self.assertTrue(self.errors())
    def test_unknown_registry(self):self.cfg['intents']['intent']['registry_policy_id']='unknown';self.assertTrue(self.errors())
    def test_duplicate_scope(self):other=copy.deepcopy(self.cfg['intents']['intent']);other['id']='second';self.cfg['intents']['second']=other;self.assertTrue(self.errors())
    def test_different_subject(self):self.d['director']['subjects']['other']=self.d['director']['subjects'].pop('judge');self.assertTrue(self.errors())
    def test_wrong_organization(self):self.d['director']['subjects']['judge']['organization_id']='old';self.assertTrue(self.errors())
    def test_dynamic_fixed_person(self):self.d['director']['registry_version']=1;self.d['director']['policies']={'members':self.d['director'].pop('subjects')['judge']};self.assertEqual(self.errors(),[])
    def test_template_without_ref(self):del self.d['director']['subjects']['judge']['shop_registry_intent_id'];self.assertTrue(self.errors())
    def test_unselected(self):self.d['director']['subjects']={};self.assertTrue(self.errors())
if __name__=='__main__':unittest.main()
