#!/usr/bin/env python3
import re
import sys
import os

def parse_benchmark_log(filename):
    """Parse TPC-DS benchmark log and extract query times."""
    results = {}
    try:
        with open(filename, 'r') as f:
            content = f.read()
            for line in content.split('\n'):
                # Pattern 1: "q3: 1234 ms" or "q3 1234 ms"
                match = re.search(r'q(\d+)[:\s]+(\d+(?:\.\d+)?)\s*(ms|seconds?)', line, re.IGNORECASE)
                if match:
                    query_num = match.group(1)
                    time_val = float(match.group(2))
                    unit = match.group(3).lower()
                    if 'second' in unit:
                        time_val *= 1000
                    results[f'q{query_num}'] = time_val

                # Pattern 2: Table format
                match = re.search(r'\|\s*q(\d+)\s*\|\s*(\d+(?:\.\d+)?)\s*\|', line)
                if match:
                    query_num = match.group(1)
                    time_val = float(match.group(2))
                    results[f'q{query_num}'] = time_val
    except Exception as e:
        print(f"Error parsing {filename}: {e}", file=sys.stderr)

    return results

def compare_results(baseline, optimized):
    """Compare baseline and optimized results."""
    all_queries = sorted(set(baseline.keys()) | set(optimized.keys()))

    if not all_queries:
        return None

    comparisons = []
    total_baseline = 0
    total_optimized = 0

    for query in all_queries:
        b_time = baseline.get(query, 0)
        o_time = optimized.get(query, 0)

        if b_time > 0 and o_time > 0:
            improvement = ((b_time - o_time) / b_time) * 100
            total_baseline += b_time
            total_optimized += o_time
            comparisons.append({
                'query': query,
                'baseline': b_time,
                'optimized': o_time,
                'improvement': improvement
            })

    return comparisons, total_baseline, total_optimized

# Main execution
baseline = parse_benchmark_log('benchmark-baseline.log') if os.path.exists('benchmark-baseline.log') else {}
optimized = parse_benchmark_log('benchmark-optimized.log') if os.path.exists('benchmark-optimized.log') else {}

print("\n" + "="*60)
print("QUERY-BY-QUERY COMPARISON")
print("="*60)

if not baseline or not optimized:
    print("\n⚠️  Could not extract timing data from logs.")
    print("Manual review of logs required.")
    print("\nBaseline queries found:", len(baseline))
    print("Optimized queries found:", len(optimized))
else:
    result = compare_results(baseline, optimized)
    if result:
        comparisons, total_baseline, total_optimized = result

        print(f"\n{'Query':<10} {'Baseline (ms)':<15} {'Optimized (ms)':<15} {'Improvement':<15}")
        print("-" * 60)

        for comp in comparisons:
            improvement_str = f"{comp['improvement']:+.1f}%"
            if comp['improvement'] > 0:
                improvement_str = f"✅ {improvement_str}"
            elif comp['improvement'] < -5:
                improvement_str = f"❌ {improvement_str}"
            else:
                improvement_str = f"   {improvement_str}"

            print(f"{comp['query']:<10} {comp['baseline']:<15.1f} {comp['optimized']:<15.1f} {improvement_str:<15}")

        if total_baseline > 0:
            overall = ((total_baseline - total_optimized) / total_baseline) * 100
            print("-" * 60)
            print(f"{'TOTAL':<10} {total_baseline:<15.1f} {total_optimized:<15.1f} {overall:+.1f}%")
            print("\n" + "="*60)
            print(f"Overall Performance: {overall:+.1f}% improvement")
            print("="*60)

        # Save results to file
        with open('comparison_results.txt', 'w') as f:
            f.write(f"Query,Baseline(ms),Optimized(ms),Improvement(%)\n")
            for comp in comparisons:
                f.write(f"{comp['query']},{comp['baseline']:.1f},{comp['optimized']:.1f},{comp['improvement']:.1f}\n")
            if total_baseline > 0:
                overall = ((total_baseline - total_optimized) / total_baseline) * 100
                f.write(f"TOTAL,{total_baseline:.1f},{total_optimized:.1f},{overall:.1f}\n")
