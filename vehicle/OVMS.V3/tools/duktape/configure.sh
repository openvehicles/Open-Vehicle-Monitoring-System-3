#!/bin/bash

cd components/duktape

python tools/configure.py \
  --config-metadata config/ \
  --source-directory src-input \
  --rom-support \
  --option-file config/examples/low_memory.yaml \
  --option-file ../../tools/duktape/ovms-duktape-config.yaml \
  --output-directory src
