"""Validate native Catch XML identities; JUnit SECTION entries are not case counts."""
import argparse,json,re,xml.etree.ElementTree as ET
from pathlib import Path

def verify(xml_path,source_path):
 source=Path(source_path).read_text(encoding='utf-8-sig')
 expected=re.findall(r'TEST_CASE\s*\(\s*"([^"\n]+)"\s*,\s*"[^"\n]*\[CudaDispatchBuffers\][^"\n]*"',source)
 if len(expected)!=17 or len(set(expected))!=17:raise ValueError('Expected17 distinct reviewed registered buffer cases')
 root=ET.parse(xml_path).getroot()
 if root.tag not in ('Catch','Catch2TestRun'):raise ValueError('Expected native Catch XML; no JUnit cardinality inference')
 actual=[node.get('name') for node in root.findall('.//TestCase')]
 if len(actual)!=len(set(actual)) or set(actual)!=set(expected):raise ValueError('Actual registered test identities differ')
 overall=root.find('OverallResults');cases=root.find('OverallResultsCases')
 if overall is None or cases is None:raise ValueError('Native Catch run totals missing')
 if int(overall.get('successes','-1'))!=145 or int(overall.get('failures','-1'))!=0 or int(overall.get('expectedFailures','0'))!=0:raise ValueError('Expected145 actual successful assertions and no failed assertions')
 if int(cases.get('successes','-1'))!=17 or int(cases.get('failures','-1'))!=0 or int(cases.get('expectedFailures','0'))!=0:raise ValueError('Expected17 actual successful cases')
 return {'status':'ACTUAL_NATIVE_CATCH_BUFFER_CONTRACTS_PASS','registered_cases':17,'successful_assertions':145,'failed_assertions':0,'case_names':actual,'GPU_or_speedup_proof':False}

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--xml',required=True);parser.add_argument('--source',required=True);args=parser.parse_args()
 print(json.dumps(verify(args.xml,args.source)))
