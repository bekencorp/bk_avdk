# ARMINO bk_avdk Development Framework

* [中文版](./README_CN.md)

Welcome to Armino SDK 2.0 bk_avdk!

bk_avdk is the audio and video SDK.
basic sdk is bk_idk.

# bk_idk Release and Supported SoCs

The following table shows bk_idk support of Beken SoCs where ![alt text][developing] and ![alt text][supported]
denote developing status and support, respectively. In developing status the build is not yet enabled and some
crucial parts could be missing(like documentation, datasheet). Please use an bk_idk release where the desired
SoC is already supported.


|Branch\Chip   |      [BK7258]          |      Comment            |
|:------------ |:-----------------:     |:----------------------: |
|release/v2.0.1|![alt text][supported]  |sdk 2.0 MP Branch        |


[supported]: https://img.shields.io/badge/-supported-green "supported"
[developing]: https://img.shields.io/badge/-developing-orange "developing"

==Tag version , please use the official release version for development.==

main branch Tag is not official release, the format is vx.x.x.x, for example:v2.0.0.19

release Branch Tag is official release, the format is vx.x.x.x，for example: v2.0.1.19

Beken SoCs released before 2022, such as BK7231N, BK7231U, BK7251 etc, are not supported by ARMINO.

# ARMINO Release and OS

The following table shows Armino support of Popular IoT OS where ![alt text][developing] and ![alt text][supported]
denote developing status and support, respectively.

|OS           |         main           |
|:----------- |:---------------------: |
|FreeRTOS     | ![alt text][supported] |
|LiteOS       | ![alt text][supported]|
|RT-Thread    | ![alt text][developing]|
|AliOS        | ![alt text][developing]|

[supported]: https://img.shields.io/badge/-supported-green "supported"
[developing]: https://img.shields.io/badge/-developing-orange "developing"

# Getting Started

clone code:

git clone --recurse-submodules https://github.com/bekencorp/bk_avdk.git -b tagname

build:

make bk7258 PROJECT=lvgl/86box

