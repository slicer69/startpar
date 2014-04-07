all:

check:
	@mkdir -p start
	@touch start/working
	@if ../startpar -a start ls 2>&1 | grep -q working ; then \
		echo success; \
	else \
		echo failure; \
	fi ; rm -rf start
