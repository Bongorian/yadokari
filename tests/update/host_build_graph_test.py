"""Check compile sharing without building or running instrument workloads."""
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
commands = subprocess.check_output([
    'make', '-B', '-n', 'build/test-backends', 'build/test-layer-preset',
    'build/test-current-save-recovery', 'build/test', 'build/test-plaits'], cwd=root, text=True)
names = re.search(r'^ENGINE_NAMES := (.+)$', (root/'src/engine_sources.mk').read_text(), re.M)[1].split()
for name in names:
    source = 'src/'+name+'.cpp'
    engine = [line for line in commands.splitlines() if source in line and '-c ' in line and 'build/engine-core/' in line]
    assert len(engine) == 1, (source, 'Each shared sanitized unit must be compiled once per profile', engine)
    assert '-fsanitize=address,undefined' in engine[0] and '-MMD' in engine[0]
for source in ('src/soundfont.cpp', 'src/fm6_voice.cpp', 'src/wavetable_bank.cpp',
               'third_party/daisysp/Source/Filters/ladder.cpp'):
    lines = [line for line in commands.splitlines() if source in line]
    assert len(lines) == 1 and '-c ' in lines[0], (source, lines)
    assert '-fsanitize=address,undefined' in lines[0] and '-MMD' in lines[0]
make = (root / 'Makefile').read_text()
for name in ('current-save-recovery', 'project-ownership', 'async-project-save', 'async-project-read'):
    assert re.search(r'^test-all:.*test-run-'+name+r'\b', make, re.M)
assert 'test-serial:\n\t$(MAKE) --no-print-directory test TEST_JOBS=1' in make
print('Host graph: shared engine/support objects, sanitizer/dependencies and maintained serial inventory passed')

# Offline optimized renderers and gprof retain their own compile flags.
variants = subprocess.check_output(['make', '-B', '-n', 'factory-content', 'showcase-render', 'fx-profile'], cwd=root, text=True)
for line in variants.splitlines():
    if '-c ' in line:
        assert '-c build/test-core-engine.o' not in line
profile_link = next(line for line in variants.splitlines() if '-o build/fx-profile-run' in line)
assert 'build/msfa/' not in profile_link and 'build/fx-profile/msfa/' in profile_link
print('Host graph: optimized renderers compile source; gprof excludes sanitizer objects')

# Generated compiler dependencies are authoritative for shared units. An
# unrelated display transport edit must not rebuild every engine translation unit.
for profile in ('engine-core','engine-tool','ui-tool'):
    rule=re.search(r'^build/'+profile+r'/%.o:([^\n]+)',make,re.M)[1]
    assert 'wildcard' not in rule and 'DEMO_HEADERS' not in rule, (profile,rule)
    assert '-include $(wildcard build/'+profile+'/*.d)' in make
print('Host graph: shared unit dependencies use actual compiler include graphs')
