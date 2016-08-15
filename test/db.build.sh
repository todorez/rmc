#!/bin/sh
# This script verify a rmc tool built in the project
# by checking content in generated  RMC database file
# checksum is from a golden sample (a verified db file)

set -e

# If tester doesn't provide checksum of a new DB, we
# use a checksum of a fully tested DB which was
# generated with this script and data in ./boards.

# To test a new DB before update default md5, run

# $ RMC_TEST_DB_MD5="" db.build.sh

# New db will be left in a temp dir for other tests.

if [ -z ${RMC_TEST_DB_MD5+x} ]; then
    RMC_TEST_DB_MD5="9bfcda281c72bb142961882b5420d366"
fi

BOARDS_DIR="./boards"

# compile rmc tool first

make -C ../ clean 1>/dev/null
make -C ../ 1>/dev/null

# NOTE: Orders in lists of files matter

# Board #1
NUC6_FINGERPRINT="NUC6i5SYB_H.fp"
NUC6_FILES="NUC6.file.1 NUC6.file.2 NUC6.file.3"

# Board #2
NUC4_FINGERPRINT="NUC4.D54250WYK.fp"
NUC4_FILES="NUC4.file.1 NUC4.file.2"

# Board #3
T100_FINGERPRINT="T100TA-32bit.fp"
T100_FILES="T100.32.file.1 T100.32.file.2"

DB_RECORDS=

TEST_TMP_DIR=$(mktemp -d)

# $1: fingerprint file
# $2: a list of file blobs
generate_record_with_checksum () {
    local CHECKSUM_FILES=
    local FILE_BLOBS=
    local BOARD_REC=

    for each in $2; do
        md5sum $BOARDS_DIR/$each |cut -d ' ' -f 1 > ${TEST_TMP_DIR}/${each}.md5
        FILE_BLOBS="$FILE_BLOBS $BOARDS_DIR/$each"
        CHECKSUM_FILES="$CHECKSUM_FILES $TEST_TMP_DIR/$each.md5"
    done

    # We mean to test rmc tool built in the project.
    BOARD_REC=$(mktemp -p $TEST_TMP_DIR --suffix=.rec)
    ../src/rmc -R -f $BOARDS_DIR/$1 -b $FILE_BLOBS $CHECKSUM_FILES -o $BOARD_REC 1>/dev/null
    echo "$BOARD_REC"
}

# Order of records matters
DB_RECORDS="$(generate_record_with_checksum "${NUC6_FINGERPRINT}" "${NUC6_FILES}") \
            $(generate_record_with_checksum "$NUC4_FINGERPRINT" "$NUC4_FILES") \
            $(generate_record_with_checksum "$T100_FINGERPRINT" "$T100_FILES")"

../src/rmc -D $DB_RECORDS -o $TEST_TMP_DIR/rmc.db

DB_CHECKSUM=$(md5sum $TEST_TMP_DIR/rmc.db |cut -d ' ' -f 1)

make -C ../ clean

if [ -z "$RMC_TEST_DB_MD5" ]; then
    echo "An empty checksum is provided, assuming you want to create a new DB:"
    echo "$TEST_TMP_DIR/rmc.db [MD5] = $DB_CHECKSUM"
elif [ "$RMC_TEST_DB_MD5" = "$DB_CHECKSUM" ]; then
    echo "RMC Database generation test: PASS"
    rm -rf $TEST_TMP_DIR
else
    echo "RMC Database generation test: FAIL"
    echo "$TEST_TMP_DIR/rmc.db [MD5] = $DB_CHECKSUM"
    echo "expected DB [MD5] = $RMC_TEST_DB_MD5"
    echo "Artifacts in test are in $TEST_TMP_DIR"
    exit 1
fi
