#!/bin/bash

# Analyze G1GC logs to compare baseline vs optimized

if [ $# -lt 1 ]; then
    echo "Usage: $0 <gc-log-file>"
    echo "Example: $0 /tmp/gc-optimized.log"
    exit 1
fi

LOG_FILE=$1

if [ ! -f "$LOG_FILE" ]; then
    echo "ERROR: Log file not found: $LOG_FILE"
    exit 1
fi

echo "================================="
echo "G1GC Log Analysis"
echo "================================="
echo "Log file: $LOG_FILE"
echo ""

# Count pause types
echo "GC Pause Count:"
echo "---------------"
YOUNG_PAUSES=$(grep "Pause Young" "$LOG_FILE" | wc -l)
YOUNG_CONCURRENT=$(grep "Pause Young (Concurrent Start)" "$LOG_FILE" | wc -l)
YOUNG_NORMAL=$(grep "Pause Young (Normal)" "$LOG_FILE" | wc -l)
REMARK_PAUSES=$(grep "Pause Remark" "$LOG_FILE" | wc -l)
CLEANUP_PAUSES=$(grep "Pause Cleanup" "$LOG_FILE" | wc -l)
FULL_GC=$(grep "Pause Full" "$LOG_FILE" | wc -l)

echo "  Young (Total):             $YOUNG_PAUSES"
echo "  Young (Concurrent Start):  $YOUNG_CONCURRENT"
echo "  Young (Normal):            $YOUNG_NORMAL"
echo "  Remark:                    $REMARK_PAUSES"
echo "  Cleanup:                   $CLEANUP_PAUSES"
echo "  Full GC:                   $FULL_GC"
echo ""

# Pause time statistics
echo "Pause Time Statistics:"
echo "----------------------"

# Extract pause times from lines like: "GC(10) Pause Young (Normal) (G1 Evacuation Pause) 1996M->897M(2128M) 37.974ms"
grep "Pause Young\|Pause Full" "$LOG_FILE" | \
    sed 's/.*) //' | \
    awk '{print $NF}' | \
    sed 's/ms$//' | \
    awk '
    BEGIN { min=999999; max=0; sum=0; count=0; }
    {
        sum += $1;
        count++;
        if ($1 < min) min = $1;
        if ($1 > max) max = $1;
    }
    END {
        if (count > 0) {
            avg = sum / count;
            print "  Total pauses:    " count;
            print "  Total time:      " sum " ms";
            print "  Min pause:       " min " ms";
            print "  Max pause:       " max " ms";
            print "  Average pause:   " avg " ms";
        } else {
            print "  No pause data found";
        }
    }'

echo ""

# Evacuation failures
echo "Evacuation Failures:"
echo "--------------------"
EVAC_FAILURES=$(grep "Evacuation Failure" "$LOG_FILE" | wc -l)
echo "  Count: $EVAC_FAILURES"
if [ $EVAC_FAILURES -gt 0 ]; then
    echo "  Details:"
    grep "Evacuation Failure" "$LOG_FILE" | head -5
fi
echo ""

# Concurrent mark cycles
echo "Concurrent Mark Cycles:"
echo "-----------------------"
MARK_CYCLES=$(grep "Concurrent Mark Cycle" "$LOG_FILE" | grep -v "GC.*Concurrent Mark Cycle$" | wc -l)
echo "  Count: $MARK_CYCLES"
if [ $MARK_CYCLES -gt 0 ]; then
    grep "Concurrent Mark Cycle" "$LOG_FILE" | grep -v "GC.*Concurrent Mark Cycle$" | \
        awk '{print $NF}' | sed 's/ms$//' | \
        awk 'BEGIN {sum=0; count=0;} {sum+=$1; count++;} END {if(count>0) print "  Avg time: " sum/count " ms"}'
fi
echo ""

# Humongous allocations
echo "Humongous Regions:"
echo "------------------"
grep "Humongous regions:" "$LOG_FILE" | tail -5 | \
    awk '{print "  " $0}'
echo ""

# Heap usage
echo "Heap Usage Pattern:"
echo "-------------------"
grep "Pause Young\|Pause Full" "$LOG_FILE" | \
    sed 's/.*) //' | \
    awk '{
        # Extract before->after(total)
        if ($0 ~ /[0-9]+M->[0-9]+M/) {
            split($0, a, " ");
            for (i in a) {
                if (a[i] ~ /[0-9]+M->[0-9]+M/) {
                    print "  " a[i];
                }
            }
        }
    }' | tail -10

echo ""
echo "================================="
echo "Summary"
echo "================================="

# Calculate GC overhead
TOTAL_TIME=$(grep "Pause Young\|Pause Full" "$LOG_FILE" | \
    sed 's/.*) //' | awk '{print $NF}' | sed 's/ms$//' | \
    awk 'BEGIN {sum=0} {sum+=$1} END {print sum}')

echo "Total GC pause time: ${TOTAL_TIME} ms"
echo ""
echo "To calculate GC overhead, you need application runtime."
echo "GC overhead = (Total GC time / Application runtime) * 100%"
echo ""
echo "If GC overhead is:"
echo "  < 5%:  GC is NOT the bottleneck, optimize I/O or algorithms"
echo "  5-20%: Moderate GC impact, optimizations may help"
echo "  > 20%: GC is a bottleneck, your optimizations WILL help"
