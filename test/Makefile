all: record queue

.PHONY: record
record:
	gcc -I ../main/include -DRECORD_TEST ../main/record.c -o record_test
	./record_test >/dev/null 2>&1

.PHONY: queue
queue:
	gcc -I ../main/include queue.c -o queue_test
	./queue_test >/dev/null 2>&1

jstest:
	gcc -D__JSTEST__ -o jstest jstest.c ../main/duk_util.c ../components/duktape/esp32_glue.c ../components/duktape/duktape.c -I ../main/include -I ../components/duktape/include -lm
