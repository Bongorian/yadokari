#!/usr/bin/env python3
"""Prepare pinned Pi4 WLAN firmware and a private WPA-PSK configuration."""
import argparse
import getpass
import hashlib
import json
import os
from pathlib import Path
import urllib.request

REVISION = 'c9d3ae6584ab79d19a4f94ccf701e888f9f87a53'
BASE = f'https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/{REVISION}/debian/'
FILES = {
    'brcmfmac43455-sdio.bin': 'config/brcm80211/cypress/cyfmac43455-sdio-minimal.bin',
    'brcmfmac43455-sdio.txt': 'config/brcm80211/brcm/brcmfmac43455-sdio.txt',
    'brcmfmac43455-sdio.clm_blob': 'config/brcm80211/cypress/cyfmac43455-sdio.clm_blob',
    'COPYRIGHT.txt': 'copyright',
}


def configuration(ssid, password, country):
    encoded = ssid.encode('utf8')
    if not 1 <= len(encoded) <= 32 or not 8 <= len(password) <= 63 or any(ord(c) < 32 or ord(c) > 126 for c in password):
        raise ValueError('SSID must be 1..32 bytes; WPA passphrase must be 8..63 printable ASCII characters')
    country = country.upper()
    if len(country) != 2 or not country.isascii() or not country.isalpha():
        raise ValueError('Supply the actual two-letter regulatory country code')
    psk = hashlib.pbkdf2_hmac('sha1', password.encode('ascii'), encoded, 4096, 32).hex()
    return f'country={country}\nupdate_config=0\nnetwork={{\n ssid={encoded.hex()}\n psk={psk}\n key_mgmt=WPA-PSK\n}}\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path, help='e.g. build/sdcard')
    parser.add_argument('--ssid')
    parser.add_argument('--country')
    parser.add_argument('--password-file', type=Path)
    args = parser.parse_args()
    if bool(args.ssid) != bool(args.country) or (args.password_file and not args.ssid):
        parser.error('SSID and country must be supplied together')
    firmware = args.destination / 'FIRMWARE'
    firmware.mkdir(parents=True, exist_ok=True)
    hashes = {}
    for name, source in FILES.items():
        data = urllib.request.urlopen(BASE + source, timeout=30).read()
        if not data:
            raise ValueError('Empty firmware download')
        (firmware / name).write_bytes(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
    (firmware / 'PROVENANCE.json').write_text(json.dumps({'revision': REVISION, 'sha256': hashes}, indent=2) + '\n')
    if args.ssid:
        password = args.password_file.read_text().rstrip('\r\n') if args.password_file else getpass.getpass('Wi-Fi passphrase: ')
        config = configuration(args.ssid, password, args.country)
        path = args.destination / 'wpa_supplicant.conf'
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'w') as output:
            output.write(config)
    print('Pi4 firmware prepared; credentials, when supplied, are stored only in wpa_supplicant.conf')


if __name__ == '__main__':
    main()
