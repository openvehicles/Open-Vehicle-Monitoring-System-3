#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
ifdef CONFIG_OVMS_COMP_WEBSERVER

COMPONENT_ADD_INCLUDEDIRS:=src
COMPONENT_SRCDIRS:=src assets
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive

COMPONENT_EXTRA_CLEAN := assets/script.js.gz assets/style.css.gz
COMPONENT_EMBED_FILES := assets/script.js.gz assets/style.css.gz

$(COMPONENT_PATH)/assets/script.js.gz : $(COMPONENT_PATH)/assets/jquery.min.js $(COMPONENT_PATH)/assets/bootstrap.min.js $(COMPONENT_PATH)/assets/ovms.js
	cat $^ | gzip -c > $@

$(COMPONENT_PATH)/assets/style.css.gz : $(COMPONENT_PATH)/assets/bootstrap.min.css $(COMPONENT_PATH)/assets/bootstrap-theme.min.css $(COMPONENT_PATH)/assets/ovms.css
	cat $^ | gzip -c > $@

src/web_framework.o: $(COMPONENT_PATH)/assets/script.js.gz $(COMPONENT_PATH)/assets/style.css.gz

CPPFLAGS += -DMTIME_ASSETS_SCRIPT_JS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/script.js.gz")[9]')
CPPFLAGS += -DMTIME_ASSETS_STYLE_CSS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/style.css.gz")[9]')

endif
endif
