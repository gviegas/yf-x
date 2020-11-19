#!/usr/bin/env sh

#
# Creates a fake include tree to simplify header inclusion.
#

YF_DIR=inc/yf/
CG_DIR=$YF_DIR/cg/
WS_DIR=$YF_DIR/ws/

# yf (root)
mkdir -p $YF_DIR
ln -sr priv/* $YF_DIR
ln -sr pub/* $YF_DIR

# cg
mkdir -p $CG_DIR
ln -sr cg/include/* $CG_DIR

# ws
mkdir -p $WS_DIR
ln -sr ws/include/* $WS_DIR
