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

COMPONENT_EXTRA_CLEAN := \
	assets/script.js \
	assets/script.js.gz \
	assets/charts.js \
	assets/charts.js.gz \
	assets/tables.js \
	assets/tables.js.gz \
	assets/style.css \
	assets/style.css.gz \
	assets/zones.json.gz
COMPONENT_EMBED_FILES := \
	assets/script.js.gz \
	assets/charts.js.gz \
	assets/tables.js.gz \
	assets/style.css.gz \
	assets/zones.json.gz \
	assets/favicon.png

$(COMPONENT_PATH)/assets/script.js : \
	$(COMPONENT_PATH)/assets/jquery.min.js \
	$(COMPONENT_PATH)/assets/bootstrap.min.js \
	$(COMPONENT_PATH)/assets/ovms.js
	cat $^ > $@ && dos2unix $@

$(COMPONENT_PATH)/assets/script.js.gz : \
	$(COMPONENT_PATH)/assets/script.js
	cat $^ | gzip -c > $@

$(COMPONENT_PATH)/assets/charts.js : \
	$(COMPONENT_PATH)/assets/highcharts.js \
	$(COMPONENT_PATH)/assets/highcharts-more.js \
	$(COMPONENT_PATH)/assets/hc-modules/bullet.js \
	$(COMPONENT_PATH)/assets/hc-modules/solid-gauge.js \
	$(COMPONENT_PATH)/assets/hc-modules/streamgraph.js \
	$(COMPONENT_PATH)/assets/hc-modules/xrange.js
	cat $^ > $@ && dos2unix $@

$(COMPONENT_PATH)/assets/charts.js.gz : \
	$(COMPONENT_PATH)/assets/charts.js
	cat $^ | gzip -c > $@

$(COMPONENT_PATH)/assets/tables.js : \
	$(COMPONENT_PATH)/assets/datatables.min.js
	cat $^ > $@ && dos2unix $@

$(COMPONENT_PATH)/assets/tables.js.gz : \
	$(COMPONENT_PATH)/assets/tables.js
	cat $^ | gzip -c > $@

$(COMPONENT_PATH)/assets/style.css : \
	$(COMPONENT_PATH)/assets/intro.css \
	$(COMPONENT_PATH)/assets/bootstrap.min.css \
	$(COMPONENT_PATH)/assets/bootstrap-theme.min.css \
	$(COMPONENT_PATH)/assets/highcharts.css \
	$(COMPONENT_PATH)/assets/datatables.min.css \
	$(COMPONENT_PATH)/assets/datatables.ovms.css \
	$(COMPONENT_PATH)/assets/ovms.css
	cat $^ > $@ && dos2unix $@

$(COMPONENT_PATH)/assets/style.css.gz : \
	$(COMPONENT_PATH)/assets/style.css
	cat $^ | gzip -c > $@

$(COMPONENT_PATH)/assets/zones.json.gz : \
	$(COMPONENT_PATH)/assets/zones.json
	cat $^ | gzip -c > $@

src/web_framework.o: \
	$(COMPONENT_PATH)/assets/script.js.gz \
	$(COMPONENT_PATH)/assets/charts.js.gz \
	$(COMPONENT_PATH)/assets/tables.js.gz \
	$(COMPONENT_PATH)/assets/style.css.gz \
	$(COMPONENT_PATH)/assets/favicon.png \
	$(COMPONENT_PATH)/assets/zones.json.gz

CPPFLAGS += -DMTIME_ASSETS_SCRIPT_JS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/script.js.gz")[9]')
CPPFLAGS += -DMTIME_ASSETS_CHARTS_JS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/charts.js.gz")[9]')
CPPFLAGS += -DMTIME_ASSETS_TABLES_JS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/tables.js.gz")[9]')
CPPFLAGS += -DMTIME_ASSETS_STYLE_CSS=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/style.css.gz")[9]')
CPPFLAGS += -DMTIME_ASSETS_FAVICON_PNG=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/favicon.png")[9]')
CPPFLAGS += -DMTIME_ASSETS_ZONES_JSON=$(shell perl -e 'print +(stat "$(COMPONENT_PATH)/assets/zones.json.gz")[9]')

endif
endif
