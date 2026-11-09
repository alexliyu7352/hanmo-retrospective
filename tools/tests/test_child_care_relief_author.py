"""作者只能提供原本人意愿，实际孩子、同意和粮耗必须由规则产生。"""
import copy
import json
import unittest
from pathlib import Path
from content.child_care_relief import check_child_care_relief

class ChildCareReliefAuthorTest(unittest.TestCase):
    """必要坏条款和授权范围门，不代表运行时接手已经发生。"""
    def setUp(self):
        """复用正常原人物作者，不种入孩子或死亡事实。"""
        self.data=json.loads((Path(__file__).resolve().parents[2]/'content/packs/authored/child_care_relief_1_0/content.json').read_text())
        self.scenario=self.data['scenarios'][0]
        self.config=self.scenario['family_care_relief']
        self.offer=next(iter(self.config['offers'].values()))
    def codes(self,index=None):
        """只取本域明确诊断，正常合包和资源校验另走公共入口。"""
        return {r[0] for r in check_child_care_relief(self.data,index or {})}
    def test_real_finite_offer(self):
        self.assertEqual(self.codes(),set())
        self.assertEqual(self.codes({'roles':self.data['roles'],'dialogues':self.data['dialogues']}),set())
    def test_offer_role_is_selected_and_instantiated(self):
        role=next(r for r in self.data["roles"] if r["dialogue_id"]==self.offer["dialogue_id"])
        self.assertIn(role["id"],self.scenario["enabled"]["roles"])
        self.assertIn({"role_id":role["id"],"actor_id":self.offer["provider_actor_id"],"enabled":True},self.scenario["initial"]["roles"])
    def test_no_seeded_child(self):
        self.config['child_actor_id']='actor:forged'
        self.assertIn('CARE_RELIEF_FIELDS',self.codes())
    def test_own_private_food_only(self):
        self.offer['terms']['source_inventory_id']='bag:society::actor.city_steward'
        self.assertIn('CARE_RELIEF_PRIVATE_FOOD',self.codes())
    def test_bad_reference_is_diagnostic(self):
        self.offer['provider_actor_id']={}
        self.assertIn('CARE_RELIEF_REFERENCE',self.codes())
    def test_period_is_bounded_integer(self):
        self.offer['terms']['prepaid_periods']=True
        self.assertIn('CARE_RELIEF_DURATION',self.codes())
    def test_scope_cannot_grant_guardianship(self):
        self.offer['scope']='permanent_guardian'
        self.assertIn('CARE_RELIEF_OFFER_FIELDS',self.codes())
    def test_selected_node_required(self):
        self.scenario['enabled']['dialogues'].remove(self.offer['dialogue_id'])
        self.assertIn('CARE_RELIEF_NODE',self.codes({'roles':self.data['roles'],'dialogues':self.data['dialogues']}))
    def test_duplicate_node_rejected(self):
        other=copy.deepcopy(self.offer);other['id']='another';self.config['offers']['another']=other
        self.assertIn('CARE_RELIEF_DUPLICATE_NODE',self.codes())

if __name__=='__main__':unittest.main()
