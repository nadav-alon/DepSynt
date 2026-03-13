#!/bin/bash

FORMULA=""
INPUTS=""
OUTPUTS=""
SEQ=()

while [[ $# -gt 0 ]]; do
  case $1 in
    -f|--formula)
      FORMULA="$2"
      shift 2
      ;;
    -i|--input)
      INPUTS="$2"
      shift 2
      ;;
    -o|--output)
      OUTPUTS="$2"
      shift 2
      ;;
    *)
      SEQ+=("$1")
      shift
      ;;
  esac
done

if [[ -z "$FORMULA" || -z "$INPUTS" || -z "$OUTPUTS" || ${#SEQ[@]} -eq 0 ]]; then
  echo "Usage: $0 -f \"<formula>\" -i \"<inputs>\" -o \"<outputs>\" <sequence...>"
  echo "Example: $0 -f \"G(z <-> (x & y))\" -i \"x,y\" -o \"z\" 10 11 01 00"
  exit 1
fi

TMP_AIG=$(mktemp /tmp/transducer_XXXXXX.aag)

# Get the absolute path to the directory containing this script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=> Running depsynt to extract dependency transducer..."
"$DIR/depsynt" --formula "$FORMULA" --input "$INPUTS" --output "$OUTPUTS" --model-name "sim_dep" --dependency-timeout 1000 --dependency-transducer-path "$TMP_AIG" > /dev/null 2>&1

if [[ ! -s "$TMP_AIG" ]]; then
  echo "Error: depsynt failed or did not produce a dependency transducer."
  echo "Run depsynt manually to see what went wrong."
  rm -f "$TMP_AIG"
  exit 1
fi

echo "=> Transducer generated successfully. Running simulation..."
"$DIR/simulate_aiger" "$TMP_AIG" "${SEQ[@]}"

# Cleanup
rm -f "$TMP_AIG"
