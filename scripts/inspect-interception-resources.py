"""Extract embedded drivers from the pinned official archive without executing it."""
import hashlib
import json
import pathlib
import re
import struct
import sys
import zipfile

EXPECTED_SHA256 = 'AD038963D6413055765128B0B931F6E765147C9916DBA79E65D872B261F9AF10'
archive = pathlib.Path(sys.argv[1])
destination = pathlib.Path(sys.argv[2])
if hashlib.sha256(archive.read_bytes()).hexdigest().upper() != EXPECTED_SHA256:
    raise ValueError('Archive differs from the reviewed official v1.0.1 release')
with zipfile.ZipFile(archive) as package:
    matches = [name for name in package.namelist()
               if name.replace('\\', '/').endswith('/command line installer/install-interception.exe')]
    if len(matches) != 1:
        raise ValueError('Expected exactly one official installer')
    data = package.read(matches[0])

pe = struct.unpack_from('<I', data, 0x3c)[0]
if data[pe:pe+4] != b'PE\0\0':
    raise ValueError('Invalid PE signature')
sections = struct.unpack_from('<H', data, pe + 6)[0]
optional_size = struct.unpack_from('<H', data, pe + 20)[0]
optional = pe + 24
magic = struct.unpack_from('<H', data, optional)[0]
if magic not in (0x10b, 0x20b):
    raise ValueError('Unsupported PE optional header')
directory = optional + (96 if magic == 0x10b else 112)
resource_rva = struct.unpack_from('<I', data, directory + 16)[0]
section_table = optional + optional_size

def offset(rva):
    for index in range(sections):
        virtual_size, address, raw_size, raw = struct.unpack_from(
            '<IIII', data, section_table + index * 40 + 8)
        if address <= rva < address + max(virtual_size, raw_size):
            return raw + rva - address
    raise ValueError(f'Unmapped RVA {rva:x}')

base = offset(resource_rva)
destination.mkdir(parents=True, exist_ok=True)
records = []

def walk(relative, path=()):
    if len(path) > 3:
        raise ValueError('Unexpected resource depth')
    here = base + relative
    named, numbered = struct.unpack_from('<HH', data, here + 12)
    for index in range(named + numbered):
        name, value = struct.unpack_from('<II', data, here + 16 + 8 * index)
        if name & 0x80000000:
            start = base + (name & 0x7fffffff)
            length = struct.unpack_from('<H', data, start)[0]
            name = data[start+2:start+2+length*2].decode('utf-16-le')
        else:
            name = str(name)
        current = path + (name,)
        if value & 0x80000000:
            walk(value & 0x7fffffff, current)
        elif current[0] == 'DRIVER':
            rva, size = struct.unpack_from('<II', data, base + value)
            start = offset(rva)
            payload = data[start:start+size]
            if len(payload) != size or not payload.startswith(b'MZ'):
                raise ValueError('Invalid embedded driver')
            filename = re.sub(r'[^a-zA-Z0-9_-]', '_', '_'.join(current)) + '.sys'
            # The pinned archive and sanitised, flat names prevent path traversal.
            (destination / filename).write_bytes(payload)
            payload_pe = struct.unpack_from('<I', payload, 0x3c)[0]
            machine = struct.unpack_from('<H', payload, payload_pe + 4)[0]
            records.append({'resource': '/'.join(current), 'file': filename,
                            'size': size, 'machine': hex(machine),
                            'sha256': hashlib.sha256(payload).hexdigest().upper()})

walk(0)
if not records:
    raise ValueError('No embedded drivers found')
print(json.dumps(records, indent=2))
