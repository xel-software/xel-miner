#!/bin/bash

cd /xel-miner

echo "set config..."

CONF_API_PASS=""
if [ ! -z ${nxt_adminPassword+x} ]
then
  echo "set nxt.adminPassword=HIDDEN"
  CONF_API_PASS="-p \"${nxt_adminPassword}\""
fi

CONF_WALLET_PASSPHRASE=""
if [ ! -z ${miner_wallet_passphrase+x} ]
then
  echo "set miner_wallet_passphrase=HIDDEN"
  CONF_WALLET_PASSPHRASE=${miner_wallet_passphrase}
fi

CONF_NB_THREADS="-t 1"
if [ ! -z ${miner_number_of_threads+x} ]
then
  echo "set miner_number_of_threads=${miner_number_of_threads}"
  CONF_NB_THREADS="-t ${miner_number_of_threads}"
fi

CONF_PORT="17876"
if [[ ! -z ${miner_testnet+x} && "${miner_testnet}" == 'true' ]]
then
  echo "set miner_testnet=${miner_testnet}"
  CONF_PORT="16876"
fi

#echo "local=${miner_use_local_node}"
CONF_NODE_ADD="http://computation-01.xel.org:$CONF_PORT"
if [[ ! -z ${miner_use_local_node} && "${miner_use_local_node}" == 'true' ]]
then
  echo "set miner_use_local_node=${miner_use_local_node}"
  CONF_NODE_ADD="http://172.41.0.8:$CONF_PORT"
fi

echo "xel_miner ${CONF_NB_THREADS} ${CONF_WALLET_PASSPHRASE} -D -o ${CONF_NODE_ADD} ${CONF_API_PASS}"

./xel_miner ${CONF_NB_THREADS} -P "${CONF_WALLET_PASSPHRASE}" -D -o "${CONF_NODE_ADD}" "${CONF_API_PASS}"
