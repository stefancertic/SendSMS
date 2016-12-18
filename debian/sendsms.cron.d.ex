#
# Regular cron jobs for the sendsms package
#
0 4	* * *	root	[ -x /usr/bin/sendsms_maintenance ] && /usr/bin/sendsms_maintenance
