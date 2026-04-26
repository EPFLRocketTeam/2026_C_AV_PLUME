
# Plume File System

The Plume File System is a lightweight file system designed for real time applications that generate
a **single never ending log file** and where no read is needed to it during runtime.

## Architecture of Plume

Plume is designed for applications that generate a stream of data at medium to high frequencies.
The flow of data is the following :
1. **Initialization of driver**: First, you write your own driver to interface with the physical drive (more about the tips on how to create it), and you create a Plume context by first giving it an arena buffer and the length of that buffer. After that, you call `plume_init` and `plume_open_write` to set up plume so it starts writting to the drive.
2. **Sending Plume data**: Whenever you want to write something, you should call `plume_write` with the appropriate buffer. You should make sure that the arena is sufficiently large to hold your entire buffer. If the arena is currently full, you will receive a status `PLUME_OK_RETRY`. If it is sensitive information, you should wait for the next tick and retry, and if it isn't you should just delete the information.
3. **Ticking**: Plume relies on ticking to decide when to flush pages. If your application is ready, you should call `plume_tick`, which will try flushing the next available page **if it is full** and if the physical drive is ready. It is important to note that at the end of the runtime, if the last page isn't full it will never be flushed. You should take that into account in the design of the data you output.

## Designing a driver

In order to design the driver, you should implement 4 methods `disk_information`, `read_block`, `write_block` and `write_block_ready`. It is recommended to implement `read_block` in a blocking way (it will only be called during init), and to implement `write_block` in a non-blocking way (using DMA for example).

If you are dealing with an SD card, it is also recommended to implement `write_block` with open-ended multiblocks writes (and if the next write isn't in the next block or does not happen quickly), you should stop the multiblock write.

## Example of data flow

```c++
int main () {
    // setup your driver here
    struct plume_driver driver = ...;

    struct plume_context context;

    // setup the arena
    uint8_t arena[1024 * 1024];
    context.arena_buffer = arena;
    context.arena_length = 1024 * 1024;

    plume_init(&context, &driver); // check it is PLUME_OK
    plume_open_write(&context);

    int i = 0;
    while (1) {
        i ++;
        plume_write(&context, (uint8_t*) &i, 4);
        plume_tick(&context);
    }
}
```

## Reading from the disk

We provide you with a command line interface to browse the disk and recover the data. To do so, you should have your physical disk on **a unix machine** or **WSL**, and run the `plume_cli` executable with your device node (e.g. `/dev/sdX` if it is an SD card) to start interfacing with it. You then have 3 commands availables :
1. **Clearing the disk**: For example, `plume_cli /dev/sdX clear 16`. This will clear the entire disk and will delete all your data, you should be careful when using that command. The number 16 represents the maximum number of files plus one (equivalent to the number of runs of your program before the disk is full).
2. **Listing files**: For example, `plume_cli /dev/sdX ls` will list the files on disk with indexes and sizes.
3. **Recovering a file**: For example, `plume_cli /dev/sdX cat 2 > myfile.txt` will recover the file of **the third run** into the file `myfile.txt`.

In order to have access to the executable, you should compile it using `cmake`, to do so you can run the following commands :
```sh
mkdir build
cd build

sudo apt-get update
sudo apt-get install -y g++ cmake valgrind libprotobuf-dev libssl-dev protobuf-compiler lcov

cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=OFF
cmake --build .
```

After which you should have the `plume_cli` executable in the `build` directory.

## Available Drivers

There are two available drivers in this repository, an inmemory driver for unit testing
and a unix driver for standard devices (such as for SD cards `/dev/sdX`) or loop devices (like `/dev/loopX`).
