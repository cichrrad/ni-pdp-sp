#!/bin/bash

# Ensure correct usage
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <graph_list.txt> <graph_source_dir>"
    exit 1
fi

GRAPH_LIST="$1"
SOURCE_DIR="$2"
CSV_FILE="results.csv"

# Clear CSV file and write the header
echo "filename,partition_size,recursive_calls,time,min_cut_weight" > "$CSV_FILE"

# Read each line from the input file
while read -r FILENAME SIZE; do
    GRAPH_PATH="${SOURCE_DIR}/${FILENAME}"

    if [ -f "$GRAPH_PATH" ]; then
        echo "Processing $FILENAME with a=$SIZE..."

        # Run mincut and capture output
        OUTPUT=$(./build/mincut "$GRAPH_PATH" "$SIZE")

        # Extract relevant information
        RECURSIVE_CALLS=$(echo "$OUTPUT" | grep "Total Recursion Calls" | awk '{print $4}')
        TIME=$(echo "$OUTPUT" | grep "Execution Time" | awk '{print $3}')
        MIN_CUT_WEIGHT=$(echo "$OUTPUT" | grep "Best Min-Cut Weight Found" | awk '{print $5}')

        # Ensure values exist before writing to CSV
        if [[ -n "$RECURSIVE_CALLS" && -n "$TIME" && -n "$MIN_CUT_WEIGHT" ]]; then
            echo "$FILENAME,$SIZE,$RECURSIVE_CALLS,$TIME,$MIN_CUT_WEIGHT" >> "$CSV_FILE"
        else
            echo "WARNING: Could not extract values for $FILENAME" | tee -a "$CSV_FILE"
        fi
    else
        echo "WARNING: File $GRAPH_PATH not found, skipping..."
    fi
done < "$GRAPH_LIST"

echo "Processing complete. Results saved to $CSV_FILE."
