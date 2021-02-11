python ./tools/configure.py \
                --rom-support \
                --rom-auto-lightfunc \
                --config-metadata config/ \
                --source-directory src-input \
                --option-file config/examples/low_memory.yaml \
                --option-file esp32.yaml \
                --fixup-file esp32_glue.h \
                --output-directory flux-duk
