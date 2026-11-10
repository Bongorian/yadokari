#!/usr/bin/env python3
"""Render per-case qualification tables from preserved device/host JSON."""
import argparse, json
from pathlib import Path
from summarize_final import summarize


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runs',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--version',type=int,required=True)
    p.add_argument('--retest-version',type=int)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    lines=['# 原典音源とstreamの再検証', '', '各条件は2秒warmup後の10秒窓。要求した全音源が発音したという意味ではなく、active/refusedはadmission結果。初回・移行中の欠落はraw logに残す。', '']
    cases=[(name,requested,a.version) for name,requested in [('native',16),('audio',12)]]
    if a.retest_version:cases += [(name,requested,a.retest_version) for name,requested in [('native',16),('audio',12)]]
    for name,requested,version in cases:
        path=a.runs/f'{name}-{version}.json'
        if not path.exists():continue
        s=summarize(path);complete=s['completion_observed'] and not s['reset_observed'] and not s['communication_errors'] and len(s['workers'])==19 and len(s['ui_windows'])==19 and len(s['admission_in_case_order'])==19
        if name=='audio':complete=complete and len(s['streams'])==19 and len(s['recordings'])==19 and all(r['saved'] and r['verified'] and r['assigned'] and r['frames'] for r in s['recordings'])
        lines += [f'## {name}: v{version}', '', f'[Raw evidence](runs/{path.name})。全19窓＋復帰＋通信／reset確認: {"完了" if complete else "未完了"}。要求native {requested} Track。', '', '| Case | 音色 | active/refused | worker平均/最大 µs | 欠落frame | stream miss | FPS L/R | 最大gap µs L/R | 入力service最大 µs | 窓判定 |', '|---|---|---|---|---|---|---|---|---|---|']
        for i,w in enumerate(s['workers']):
            c=w['case'];u=next((r for r in s['ui_windows'] if r['case']==c),{});st=next((r for r in s['streams'] if r['case']==c),{})
            adm=s['admission_in_case_order'][i] if i<len(s['admission_in_case_order']) else {}
            counts=f"{adm['active_mask'].bit_count()}/{adm['refused_mask'].bit_count()}" if adm else '?'
            sound=f'RESONATOR {c}' if c<3 else f'TEXTURE {(c-3)//4} / Q{(c-3)%4}'
            ui_ok=u.get('qualified',False)
            lines.append(f"| {c} | {sound} | {counts} | {w['mean_us']}/{w['max_us']} | {w['missing_frames']} | {st.get('stream_misses','—')} | {u.get('left_fps','?')}/{u.get('right_fps','?')} | {u.get('left_gap_us','?')}/{u.get('right_gap_us','?')} | {u.get('input_service_gap_us','?')} | {'PASS' if w['continuous'] and ui_ok else 'FAIL/不足'} |")
        lines += ['','入力serviceはソフトウェアのpoll間隔であり、物理押下からLCDまでの遅延ではない。USB bad/ring増分と録音失敗も窓判定に含む。','']
    (a.output/'native-cases.md').write_text('\n'.join(lines)+'\n')
    hp=a.runs/'host-final/results.json'
    if hp.exists():
        h=json.loads(hp.read_text());done={r['target']:r for r in h['results']}
        lines=['# 最終ホスト回帰一覧','',f"開始source `{h['source']}`。{len(done)}/{len(h['selected'])} target終了、成功{sum(r['exit']==0 for r in done.values())}。runtime固定後のrun。firmware-onlyの最適化は別のCircle build／実機測定で判定。",'', '[機械可読結果](runs/host-final/results.json)。ASan/UBSan、target上限900秒。', '', '| Target | exit | 秒 | Log |','|---|---|---|---|']
        for target in h['selected']:
            r=done.get(target)
            lines.append(f"| {target} | {r['exit'] if r else '未完了'} | {r['seconds'] if r else '—'} | "+(f"[log](runs/host-final/{r['log']})" if r else '—')+' |')
        (a.output/'host-targets.md').write_text('\n'.join(lines)+'\n')
    print('Wrote qualification tables; raw evidence is unchanged')


if __name__=='__main__':main()
