#!/bin/bash

set -e

# 如果没有build目录就创建
if [ ! -d "$(pwd)/build" ]; then
    mkdir "$(pwd)/build"
fi   

# 清除build目录下的所有文件
rm -rf "$(pwd)/build"/*

# 进入build目录并生成构建文件
cd "$(pwd)/build"
cmake ..
make

# 回到项目根目录
cd ..

# 创建头文件目录（如果不存在）
if [ ! -d "/usr/include/mymuduo" ]; then
    sudo mkdir "/usr/include/mymuduo"
fi

# 拷贝头文件到 /usr/include/mymuduo
for header in *.h; do
    if [ -f "$header" ]; then
        sudo cp "$header" "/usr/include/mymuduo"
    fi
done

# 拷贝so库到 /usr/lib
sudo cp "$(pwd)/lib/libmymuduo.so" "/usr/lib"

# 刷新动态库缓存
sudo ldconfig

echo "安装完成"