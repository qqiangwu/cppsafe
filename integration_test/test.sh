srcfile="example.cpp"
extra=""

if [[ ! -z $1 ]];
then
    srcfile=$1
fi

if [[ ! -z $2 ]];
then
    extra=$2;
fi

if [[ -f ../build/debug/cppsafe ]];
then
    binary=../build/debug/cppsafe
else
    binary=../build/cppsafe
fi

src_args=$(head -n1 ${srcfile} | grep "// ARGS:" | awk -F ':' '{print $2}')

$binary "${srcfile}" $src_args $extra -- -Xclang -verify -std=c++20 -w
