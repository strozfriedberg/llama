import sys
import xml.etree.ElementTree as ET

data = sys.stdin.read()
root = ET.fromstring(data)

test_case = [child for child in root if child.attrib.get("name") == "LlamaParserBenchmark"][0]
benchmark_results = [child for child in test_case if child.attrib.get("name") == "parser"][0]
mean_stats = [child for child in benchmark_results if child.tag == "mean"][0]

commit = sys.argv[1]
mean = mean_stats.attrib.get("value")
lower_bound = mean_stats.attrib.get("lowerBound")
upper_bound = mean_stats.attrib.get("upperBound")

with open("test/benchmarks/parser_benchmark_report.csv", "a") as f:
    if f.tell() == 0:
        f.write("commit_hash,mean,lower_bound,upper_bound\n")
    f.write(f"{commit},{mean},{lower_bound},{upper_bound}\n")
