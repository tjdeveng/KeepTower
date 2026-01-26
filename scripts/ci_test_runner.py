#!/usr/bin/env python3
import sys
import os
import json
import subprocess
import re
import argparse
import time

# ANSI colors
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_header(msg):
    print(f"{Colors.HEADER}{Colors.BOLD}=== {msg} ==={Colors.ENDC}")

def print_success(msg):
    print(f"{Colors.OKGREEN}✓ {msg}{Colors.ENDC}")

def print_fail(msg):
    print(f"{Colors.FAIL}✗ {msg}{Colors.ENDC}")

def print_warn(msg):
    print(f"{Colors.WARNING}⚠ {msg}{Colors.ENDC}")

class TestRunner:
    def __init__(self, build_dir, baseline_file):
        self.build_dir = build_dir
        self.baseline_file = baseline_file
        self.baselines = self._load_baselines()
        self.failed_blocking = False
        self.failed_non_blocking = False

    def _load_baselines(self):
        if not os.path.exists(self.baseline_file):
            print_warn(f"Baseline file not found at {self.baseline_file}. Creating default.")
            return {}
        with open(self.baseline_file, 'r') as f:
            return json.load(f)

    def run_suite(self, name, executable, blocking=True, check_performance=False):
        print_header(f"Running Suite: {name}")

        cmd = [os.path.join(self.build_dir, 'tests', executable)]

        # If checking performance, we need to capture output specifically
        # Otherwise standard interaction is fine

        start_time = time.time()
        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False
            )
        except FileNotFoundError:
             print_fail(f"Executable not found: {executable}")
             if blocking: self.failed_blocking = True
             return

        duration = time.time() - start_time

        # Print output slightly indented
        for line in result.stdout.splitlines():
            print(f"  {line}")

        if result.returncode != 0:
            print_fail(f"Suite {name} FAILED (Exit Code: {result.returncode})")
            if blocking:
                self.failed_blocking = True
            else:
                self.failed_non_blocking = True
                print_warn("Non-blocking failure ignored for merge gate.")
        else:
            print_success(f"Suite {name} PASSED in {duration:.2f}s")

            if check_performance:
                self._analyze_performance(result.stdout)

    def _analyze_performance(self, output):
        print_header("Performance Analysis")

        # Parse output for timing information
        # Looking for lines like: "Migration of 19 users took: 1234ms"
        # and "SHA3-256: 100 iterations in 5ms"

        # 1. Batch Migration
        match = re.search(r"Migration of 19 users took: (\d+)ms", output)
        if match:
            actual = int(match.group(1))
            limit = self.baselines.get('migration_performance', {}).get('batch_20_users_max_ms', 30000)
            self._check_metric("Batch Migration (20 users)", actual, limit)

        # 2. SHA3 Speed
        match = re.search(r"SHA3-256:\s+\d+\s+iterations in\s+(\d+)ms", output)
        if match:
            actual = int(match.group(1))
            limit = self.baselines.get('hash_computation', {}).get('sha3_256_max_ms', 10)
            self._check_metric("SHA3-256 Speed", actual, limit)

        # 3. PBKDF2 Speed
        match = re.search(r"PBKDF2:\s+\d+\s+iterations in\s+(\d+)ms", output)
        if match:
            actual = int(match.group(1))
            # Relaxed limit for CI environments which might be slower
            limit = self.baselines.get('hash_computation', {}).get('pbkdf2_max_ms', 100) * 1.5
            self._check_metric("PBKDF2 Speed", actual, limit)

    def _check_metric(self, name, actual, limit):
        if actual > limit:
            print_fail(f"{name}: {actual}ms > {limit}ms (Regression!)")
            # Performance regressions are non-blocking by default in Priority 3, but we warn loudly
            self.failed_non_blocking = True
        else:
            print_success(f"{name}: {actual}ms <= {limit}ms")

def main():
    parser = argparse.ArgumentParser(description='CI Test Runner for Username Hash Migration')
    parser.add_argument('--build-dir', default='build', help='Path to meson build directory')
    args = parser.parse_args()

    # Paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    baseline_path = os.path.join(project_root, 'tests', 'data', 'performance_baseline.json')

    runner = TestRunner(args.build_dir, baseline_path)

    # ---------------------------------------------------------
    # Priority 1: Core Logic (Strict Blocking)
    # ---------------------------------------------------------
    runner.run_suite(
        "P1: Core Logic",
        "username_hash_migration_test",
        blocking=True
    )

    # ---------------------------------------------------------
    # Priority 2: Advanced Scenarios (Strict Blocking)
    # ---------------------------------------------------------
    runner.run_suite(
        "P2: Advanced Scenarios",
        "username_hash_migration_priority2_test",
        blocking=True
    )

    # ---------------------------------------------------------
    # Priority 3: Performance & Edge Cases (Conditional)
    # ---------------------------------------------------------
    runner.run_suite(
        "P3: Performance & Edge",
        "username_hash_migration_priority3_test",
        blocking=False, # Treated as separate gating logic usually, but here we flag it
        check_performance=True
    )

    # ---------------------------------------------------------
    # Concurrency (Flaky Management)
    # ---------------------------------------------------------
    # Simple retry logic for concurrency tests
    max_retries = 3
    for i in range(max_retries):
        print_header(f"P3: Concurrency (Attempt {i+1}/{max_retries})")
        cmd = [os.path.join(args.build_dir, 'tests', 'username_hash_migration_concurrency_test')]

        start_time = time.time()
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        if result.returncode == 0:
            print_success(f"Concurrency Tests PASSED in {time.time() - start_time:.2f}s")
            break
        else:
            print_warn(f"Concurrency Tests FAILED (Exit: {result.returncode})")
            if i == max_retries - 1:
                print_fail("Concurrency Tests FAILED after max retries")
                runner.failed_blocking = True # Concurrency failure after retries is blocking

    # Final Summary
    print("\n" + "="*40)
    if runner.failed_blocking:
        print_fail("BUILD FAILED: Blocking tests failed.")
        sys.exit(1)
    elif runner.failed_non_blocking:
        print_warn("BUILD UNSTABLE: Non-blocking tests failed or performance regression.")
        sys.exit(0) # Warnings don't fail the build in this config, just alert
    else:
        print_success("BUILD SUCCESS: All tests passed.")
        sys.exit(0)

if __name__ == "__main__":
    main()
