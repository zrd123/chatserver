##########################################################################
# File Name: autoBuild.sh
# Author: zrd
# mail: 624453665@qq.com
# Created Time: 2023年01月12日 星期四 15时39分12秒
#########################################################################
#!/bin/zsh
PATH=/home/edison/bin:/home/edison/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/work/tools/gcc-3.4.5-glibc-2.3.6/bin
export PATH

set -x
rm -rf  `pwd`/build/*
cd `pwd`/build &&
	cmake .. &&
	make
rm -rf  `pwd`/build/*