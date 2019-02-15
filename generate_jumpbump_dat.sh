#!/bin/bash

#CURRENT_SCRIPT="`readlink -f $0`"
#SRC_DIR="`dirname ${CURRENT_SCRIPT}`"
SRC_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

gcc ${SRC_DIR}/modify/gobpack.c 
./a.out ${SRC_DIR}/data/numbers
./a.out ${SRC_DIR}/data/objects
./a.out ${SRC_DIR}/data/rabbit
./a.out ${SRC_DIR}/data/font
gcc -o b.out ${SRC_DIR}/modify/jnbpack.c
./b.out \
	-o jumpbump.dat \
	${SRC_DIR}/data/bump.mod \
	${SRC_DIR}/data/calib.dat \
	${SRC_DIR}/data/death.smp \
	${SRC_DIR}/data/fly.smp \
	${SRC_DIR}/data/jump.mod \
	${SRC_DIR}/data/jump.smp \
	${SRC_DIR}/data/levelmap.txt \
	${SRC_DIR}/data/level.pcx \
	${SRC_DIR}/data/mask.pcx \
	${SRC_DIR}/data/menu.pcx \
	${SRC_DIR}/data/menumask.pcx \
	${SRC_DIR}/data/scores.mod \
	${SRC_DIR}/data/splash.smp \
	${SRC_DIR}/data/spring.smp \
	numbers.gob \
	objects.gob \
	rabbit.gob \
	font.gob

