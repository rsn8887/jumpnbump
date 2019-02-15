#!/bin/bash

#CURRENT_SCRIPT="`readlink -f $0`"
SRC_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo \#define JNB_VERSION \"`cat "${SRC_DIR}/VERSION"`\" > version.h
echo \#define JNB_VERSION \"`cat "${SRC_DIR}/VERSION"`\" > ${SRC_DIR}/version.h

