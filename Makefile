.PHONY: build
build:
	cat main/include/version |tr -d '\n' >main/include/version.h
	git rev-parse --short HEAD |tr -d '\n' >>main/include/version.h
	echo '"' >>main/include/version.h
	idf.py build

.PHONY: monitor
monitor:
	idf.py monitor

.PHONY: flash
flash:
	idf.py flash monitor

.PHONY: docs
docs:
	./scripts/json2mddoc.py spiffs_image/timer.js >docs/timer.md
	./scripts/json2mddoc.py spiffs_image/lorawanlib.js >docs/lorawanlib.md
	./scripts/json2mddoc.py spiffs_image/util.js >docs/util.md
	./scripts/json2mddoc.py main/duk_fs.c >docs/filesystem.md
	./scripts/json2mddoc.py main/lora_main.c >docs/lora.md
	./scripts/json2mddoc.py main/web_service.c >docs/webservice.md
	./scripts/json2mddoc.py main/platform.c >docs/platform.md
	./scripts/json2mddoc.py main/duk_crypto.c >docs/crypto.md

.PHONY: test
test:
	cd test; make

.PHONY: clean
clean:
	rm -rf build

.PHONY: onboardtest
onboardtest:
	@echo "\n192.168.4.1 is the default IP for Wifi AP\n"
	curl http://192.168.4.1/control?setload=/cryptotest.js
	curl http://192.168.4.1/control?reset
