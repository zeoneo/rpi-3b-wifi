#!/bin/bash

docker run --rm -v $(pwd):/src_dir -w /src_dir zeoneo/rip-3b-32bit-build make