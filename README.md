# Brief
🎬 slate: Utilities and libraries for working with video files produced by some cameras.

# Supported vendors

* GoPro
* Insta360

# Command-line tools

[![Build utils](https://github.com/kya8/slate/actions/workflows/build-utils.yml/badge.svg?event=push)](https://github.com/kya8/slate/actions/workflows/build-utils.yml)

* `camerainfo`: Dump various information and metadata extracted from video files.

* `insta360_normalize`: Undistort panoramic images from Insta360 cameras.

* `mp4join`: Join consecutive mp4 files in a lossless manner. Useful for cameras (e.g. GoPro) that split output video into fixed-sized chapters.
Copy of [mp4join](https://github.com/kya8/mp4join).

* `calibrator`: Simple wrapper for camera calibration utilities in OpenCV.
