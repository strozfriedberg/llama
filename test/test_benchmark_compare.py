import unittest
import sqlite3
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))
from benchmark_compare import init_database, parse_xml_benchmarks, store_benchmarks, load_benchmarks_for_commit, get_most_recent_commit

class TestDatabaseInit(unittest.TestCase):
    def setUp(self):
        self.test_db = "/tmp/test_benchmarks.db"
        if os.path.exists(self.test_db):
            os.remove(self.test_db)

    def tearDown(self):
        if os.path.exists(self.test_db):
            os.remove(self.test_db)

    def test_init_database_creates_table(self):
        """Database initialization creates benchmarks table with correct schema"""
        conn = init_database(self.test_db)
        cursor = conn.cursor()

        # Check table exists
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='benchmarks'")
        self.assertIsNotNone(cursor.fetchone())

        # Check schema
        cursor.execute("PRAGMA table_info(benchmarks)")
        columns = {row[1]: row[2] for row in cursor.fetchall()}

        expected_columns = {
            'id': 'INTEGER',
            'commit_hash': 'TEXT',
            'timestamp': 'TEXT',
            'benchmark_name': 'TEXT',
            'mean_ns': 'REAL',
            'lower_bound_ns': 'REAL',
            'upper_bound_ns': 'REAL',
            'std_dev_ns': 'REAL',
            'samples': 'INTEGER',
            'iterations': 'INTEGER'
        }

        for col, col_type in expected_columns.items():
            self.assertIn(col, columns)
            self.assertEqual(columns[col], col_type)

        conn.close()

    def test_init_database_creates_indexes(self):
        """Database initialization creates required indexes"""
        conn = init_database(self.test_db)
        cursor = conn.cursor()

        cursor.execute("SELECT name FROM sqlite_master WHERE type='index'")
        indexes = [row[0] for row in cursor.fetchall()]

        self.assertIn('idx_commit_benchmark', indexes)
        self.assertIn('idx_timestamp', indexes)

        conn.close()

class TestXMLParser(unittest.TestCase):
    def test_parse_xml_benchmarks(self):
        """Parse Catch2 XML and extract benchmark results"""
        xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<Catch2TestRun name="benchmarks">
  <TestCase name="LlamaLexerBenchmark">
    <BenchmarkResults name="lexer" samples="100" resamples="100000" iterations="8">
      <mean value="1900.82" lowerBound="1887.06" upperBound="1933.43" ci="0.95"/>
      <standardDeviation value="102.154" lowerBound="40.1679" upperBound="177.695" ci="0.95"/>
    </BenchmarkResults>
  </TestCase>
  <TestCase name="LlamaParser">
    <BenchmarkResults name="parser" samples="100" iterations="9">
      <mean value="1727.63" lowerBound="1629.58" upperBound="2017.73" ci="0.95"/>
      <standardDeviation value="784.178" lowerBound="315.857" upperBound="1697.8" ci="0.95"/>
    </BenchmarkResults>
  </TestCase>
</Catch2TestRun>"""

        with open('/tmp/test_bench.xml', 'w') as f:
            f.write(xml_content)

        results = parse_xml_benchmarks('/tmp/test_bench.xml')

        self.assertEqual(len(results), 2)

        # Check first benchmark
        self.assertEqual(results[0]['name'], 'lexer')
        self.assertEqual(results[0]['mean_ns'], 1900.82)
        self.assertEqual(results[0]['lower_bound_ns'], 1887.06)
        self.assertEqual(results[0]['upper_bound_ns'], 1933.43)
        self.assertEqual(results[0]['std_dev_ns'], 102.154)
        self.assertEqual(results[0]['samples'], 100)
        self.assertEqual(results[0]['iterations'], 8)

        # Check second benchmark
        self.assertEqual(results[1]['name'], 'parser')
        self.assertEqual(results[1]['mean_ns'], 1727.63)

        os.remove('/tmp/test_bench.xml')

class TestStoreBenchmarks(unittest.TestCase):
    def setUp(self):
        self.test_db = "/tmp/test_benchmarks.db"
        if os.path.exists(self.test_db):
            os.remove(self.test_db)
        self.conn = init_database(self.test_db)

    def tearDown(self):
        self.conn.close()
        if os.path.exists(self.test_db):
            os.remove(self.test_db)

    def test_store_benchmarks(self):
        """Store benchmark results in database"""
        benchmarks = [
            {
                'name': 'lexer',
                'mean_ns': 1900.82,
                'lower_bound_ns': 1887.06,
                'upper_bound_ns': 1933.43,
                'std_dev_ns': 102.154,
                'samples': 100,
                'iterations': 8
            },
            {
                'name': 'parser',
                'mean_ns': 1727.63,
                'lower_bound_ns': 1629.58,
                'upper_bound_ns': 2017.73,
                'std_dev_ns': 784.178,
                'samples': 100,
                'iterations': 9
            }
        ]

        count = store_benchmarks(self.conn, benchmarks, 'abc1234', '2025-12-27T10:00:00')

        self.assertEqual(count, 2)

        # Verify stored data
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM benchmarks WHERE commit_hash='abc1234' ORDER BY benchmark_name")
        rows = cursor.fetchall()

        self.assertEqual(len(rows), 2)

        # Check first row (lexer)
        self.assertEqual(rows[0][2], '2025-12-27T10:00:00')  # timestamp
        self.assertEqual(rows[0][3], 'lexer')  # benchmark_name
        self.assertEqual(rows[0][4], 1900.82)  # mean_ns
        self.assertEqual(rows[0][5], 1887.06)  # lower_bound_ns
        self.assertEqual(rows[0][6], 1933.43)  # upper_bound_ns

class TestLoadBenchmarks(unittest.TestCase):
    def setUp(self):
        self.test_db = "/tmp/test_benchmarks.db"
        if os.path.exists(self.test_db):
            os.remove(self.test_db)
        self.conn = init_database(self.test_db)

        # Store some test data
        benchmarks = [
            {'name': 'lexer', 'mean_ns': 1900.82, 'lower_bound_ns': 1887.06,
             'upper_bound_ns': 1933.43, 'std_dev_ns': 102.154, 'samples': 100, 'iterations': 8},
            {'name': 'parser', 'mean_ns': 1727.63, 'lower_bound_ns': 1629.58,
             'upper_bound_ns': 2017.73, 'std_dev_ns': 784.178, 'samples': 100, 'iterations': 9}
        ]
        store_benchmarks(self.conn, benchmarks, 'abc1234', '2025-12-26T10:00:00')
        store_benchmarks(self.conn, benchmarks, 'def5678', '2025-12-27T10:00:00')

    def tearDown(self):
        self.conn.close()
        if os.path.exists(self.test_db):
            os.remove(self.test_db)

    def test_load_benchmarks_for_commit(self):
        """Load benchmarks for a specific commit"""
        results = load_benchmarks_for_commit(self.conn, 'abc1234')

        self.assertEqual(len(results), 2)
        self.assertEqual(results[0]['name'], 'lexer')
        self.assertEqual(results[0]['mean_ns'], 1900.82)
        self.assertEqual(results[1]['name'], 'parser')

    def test_get_most_recent_commit(self):
        """Get most recent commit hash by timestamp"""
        commit_hash, timestamp = get_most_recent_commit(self.conn)

        self.assertEqual(commit_hash, 'def5678')
        self.assertEqual(timestamp, '2025-12-27T10:00:00')

if __name__ == '__main__':
    unittest.main()
