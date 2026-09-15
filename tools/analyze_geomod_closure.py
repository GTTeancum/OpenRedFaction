"""Summarize RF_GEOMOD_CLOSURE_ALL=1 interior-test output without hiding failures."""
import argparse,json,re
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path);args=parser.parse_args()
    rows=[];widths=[];matches=[];nearest=[]
    for line in args.log.read_text().splitlines():
        if line.startswith('CLOSURE_DIAGNOSTIC'):
            matches.append(int(re.search(r'matches(\d+)',line)[1]))
        if line.startswith('CLOSURE_INTERVAL'):
            widths.append(float(line.split()[2]))
        if line.startswith('CLOSURE_NEAREST'):
            fields=line.split();nearest.append(float(fields[4]) if int(fields[6]) else None)
        if line.startswith('STRESS '):
            assert len(matches)==len(widths)
            row=dict(stress=line,failed_intervals=len(widths),
                min_width=min(widths,default=0),max_width=max(widths,default=0),
                match_counts={str(k):matches.count(k) for k in sorted(set(matches))})
            if nearest:
                assert len(nearest)==len(widths)
                row.update(opposed_candidate_missing=sum(v is None for v in nearest),
                    largest_opposed_separation=max((v for v in nearest if v is not None),default=0),
                    longest_interval_opposed_separation=nearest[widths.index(max(widths))])
            rows.append(row);widths=[];matches=[];nearest=[]
    assert len(rows)==6 and not widths and not matches,'Expected six complete stress cuts'
    output=args.log.with_suffix('.summary.json')
    output.write_text(json.dumps(dict(scope='Intervals failing the existing closure tolerances; widths are edge lengths, not gap widths.',cuts=rows),indent=2)+'\n')
    for row in rows:print(row['stress'],'failed intervals',row['failed_intervals'],'largest interval',row['max_width'])
    print(output)

if __name__=='__main__':main()
