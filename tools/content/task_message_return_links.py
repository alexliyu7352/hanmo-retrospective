"""Q03只给原私人送信增加真实回程；原去程、返报和取消来源须显式配齐。"""
PROFILE='private_round_trip_letter_v1'
CAPABILITY='task_message_returns_v1'
def check_task_message_returns(pack,index):
 errors=[]
 for number,scene in enumerate(pack.get('scenarios',[])):
  if 'task_message_returns' not in scene:continue
  root=('scenarios',number,'task_message_returns');block=scene['task_message_returns']
  def err(code,path,text):errors.append((code,path,text))
  if not isinstance(block,dict) or set(block)!={'version','profile','policies'} or type(block.get('version')) is not int or block['version']!=1 or block.get('profile')!=PROFILE or not isinstance(block.get('policies'),list) or not 1<=len(block['policies'])<=4096:
   err('TASK_MESSAGE_RETURN_PROFILE',root,'须明确回书1及有限非空政策');continue
  caps=set(pack.get('required_capabilities',[]));required={CAPABILITY,'native_dynamic_world','multi_actor','dialogue','tasks.message','task_message_handoffs_v1','task_message_reports_v2','task_static_assets_v1','family_life_v1'}
  if not required<=caps:err('TASK_MESSAGE_RETURN_CAPABILITY',root,'须原生Task及真实去程、返报、返还、生命能力')
  if scene.get('task_message_handoff_profile')!='observed_letter_delivery_and_report_v2' or scene.get('task_static_asset_profile')!='original_task_assets_v1':err('TASK_MESSAGE_RETURN_ORIGINAL_PROFILE',root,'须原报告2和真实实物返还1')
  dynamic=scene.get('dynamic_world',{});life=scene.get('family_life',{})
  if not isinstance(dynamic,dict) or dynamic.get('profile')!='local_society_v1' or not isinstance(life,dict) or life.get('profile')!='verified_military_deaths_v1':err('TASK_MESSAGE_RETURN_LIFE_PROFILE',root,'须原生同World实际生命前界')
  enabled=scene.get('enabled',{});seen=set();offers=set();extensions=scene.get('task_extensions',{});extended={p.get('offer_id') for p in extensions.get('policies',[]) if isinstance(p,dict)} if isinstance(extensions,dict) else set()
  for n,p in enumerate(block['policies']):
   path=root+('policies',n);keys={'id','offer_id','reply_item_id','recipient_role_id','dialogue_id','node_id'}
   if not isinstance(p,dict) or set(p)!=keys or any(not isinstance(p[k],str) or not p[k] or len(p[k])>256 for k in keys):err('TASK_MESSAGE_RETURN_POLICY',path,'只准具名原委托、不同回书和原收件人声明');continue
   if p['id'] in seen or p['offer_id'] in offers:err('TASK_MESSAGE_RETURN_DUPLICATE',path,'同一原Offer至多一份回书条款')
   seen.add(p['id']);offers.add(p['offer_id'])
   if p['offer_id'] in extended:err('TASK_MESSAGE_RETURN_EXTENSION',path,'首片同一委托不能另改期限')
   offer=index.get('offers',{}).get(p['offer_id'],{});issuer=offer.get('issuer',{});goal=offer.get('goal',{})
   if (not isinstance(issuer,dict) or set(issuer)!={'kind','id'} or issuer.get('kind')!='actor' or issuer.get('id') not in enabled.get('actors',[]) or not isinstance(goal,dict) or goal.get('receiver_actor') not in enabled.get('actors',[]) or issuer.get('id')==goal.get('receiver_actor') or offer.get('template')!='message' or p['offer_id'] not in enabled.get('offers',[]) or type(offer.get('deadline_ticks')) is not int or offer['deadline_ticks']<1):err('TASK_MESSAGE_RETURN_PRIVATE_TASK',path,'仅已启用原Actor送信私约');continue
   if p['reply_item_id']==goal.get('message_id') or any(i not in enabled.get('items',[]) or i not in index.get('items',{}) for i in [p['reply_item_id'],goal.get('message_id')]):err('TASK_MESSAGE_RETURN_DISTINCT_ITEM',path,'回书须为不同原物品，去程信件不能冒回书')
   recipient=index.get('roles',{}).get(p['recipient_role_id'],{});sender_id=offer.get('report_role');sender=index.get('roles',{}).get(sender_id,{})
   for role_id,role,actor,service in [(p['recipient_role_id'],recipient,goal.get('receiver_actor'),'deliver_message'),(sender_id,sender,issuer['id'],'report_offer')]:
    if role_id not in enabled.get('roles',[]) or role.get('organization_id') is not None or role.get('initial_actor')!=actor or role.get('place_id') not in enabled.get('places',[]) or service not in role.get('services',[]) or role.get('dialogue_id') not in enabled.get('dialogues',[]):err('TASK_MESSAGE_RETURN_ORIGINAL_ROLE',path,'须原本人私人岗位、实际地点与对应服务')
   dialogue=index.get('dialogues',{}).get(p['dialogue_id'],{});nodes=[n for n in dialogue.get('nodes',[]) if isinstance(n,dict) and n.get('id')==p['node_id']]
   if recipient.get('dialogue_id')!=p['dialogue_id'] or dialogue.get('context_role')!=p['recipient_role_id'] or len(nodes)!=1 or nodes[0].get('kind')!='say' or nodes[0].get('speaker') not in ('$provider',goal.get('receiver_actor')) or 'command' in nodes[0]:err('TASK_MESSAGE_RETURN_RECIPIENT_SAY',path,'原收件人本人SAY才可声明托回')
   talk=index.get('dialogues',{}).get(sender.get('dialogue_id'),{})
   if talk.get('context_role')!=sender_id or not any(n.get('kind')=='say' and n.get('speaker') in ('$provider',issuer['id']) and 'command' not in n for n in talk.get('nodes',[]) if isinstance(n,dict)):err('TASK_MESSAGE_RETURN_SENDER_SAY',path,'原sender须可到达自己的接报说话节点')
 return errors
