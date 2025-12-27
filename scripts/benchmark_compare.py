#!/usr/bin/env python3
"""
ABOUTME: Benchmark comparison tool for tracking performance across commits
ABOUTME: Compares Catch2 XML results, stores in SQLite, detects regressions
"""

import sqlite3
import xml.etree.ElementTree as ET

def init_database(db_path):
    """Initialize database with benchmarks table and indexes"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS benchmarks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            commit_hash TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            benchmark_name TEXT NOT NULL,
            mean_ns REAL NOT NULL,
            lower_bound_ns REAL NOT NULL,
            upper_bound_ns REAL NOT NULL,
            std_dev_ns REAL NOT NULL,
            samples INTEGER,
            iterations INTEGER
        )
    """)

    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_commit_benchmark
        ON benchmarks(commit_hash, benchmark_name)
    """)

    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_timestamp
        ON benchmarks(timestamp)
    """)

    conn.commit()
    return conn

def parse_xml_benchmarks(xml_path):
    """Parse Catch2 XML file and extract benchmark results

    Returns list of dicts with keys: name, mean_ns, lower_bound_ns,
    upper_bound_ns, std_dev_ns, samples, iterations
    """
    tree = ET.parse(xml_path)
    root = tree.getroot()

    results = []

    for test_case in root.findall('.//TestCase'):
        for benchmark in test_case.findall('.//BenchmarkResults'):
            mean_elem = benchmark.find('mean')
            std_dev_elem = benchmark.find('standardDeviation')

            result = {
                'name': benchmark.get('name'),
                'mean_ns': float(mean_elem.get('value')),
                'lower_bound_ns': float(mean_elem.get('lowerBound')),
                'upper_bound_ns': float(mean_elem.get('upperBound')),
                'std_dev_ns': float(std_dev_elem.get('value')),
                'samples': int(benchmark.get('samples', 0)),
                'iterations': int(benchmark.get('iterations', 0))
            }

            results.append(result)

    return results
