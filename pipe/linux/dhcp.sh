#!/bin/sh

ip=/sbin/ip

case "$reason" in
    PREINIT)
        ${ip} -4 link set dev ${interface} up
        ;;
    BOUND|RENEW|REBIND|REBOOT)
        # Add IP address to interface. Provide no subnet so OS doesn't think there's a network here.
        ${ip} -4 addr flush dev ${interface}
        ${ip} -4 addr add ${new_ip_address}/32 dev ${interface}

        ;;
    EXPIRE|FAIL|RELEASE|STOP)
        # Remove old IP address
        ${ip} -4 addr flush dev ${interface}

        ;;
esac