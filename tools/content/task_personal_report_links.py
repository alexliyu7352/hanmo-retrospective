"""原信件真实去程后向原发信人Known本人SAY返报；不给空岗位通用服务权。"""
PROFILE='known_private_sender_report_v1'
CAPABILITY='task_personal_message_reports_v1'
def check_task_personal_reports(pack,index):
 errors=[]
 for number,scene in enumerate(pack.get('scenarios',[])):
  dynamic=scene.get('dynamic_world',{});block=dynamic.get('task_personal_message_reports') if isinstance(dynamic,dict) else None
  if block is None:
   if scene.get('task_message_handoff_profile')=='known_personal_delivery_and_sender_report_v4':errors.append(('TASK_PERSONAL_REPORT_PROFILE',('scenarios',number),'handoff3须同时明确实际接信目录'))
   continue
  root=('scenarios',number,'dynamic_world','task_personal_message_reports')
  def err(code,path,text):errors.append((code,path,text))
  if not isinstance(block,dict) or set(block)!={'version','profile','policies'} or type(block.get('version')) is not int or block['version']!=1 or block.get('profile')!=PROFILE or not isinstance(block.get('policies'),list) or not 1<=len(block['policies'])<=4096:
   err('TASK_PERSONAL_REPORT_PROFILE',root,'须明确私人返报1及有限非空原委托政策');continue
  required={CAPABILITY,'native_dynamic_world','multi_actor','dialogue','tasks.message','task_message_handoffs_v1','task_message_reports_v2','task_static_assets_v1','family_life_v1','known_personal_dialogues_v1'}
  if not required<=set(pack.get('required_capabilities',[])):err('TASK_PERSONAL_REPORT_CAPABILITY',root,'须原Task、实交、返报、实物取消、生命及Known能力')
  if 'task_message_notices' in scene:err('TASK_PERSONAL_REPORT_NOTICE_PROFILE',root,'首片不改变原死讯岗位、继位或身后清店来源')
  if scene.get('task_message_handoff_profile') not in ('known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4') or scene.get('task_static_asset_profile')!='original_task_assets_v1' or 'task_message_returns' in scene:err('TASK_PERSONAL_REPORT_ORIGINAL_PROFILE',root,'首片须handoff3和原实物1，不能同时改变回书2条款')
  delivery=dynamic.get('task_personal_message_deliveries')
  if scene.get('task_message_handoff_profile')!='known_personal_delivery_and_sender_report_v4' or not isinstance(delivery,dict) or delivery.get('version')!=1 or delivery.get('profile')!='known_private_recipient_v1' or 'task_personal_message_deliveries_v1' not in pack.get('required_capabilities',[]):err('TASK_PERSONAL_REPORT_DELIVERY_PAIR',root,'须同一原委托的Q01去程和handoff4')
  delivery_offers={p.get('offer_id') for p in delivery.get('policies',[]) if isinstance(p,dict)} if isinstance(delivery,dict) else set()
  life=scene.get('family_life',{});known=dynamic.get('known_personal_dialogues',{})
  if dynamic.get('profile')!='local_society_v1' or not isinstance(life,dict) or life.get('profile')!='verified_military_deaths_v1' or not isinstance(known,dict) or known.get('version')!=1:err('TASK_PERSONAL_REPORT_KNOWN_PROFILE',root,'必须已有原生命与Known本人目录')
  templates=known.get('templates',[]) if isinstance(known,dict) else [];enabled=scene.get('enabled',{});seen=set();offers=set();extensions=scene.get('task_extensions',{});extended={p.get('offer_id') for p in extensions.get('policies',[]) if isinstance(p,dict)} if isinstance(extensions,dict) else set()
  for offset,p in enumerate(block['policies']):
   path=root+('policies',offset);keys={'id','offer_id','personal_template_id','sender_say_node_id'}
   if not isinstance(p,dict) or set(p)!=keys or any(not isinstance(p[k],str) or not 1<=len(p[k])<=256 for k in keys):err('TASK_PERSONAL_REPORT_POLICY',path,'只能引用原委托、Known模板和发信人SAY');continue
   if p['id'] in seen or p['offer_id'] in offers:err('TASK_PERSONAL_REPORT_DUPLICATE',path,'一份原委托只选择一项接信条款')
   seen.add(p['id']);offers.add(p['offer_id'])
   if p['offer_id'] not in delivery_offers:err('TASK_PERSONAL_REPORT_ORIGINAL_DELIVERY',path,'返报须原已选择Q01的同份私人信件')
   if p['offer_id'] in extended:err('TASK_PERSONAL_REPORT_EXTENSION',path,'首片同一原委托不改期限')
   offer=index.get('offers',{}).get(p['offer_id'],{});issuer=offer.get('issuer',{});goal=offer.get('goal',{})
   if not isinstance(issuer,dict) or set(issuer)!={'kind','id'} or issuer.get('kind')!='actor' or issuer.get('id') not in enabled.get('actors',[]) or not isinstance(goal,dict) or goal.get('receiver_actor') not in enabled.get('actors',[]) or issuer.get('id')==goal.get('receiver_actor') or offer.get('template')!='message' or p['offer_id'] not in enabled.get('offers',[]) or type(offer.get('deadline_ticks')) is not int or offer['deadline_ticks']<1:err('TASK_PERSONAL_REPORT_PRIVATE_TASK',path,'须已启用原Actor私人送信');continue
   if goal.get('message_id') not in index.get('items',{}) or goal.get('message_id') not in enabled.get('items',[]):err('TASK_PERSONAL_REPORT_ORIGINAL_LETTER',path,'只传递原Task生成的已启用信件')
   matched=[t for t in templates if isinstance(t,dict) and t.get('id')==p['personal_template_id']]
   if len(matched)!=1 or matched[0].get('provider')!={'kind':'installed_actor','source_id':issuer.get('id')}:err('TASK_PERSONAL_REPORT_RECIPIENT',path,'Known provider须精确原私人发信者');continue
   t=matched[0];dialogue=index.get('dialogues',{}).get(t.get('dialogue_id'),{});nodes=[n for n in dialogue.get('nodes',[]) if isinstance(n,dict) and n.get('id')==p['sender_say_node_id']]
   if t.get('dialogue_id') not in enabled.get('dialogues',[]) or dialogue.get('context_role') not in (None,'') or len(nodes)!=1 or nodes[0].get('kind')!='say' or nodes[0].get('speaker')!='$provider' or 'command' in nodes[0]:err('TASK_PERSONAL_REPORT_RECIPIENT_SAY',path,'须原无岗位对白中的provider本人SAY')
 return errors
