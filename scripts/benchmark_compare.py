#!/usr/bin/env python3
"""
ABOUTME: Benchmark comparison tool for tracking performance across commits
ABOUTME: Compares Catch2 XML results, stores in SQLite, detects regressions
"""

import sqlite3
import xml.etree.ElementTree as ET
from datetime import datetime

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

def store_benchmarks(conn, benchmarks, commit_hash, timestamp=None):
    """Store benchmark results in database

    Args:
        conn: SQLite connection
        benchmarks: List of benchmark dicts from parse_xml_benchmarks
        commit_hash: Git commit hash or label
        timestamp: ISO 8601 timestamp (defaults to now)

    Returns:
        Number of benchmarks stored
    """
    if timestamp is None:
        timestamp = datetime.now().isoformat()

    cursor = conn.cursor()

    for bench in benchmarks:
        cursor.execute("""
            INSERT INTO benchmarks
            (commit_hash, timestamp, benchmark_name, mean_ns, lower_bound_ns,
             upper_bound_ns, std_dev_ns, samples, iterations)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            commit_hash,
            timestamp,
            bench['name'],
            bench['mean_ns'],
            bench['lower_bound_ns'],
            bench['upper_bound_ns'],
            bench['std_dev_ns'],
            bench['samples'],
            bench['iterations']
        ))

    conn.commit()
    return len(benchmarks)

def load_benchmarks_for_commit(conn, commit_hash):
    """Load benchmark results for a specific commit

    Returns list of dicts with same structure as parse_xml_benchmarks
    """
    cursor = conn.cursor()
    cursor.execute("""
        SELECT benchmark_name, mean_ns, lower_bound_ns, upper_bound_ns,
               std_dev_ns, samples, iterations
        FROM benchmarks
        WHERE commit_hash = ?
        ORDER BY benchmark_name
    """, (commit_hash,))

    results = []
    for row in cursor.fetchall():
        results.append({
            'name': row[0],
            'mean_ns': row[1],
            'lower_bound_ns': row[2],
            'upper_bound_ns': row[3],
            'std_dev_ns': row[4],
            'samples': row[5],
            'iterations': row[6]
        })

    return results

def get_most_recent_commit(conn):
    """Get the most recent commit hash and timestamp

    Returns tuple of (commit_hash, timestamp) or (None, None) if empty
    """
    cursor = conn.cursor()
    cursor.execute("""
        SELECT commit_hash, timestamp
        FROM benchmarks
        ORDER BY timestamp DESC
        LIMIT 1
    """)

    row = cursor.fetchone()
    if row:
        return row[0], row[1]
    return None, None

# Comparison result status constants
DEFINITELY_BETTER = "Definitely Better"
DEFINITELY_WORSE = "Definitely Worse"
PROBABLY_BETTER = "Probably Better"
PROBABLY_WORSE = "Probably Worse"
INSIGNIFICANT = "Insignificant"

def compare_benchmarks(baseline, current):
    """Compare two benchmark results using confidence interval analysis

    Args:
        baseline: Benchmark dict from parse_xml_benchmarks or load_benchmarks_for_commit
        current: Benchmark dict from parse_xml_benchmarks or load_benchmarks_for_commit

    Returns:
        Dict with keys: status, percent_change, baseline, current
    """
    mean_b = baseline['mean_ns']
    lower_b = baseline['lower_bound_ns']
    upper_b = baseline['upper_bound_ns']

    mean_n = current['mean_ns']
    lower_n = current['lower_bound_ns']
    upper_n = current['upper_bound_ns']

    percent_change = ((mean_n - mean_b) / mean_b) * 100

    # Determine status based on confidence interval analysis
    if upper_n < lower_b:
        status = DEFINITELY_BETTER
    elif lower_n > upper_b:
        status = DEFINITELY_WORSE
    elif upper_n < mean_b or mean_n < lower_b:
        status = PROBABLY_BETTER
    elif lower_n > mean_b or mean_n > upper_b:
        status = PROBABLY_WORSE
    else:
        status = INSIGNIFICANT

    return {
        'status': status,
        'percent_change': percent_change,
        'baseline': baseline,
        'current': current
    }
