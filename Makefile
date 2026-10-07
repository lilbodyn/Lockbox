IMAGE ?= lockbox-lab2-build:local

.PHONY: image build clean test coverage sanitize

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

test:
	python3 tools/check_layers.py
	python3 tools/check_wiring.py
	$(MAKE) -C tests test

coverage:
	$(MAKE) -C tests coverage

sanitize:
	$(MAKE) -C tests sanitize
