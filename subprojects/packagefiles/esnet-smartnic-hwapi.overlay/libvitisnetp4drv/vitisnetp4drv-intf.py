#!/usr/bin/env python3

import jinja2
import pathlib
import sys

#-------------------------------------------------------------------------------
ip_name = sys.argv[1]
intf_c = pathlib.Path(sys.argv[2])
intf_h = pathlib.Path(sys.argv[3])
intf_map = pathlib.Path(sys.argv[4])

context = {
    'ip_name': ip_name,
    'intf_h': intf_h,
    'intf_h_def': intf_h.name.
                  translate(str.maketrans({'-':'_', '.':'_'})).
                  upper()
}

#-------------------------------------------------------------------------------
loader = jinja2.FileSystemLoader([pathlib.Path(__file__).parent])
env = jinja2.Environment(loader=loader)

for intf in (intf_c, intf_h, intf_map):
    template = env.get_template('vitisnetp4drv-intf' + intf.suffix + '.j2')
    with intf.open(mode='w') as fd:
        template.stream(**context).dump(fd)
