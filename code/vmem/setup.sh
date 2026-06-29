#!/bin/bash
set -e

NACHOS=./nachos
USERLAND=../userland

RS=$RANDOM
echo "Usando seed: $RS"

$NACHOS -f
$NACHOS -cp $USERLAND/shell shell
$NACHOS -cp $USERLAND/ls ls
$NACHOS -cp $USERLAND/cd cd
$NACHOS -cp $USERLAND/mkdir mkdir
$NACHOS -cp $USERLAND/halt halt
$NACHOS -cp $USERLAND/cp cp
$NACHOS -cp $USERLAND/cat cat
$NACHOS -cp $USERLAND/cat.c cat.c
$NACHOS -cp $USERLAND/cp.c cp.c
$NACHOS -x shell -m 1024 -rs $RS
