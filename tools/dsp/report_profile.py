#!/usr/bin/env python3
"""Render tables from a completed profile; never choose a favourable repeat."""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('profile', type=Path)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--adopted-manifest', type=Path)
a = p.parse_args()
data = json.loads(a.profile.read_text())
adopted = {c['case']: c for c in json.loads(a.adopted_manifest.read_text())['cases']} if a.adopted_manifest else {}
assert data['suite'] and not data['suite']['aborted'] and data['suite']['restored']
lines = ['# DSP測定表', '',
         '生成元: `tools/dsp/report_profile.py`。µsは24frame当たり。平均/最大は観測値であり全状態の上限ではない。',
         '細分timerの費用を含む。nativeは内部voice数ではなくTrack DSP全体。`—`はactive値未取得で較正不可。', '',
         '## 宣言costと実測', '',
         '|Case|音源 / variant|旧/採用cost|単位|平均µs|最大µs|参考平均/最大cost|参考voice枠|',
         '|---:|---|---:|---|---:|---:|---:|---:|']

def f(value):
    return '—' if value is None else f'{value:.3f}'

for c in data['cost_table']:
    m = c['measurement'] or {}
    selected = adopted.get(c['case'], c)
    lines.append(f"|{c['case']}|{c['name']} / {c['variant']}|{c['declared_cost']}/{selected['declared_cost']}|{c['measurement_unit']}|"
                 f"{f(m.get('mean_active_24_us'))}|{f(m.get('max_active_24_us'))}|"
                 f"{c.get('reference_mean_cost', '—')}/{c.get('reference_peak_cost', '—')}|"
                 f"{c.get('reference_safe_poly_slots') if c.get('reference_safe_poly_slots') is not None else '—'}|")
lines += ['', '## Native予約µs', '',
          '|Case|音源 / variant|旧平均/burst|採用平均/burst|実測平均/最大block|安全率1.25参考平均/burst|',
          '|---:|---|---:|---:|---:|---:|']
for c in data['cost_table']:
    if not c['native_avg_us']:
        continue
    m = c['measurement'] or {}
    selected = adopted.get(c['case'], c)
    lines.append(f"|{c['case']}|{c['name']} / {c['variant']}|{c['native_avg_us']}/{c['native_burst_us']}|"
                 f"{selected['native_avg_us']}/{selected['native_burst_us']}|"
                 f"{f(m.get('mean_active_24_us'))}/{f(m.get('max_block_us'))}|"
                 f"{c.get('native_reference_avg', '—')}/{c.get('native_reference_burst', '—')}|")
lines += ['', '## Insert FX', '',
          'Track Aの実呼出しを1条件ごとに集計（番号と名称は `src/fx_registry.h`）。', '',
          '|Case|FX type|平均µs/block|最大µs/block|worker平均/最大µs|',
          '|---:|---:|---:|---:|---:|']
for c in data['completed']:
    if c['kind'] != 2:
        continue
    m = next((b for b in c['bins'] if b['kind'] == 'insert' and b['path'] == 0 and b['type'] == c['variant']), None)
    if m is None:
        assert c['variant'] == 0, 'Missing measured FX bin'
        m = {'all_us': 0, 'max_block_us': 0}  # BYPASS skips process entirely.
    d = c['deadline']
    lines.append(f"|{c['case']}|{c['variant']}|{f(m['all_us']/c['profile']['sampled'])}|"
                 f"{f(m['max_block_us'])}|{d['avg_us']}/{d['max_us']}|")
systems = ['Drive', 'Filter', 'Track compressor', 'Delay', 'Reverb', 'Throws',
           'Beat repeat', 'Performance', 'Master compressor', 'Limiter', 'Record/capture', 'Reserved']
lines += ['', '## 全FX chainの段別内訳', '',
          'inclusiveなstageと、その内部にあるengine/FX binは加算しない。Timer未呼出しの段は未測定。', '',
          '|段|平均µs/block|最大µs/block|', '|---|---:|---:|']
for c in data['completed']:
    if c['kind'] != 6:
        continue
    for b in c['bins']:
        if b['kind'] not in ('system', 'stage', 'insert'):
            continue
        name = systems[b['system']] if b['kind'] == 'system' else (
            ['Control', 'Events', 'Tracks', 'Output', 'Total'][b['stage']] if b['kind'] == 'stage'
            else f"Insert path {b['path']} type {b['type']}")
        lines.append(f"|{name}|{f(b['all_us']/c['profile']['sampled'])}|{f(b['max_block_us'])}|")
lines += ['', '## 全条件のdeadline / 欠落', '',
          'worker平均は500µs deadlineに対する値。profileのcontrol最大はwarmup/loadを含む。',
          'native-active/refusedはbitmask。Layersの理由は同じraw logにTrack別で保持。', '',
          '|Case|kind / 音源 / variant|要求Track×note|main/poly peak|平均/最大µs|余裕µs（平均）|>500µs|窓/条件欠落frame|control最大µs|native active/refused|',
          '|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|']
for c in data['completed']:
    d, s = c['deadline'], c['profile']
    lines.append(f"|{c['case']}|{c['kind']} / {c['name']} / {c['variant']}|{c['tracks']}×{c['notes']}|"
                 f"{s['main_peak']}/{s['poly_peak']}|{d['avg_us']}/{d['max_us']}|{d['mean_headroom_us']}|"
                 f"{d['over500']}|{d['window_missing']}/{d['condition_missing']}|{f(s['control_max_us'])}|"
                 f"{d['native_active']:04x}/{d['refused']:04x}|")
lines += ['', f"Suite: `{json.dumps(data['suite'], sort_keys=True)}`。Skipped: {len(data['skipped'])}。",
          f"Log SHA-256: `{data['log_sha256']}`。Manifest SHA-256: `{data['manifest_sha256']}`。", '']
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text('\n'.join(lines))
