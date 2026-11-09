"""文书交接作者只显式打开新收据，不伪造送达，不改变旧情景。"""
from copy import deepcopy
import json
from pathlib import Path
import unittest
from content.task_message_handoff_links import check_task_message_handoffs


class TaskMessageHandoffAuthoring(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[1]
        self.document = json.loads((self.root / 'content/packs/authored/task_contacts_1_6/content.json').read_text())
        self.old = json.loads((self.root / 'content/packs/authored/task_contacts_1_5/content.json').read_text())

    def test_old_scenarios_and_single_explicit_new_selection(self):
        before = deepcopy(self.document)
        self.assertEqual(check_task_message_handoffs(self.document, {}), [])
        self.assertEqual(self.document, before)
        self.assertEqual(self.document['scenarios'][:-1], self.old['scenarios'])
        self.assertEqual(self.document['scenarios'][-1]['task_message_handoff_profile'], 'observed_letter_delivery_v1')
        self.assertEqual(self.document['offers'], self.old['offers'])
        self.assertEqual(self.document['dialogues'], self.old['dialogues'])
        self.assertEqual(self.document['scenarios'][-1]['initial'], self.old['scenarios'][0]['initial'])

    def test_explicit_report_version_preserves_previous_scenarios(self):
        report = json.loads((self.root / 'content/packs/authored/task_contacts_1_7/content.json').read_text())
        self.assertEqual(report['scenarios'][:-1], self.document['scenarios'])
        self.assertEqual(report['offers'], self.document['offers'])
        self.assertEqual(report['scenarios'][-1]['task_message_handoff_profile'], 'observed_letter_delivery_and_report_v2')
        self.assertEqual(check_task_message_handoffs(report, {}), [])
        report['required_capabilities'].remove('task_message_reports_v2')
        self.assertEqual(check_task_message_handoffs(report, {})[0][0], 'TASK_MESSAGE_REPORT_CAPABILITY')

    def test_missing_capability_and_unknown_profile(self):
        self.document['required_capabilities'].remove('task_message_handoffs_v1')
        self.assertEqual(check_task_message_handoffs(self.document, {})[0][0], 'TASK_MESSAGE_HANDOFF_CAPABILITY')
        self.document['scenarios'][-1]['task_message_handoff_profile'] = 'instant_remote_notification'
        self.assertEqual(check_task_message_handoffs(self.document, {})[0][0], 'TASK_MESSAGE_HANDOFF_PROFILE')

    def test_old_capability_union_does_not_add_history(self):
        original = deepcopy(self.old)
        original['required_capabilities'].append('task_message_handoffs_v1')
        self.assertEqual(check_task_message_handoffs(original, {}), [])
        self.assertEqual(original['scenarios'], self.old['scenarios'])
        for scenario in self.document['scenarios']:
            self.assertNotIn('task_message_handoffs', scenario['initial'])
            self.assertNotIn('notification_fact_id', scenario['initial'])


if __name__ == '__main__':
    unittest.main()
