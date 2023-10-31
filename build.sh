#!/bin/bash
mkdir -p build
bin/scpp main.c build/main.i
bin/scc build/main.i build/main.asm
bin/asm build/main.asm build/fat32_to_ext2 build/main.map
