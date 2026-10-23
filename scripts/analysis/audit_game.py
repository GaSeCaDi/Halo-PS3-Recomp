"""Read-only inventory and bounded parsing of XEX2 metadata. No decryption or execution."""
import argparse, collections, csv, hashlib, json, pathlib, struct
from datetime import datetime, timezone

def xex_metadata(path):
    size=path.stat().st_size
    with path.open('rb') as f:
        fixed=f.read(24)
        if fixed[:4]!=b'XEX2': return {'format':'unrecognized','magic_hex':fixed[:4].hex()}
        flags,payload,reserved,security,count=struct.unpack_from('>5I',fixed,4)
        if count>4096 or 24+count*8>payload or payload>min(size,16*1024*1024):
            raise ValueError('XEX header bounds invalid: '+str(path))
        f.seek(0); data=f.read(payload)
    def read(fmt,offset):
        n=struct.calcsize(fmt)
        if offset<0 or offset+n>len(data): raise ValueError('Metadata outside header')
        return struct.unpack_from(fmt,data,offset)
    options=dict(struct.iter_unpack('>II',data[24:24+8*count]))
    out={'format':'XEX2','module_flags':hex(flags),'payload_offset':payload,
         'optional_header_count':count,'optional_headers':{hex(k):hex(v) for k,v in options.items()}}
    for key,name in [(0x10100,'entry_point'),(0x10201,'image_base')]:
        if key in options: out[name]=hex(options[key])
    if 0x3ff in options:
        length,enc,comp=read('>IHH',options[0x3ff])
        out['file_format']={'size':length,'encryption':enc,'compression':comp,
                           'encryption_name':{0:'none',1:'normal'}.get(enc,'unknown'),
                           'compression_name':{0:'none',1:'basic',2:'normal',3:'delta'}.get(comp,'unknown')}
    if 0x40006 in options:
        vals=read('>4I4BI',options[0x40006])
        out['execution']={'media_id':f'{vals[0]:08X}','version_raw':f'{vals[1]:08X}',
                          'base_version_raw':f'{vals[2]:08X}','title_id':f'{vals[3]:08X}',
                          'platform':vals[4],'executable_table':vals[5],
                          'disc_number':vals[6],'disc_count':vals[7]}
    if 0x200ff in options:
        off=options[0x200ff]; length,=read('>I',off)
        if length<4 or length>len(data)-off or (length-4)%16: raise ValueError('Static library bounds')
        libraries=[]
        for pos in range(off+4,off+length,16):
            major,minor,build,approval,qfe=read('>3H2B',pos+8)
            libraries.append({'name':data[pos:pos+8].rstrip(b'\0').decode('ascii','replace'),
                              'version':f'{major}.{minor}.{build}.{qfe}','approval_raw':approval})
        out['static_libraries']=libraries
    if 0x183ff in options:
        off=options[0x183ff]; length,=read('>I',off)
        if 4<=length<=len(data)-off: out['original_pe_name']=data[off+4:off+length].split(b'\0')[0].decode('ascii','replace')
    if 0x103ff in options:
        off=options[0x103ff]; length,strings_size,strings_count=read('>III',off)
        if strings_size>len(data)-(off+12) or length>len(data)-off: raise ValueError('Import metadata bounds')
        names=[s.decode('ascii','replace') for s in data[off+12:off+12+strings_size].split(b'\0') if s][:strings_count]
        pos=off+12+strings_size; end=off+length; libs=[]
        while pos+40<=end:
            libsize,=read('>I',pos)
            if libsize<40 or pos+libsize>end: raise ValueError('Import library bounds')
            idx,entries=read('>HH',pos+36)
            libs.append({'name':names[idx] if idx<len(names) else f'index_{idx}',
                         'record_count':entries,'name_index':idx})
            pos+=libsize
        out['imports']={'names':names,'declared_name_count':strings_count,'libraries':libs}
    return out

def main():
    p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True)
    args=p.parse_args();root=args.root.resolve();out=args.out.resolve()
    if not root.is_dir(): raise SystemExit('Missing input folder')
    if out==root or root in out.parents: raise SystemExit('Reports must be outside input folder')
    out.mkdir(parents=True,exist_ok=True);rows=[];binaries=[];extensions=collections.Counter();total=0
    for path in sorted(root.rglob('*')):
        if not path.is_file(): continue
        before=path.stat(); h=hashlib.sha256()
        with path.open('rb') as f:
            while chunk:=f.read(4*1024*1024):h.update(chunk)
        after=path.stat()
        if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):raise RuntimeError('Input changed during hashing')
        rel=path.relative_to(root).as_posix();ext=path.suffix.lower();extensions[ext]+=1;total+=after.st_size
        rows.append({'path':rel,'bytes':after.st_size,'sha256':h.hexdigest()})
        if ext in ('.xex','.dll','.exe'):
            item={'path':rel,'sha256':h.hexdigest()};item.update(xex_metadata(path));binaries.append(item)
    with (out/'inventory.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=['path','bytes','sha256']);writer.writeheader();writer.writerows(rows)
    result={'generated_at_utc':datetime.now(timezone.utc).isoformat(),'input_directory':str(root),'file_count':len(rows),
            'total_bytes':total,'extensions':dict(extensions),'binaries':binaries,
            'limits':['Metadata only; no executable decryption, disassembly or decompilation performed.',
                      'Hashes identify this local copy; they do not authenticate it as an unmodified retail release.']}
    (out/'audit.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(result,indent=2,ensure_ascii=False))
if __name__=='__main__':main()

