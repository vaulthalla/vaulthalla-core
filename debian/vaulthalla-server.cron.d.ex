#
# Regular cron jobs for the vaulthalla package.
#
0 4	* * *	root	[ -x /usr/bin/vaulthalla_maintenance ] && /usr/bin/vaulthalla_maintenance
