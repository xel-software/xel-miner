#!/bin/bash

mkdir build 2>/dev/null

_RELEASE_NAME="$(uname -s  | tr '[:upper:]' '[:lower:]')-$(arch)"
_BUILD_DIR=build/xel-miner-$_RELEASE_NAME

rm -Rf $_BUILD_DIR
mkdir $_BUILD_DIR

mkdir $_BUILD_DIR/ElasticPL
mkdir $_BUILD_DIR/work

cp xel_miner $_BUILD_DIR/

cp xel_miner $_BUILD_DIR/xel_miner
cp ElasticPL/ElasticPLFunctions.h $_BUILD_DIR/ElasticPL/ElasticPLFunctions.h
cp ElasticPL/libElasticPLFunctions.a $_BUILD_DIR/ElasticPL/libElasticPLFunctions.a

cd build
tar -cf xel-miner-$_RELEASE_NAME.tgz xel-miner-$_RELEASE_NAME