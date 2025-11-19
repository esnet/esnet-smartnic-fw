#! /usr/bin/env python3

import sys
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
@click.option('--phys-slot',
              help="Physical slot number to prepend to the portid.",
)
@click.option('--sys-desc',
              help="LLDP System Description prefix to send",
              default="ESnet SmartNIC",
              show_default=True,
)
@click.option('--component',
              help="LLDP Chassis Component prefix to send",
              default="ESnet SmartNIC",
              show_default=True,
)
@click.option('--sw-ver',
              help="Software version to include in LLDP System Description",
)
@click.option('-t', '--ttl',
              help="LLDP Time To Live in seconds.  Note that msgs are sent every TTL/2.",
              default=20,
              show_default=True,
)

def main(slotaddr, host, phys_slot, ttl, sys_desc, component, sw_ver):

    print("=" * 30)
    print("LLDP Configuration")
    print("  PCIe Slot : {:s}".format(slotaddr))
    print("  Host FQDN : {:s}".format(host))
    print("  Phys Slot : {:s}".format(phys_slot if phys_slot else "<not provided>"))
    print("  Component : {:s}".format(component))
    print("  Sys Desc  : {:s}".format(sys_desc))
    print("  SW Version: {:s}".format(sw_ver if sw_ver else "<not provided>"))
    print("  TTL       : {:d} seconds".format(ttl))
    print()

    # Build up the LLDP Context objects and generate the LLDP packets
    LLDPContext = namedtuple('LLDPContext', [
        'tapif',
        'packet',
    ])

    port_bindings = [
        ("sn-pf0", "cmac0"),
        ("sn-pf1", "cmac1"),
    ]

    # Make sure all tap interfaces exist before starting
    print("Verifying all configured interfaces are ready")
    all_ready = True
    for tapif, _ in port_bindings:
        try:
            dev = conf.ifaces.dev_from_name(tapif)
        except:
            # The tap interface doesnt exist yet, log an error
            print("  Error: {:s} Interface not available yet".format(tapif))
            all_ready = False
            continue

        if not dev.flags.LOWER_UP:
            # The tap interface isn't up yet, log an error
            print("  Error: {:s} Interface is down".format(tapif))
            all_ready = False
        else:
            # The tap interface is present and up
            print("  OK: {:s} Interface is present and ready: {:s}".format(tapif, str(dev.flags)))

    if not all_ready:
        # if any errors were found during interface checks, exit
        print("  Fatal: Not all interfaces are ready yet, exiting.")
        print()
        sys.exit(1)

    print("  All interfaces are ready: {:s}".format(", ".join([tapif for tapif, _ in port_bindings])))
    print()

    print("Preparing LLDP packets:")
    lldp_contexts = []
    for tapif, portid in port_bindings:
        print("  {} -> {}".format(tapif, portid))
        component_full = "{:s} {:s}".format(
            component,
            slotaddr
        )
        sys_desc_full   = "{:s} HW={:s} FW={:s}".format(
            sys_desc,
            os.environ['SN_HW_VER'],
            os.environ['SN_FW_VER'],
        )
        if sw_ver is not None:
            sys_desc_full += " SW={:s}".format(sw_ver)

        if phys_slot is None or phys_slot == "":
            portid_full = "{:s}".format(portid)
        else:
            portid_full = "{:s}/{:s}".format(phys_slot, portid)

        packet = (
            Ether(dst="01:80:c2:00:00:0e",
                  src=get_if_hwaddr(tapif)) /
            LLDPDU() /
            LLDPDUChassisID(subtype="chassis component",
                            id=component_full) /
            LLDPDUPortID(subtype="interface name",
                         id=portid_full) /
            LLDPDUTimeToLive(ttl=ttl) /
            LLDPDUSystemName(system_name=host) /
            LLDPDUSystemDescription(description = sys_desc_full) /
            LLDPDUEndOfLLDPDU()
        )
        lldp_contexts.append(LLDPContext(tapif=tapif, packet=packet))

    print()

    print("Beginning periodic packet transmission at a period of {:4.2f}s".format(ttl/2))
    while(True):
        for ctx in lldp_contexts:
            try:
                sendp(ctx.packet, verbose=False, iface=ctx.tapif)
            except OSError as e:
                if e.errno == 100:
                    # Log any errno 100 "Network is down" errors
                    print("Warning: {:s} Network down".format(ctx.tapif))
                else:
                    # Pass all other errors up the chain
                    raise

        time.sleep(ttl / 2)

if __name__ == '__main__':
    sys.exit(main(auto_envvar_prefix='SN_LLDP'))

