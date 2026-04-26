# The following script describes the commands used to generate the test images
dd if=/dev/zero of=empty_512_16.img bs=512  count=16
dd if=/dev/zero of=empty_1024_8.img bs=1024 count=8

python3 create_lorem.py > lorem_ipsum.img

dd if=/dev/zero of=lorem_ipsum_write_order.img bs=512 count=4
dd if=/dev/zero of=lorem_ipsum_write_random_access.img bs=512 count=4
dd if=/dev/zero of=lorem_ipsum_write_failures.img bs=512 count=4

mkdir -p empty_folder
python3 create_disk.py 16 512 128 empty_folder > empty_disk.img
python3 create_disk.py 16 512 128 single_file  > single_file.img
python3 create_disk.py 16 512 2048 many_files   > many_files.img
