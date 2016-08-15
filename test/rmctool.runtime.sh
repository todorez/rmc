#!/bin/sh
# This script is to run on a target board to verify basic
# functions of rmc tool and libraries linked into it in
# Linux user space.

usage () {
    echo "rmctool.runtime.sh rmc_database [test_temp_dir]"
    echo "RMC_BIN=pathname_of_rmc rmctool.runtime.sh rmc_database [test_temp_dir]"
}

if [ ! $# -eq 2 ] && [ ! $# -eq 1 ]; then
    usage
    exit 1
fi

RMC_DB_FILE=$1

while true; do
    echo "Select target to test [1-3]:"
    echo "1 NUC Gen6 i5SYB(H)"
    echo "2 NUC Gen4 D54250WYK"
    echo "3 T100TA (32bit)"

    read input

    if [ $input -lt 4 ] && [ $input -gt 0 ]; then
        break;
    fi

done

BOARDS="NUC6 NUC4 T100TA"

RMC_TEST_TARGET=$(echo $BOARDS|cut -d ' ' -f $input)

NUC6_FILES="NUC6.file.1 NUC6.file.2 NUC6.file.3"
NUC6_FP_MD5="35c53989630d16b983ac3da277beacc2"

NUC4_FILES="NUC4.file.1 NUC4.file.2"
NUC4_FP_MD5="dbf3162170a377eb0d78d822a7624d12"

T100TA_FILES="T100.32.file.1 T100.32.file.2"
T100TA_FP_MD5="59eb7a5aab9924587e433de88bf3e174"

# Prepare dir for test
if [ -z ${2:+x} ]; then
    TEST_DIR=$(mktemp -d)
else
    TEST_DIR=$2
    rm -rf $TEST_DIR
    mkdir -p $TEST_DIR
fi

if [ -z ${RMC_BIN:+x} ]; then
    RMC_BIN=$(which rmc)
fi

echo "Test: Start - Data will be stored in $TEST_DIR"
echo "Test: rmc tool in test: $RMC_BIN"
echo ""
echo "Test: Obtain Fingerprint - $RMC_TEST_TARGET"
# Test: obtain fingerprint

if $RMC_BIN -F -o $TEST_DIR/rmc.test.fp 1>/dev/null; then
    RUNTIME_FP=$(md5sum $TEST_DIR/rmc.test.fp|cut -d ' ' -f 1)
    eval EXPECTED_FP_MD5=\$${RMC_TEST_TARGET}_FP_MD5
    if [ "$EXPECTED_FP_MD5" != "$RUNTIME_FP" ]; then
        echo "Test Failed: obtained fingerprint $TEST_DIR/rmc.test.fp differs from the expected"
        exit 1
    fi
else
    echo "Test Failed: obtain fingerprint"
    exit 1
fi

# Positive Test
eval FILES_TO_QUERY=\$${RMC_TEST_TARGET}_FILES
echo "Test: Positive Query - $RMC_TEST_TARGET"
for each in $FILES_TO_QUERY; do
    if $RMC_BIN -B "$each" -d "$RMC_DB_FILE" -o "$TEST_DIR/$each"; then
        if $RMC_BIN -B "$each.md5" -d "$RMC_DB_FILE" -o "$TEST_DIR/$each.md5"; then
            FILE_MD5_IN_DB=$(cat "$TEST_DIR/$each.md5")
            if [ ! "$FILE_MD5_IN_DB" = "$(md5sum $TEST_DIR/$each |cut -d ' ' -f 1)" ]; then
                echo "Test Failed: File $TEST_DIR/$each is obtained but with mismatched checksum"
                exit 1
            fi
        else
            echo "Test Failed: Cannot query checksum file of $each from database $RMC_DB_FILE"
            exit 1
        fi
    else
        echo "Test Failed: Cannot query $each from database $RMC_DB_FILE"
        exit 1
    fi
done

# Negative Test. We shall not get any files specific to another type of board
# Note: test data used should have different file names across boards
for board in $BOARDS; do
    # skip the right board
    if [ "$board" = "$RMC_TEST_TARGET" ]; then
        continue
    fi
    echo "Test: Negative Query - $board"
    eval FILES_TO_QUERY=\$${board}_FILES
    for each in $FILES_TO_QUERY; do
        if $RMC_BIN -B "$each" -d "$RMC_DB_FILE" -o "$TEST_DIR/$each"; then
            echo "Test Failed: rmc tool returned success when querying $each specific to another board type $board from $RMC_DB_FILE"
            exit 1
        elif ls "$TEST_DIR/$each" 2>/dev/null; then
            echo "Test Failed: rmc tool generated data when querying failed"
            exit 1
        fi
    done
done

echo "Test: All Pass - Data in $TEST_DIR"
