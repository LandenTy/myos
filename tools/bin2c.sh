#!/bin/bash
INPUT=$1
SYMBOL=$2
echo "#include <stdint.h>"
echo "uint8_t ${SYMBOL}_data[] = {"
xxd -i < "$INPUT" | head -n -1 | tail -n +1
echo "};"
echo "uint32_t ${SYMBOL}_size = sizeof(${SYMBOL}_data);"
