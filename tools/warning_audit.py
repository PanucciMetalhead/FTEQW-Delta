#!/usr/bin/env python3
"""Deduplicate compiler warning locations in saved build logs without suppressing them."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', type=Path, nargs='+')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    warnings = {}
    other = set()
    ansi = re.compile(r'\x1b\[[0-9;]*m')
    for log in args.logs:
        for raw in log.read_text(errors='replace').splitlines():
            line = ansi.sub('', raw)
            match = re.match(r'(.+?):(\d+)(?::\d+)?: warning: (.*)', line)
            if match:
                file, number, message = match.groups()
                prefix = str(ROOT) + '/'
                if file.startswith(prefix): file = file[len(prefix):]
                flag = re.search(r'\[(-W[^]]+)\]', message)
                category = flag[1] if flag else 'tool-warning'
                warnings[(file, int(number), category)] = message
            elif 'warning:' in line:
                other.add(line)
    entries = [{'file': file, 'line': number, 'category': category, 'message': message}
               for (file, number, category), message in sorted(warnings.items())]
    report = {'source_locations': len(entries),
              'categories': dict(sorted(Counter(w['category'] for w in entries).items())),
              'warnings': entries, 'other_warnings': sorted(other)}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ['source_locations', 'categories']}, indent=2))


if __name__ == '__main__':
    main()
