#!/bin/bash

readonly PROGNAME=$(basename "$0")
readonly CACHE_PATH=./.cache

export PYTHONIOENCODING=utf8

DEBUG_OUTPUT="false"
CONTINUE="false"
EVAL_CONTRACTS="true"
FETCH_CONTRACTS="true"
JSON_RPC="false"
JSON_RPC_ENDPOINT="http://localhost:8545"
POROSITY_TIMEOUT="10"
TOUCH_CONTRACTS="true"

clear_cache() {
  rm -rf $CACHE_PATH
  echo "Local cache cleared"
}

configure_rpc() {
  echo "Using JSON-RPC endpoint: ${JSON_RPC_ENDPOINT}"
  # TODO: allow endpoint to be configurable
}

eval_contracts() {
  for path in $CACHE_PATH/0x*; do
    address=${path##*/}
    bytecode=$(cat $CACHE_PATH/${address}/bytecode)
    abi=""
    if [ -f $CACHE_PATH/${address}/abi ]; then
      abi=$(cat $CACHE_PATH/${address}/abi)
    fi
    eval_contract $address $bytecode $abi
  done
}

eval_contract() {
  address=$1
  bytecode=$2
  abi=$3

  if [ -z "$bytecode" ]; then
    echo "WARNING: Skipping evaluation of contract at ${address}; no bytecode resolved"
    return -1
  fi

  if [ "$DEBUG_OUTPUT" == "true" ]; then
    echo "DEBUG: Using porosity timeout value of ${POROSITY_TIMEOUT}"

    printf -- "-----------------------------------------------"
    printf -- "Contract ${address}:"
    printf -- "-----------------------------------------------\n"
    printf -- "Bytecode:"
    printf -- "-----------------------------------------------"
    printf -- "${bytecode}"
    printf -- "-----------------------------------------------\n"
    printf -- "ABI:"
    printf -- "-----------------------------------------------"
    printf -- "${abi}"
    printf -- "-----------------------------------------------\n"
  fi

  interface=-1
  disassembler=-1
  decompiler=-1

  if [ -z "$abi" ]; then
    echo "WARNING: Evaluating contract at ${address} without ABI"
    interface=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --code $bytecode --list >> $CACHE_PATH/${address}/interface)
  else
    if [ "$DEBUG_OUTPUT" == "true" ]; then
      echo "DEBUG: Evaluating contract at ${address} with ABI ${abi}"
    fi
    interface=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --abi $abi --code $bytecode --list > $CACHE_PATH/${address}/interface)
  fi

  if [ $? -eq 124 ]; then  # timed out exit status
    echo "WARNING: Evaluation of contract at ${address} has failed; timed out after ${porosity_timeout} seconds"
    return -1
  else
    if [ -z "$abi" ]; then
      disassembler=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --code $bytecode --disassm > $CACHE_PATH/${address}/opcodes)
      decompiler=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --code $bytecode --decompile > $CACHE_PATH/${address}/decompiled_source)
    else
      disassembler=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --abi $abi --code $bytecode--disassm > $CACHE_PATH/${address}/opcodes)
      decompiler=$(timeout --signal KILL $POROSITY_TIMEOUT porosity --abi $abi --code $bytecode --decompile > $CACHE_PATH/${address}/decompiled_source)
    fi

    printf -- "-----------------------------------------------\n"
    printf -- "Finished scanning contract ${address}:\n"
    printf -- "-----------------------------------------------\n\n"
  fi

  return 0
}

fetch_etherscan_contract() {
  address=$1

  echo "Fetching Ethereum contract address from etherscan: ${address}"
  response=$(curl --silent https://etherscan.io/address/${address} | sed 's/<br>/\n&/g' | sed 's/<br\/>/\n&/g' | sed "s/<pre class='wordwrap'(.*)>/\\n&/gI")

  bytecode=$(echo "$response" | grep -iv 'ace.js' \
                              | grep -iv 'js-sourcecopyarea' \
                              | grep -iv 'js-copytextarea' \
                              | grep -iv 'constructor arguments' \
                              | grep -iv 'bzzr://' \
                              | grep -iv '12pc' \
                              | egrep '<pre|verifiedbytecode|15pc' \
                              | sed "s/<div id='verifiedbytecode2'/&\\n/gI" \
                              | awk -v pattern='>(.*)[<\\/pre>|<\\/div>]$' '{ while (match($0, pattern)) { printf("%s\n", substr($0, RSTART + 1, RLENGTH - 7)); $0=substr($0, RSTART + RLENGTH) } }' \
                              | sed 's/<\/div>$//g')

  echo "${bytecode}" > $CACHE_PATH/${address}/bytecode

  if [ "$DEBUG_OUTPUT" == "true" ]; then
    echo "DEBUG: Retrieved bytecode ${bytecode} for contract at address: ${address}"
  fi

  fetch_etherscan_contract_abi $address
}

fetch_etherscan_contract_abi() {
  address=$1

  if [ "$DEBUG_OUTPUT" == "true" ]; then
    echo "Fetching Ethereum contract ABI from etherscan: ${address}"
  fi

  abi=$(curl --silent https://api.etherscan.io/api?module=contract&action=getabi&address=${address} | python -c "import sys, json; print json.load(sys.stdin)['result']")

  if [ ! -z "$abi" ]; then
    if [ "$DEBUG_OUTPUT" == "true" ]; then
      echo "DEBUG: Retrieved abi ${abi} for contract at address: ${address}"
    fi

    echo "${abi}" > $CACHE_PATH/${address}/abi
  fi
}

fetch_rpc_contract() {
  address=$1
  block=$2
  if [ -z "$block" ]; then
    block="latest"
  fi

  bytecode=$(curl --silent -X POST $JSON_RPC_ENDPOINT --data "{\"jsonrpc\": \"2.0\", \"method\": \"eth_getCode\", \"params\": [\"${address}\", \"${block}\"], \"id\": 1}" | python -c "import sys, json; print json.load(sys.stdin)['result']")
  echo "${bytecode}" > $CACHE_PATH/${address}/bytecode

  if [ "$DEBUG_OUTPUT" == "true" ]; then
    echo "DEBUG: Retrieved bytecode ${bytecode} for contract at address: ${address}"
  fi

  fetch_etherscan_contract_abi $address
}

fetch_contracts() {
  for path in $CACHE_PATH/0x*; do
    address=${path##*/}

    if [ "$JSON_RPC" == "false" ]; then
      fetch_etherscan_contract $address
    elif [ "$JSON_RPC" == "true" ]; then
      fetch_rpc_contract $address
    fi
  done
}

touch_contract() {
  address=$1
  creation_block=$2

  mkdir -p $CACHE_PATH/${address}

  if [ ! -z "$creation_block" ]; then
    echo "${creation_block}" > $CACHE_PATH/${address}/.creation_block
  fi
}

touch_etherscan_contracts() {
  page=1
  max_page=400
  while [ $page -le $max_page ]
  do
    echo "Fetching Ethereum contract addresses via Etherscan (${page}/${max_page})"
    curl --silent https://etherscan.io/accounts/c/${page} | sed 's/<\/a[^>]*>/\n&/g' | awk -v pattern='>0x(.*)$' '{ while (match($0, pattern)) { printf("%s\n", substr($0, RSTART + 1, RLENGTH)); $0=substr($0, RSTART + RLENGTH) } }' | while read address; do
      touch_contract $address
    done

    page=$((page + 1))
  done
}

touch_rpc_contracts() {
  # TODO-- calculate cached_block & historical_scan_complete
  block=$(($(curl --silent -H 'content-type: application/json' -X POST $JSON_RPC_ENDPOINT --data "{\"jsonrpc\": \"2.0\", \"method\": \"eth_blockNumber\", \"params\": [], \"id\": 83}" | awk -F\" '{print toupper($10)}')))

  while [ $block -gt 0 ]
  do
    hex=$(echo "obase=16; ${block}" | bc)

    if [ "$DEBUG_OUTPUT" == "true" ]; then
      echo "Fetching block ${hex} via JSON-RPC"
    fi

    transactions=($(curl --silent -H 'content-type: application/json' -X POST $JSON_RPC_ENDPOINT --data "{\"jsonrpc\": \"2.0\", \"method\": \"eth_getBlockByNumber\", \"params\": [\"0x${hex}\", true], \"id\": 1}" | python -c "import sys, json; txns = json.load(sys.stdin)['result']['transactions']; print '\n'.join(map(lambda tx: tx['hash'], txns))"))

    if [ "$DEBUG_OUTPUT" == "true" ]; then
      echo "Fetched ${transactions[@]} transactions for block 0x${hex}"
    fi

    for txn_hash in "${transactions[@]}"; do
      if [ "$DEBUG_OUTPUT" == "true" ]; then
        echo "Fetching transaction ${txn_hash}"
      fi

      contract_address=$(curl --silent -H 'content-type: application/json' -X POST $JSON_RPC_ENDPOINT --data "{\"jsonrpc\": \"2.0\", \"method\": \"eth_getTransactionReceipt\", \"params\": [\"${txn_hash}\"], \"id\": 1}" | python -c "import sys, json; print json.load(sys.stdin)['result']['contractAddress']" | awk '{gsub("None", "", $0); print $0}')
      if [ ! -z "$contract_address" ]; then
        if [ "$DEBUG_OUTPUT" == "true" ]; then
          echo "Transaction ${txn_hash} created contract: ${contract_address}"
        fi

        touch_contract $contract_address "0x${hex}"
      fi
    done

    block=$((block - 1))
  done
}

touch_contracts() {
  echo "Starting scan of Ethereum network for contracts"

  if [ "$JSON_RPC" == "false" ]; then
    touch_etherscan_contracts
  elif [ "$JSON_RPC" == "true" ]; then
    touch_rpc_contracts
  fi
}

cleanup() {
  echo "${PROGNAME} exiting"
}

main() {
  mkdir -p $CACHE_PATH

  trap cleanup EXIT INT TERM

  if [ "$CONTINUE" == "false" ]; then
    touch $CACHE_PATH/.params
    echo "$@" > $CACHE_PATH/.params

    if [ "$TOUCH_CONTRACTS" == "true" ]; then
      touch_contracts
    fi

    if [ "$FETCH_CONTRACTS" == "true" ]; then
      fetch_contracts
    fi

    if [ "$EVAL_CONTRACTS" == "true" ]; then
      eval_contracts
    fi
  else
    params=$(cat $CACHE_PATH/.params)
    echo "WARNING: --continue is not yet implemented; params: ${params}"
  fi
}

usage() {
  cat <<EOF
Usage:
  $PROGNAME [options]
Description:
  Scan the Ethereum network using etherscan.io (or JSON-RPC if --rpc is specified).
  This has only been tested on Ubuntu 16.04.2 LTS.
Options
  -c, --continue              resume scan of Ethereum network (WIP)
  -e, --eval-only             only evaluate previously cached contract addresses-- does not resolve contract addresses
  -f, --fetch-only            only fetches and caches bytecode and ABI for previously touched contract addresses-- does not invoke porosity
  -r, --rpc                   scan Ethereum network using JSON RPC
  -t, --touch-only            only caches contract addresses found by scanning Ethereum network-- does not invoke porosity
  -h, --help                  display this help and exit
  -v, --verbose               enable verbose output (not yet implemented)
  -d, --debug                 debug output
  --clear-cache               clear local contracts cache
  --porosity-timeout SECONDS  number of seconds to wait for porosity to evaluate a single contract (not yet supported; defaults to 10 seconds)
Example:
  $PROGNAME --clear-cache
  $PROGNAME --continue

EOF
  exit 2;
}

OPTS=$(getopt --options cdefhrtv --long clear-cache,continue,debug,eval-only,fetch-only,help,porosity-timeout:,rpc,touch-only,verbose -n "${PROGNAME}" -- "$@")
eval set -- "$OPTS"
while true ; do
  case "$1" in
    --clear-cache) clear_cache ; shift ;;
    # TODO: --porosity-timeout) ; shift ;;
    -t|--touch-only) EVAL_CONTRACTS="false" && FETCH_CONTRACTS="false" ; shift ;;
    -c|--continue) CONTINUE="true" ; shift ;;
    -e|--eval-only) FETCH_CONTRACTS="false" && TOUCH_CONTRACTS="false" ; shift ;;
    -f|--fetch-only) EVAL_CONTRACTS="false" && TOUCH_CONTRACTS="false" ; shift ;;
    -r|--rpc) JSON_RPC="true" && configure_rpc ; shift ;;
    -h|--help) usage ; shift ;;
    -d|--debug) DEBUG_OUTPUT="true" ; shift ;;
    -v|--verbose) echo "WARNING: --verbose not yet implemented" && VERBOSE_OUTPUT="true" ; shift ;;
    --) main ; shift ; break ;;
    *) echo "Invalid usage of ${PROGNAME}; see ${PROGNAME} --help" ; exit 1 ;;
  esac
done
