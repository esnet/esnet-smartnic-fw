#!/usr/bin/env python3

import jinja2
import pathlib
import sys

#-------------------------------------------------------------------------------
intf_c = pathlib.Path(sys.argv[1])
ip_names = sys.argv[2:]

context = {
    'ip_names': ip_names,
}

#-------------------------------------------------------------------------------
loader = jinja2.FileSystemLoader([pathlib.Path(__file__).parent])
env = jinja2.Environment(loader=loader)
template = env.get_template('vitisnetp4drv-intf-table.c.j2')

with intf_c.open(mode='w') as fd:
    template.stream(**context).dump(fd)
