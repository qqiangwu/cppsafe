#! /bin/bash

set -e
set -o pipefail

function detect_inc() {
    inc=""
    starts_inc_lines=false
    echo | c++ -E -Wp,-v - 2>&1 | while read line
    do
        if [[ $line =~ "End of search list" ]];
        then
            export CPPSAFE_INC="${inc}"
            break
        fi

        if [[ $line =~ "search starts here" ]];
        then
            starts_inc_lines=true
            continue
        fi

        if [[ $starts_inc_lines == true ]];
        then
            incdir=$(echo ${line} | awk '{print $1}')
            inc="${inc} -isystem=${incdir}"
        fi
    done
}

detect_inc

bash test.sh
bash test.sh use-after-move.cpp
bash test.sh debug_functions.cpp

bash test.sh annotation_lifetime_const.cpp
bash test.sh annotation_owner_and_pointer.cpp
bash test.sh annotation_lifetime_inout.cpp
bash test.sh annotation_contract.cpp
