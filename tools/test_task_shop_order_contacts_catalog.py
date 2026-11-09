"""显式接触profile及原三域配对的有限作者源门；不冒真实资金/生命/开工验收。"""
import copy,json,unittest
from pathlib import Path
from tools.content.task_shop_order_contact_links import check_task_shop_order_contacts

def fixture():
    """声明真实旧订单目录；不预填持久状态。"""
    return {'required_capabilities':['task_shop_order_contacts_v1','task_shop_orders_v1','family_life_v1'],'scenarios':[{'task_shop_order_contact_profile':'original_shop_order_contacts_v1','task_shop_order_profile':'private_shop_orders_v1','family_life':{'profile':'verified_military_deaths_v1'},'dynamic_world':{'profile':'local_society_v1','private_shops':{'orders':{'policies':{'one':{'id':'one'}}}}}}]}
class TaskShopContactCatalog(unittest.TestCase):
    def test_explicit_pair(self):self.assertEqual(check_task_shop_order_contacts(fixture(),{}),[])
    def test_old_profile_unchanged(self):self.assertEqual(check_task_shop_order_contacts({'required_capabilities':fixture()['required_capabilities'],'scenarios':[{}]},{}),[])
    def test_unknown_and_null(self):
        for value in [None,True,2,'other']:
            p=fixture();p['scenarios'][0]['task_shop_order_contact_profile']=value;self.assertTrue(check_task_shop_order_contacts(p,{}))
    def test_each_cap(self):
        for cap in fixture()['required_capabilities']:
            p=fixture();p['required_capabilities'].remove(cap);self.assertTrue(check_task_shop_order_contacts(p,{}))
    def test_original_profile(self):
        p=fixture();del p['scenarios'][0]['task_shop_order_profile'];self.assertTrue(check_task_shop_order_contacts(p,{}))
    def test_real_life(self):
        for value in [None,{},[],{'profile':'assume_alive'}]:
            p=fixture();p['scenarios'][0]['family_life']=value;self.assertTrue(check_task_shop_order_contacts(p,{}))
    def test_orders_required(self):
        for value in [None,{},[],{'profile':'local_society_v1','private_shops':{'orders':{'policies':{}}}}]:
            p=fixture();p['scenarios'][0]['dynamic_world']=value;self.assertTrue(check_task_shop_order_contacts(p,{}))
    def test_normal_pack_does_not_seed_facts(self):
        root=Path(__file__).resolve().parents[1];content=json.loads((root/'content/packs/authored/task_shop_order_contacts_1_0/content.json').read_text());self.assertEqual(check_task_shop_order_contacts(content,{}),[])
        for scene in content['scenarios']:
            self.assertNotIn('task_shop_order_contacts',scene.get('initial_state',{}));self.assertNotIn('task_shop_orders',scene.get('initial_state',{}))
if __name__=='__main__':unittest.main()
