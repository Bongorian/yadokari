#!/usr/bin/env python3
"""Fetch SHA-256 checked upstream snapshots for independent fidelity builds."""
import hashlib
import io
from pathlib import Path
import tarfile
import urllib.request

root=Path(__file__).resolve().parents[2]
deps=root/'.deps';deps.mkdir(exist_ok=True)
specs=[('eurorack','f4cfa7697095a28d292bff758beb51deb49b5a86','7bade44c4b3839c7e505a128e68896cf0b039849b8bc824a41e0051e1dfb016e'),
       ('clouds','2a468734bdd53bea0f25165bf34d92a4c571db13','2fde12b1379a148668b88a5e88b703aaa043381f57db6df5bc584d13df0312a2'),
       ('stmlib','e3bd7c9cc00e4364166f9905c0509b6ffd0535ec','eff5d6642a93498996661e0dd4b5a1de4b98093f25a009166910726d1262b5d2')]
for repo,revision,digest in specs:
    archive=deps/f'{repo}-reference.tar.gz'
    if not archive.exists():
        data=urllib.request.urlopen(f'https://codeload.github.com/pichenettes/{"eurorack" if repo=="clouds" else repo}/tar.gz/{revision}',timeout=60).read()
        if hashlib.sha256(data).hexdigest()!=digest:raise RuntimeError('Archive digest mismatch')
        archive.write_bytes(data)
    data=archive.read_bytes()
    if hashlib.sha256(data).hexdigest()!=digest:raise RuntimeError(f'{repo} archive digest mismatch')
    target=deps/f'{repo}-reference';target.mkdir(exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as tar:
        for entry in tar:
            parts=Path(entry.name).parts[1:]
            if not entry.isfile() or not parts:continue
            if '..' in parts or Path(*parts).is_absolute():raise RuntimeError('Unsafe archive path')
            path=target.joinpath(*parts);path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(tar.extractfile(entry).read())
ref=deps/'pristine-mutable';ref.mkdir(exist_ok=True)
for name,target in [('rings','../eurorack-reference/rings'),('clouds','../clouds-reference/clouds'),('stmlib','../stmlib-reference')]:
    link=ref/name
    if link.is_symlink():link.unlink()
    link.symlink_to(target,target_is_directory=True)
print('Independent reference snapshots verified and prepared')
