#!/usr/bin/env python3

#
# SG
# shdc.py
#
# Copyright © 2021 Gustavo C. Viegas.
#

import subprocess

vert = [
    ('Model', '',        ['-DINSTANCE_N=1',  '-DJOINT_N=64']),
    ('Model', 'Model2',  ['-DINSTANCE_N=2',  '-DJOINT_N=64']),
    ('Model', 'Model4',  ['-DINSTANCE_N=4',  '-DJOINT_N=64']),
    ('Model', 'Model8',  ['-DINSTANCE_N=8',  '-DJOINT_N=64']),
    ('Model', 'Model16', ['-DINSTANCE_N=16', '-DJOINT_N=64']),
    ('Model', 'Model32', ['-DINSTANCE_N=32', '-DJOINT_N=64'])
]

frag = [
    ('Model', '', [])
]

srcDir = 'tmp/shd/'
dstDir = 'bin/'
lang = ''
prefix = ''
suffix = '.bin'

compiler = 'tmp/shdc'

def compile(src, type, out, extra):
    i = srcDir + src + type + lang
    o = dstDir + prefix + (src if out == '' else out) + type + suffix
    subprocess.run([compiler, '-V', i, '-o', o] + extra)

def compileVert():
    for src, out, extra in vert:
        compile(src, '.vert', out, extra)

def compileFrag():
    for src, out, extra in frag:
        compile(src, '.frag', out, extra)

if __name__ == '__main__':
    compileVert()
    compileFrag()
