#!/bin/bash

OUTPUT=0

function print_message {
  printf "\n\n"
  echo "==============================="
  echo "$1"
  echo "==============================="
}

function check_and_run {
  if [ -f $1 ]; then
    if [ $OUTPUT -eq 0 ]; then
      if ! eval $1 &> /dev/null ; then
        print_message "$1 failed"
        exit -1
      fi
    else
      if ! eval $1 ; then
        print_message "$1 failed"
        exit -1
      fi
    fi
  else
    print_message "$1 not existed, failed at compilation ???"
    exit -1
  fi
}

if [ $# -ne 0 ]; then
  OUTPUT=1
fi

check_and_run "bin/jinja-test"
check_and_run "bin/opt-test"
check_and_run "bin/unit-test"

print_message "All tests passed!"
