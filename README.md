# fcks-sched

## Setup
```shell
git submodule update --init --recursive
```

## Build process
```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Running
```shell
./build/fcks_sched
```