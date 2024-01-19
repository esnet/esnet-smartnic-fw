from robot.api import logger

import regio.regmap as regmap
import regmap_esnet_smartnic.toplevel as top

BAR_INFO = tuple(sorted(top.BAR_INFO.items(), key=lambda pair: pair[0]))

#---------------------------------------------------------------------------------------------------
class Device:
    BAR_FMT = 'bar{}'

    def __init__(self, pci_id=None):
        if not pci_id:
            pci_id = 'none'
        self.pci_id = pci_id

    def start(self):
        for bid, info in BAR_INFO:
            bar = self.BAR_FMT.format(bid)
            spec = info['spec_cls']()

            if self.pci_id == 'none': # For testing without hardware.
                path = f'/scratch/regio-esnet-smartnic.{self.pci_id}.{bar}.bin'
                io = regmap.FileMmapIOForSpec(spec, path)
            else:
                io = None

            proxy = info['new_proxy'](self.pci_id, spec, io)
            regmap.start_io(proxy)
            setattr(self, bar, proxy)

            logger.info(f'Started IO for device {self.pci_id} on {bar} using regmap spec {spec}.')

    def stop(self):
        for bid, _ in reversed(BAR_INFO):
            bar = self.BAR_FMT.format(bid)
            proxy = getattr(self, bar)
            regmap.stop_io(proxy)
            delattr(self, bar)

            logger.info(f'Stopped IO for device {self.pci_id} on {bar}.')

#---------------------------------------------------------------------------------------------------
# This variable is set by the global setup fixture when starting up the regmap proxy. It should be
# used for all accesses to the device under test.
dev = None
