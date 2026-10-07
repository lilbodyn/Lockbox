CC ?= cc
GCOV ?= gcov
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD := build/$(PLATFORM)
COVERAGE := coverage/$(PLATFORM)
SOURCES := test_lockbox.c ../firmware/Sensors/mpu6050.c ../firmware/Sensors/analog.c ../firmware/Guard/guard.c ../firmware/Guard/commands.c
FLAGS := -std=c11 -Wall -Wextra -Werror -g -I../firmware/Sensors -I../firmware/Guard
.PHONY: test coverage sanitize
test: $(BUILD)/test_lockbox
	./$(BUILD)/test_lockbox
$(BUILD)/test_lockbox: $(SOURCES) $(wildcard ../firmware/Sensors/*.h ../firmware/Guard/*.h)
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) $(SOURCES) -o $@
sanitize:
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(BUILD)/sanitized
	./$(BUILD)/sanitized
coverage:
	mkdir -p $(COVERAGE)
	rm -f $(COVERAGE)/*.gcda $(COVERAGE)/*.gcno
	$(CC) $(FLAGS) --coverage $(SOURCES) -o $(COVERAGE)/test_lockbox
	./$(COVERAGE)/test_lockbox
	cd $(COVERAGE) && $(GCOV) -b -c *.gcno
