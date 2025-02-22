#!/bin/bash

# Ensure correct usage
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <graph_list.txt>"
    exit 1
fi

GRAPH_LIST="$1"
LOG_FILE="results.log"

# Clear log file before starting
echo "Running mincut on all graph files..." > "$LOG_FILE"
echo "===================================" >> "$LOG_FILE"

# Read each line of the input file
while read -r FILENAME SIZE; do
    if [ -f "../graphs-dataset/${FILENAME}" ]; then
        echo "Processing ../graphs-dataset/${FILENAME} with a=$SIZE..." | tee -a "$LOG_FILE"
        ./build/mincut "../graphs-dataset/${FILENAME}" "$SIZE" >> "$LOG_FILE" 2>&1
        echo "-----------------------------------" >> "$LOG_FILE"
    else
        echo "WARNING: File ../graphs-dataset/${FILENAME} not found, skipping..." | tee -a "$LOG_FILE"
    fi
done < "$GRAPH_LIST"

echo "Processing complete. Results saved to $LOG_FILE."

