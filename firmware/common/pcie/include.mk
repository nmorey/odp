SRCDIRS  += $(dir $$(echo $(MAKEFILE_LIST) | awk '{ print $$NF}'))
