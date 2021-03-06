################################################################################
#
#  NetFPGA-10G http://www.netfpga.org
#
#  Author:
#        Yilong Geng
#
#  Description:
#        Code for generating pcap files according to traffic models and replaying
#        generated packets through commodity NICs.
#
#  Copyright notice:
#        Copyright (C) 2010, 2011 The Board of Trustees of The Leland Stanford
#                                 Junior University
#
#  Licence:
#        This file is part of the NetFPGA 10G development base package.
#
#        This file is free code: you can redistribute it and/or modify it under
#        the terms of the GNU Lesser General Public License version 2.1 as
#        published by the Free Software Foundation.
#
#        This package is distributed in the hope that it will be useful, but
#        WITHOUT ANY WARRANTY; without even the implied warranty of
#        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#        Lesser General Public License for more details.
#
#        You should have received a copy of the GNU Lesser General Public
#        License along with the NetFPGA source package.  If not, see
#        http://www.gnu.org/licenses/.
#
#


import os
from scapy import *
from scapy.all import *
from subprocess import Popen, PIPE
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('number', type=int)
parser.add_argument('length', type=int)
parser.add_argument('filename')
args = parser.parse_args()

#base timestamp of all engines is 1
class CBR_Engine:
    # pkt_rate in pkts/second, pkt_length in bytes
    def __init__(self, engine_name, pkt_rate, pkt_length):
        self.engine_name = engine_name
        self.pkt_rate = float(pkt_rate)
        self.pkt_length = pkt_length

    def generate(self, pkt_number):
        pkts = [None]*pkt_number
        for i in range(pkt_number):
            pkts[i] = Ether(''.join('X' for i in range(self.pkt_length)))
            pkts[i].time = i/self.pkt_rate +1
        wrpcap(self.engine_name + '.cap', pkts)

class Poisson_Engine:
    def __init__(self, engine_name, pkt_rate, pkt_length):
        self.engine_name = engine_name
        self.pkt_rate = float(pkt_rate)
        self.pkt_length = pkt_length

    def generate(self, pkt_number):
        pkts = [None]*pkt_number
        time = 1
        for i in range(pkt_number):
            pkts[i] = Ether(''.join('X' for i in range(self.pkt_length)))
            delta = random.expovariate(self.pkt_rate)
            pkts[i].time = time + delta
            time = time + delta
        wrpcap(self.engine_name + '.cap', pkts)

class Port_Arbiter:
    def __init__(self, iface, engine_list):
        self.iface = iface
        self.engine_list = engine_list

    def merge_queues(self):
        pos = [0]*len(self.engine_list);
        pcap_list = [None]*len(self.engine_list);
        for i in range(len(self.engine_list)):
            pcap_list[i] = rdpcap(self.engine_list[i] + '.cap')
        queues_num = len(self.engine_list)
        pkts_num = sum(len(pkts) for pkts in pcap_list)
        pkts = [None]*pkts_num
        while(sum(pos)<pkts_num):
            for i in range(queues_num):
                if(pos[i] < len(pcap_list[i])):
                    queue_id = i
                    break
            for i in range(queues_num):
                if(pos[i] < len(pcap_list[i]) and pcap_list[i][pos[i]].time < pcap_list[queue_id][pos[queue_id]].time):
                    queue_id = i
            pkts[sum(pos)] = pcap_list[queue_id][pos[queue_id]]
            pos[queue_id] = pos[queue_id] + 1
        wrpcap(self.iface + '.cap', pkts)


class Rate_Limiter:
    # rate in bits per second
    def __init__(self, iface, rate):
        self.iface = iface
        self.rate = float(rate)
        self.limit_rate()

    def limit_rate(self):
        pkts = rdpcap(self.iface + '.cap')
        last_pkt_end_time = 0
        for pkt in pkts:
            pkt.time = max(pkt.time, last_pkt_end_time)
            last_pkt_end_time = pkt.time + len(pkt)*8/self.rate
        wrpcap(self.iface + '.cap', pkts)

class Pcap_Replay:
    def __init__(self, iface):
        self.iface = iface

    def replay(self):
        proc = Popen("sudo tcpreplay -i "+self.iface+' nf3_delay.cap', stdout=PIPE, shell=True)
        print proc.stdout.read()

if __name__=="__main__":
    """
    #CBR engine
    cbr = CBR_Engine('cbr', 100, 1500)
    cbr.generate(30)
    #Poisson engine
    poisson = Poisson_Engine('poisson', 100, 1500)
    poisson.generate(30)
    #Arbiter for port eth4
    arbiter = Port_Arbiter('nf3', ['cbr', 'poisson'])
    arbiter.merge_queues()
    #Rate limiter for port eth4
    rate_limiter = Rate_Limiter('nf3', 10000000000000000000000)
    #Start replaying on port eth4
    replayer = Pcap_Replay('nf3')
    replayer.replay()
    """
    cbr = CBR_Engine(args.filename, 100, args.length)
    cbr.generate(args.number)
