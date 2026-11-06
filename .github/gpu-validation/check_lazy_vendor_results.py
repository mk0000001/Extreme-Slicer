"""Validate named production cache cases from native Catch XML, not JUnit SECTION counts."""
import argparse
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

def verify(xml_path, source_path):
    source = Path(source_path).read_text(encoding='utf-8-sig')
    expected = re.findall(r'TEST_CASE\s*\(\s*"([^"\n]+)"\s*,\s*"[^"\n]*\[LazyVendorVersion\][^"\n]*"', source)
    if len(expected) != 7 or len(set(expected)) != 7:
        raise ValueError('Expected seven distinct registered production lazy-version cases')
    root = ET.parse(xml_path).getroot()
    if root.tag not in ('Catch', 'Catch2TestRun'):
        raise ValueError('Native Catch XML required; JUnit SECTION counts are not case counts')
    actual = [node.get('name') for node in root.findall('TestCase')]
    if len(actual) != 7 or len(set(actual)) != 7 or set(actual) != set(expected):
        raise ValueError('Actual test identities differ from the production source')
    for tag, count in [('OverallResults', 85), ('OverallResultsCases', 7)]:
        totals = root.find(tag)
        if totals is None or int(totals.get('successes', '-1')) != count:
            raise ValueError('Expected85 successful assertions in seven successful cases')
        if any(int(totals.get(key, '0')) != 0 for key in ('failures', 'expectedFailures', 'skips')):
            raise ValueError('Failed, skipped or expected-failure cases are not a pass')
    for case in root.findall('TestCase'):
        result = case.find('OverallResult')
        if result is None or result.get('success') != 'true' or int(result.get('skips', '0')) != 0:
            raise ValueError('Each actual named case must succeed without skips')
    return {'status': 'ACTUAL_NATIVE_LAZY_VENDOR_CONTRACTS_PASS', 'registered_cases': 7,
            'successful_assertions': 85, 'failed_assertions': 0, 'case_names': actual,
            'startup_timing_or_speedup_proof': False}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--xml', required=True)
    parser.add_argument('--source', required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.xml, args.source)))
