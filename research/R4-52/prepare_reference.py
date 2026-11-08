"""Extract locally qualified stream references; never download benchmark code."""
import hashlib,json,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];base=root/'build-research/resident-native-replay'
out=root/'build-research/native-slot-cache-r52';out.mkdir(exist_ok=True)
references=[]
for revision in ['r51','r52']:
 manifest=json.loads((base/(revision+'-qualified-batches-fingerprint.local.json')).read_text())
 archive=base/(revision+'-qualified-batches-sources.zip')
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==manifest['source_archive_sha256']
 with zipfile.ZipFile(archive)as z:source=z.read('research/R4-49/NativeVertexStream.hpp')
 assert b'class NativeVertexStream {'in source and b'bool materialize(std::size_t count)'in source
 (out/(revision+'-stream-reference.local.hpp')).write_bytes(source.replace(b'NativeVertexStream',('Reference'+revision.upper()+'Stream').encode()))
 references.append({'revision':revision,'archive_sha256':manifest['source_archive_sha256'],'stream_sha256':hashlib.sha256(source).hexdigest()})
(out/'reference.local.json').write_text(json.dumps({'references':references,'diagnostic_only':True,'game_fps_evidence':False},indent=2)+'\n')
print('Verified qualified R51/R52 stream references')
