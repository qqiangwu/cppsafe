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

../build/debug/cppsafe "${srcfile}" $CPPSAFE_INC --extra-arg=-Xclang --extra-arg=-verify --extra-arg=-isystem/Library/Developer/CommandLineTools/usr/include --extra-arg=-std=c++20 --extra-arg=-Wno-dangling-gsl --extra-arg=-Wno-return-stack-address --extra-arg=-Wno-unused $extra
