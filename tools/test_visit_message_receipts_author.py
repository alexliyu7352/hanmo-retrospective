"""显式新段作者必要源门；普通素材compose与World仍由完整同代验收。"""
import copy,json,unittest
from pathlib import Path
from tools.content.visit_message_receipt_links import check_visit_message_receipts
class VisitMessageReceiptAuthorTest(unittest.TestCase):
    def setUp(self):
        self.repo=Path(__file__).resolve().parents[1];self.pack=json.loads((self.repo/'content/packs/authored/visit_appointments_1_2/content.json').read_text());self.scene=self.pack['scenarios'][0]
    def errors(self):return check_visit_message_receipts(self.pack,{})
    def test_exact_normal_author_and_pure(self):
        original=copy.deepcopy(self.pack);self.assertEqual([],self.errors());self.assertEqual(original,self.pack)
    def test_old_scene_remains_unselected(self):
        old=json.loads((self.repo/'content/packs/authored/visit_appointments_1_1/content.json').read_text());self.assertEqual([],check_visit_message_receipts(old,{}));self.assertNotIn('visit_message_receipt_profile',old['scenarios'][0])
    def test_capability_pair(self):
        self.pack['required_capabilities'].remove('visit_message_receipts_v1');self.assertTrue(self.errors())
    def test_original_catalog(self):
        del self.scene['visit_messages'];self.assertTrue(self.errors())
    def test_unknown_profile(self):
        self.scene['visit_message_receipt_profile']='permanent_visit_message_results_v2';self.assertTrue(self.errors())
    def test_real_world(self):
        self.scene['dynamic_world']['profile']='fake';self.assertTrue(self.errors())
    def test_no_boolean_version(self):
        self.scene['visit_messages']['version']=True;self.assertTrue(self.errors())
if __name__=='__main__':unittest.main()
