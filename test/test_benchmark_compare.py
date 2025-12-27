import unittest
import sqlite3
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))
from benchmark_compare import init_database

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

if __name__ == '__main__':
    unittest.main()
