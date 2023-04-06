# SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0 OR MIT

CHERI ?= $(HOME)/cheri
SDKPREFIX ?= $(CHERI)/output/morello-sdk/bin/
CFLAGS ?= -Wall -Wextra -pedantic -O1 -g
MORELLO_PURECAP := --config cheribsd-morello-purecap.cfg
MORELLO_HYBRID := --config cheribsd-morello-hybrid.cfg

.PHONY: all clean clang-format clang-format-check examples-morello-purecap examples-morello-hybrid

all: test-morello-purecap test-morello-hybrid libcapmap-morello-purecap.so libcapmap-morello-hybrid.so examples-morello-purecap examples-morello-hybrid

clean:
	rm -rf obj test-morello-purecap test-morello-hybrid libcapmap-*.so

clang-format:
	$(SDKPREFIX)clang-format -i tests/*.cc tests/*.h include/*.h src/*.cc

clang-format-check:
	@! $(SDKPREFIX)clang-format -output-replacements-xml tests/*.cc tests/*.h include/*.h src/*.cc | grep -c '<replacement ' > /dev/null



test-morello-purecap: libcapmap-morello-purecap.so tests/*.cc tests/*.h
	$(SDKPREFIX)clang++ -Wl,-rpath,. -L. -lcapmap-morello-purecap $(CFLAGS) $(MORELLO_PURECAP) -I. tests/*.cc -std=c++14 -o $@

obj/morello-purecap/%.o: src/%.cc include/*.h
	@mkdir -p obj/morello-purecap
	$(SDKPREFIX)clang++ -fPIC $(CFLAGS) $(MORELLO_PURECAP) -I. $< -std=c++14 -c -o $@

OBJS_MORELLO_PURECAP := $(patsubst src/%.cc,obj/morello-purecap/%.o,$(wildcard src/*.cc))
libcapmap-morello-purecap.so: $(OBJS_MORELLO_PURECAP) include/*.h
	$(SDKPREFIX)clang++ -fPIC -shared $(CFLAGS) $(MORELLO_PURECAP) -lutil -I. $(OBJS_MORELLO_PURECAP) -std=c++14 -o $@



test-morello-hybrid: libcapmap-morello-hybrid.so tests/*.cc tests/*.h
	$(SDKPREFIX)clang++ -Wl,-rpath,. -L. -lcapmap-morello-hybrid $(CFLAGS) $(MORELLO_HYBRID) -I. tests/*.cc -std=c++14 -o $@

obj/morello-hybrid/%.o: src/%.cc include/*.h
	@mkdir -p obj/morello-hybrid
	$(SDKPREFIX)clang++ -fPIC $(CFLAGS) $(MORELLO_HYBRID) -I. $< -std=c++14 -c -o $@

OBJS_MORELLO_HYBRID := $(patsubst src/%.cc,obj/morello-hybrid/%.o,$(wildcard src/*.cc))
libcapmap-morello-hybrid.so: $(OBJS_MORELLO_HYBRID) include/*.h
	$(SDKPREFIX)clang++ -fPIC -shared $(CFLAGS) $(MORELLO_HYBRID) -lutil -I. $(OBJS_MORELLO_HYBRID) -std=c++14 -o $@
