include ../Makefile.inc

include $(wildcard ../Makefile.local)

.PHONY: classReWriter

all: elephantTracksRewriter$(ETJAR_VERSION).jar manifest classReWriter

classReWriter:
	$(MAKE) BASEDIR=$(BASEDIR) --directory=classReWriter all

elephantTracksRewriter$(ETJAR_VERSION).jar: classReWriter  manifest 
	jar cfm elephantTracksRewriter$(ETJAR_VERSION).jar manifest classReWriter

manifest:
	echo "Main-Class: classReWriter.ClassReWriter" > manifest
	echo "Class-Path: $(ASMJAR)" >> manifest
	echo "Can-Set-Native-Method-Prefix: true" >> manifest

clean:
	$(MAKE) --directory=classReWriter clean
	rm -f elephantTracksRewriter$(ETJAR_VERSION).jar manifest
