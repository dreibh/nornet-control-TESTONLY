3,8,13,18,23,28,33,38,43,48,53,58 * * * *  root    test -x /etc/init.d/nornet-tunnelbox && /etc/init.d/nornet-tunnelbox check-and-configure
*/4 * * * *                                root    /usr/bin/Watchdog /etc/nornet/watchdog-config >/var/log/nornet-watchdog 2>&1
