#! /usr/bin/env python3

# filter out Cryptography Deprecation Warnings from Scapy
import warnings
from cryptography.utils import CryptographyDeprecationWarning
warnings.filterwarnings("ignore", category=CryptographyDeprecationWarning)

import click
import time
from scapy.all import *
from collections import namedtuple

load_contrib("lldp")

@click.command()
@click.option('-s', '--slotaddr',
              help="PCIe BDF address of the FPGA card.  Value can also be loaded from SN_LLDP_SLOTADDR env var.",
              show_default=True,
              required=True,
)
@click.option('-h', '--host',
              help="Hostname FQDN (eg. lbnl59-ht1.es.net)",
              required=True,
)
@click.option('-t', '--ttl',
              help="LLDP Time To Live in seconds.  Note that msgs are sent every TTL/2.",
              default=20,
              show_default=True,
)

def main(slotaddr, host, ttl):

    print("LLDP Configuration")
    print("  PCIe Slot: {:s}".format(slotaddr))
    print("  Host FQDN: {:s}".format(host))
    print("  TTL      : {:d} seconds".format(ttl))

    # Build up the LLDP Context objects and generate the LLDP packets
    LLDPContext = namedtuple('LLDPContext', [
        'tapif',
        'packet',
    ])

    port_bindings = [
        ("sn-pf0", "cmac0"),
        ("sn-pf1", "cmac1"),
    ]

    lldp_contexts = []
    for tapif, portid in port_bindings:
        print("  {} -> {}".format(tapif, portid))
        packet = (
            Ether(dst="01:80:c2:00:00:0e",
                  src=get_if_hwaddr(tapif)) /
            LLDPDU() /
            LLDPDUChassisID(subtype="chassis component",
                            id="ESnet SmartNIC " + slotaddr) /
            LLDPDUPortID(subtype="interface name",
                         id=portid) /
            LLDPDUTimeToLive(ttl=ttl) /
            LLDPDUSystemName(system_name=host) /
            LLDPDUSystemDescription(description="ESnet SmartNIC HW=" +
                                    os.environ['SN_HW_VER'] +
                                    " FW=" + os.environ['SN_FW_VER']) /
            LLDPDUEndOfLLDPDU()
        )
        lldp_contexts.append(LLDPContext(tapif=tapif, packet=packet))

    while(True):
        for ctx in lldp_contexts:
            sendp(ctx.packet, verbose=False, iface=ctx.tapif)
        time.sleep(ttl / 2)

if __name__ == '__main__':
    sys.exit(main(auto_envvar_prefix='SN_LLDP'))

