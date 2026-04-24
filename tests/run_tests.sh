
# This test script should be run inside the build/ folder
#   either as bash ../tests/run_tests.sh coverage
#   or as     bash ../tests/run_tests.sh memcheck

cp -r ../tests/drivers/images .

EMPTY_512_16=$(sudo losetup -f --show --sector-size 512  images/empty_512_16.img)
EMPTY_1024_8=$(sudo losetup -f --show --sector-size 1024 images/empty_1024_8.img)
LOREM_IPSUM=$(sudo losetup -f --show --sector-size 512 images/lorem_ipsum.img)
LOREM_IPSUM_WO=$(sudo losetup -f --show --sector-size 512 images/lorem_ipsum_write_order.img)
LOREM_IPSUM_WR=$(sudo losetup -f --show --sector-size 512 images/lorem_ipsum_write_random_access.img)
LOREM_IPSUM_WF=$(sudo losetup -f --show --sector-size 512 images/lorem_ipsum_write_failures.img)

echo [fixture] empty_512_16.img: $EMPTY_512_16
echo [fixture] empty_1024_8.img: $EMPTY_1024_8
echo [fixture] lorem_ipsum.img: $LOREM_IPSUM
echo [fixture] lorem_ipsum_write_order.img: $LOREM_IPSUM_WO
echo [fixture] lorem_ipsum_write_random_access.img: $LOREM_IPSUM_WR
echo [fixture] lorem_ipsum_write_failures.img: $LOREM_IPSUM_WF

export EMPTY_512_16
export EMPTY_1024_8
export LOREM_IPSUM
export LOREM_IPSUM_WO
export LOREM_IPSUM_WR
export LOREM_IPSUM_WF

sudo -E $@

echo [fixture] cleanup
sudo losetup -d "$EMPTY_512_16"
sudo losetup -d "$EMPTY_1024_8"
sudo losetup -d "$LOREM_IPSUM"
sudo losetup -d "$LOREM_IPSUM_WO"
sudo losetup -d "$LOREM_IPSUM_WR"
sudo losetup -d "$LOREM_IPSUM_WF"

rm -rf ./images
