#!/usr/bin/python
import sys
import re
import socket
import fcntl
import termios
import struct
import curses
import signal
import copy
import time


def getTerminalSize():

    def ioctl_GWINSZ(fd):
        try:
            cr = struct.unpack('hh', fcntl.ioctl(fd, termios.TIOCGWINSZ,
                                                 '1234'))
        except:
            return
        return cr
    cr = ioctl_GWINSZ(0) or ioctl_GWINSZ(1) or ioctl_GWINSZ(2)
    return int(cr[1]), int(cr[0])


def get_ip_address(ifname):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        return socket.inet_ntoa(fcntl.ioctl(s.fileno(), 0x8915,
                                            struct.pack('256s',
                                                        ifname[:15]))[20:24])
    except IOError:
        return "A"


def GetInterfaces():
    ret = {}
    f = open("/proc/net/dev", "r")
    data = f.read()
    f.close()
    r = re.compile("[:\s]+")
    lines = re.split("[\r\n]+", data)
    for line in lines[2:]:
        line = line.strip()
        columns = r.split(line)
        if len(columns) < 17:
            continue
        info = {}
        info["rx_bytes"] = columns[1]
        info["rx_packets"] = columns[2]
        info["rx_errors"] = columns[3]
        info["rx_dropped"] = columns[4]
        info["rx_fifo"] = columns[5]
        info["rx_frame"] = columns[6]
        info["rx_compressed"] = columns[7]
        info["rx_multicast"] = columns[8]
        info["tx_bytes"] = columns[9]
        info["tx_packets"] = columns[10]
        info["tx_errors"] = columns[11]
        info["tx_dropped"] = columns[12]
        info["tx_fifo"] = columns[13]
        info["tx_frame"] = columns[14]
        info["tx_compressed"] = columns[15]
        info["tx_multicast"] = columns[16]
        iface = columns[0]
        info["ip"] = get_ip_address(iface)
        ret[iface] = info

    return ret


def signal_handler(signal, frame):
    curses.endwin()
    sys.exit(0)


def myexcepthook(exctype, value, tb):
    curses.endwin()
    backup(exctype, value, tb)


signal.signal(signal.SIGINT, signal_handler)
backup = sys.excepthook
sys.excepthook = myexcepthook
title = "Interfaces stastistic"
if (len(sys.argv) > 1):
    regex = sys.argv[1]
else:
    regex = "vnic|portEpic"

old_interface = {}
while 1:
    myscreen = curses.initscr()
    (width, height) = getTerminalSize()
    usefull_metric = ["rx_bytes", "rx_packets", "tx_bytes", "tx_packets"]
    interfaces_info = GetInterfaces()

    filtered_interfaces_info = dict((interface, interfaces_info[interface])
                                    for interface in interfaces_info.keys()
                                    if re.match(regex, interface))
    myscreen.addstr(1, (width - len(title))/2, title)
    for(iface) in sorted(filtered_interfaces_info.keys()):
        info = filtered_interfaces_info[iface]
        try:
            old_info = old_interface[iface]
        except:
            myscreen.addstr("Failed")
            old_info = {}
            curses.endwin()

        myscreen.addstr("\nInterface: %s ip = %s\n\t" % (iface, info["ip"]))
        for (metric) in sorted(usefull_metric):
            value = info[metric]
            try:
                old_value = old_info[metric]
                if old_value != value:
                    attribut = curses.A_STANDOUT
                else:
                    attribut = 0
            except:
                attribut = 0

            myscreen.addstr("%s = " % metric)
            myscreen.addstr(value, attribut)
            myscreen.addstr("\t\t\t")

    old_interface = copy.deepcopy(interfaces_info)
    time.sleep(1)
