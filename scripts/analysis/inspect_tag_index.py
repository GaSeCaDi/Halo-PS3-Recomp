"""Independent read-only reference for the observed Halo 3 retail tag index."""
import pathlib,struct,json,csv,collections,argparse,zlib

def inspect(path):
 with path.open('rb') as f:
  h=f.read(0x3000);size=path.stat().st_size
  if len(h)!=0x3000 or h[:4]!=b'head' or h[-4:]!=b'foot':raise ValueError('header signature')
  u=lambda o:struct.unpack_from('>I',h,o)[0]
  if u(4)!=11 or u(8)!=size or u(12)!=0:raise ValueError('unsupported cache')
  sections=[]
  for i in range(4):
   delta=u(0x46c+i*4);va=u(0x47c+i*8);length=u(0x480+i*8);start=(va+delta)&0xffffffff
   if start+length>size:raise ValueError('section bounds')
   sections.append({'start':start,'size':length,'delta':delta})
  ts=sections[2];ss=sections[0];base=u(0x2e8)
  def read(off,n):
   if off<0 or n<0 or off+n>size:raise ValueError('file bounds')
   f.seek(off);b=f.read(n)
   if len(b)!=n:raise ValueError('short read')
   return b
  def tagoff(va,n):
   if va<base or va-base+n>ts['size']:raise ValueError(f'tag VA bounds {va:08x}')
   return ts['start']+va-base
  def stroff(va,n):
   off=(va+ss['delta'])&0xffffffff
   if off<ss['start'] or off+n>ss['start']+ss['size']:raise ValueError('string section bounds')
   return off
  if ts['size']==0:return {'map':path.stem,'tag_count':0,'empty_tag_section':True,'index_crc32':'00000000','scenario_prefix_crc32':'00000000'},[]
  thoff=tagoff(u(0x10),40);th=struct.unpack('>10I',read(thoff,40))
  if th[9]!=0x74616773:raise ValueError('tags signature')
  gc,groupsva,tc,tagsva,n_globals,globalsva=th[:6]
  if gc>4096 or tc>65536 or n_globals>4096 or tc!=u(0x2b4):raise ValueError('counts')
  groups=read(tagoff(groupsva,gc*16),gc*16)
  instances=read(tagoff(tagsva,tc*8),tc*8)
  names=read(stroff(u(0x2b8),u(0x2bc)),u(0x2bc))
  indices=struct.unpack('>'+str(tc)+'I',read(stroff(u(0x2c0),tc*4),tc*4))
  tags=[];counts=collections.Counter();max_name=0;nulls=0
  for i in range(tc):
   group,salt,va=struct.unpack_from('>HHI',instances,i*8)
   ni=indices[i];name=''
   if ni!=0xffffffff:
    if ni>=len(names):raise ValueError('name offset')
    end=names.find(b'\0',ni)
    if end<0:raise ValueError('unterminated name')
    name=names[ni:end].decode('ascii');max_name=max(max_name,len(name))
   if group==0xffff:
    nulls+=1;fourcc='';off=None
   else:
    if group>=gc:raise ValueError('group index')
    fourcc=groups[group*16:group*16+4].hex();off=tagoff(va,1);counts[fourcc]+=1
   tags.append({'index':i,'id':f'{salt:04x}{i:04x}','group_index':group,'group_hex':fourcc,
                'group':bytes.fromhex(fourcc).decode('ascii','backslashreplace') if fourcc else '',
                'virtual_address':f'{va:08x}','file_offset':off,'name':name})
  globals=[]
  for group,datum in struct.iter_unpack('>4sI',read(tagoff(globalsva,n_globals*8),n_globals*8)):
   index=datum&0xffff
   if index>=tc or int(tags[index]['id'],16)!=datum:raise ValueError('global datum')
   globals.append({'group':group.decode('ascii','backslashreplace'),'id':f'{datum:08x}','index':index,'name':tags[index]['name']})
  crc=0
  for block in (groups,instances,read(stroff(u(0x2c0),tc*4),tc*4),names,read(tagoff(globalsva,n_globals*8),n_globals*8)):crc=zlib.crc32(block,crc)
  scenario=next((t for t in tags if t['group']=='scnr'),None)
  scenario_crc=zlib.crc32(read(scenario['file_offset'],64)) if scenario else 0
  return {'index_crc32':f'{crc:08x}','scenario_prefix_crc32':f'{scenario_crc:08x}','map':path.stem,'file_size':size,'virtual_base':f'{base:08x}','sections':sections,'tag_header_offset':thoff,
          'group_count':gc,'tag_count':tc,'null_count':nulls,'max_name_length':max_name,'global_count':n_globals,'globals':globals,
          'group_counts':dict(counts),'scenario_tags':[t for t in tags if t['group']=='scnr']},tags

def main():
 p=argparse.ArgumentParser();p.add_argument('--maps',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 result=[]
 for path in sorted(a.maps.glob('*.map')):
  try:
   summary,tags=inspect(path)
   if tags:
    with (a.out/(path.stem+'-tags.csv')).open('w',encoding='utf-8',newline='') as f:
     w=csv.DictWriter(f,fieldnames=list(tags[0]));w.writeheader();w.writerows(tags)
   summary['status']='PASS';result.append(summary)
   print(path.name,'PASS',summary.get('tag_count'),flush=True)
  except Exception as e:
   result.append({'map':path.stem,'status':'FAIL','error':str(e)});print(path.name,'FAIL',e,flush=True)
 (a.out/'maps-index-reference.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
if __name__=='__main__':main()
