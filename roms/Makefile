# =============================================================================
#  WAGNOSTIC — ROMs Makefile
# =============================================================================

ROM_DIRS = \
	display_test \
	input_test \
	audio_wav \
	audio_mp3 \
	audio_ogg \
	full_test \
	full_test_static \
	full_test_dynamic \
	buttons_test \
	fallback_test \
	bpp1_test \
	bpp2_test \
	bpp4_test \
	exotic1_test \
	exotic2_test

.PHONY: all clean $(ROM_DIRS)

all: $(ROM_DIRS)

$(ROM_DIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f *.wasm
