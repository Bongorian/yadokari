"""Browser-equivalent requests against the same file-service core on loopback."""
import hashlib
import http.client
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from urllib.parse import urlencode

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix='mgx13-http-') as directory:
        root = Path(directory)
        proc = subprocess.Popen([str(ROOT/'build/file-web-server'), directory], stdout=subprocess.PIPE, text=True)
        try:
            port = int(proc.stdout.readline().split()[1])

            def request(method, route, body=None, code='TESTCODE', cookie=False):
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
                headers = {'Cookie': 'MGX='+code} if cookie else {'X-Groove-Code': code}
                connection.request(method, route, body, headers)
                response = connection.getresponse()
                result = response.status, response.read()
                connection.close()
                return result

            def upload(name, body, sha=None):
                return request('PUT', '/api/v1/file?'+urlencode({'name': name, 'sha256': sha or hashlib.sha256(body).hexdigest()}), body)

            assert request('GET', '/')[0] == 200
            assert request('GET', '/api/v1/status', code='wrong')[0] == 403
            for source, name, shelf, path in [
                ('Song.mgxs', 'Song.mgxp', 0, 'PROJECTS/Song.mgxs'),
                ('Preset.mgxp', 'Preset.mgxs', 1, 'PRESETS/Preset.mgxp'),
                ('Kit.mgxk', 'Kit.mgxp', 2, 'KITS/Kit.mgxk'),
                ('Tone.wav', 'Tone.wav', 3, 'ASSETS/SAMPLES/Tone.wav'),
                ('Bank.sf2', 'Bank.sf2', 4, 'ASSETS/SOUNDFONTS/Bank.sf2'),
                ('Pattern.mid', 'Pattern.mid', 5, 'ASSETS/MIDI/Pattern.mid'),
            ]:
                body = (root/'FIXTURES'/source).read_bytes()
                status, response = upload(name, body)
                assert status == 200, (source, status, response)
                status, listing = request('GET', '/api/v1/list?shelf='+str(shelf))
                assert status == 200 and path in [f['path'] for f in json.loads(listing)['files']]
                status, received = request('GET', '/api/v1/file?'+urlencode({'path': path}), cookie=True)
                assert status == 200 and hashlib.sha256(received).digest() == hashlib.sha256(body).digest()
            status, response = upload('Bank.syx', (root/'FIXTURES/Bank.syx').read_bytes())
            assert status == 200, response
            bank = json.loads(response)['path']
            files = json.loads(request('GET', '/api/v1/list?shelf=1')[1])['files']
            assert sum(f['path'].startswith(bank+'/') for f in files) == 32
            assert not list(root.rglob('*.syx.tmp'))
            original = (root/'ASSETS/SAMPLES/Tone.wav').read_bytes()
            for name, body, sha in [('Tone.wav', b'broken', None), ('Tone.wav', original, '0'*64), ('../bad.wav', original, None), ('Bad.txt', original, None)]:
                assert upload(name, body, sha)[0] == 400
                assert (root/'ASSETS/SAMPLES/Tone.wav').read_bytes() == original
            old = bytearray((root/'FIXTURES/Song.mgxs').read_bytes()); old[4] = 8
            assert upload('Old.mgxs', old)[0] == 400
            assert upload('Tone.wav', original)[0] == 200
            for path in ['PROJECTS/Open.mgxs', 'ASSETS/SAMPLES/Locked.wav']:
                assert request('DELETE', '/api/v1/file?'+urlencode({'path': path}))[0] == 423
            assert upload('Locked.wav', original)[0] == 423
            for path in ['../GROOVE/DEVICE.CFG', 'FACTORY/DEBUG/Benchmark.mgxs', '/wpa_supplicant.conf']:
                assert request('DELETE', '/api/v1/file?'+urlencode({'path': path}))[0] == 400
            # A large upload is received incrementally and round-tripped unchanged.
            large = bytearray((root/'FIXTURES/Bank.sf2').read_bytes())
            large += b'JUNK'+(2*1024*1024).to_bytes(4, 'little')+b'\0'*(2*1024*1024)
            large[4:8] = (len(large)-8).to_bytes(4, 'little')
            assert upload('Large.sf2', large)[0] == 200
            assert hashlib.sha256(request('GET', '/api/v1/file?path=ASSETS/SOUNDFONTS/Large.sf2')[1]).digest() == hashlib.sha256(large).digest()
            # Broken TCP client must clean only its own partially received file.
            broken = socket.create_connection(('127.0.0.1', port))
            broken.sendall(b'PUT /api/v1/file?name=Partial.sf2&sha256='+b'0'*64+b' HTTP/1.1\r\nHost: local\r\nX-Groove-Code: TESTCODE\r\nContent-Length: 1000000\r\n\r\nabc')
            broken.close()
            assert request('GET', '/api/v1/status')[0] == 200
            assert not list(root.rglob('*.tmp'))
            assert request('DELETE', '/api/v1/file?path=ASSETS/SAMPLES/Tone.wav')[0] == 200
            proc.terminate(); proc.wait(timeout=5)
            assert proc.returncode == 0 and not list(root.rglob('*.tmp'))
            try:
                socket.create_connection(('127.0.0.1', port), timeout=1)
                raise AssertionError('Server reachable after close')
            except ConnectionRefusedError:
                pass
            print('PASS HTTP: seven formats, shelves, download hashes, auth, rejection, protected references, streaming, disconnect and server close')
        finally:
            if proc.poll() is None:
                proc.terminate(); proc.wait(timeout=5)
    page = (ROOT/'src/file_web_assets.h').read_text().split('<script>')[1].split('</script>')[0]
    with tempfile.TemporaryDirectory(prefix='mgx13-js-') as directory:
        js = Path(directory)/'page.js'; js.write_text(page)
        subprocess.run(['node', '--check', str(js)], check=True)
        hash_code = page[page.index('class Hash'):page.index('function upload(')]
        vectors = ['', 'abc', 'x'*100003]
        js.write_text(hash_code+'\nfor(const text of '+json.dumps(vectors)+'){const h=new Hash();const b=new TextEncoder().encode(text);for(let i=0;i<b.length;i+=32768)h.add(b.slice(i,i+32768));console.log(h.finish())}')
        actual = subprocess.check_output(['node', str(js)], text=True).splitlines()
        assert actual == [hashlib.sha256(s.encode()).hexdigest() for s in vectors]
    print('PASS offline JavaScript syntax and streaming SHA256 vectors')


if __name__ == '__main__':
    main()
