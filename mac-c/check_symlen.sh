#!/bin/bash
# Determine MAC's symbol significance empirically from the golden dump:
# if LBYTE, LBYTE1 and LBYTE2 all appear as distinct entries, significance
# is greater than 5 characters.
GOLD=../reference/ASYMB.SYMB
echo "--- entries matching LBYTE / SBYTE ---"
tr -d ' \r' < "$GOLD" | grep -E '^(LBYTE|SBYTE)'
echo
echo "--- name-length histogram in the golden dump ---"
tr -d ' \r' < "$GOLD" | grep '=' | cut -d= -f1 | \
  awk '{ print length }' | sort -n | uniq -c
echo
echo "--- longest names ---"
tr -d ' \r' < "$GOLD" | grep '=' | cut -d= -f1 | \
  awk '{ print length, $0 }' | sort -rn | head -8
