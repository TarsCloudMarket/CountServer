#!/bin/bash
PROJECT_BIN_DIR=$1
killall -9 CountServer
sleep 1s

set -x

rm -rf ${PROJECT_BIN_DIR}/CountServer/1/data
rm -rf ${PROJECT_BIN_DIR}/CountServer/2/data
rm -rf ${PROJECT_BIN_DIR}/CountServer/3/data

nohup ${PROJECT_BIN_DIR}/bin/CountServer --config=${PROJECT_BIN_DIR}/../count-1.conf > CountServer_1.log 2>&1 &
nohup ${PROJECT_BIN_DIR}/bin/CountServer --config=${PROJECT_BIN_DIR}/../count-2.conf > CountServer_2.log 2>&1 &
nohup ${PROJECT_BIN_DIR}/bin/CountServer --config=${PROJECT_BIN_DIR}/../count-3.conf > CountServer_3.log 2>&1 &