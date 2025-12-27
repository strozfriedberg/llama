#!/usr/bin/env python3
"""
ABOUTME: Benchmark comparison tool for tracking performance across commits
ABOUTME: Compares Catch2 XML results, stores in SQLite, detects regressions
"""

import sqlite3
import xml.etree.ElementTree as ET
from datetime import datetime
import argparse
import os

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

def compare_benchmark_sets(baseline, current):
    """Compare two sets of benchmarks

    Args:
        baseline: List of benchmark dicts
        current: List of benchmark dicts

    Returns:
        Dict with keys:
            'comparisons': List of comparison results for matching benchmarks
            'new': List of benchmarks in current but not baseline
            'removed': List of benchmarks in baseline but not current
    """
    baseline_by_name = {b['name']: b for b in baseline}
    current_by_name = {b['name']: b for b in current}

    comparisons = []
    for name in sorted(current_by_name.keys()):
        if name in baseline_by_name:
            comp = compare_benchmarks(baseline_by_name[name], current_by_name[name])
            comp['name'] = name
            comparisons.append(comp)

    new = [current_by_name[name] for name in sorted(current_by_name.keys())
           if name not in baseline_by_name]

    removed = [baseline_by_name[name] for name in sorted(baseline_by_name.keys())
               if name not in current_by_name]

    return {
        'comparisons': comparisons,
        'new': new,
        'removed': removed
    }

def format_time(nanoseconds):
    """Format nanoseconds to human-readable time"""
    if nanoseconds < 1000:
        return f"{nanoseconds:.2f}ns"
    elif nanoseconds < 1000000:
        return f"{nanoseconds/1000:.2f}us"
    elif nanoseconds < 1000000000:
        return f"{nanoseconds/1000000:.2f}ms"
    else:
        return f"{nanoseconds/1000000000:.2f}s"

def format_comparison_output(comparison_result, baseline_info, verbose=False):
    """Format comparison results for display

    Args:
        comparison_result: Dict from compare_benchmark_sets
        baseline_info: Tuple of (commit_hash, timestamp)
        verbose: If True, include detailed CI information

    Returns:
        Formatted string output
    """
    lines = []

    commit_hash, timestamp = baseline_info
    lines.append(f"Comparing current results to baseline (commit: {commit_hash}, {timestamp})")
    lines.append("")

    # Table header
    lines.append(f"{'Benchmark':<30} {'Status':<20} {'Mean Change':<15} {'Baseline':<15} {'Current':<15}")
    lines.append("-" * 95)

    # Comparison rows
    for comp in comparison_result['comparisons']:
        name = comp['name']
        status = comp['status']
        change = f"{comp['percent_change']:+.1f}%"
        baseline_mean = format_time(comp['baseline']['mean_ns'])
        current_mean = format_time(comp['current']['mean_ns'])

        lines.append(f"{name:<30} {status:<20} {change:<15} {baseline_mean:<15} {current_mean:<15}")

        if verbose:
            bl = comp['baseline']
            cu = comp['current']
            lines.append(f"  Baseline: mean={format_time(bl['mean_ns'])}, CI=[{format_time(bl['lower_bound_ns'])}, {format_time(bl['upper_bound_ns'])}]")
            lines.append(f"  Current:  mean={format_time(cu['mean_ns'])}, CI=[{format_time(cu['lower_bound_ns'])}, {format_time(cu['upper_bound_ns'])}]")
            lines.append("")

    # New benchmarks
    if comparison_result['new']:
        lines.append("")
        lines.append("New benchmarks (not in baseline):")
        for bench in comparison_result['new']:
            lines.append(f"  - {bench['name']}")

    # Removed benchmarks
    if comparison_result['removed']:
        lines.append("")
        lines.append("Removed benchmarks (not in current):")
        for bench in comparison_result['removed']:
            lines.append(f"  - {bench['name']}")

    # Summary
    better = sum(1 for c in comparison_result['comparisons']
                 if c['status'] in [DEFINITELY_BETTER, PROBABLY_BETTER])
    worse = sum(1 for c in comparison_result['comparisons']
                if c['status'] in [DEFINITELY_WORSE, PROBABLY_WORSE])
    insignificant = sum(1 for c in comparison_result['comparisons']
                       if c['status'] == INSIGNIFICANT)

    lines.append("")
    lines.append(f"Summary: {better} better, {worse} worse, {insignificant} insignificant, "
                f"{len(comparison_result['new'])} new, {len(comparison_result['removed'])} removed")

    return "\n".join(lines)

def cmd_store(args):
    """Handle 'store' command"""
    if not os.path.exists(args.xml_file):
        print(f"Error: XML file not found: {args.xml_file}")
        return 1

    benchmarks = parse_xml_benchmarks(args.xml_file)

    conn = init_database(args.db)
    count = store_benchmarks(conn, benchmarks, args.commit)
    conn.close()

    print(f"Stored {count} benchmarks for commit {args.commit}")
    return 0

def cmd_compare(args):
    """Handle 'compare' command"""
    conn = init_database(args.db)

    # Determine baseline
    if args.baseline:
        baseline_commit = args.baseline
    else:
        baseline_commit, baseline_timestamp = get_most_recent_commit(conn)
        if baseline_commit is None:
            print("Error: No baseline found in database. Use --baseline or store results first.")
            conn.close()
            return 1

    baseline_benchmarks = load_benchmarks_for_commit(conn, baseline_commit)
    if not baseline_benchmarks:
        print(f"Error: No benchmarks found for baseline commit: {baseline_commit}")
        conn.close()
        return 1

    # Get timestamp for baseline
    cursor = conn.cursor()
    cursor.execute("SELECT timestamp FROM benchmarks WHERE commit_hash=? LIMIT 1",
                  (baseline_commit,))
    baseline_timestamp = cursor.fetchone()[0]

    # Determine current (XML file or commit hash)
    current_arg = args.current
    if current_arg.endswith('.xml'):
        if not os.path.exists(current_arg):
            print(f"Error: XML file not found: {current_arg}")
            conn.close()
            return 1
        current_benchmarks = parse_xml_benchmarks(current_arg)
    else:
        current_benchmarks = load_benchmarks_for_commit(conn, current_arg)
        if not current_benchmarks:
            print(f"Error: No benchmarks found for commit: {current_arg}")
            conn.close()
            return 1

    conn.close()

    # Compare and output
    comparison = compare_benchmark_sets(baseline_benchmarks, current_benchmarks)
    output = format_comparison_output(comparison, (baseline_commit, baseline_timestamp),
                                     verbose=args.verbose)
    print(output)

    return 0

def main():
    parser = argparse.ArgumentParser(
        description='Track and compare benchmark performance across commits'
    )
    parser.add_argument('--db', default='benchmarks.db',
                       help='Database path (default: benchmarks.db)')
    parser.add_argument('--verbose', action='store_true',
                       help='Show detailed CI information')

    subparsers = parser.add_subparsers(dest='command', required=True)

    # Store command
    store_parser = subparsers.add_parser('store',
                                         help='Store benchmark results in database')
    store_parser.add_argument('xml_file', help='Catch2 XML output file')
    store_parser.add_argument('--commit', required=True,
                             help='Commit hash or label')

    # Compare command
    compare_parser = subparsers.add_parser('compare',
                                          help='Compare benchmark results')
    compare_parser.add_argument('current',
                               help='Current results (XML file or commit hash)')
    compare_parser.add_argument('--baseline',
                               help='Baseline commit hash (default: most recent)')

    args = parser.parse_args()

    if args.command == 'store':
        return cmd_store(args)
    elif args.command == 'compare':
        return cmd_compare(args)

    return 1

if __name__ == '__main__':
    exit(main())
