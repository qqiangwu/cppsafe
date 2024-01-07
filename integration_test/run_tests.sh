#! /bin/bash

set -e
set -o pipefail

no_err=$1

function detect_inc() {
    inc=""
    starts_inc_lines=false
    echo | c++ -E -xc++ -Wp,-v - 2>&1 | while read line
    do
        if [[ $line =~ "End of search list" ]];
        then
            echo "${inc}"
            return
        fi

        if [[ $line =~ "search starts here" ]];
        then
            starts_inc_lines=true
            continue
        fi

        if [[ $starts_inc_lines == true ]];
        then
            incdir=$(echo ${line} | awk '{print $1}')
            inc="${inc} --extra-arg=-isystem${incdir}"
        fi
    done
}

export CPPSAFE_INC=$(detect_inc)

function run()
{
    set +e

    echo "test $1"
    out=$(bash test.sh $1 2>&1)
    if [[ ! $? -eq 0 ]];
    then
        if [[ -z ${no_err} ]];
        then
            echo "test $1 failed, please rerun it with:"
            echo "    bash test.sh $1"
            echo "detail:"
            echo "${out}"
            exit 1
        else
            echo "    FAIL test $1"
        fi
    fi
}

for cpp in */*.cpp;
do
    run "${cpp}"
done

run debug_functions.cpp
run example.cpp