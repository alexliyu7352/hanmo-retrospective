"""NPC续签作者仅选择明确单源方案；旧默认和来源失败不被自动修补。"""
import copy
import unittest
from content.political_supply_renewal_links import check_political_supply_renewals


def fixture():
    """有限目录样本不预制Common批准、任务或政策事实。"""
    return {'required_capabilities':['political_supply_renewals_v1','economic_supply_policies_v1'], 'scenarios':[{'dynamic_world':{
        'profile':'local_society_v1','political_takeover':{'enabled':True,'rules':{'rule':{'successor_office_id':'office'}}},
        'organization':{'offices':{'office':{'organization_id':'successor'}}},
        'economic_supply_policies':{'version':1,'policies':{'policy':{'organization_id':'successor','original_source_key':'original'}}},
        'director':{'registry_version':1,'policies':{'npc':{'organization_id':'successor','templates':['political_supply_renewal'],'supply_renewal_intent_id':'intent'}},
                    'political_supply_renewals':{'version':1,'intents':{'intent':{'id':'intent','takeover_rule_id':'rule','author_policy_id':'policy'}}}}
    }}]}


class Authoring(unittest.TestCase):
    """引用错配、未知字段、缺显式profile都必须在正规作者门拒绝。"""
    def test_explicit(self):
        self.assertEqual(check_political_supply_renewals(fixture(),{}),[])
    def test_old_default(self):
        self.assertEqual(check_political_supply_renewals({'scenarios':[{'dynamic_world':{'director':{}}}]},{}),[])
    def test_reject_boundaries(self):
        mutations=[lambda d:d['director']['political_supply_renewals'].update(version=2),
                   lambda d:d['director']['political_supply_renewals'].update(runtime_approval='fake'),
                   lambda d:d['director']['policies']['npc'].update(supply_renewal_intent_id='missing'),
                   lambda d:d['organization']['offices']['office'].update(organization_id='other'),
                   lambda d:d['director'].update(registry_version=0),
                   lambda d:d.pop('economic_supply_policies'),
                   lambda d:d['director']['policies']['npc'].update(templates=['await_adjustment'])]
        for mutate in mutations:
            sample=fixture();mutate(sample['scenarios'][0]['dynamic_world'])
            with self.subTest(mutate=mutate):self.assertTrue(check_political_supply_renewals(sample,{}))
    def test_competing_source(self):
        sample=fixture();intents=sample['scenarios'][0]['dynamic_world']['director']['political_supply_renewals']['intents']
        intents['other']=dict(intents['intent'],id='other');self.assertTrue(check_political_supply_renewals(sample,{}))
    def test_capability(self):
        sample=fixture();sample['required_capabilities'].remove('political_supply_renewals_v1');self.assertTrue(check_political_supply_renewals(sample,{}))


if __name__=='__main__':unittest.main()
