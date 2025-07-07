mkdir -p ./build
cd ./build
cmake ..
make

for file in $(ls ./t*); do
    echo ""
    echo "================================="
    echo "Running $file"
    LD_PRELOAD=./liblockdep_interpose.so ./$file
    echo "================================="
    echo ""
done
