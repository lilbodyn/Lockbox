CC ?= cc
GCOV ?= gcov
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD := build/$(PLATFORM)
COVERAGE := coverage/$(PLATFORM)
SOURCES := test_lockbox.c ../firmware/Sensors/mpu6050.c ../firmware/Sensors/analog.c ../firmware/Guard/guard.c ../firmware/Guard/commands.c
FLAGS := -std=c11 -Wall -Wextra -Werror -g -I../firmware/Sensors -I../firmware/Guard
ARCHIVE_SOURCES := test_archive.c ../firmware/Guard/archive.c ../firmware/Guard/releases.c ../firmware/Guard/guard.c ../firmware/Guard/commands.c ../firmware/Sensors/analog.c
SERIAL_SOURCES := test_serial.c ../firmware/Board/serial.c ../firmware/Guard/commands.c
.PHONY: test coverage sanitize
test: $(BUILD)/test_lockbox $(BUILD)/test_archive $(BUILD)/test_serial
	./$(BUILD)/test_lockbox
	./$(BUILD)/test_archive
	./$(BUILD)/test_serial
$(BUILD)/test_serial: $(SERIAL_SOURCES) fakes/main.h ../firmware/Board/serial.h
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) -Ifakes -I../firmware/Board $(SERIAL_SOURCES) -o $@
$(BUILD)/test_archive: $(ARCHIVE_SOURCES) $(wildcard ../firmware/Guard/*.h)
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) $(ARCHIVE_SOURCES) -o $@
$(BUILD)/test_lockbox: $(SOURCES) $(wildcard ../firmware/Sensors/*.h ../firmware/Guard/*.h)
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) $(SOURCES) -o $@
sanitize:
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(BUILD)/sanitized
	./$(BUILD)/sanitized
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(ARCHIVE_SOURCES) -o $(BUILD)/archive_sanitized
	./$(BUILD)/archive_sanitized
	$(CC) $(FLAGS) -Ifakes -I../firmware/Board -fsanitize=address,undefined -fno-omit-frame-pointer $(SERIAL_SOURCES) -o $(BUILD)/serial_sanitized
	./$(BUILD)/serial_sanitized
coverage:
	mkdir -p $(COVERAGE)
	rm -f $(COVERAGE)/*.gcda $(COVERAGE)/*.gcno
	$(CC) $(FLAGS) --coverage $(abspath $(SOURCES)) -o $(COVERAGE)/test_lockbox
	./$(COVERAGE)/test_lockbox
	cd $(COVERAGE) && $(GCOV) -b -c *.gcno
	mkdir -p $(COVERAGE)/archive
	rm -f $(COVERAGE)/archive/*.gcda $(COVERAGE)/archive/*.gcno $(COVERAGE)/archive/*.gcov
	$(CC) $(FLAGS) --coverage $(abspath $(ARCHIVE_SOURCES)) -o $(COVERAGE)/archive/test_archive
	./$(COVERAGE)/archive/test_archive
	cd $(COVERAGE)/archive && $(GCOV) -b -c *.gcno
