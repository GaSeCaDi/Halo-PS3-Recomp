import pathlib,json,zlib,hashlib,csv
root=pathlib.Path(r'C:\H3-PS3')
source=root/'HALO 3/maps/mainmenu.map';stage=root/'dist/map-reader/mainmenu.map'
with source.open('rb') as f:header=f.read(0x3000)
def field(offset,size):return header[offset:offset+size].split(b'\0')[0].decode('ascii')
expected={'name':field(0x18c,32),'version':str(int.from_bytes(header[4:8],'big')),
          'file_size':str(source.stat().st_size),'header_bytes':'12288',
          'build':field(0x11c,32),'scenario':field(0x1b0,256),
          'header_crc32':f'{zlib.crc32(header):08x}','parser_checks':'8/8'}
report=(root/'dist/map-reader/map-reader-result.txt').read_text(encoding='utf-8')
assert report.splitlines()[0]=='H3MAP_PASS'
actual=dict(line.split('=',1) for line in report.splitlines()[1:] if '=' in line)
for key,value in expected.items():assert actual[key]==value,(key,actual[key],value)
tty=(root/'logs/map-reader-TTY.log').read_text(encoding='utf-8-sig');assert 'H3MAP_EXIT 0' in tty
hash_source=hashlib.file_digest(source.open('rb'),'sha256').hexdigest()
hash_stage=hashlib.file_digest(stage.open('rb'),'sha256').hexdigest()
with (root/'research/inventory.csv').open(encoding='utf-8-sig',newline='') as f:record=next(r for r in csv.DictReader(f) if r['path']=='maps/mainmenu.map')
assert hash_source==hash_stage==record['sha256']
exe=root/'dist/map-reader/h3ps3_map_reader.self'
result={'status':'PASS','expected':expected,'ps3_report':actual,'ps3_exit_marker':0,
        'original_matches_initial_inventory':True,'map_sha256':hash_source,
        'self_bytes':exe.stat().st_size,'self_sha256':hashlib.file_digest(exe.open('rb'),'sha256').hexdigest(),
        'limitations':'Header recognition only. No tag loading, scene rendering, game runtime or real PS3 hardware validation.'}
(root/'research/map-reader-validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
