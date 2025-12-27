#!/usr/bin/env python3
"""
ABOUTME: Benchmark comparison tool for tracking performance across commits
ABOUTME: Compares Catch2 XML results, stores in SQLite, detects regressions
"""

import sqlite3

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
