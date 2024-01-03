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

for cpp in *.cpp;
do
    echo "test ${cpp}"
    bash test.sh "${cpp}"
done