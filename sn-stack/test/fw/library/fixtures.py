from robot.api.deco import keyword, library

import os
import variables # Module used for passing global variables to test cases.

#---------------------------------------------------------------------------------------------------
@library
class Library:
    ENV_VAR = 'REGIO_ESNET_SMARTNIC_PCI_IDS'

    def __init__(self, pci_id=None):
        if pci_id is None:
            pci_ids = os.environ.get(self.ENV_VAR)
            if pci_ids is None:
                raise ValueError(f'PCI device ID env variable "{self.ENV_VAR}" is unset.')

            pci_ids = pci_ids.split()
            if not pci_ids:
                raise ValueError(f'PCI device ID env variable "{self.ENV_VAR}" is set but empty.')
            pci_id = pci_ids[0]

        if not pci_id:
            raise ValueError(
                'Missing PCI device ID. Please specify as a parameter to the fixture or via the '
                f'env variable "{self.ENV_VAR}".')
        self.pci_id = pci_id

    @keyword
    def device_start(self):
        self.dev = variables.Device(self.pci_id)
        self.dev.start()

        # Make the device available to test cases.
        variables.dev = self.dev

    @keyword
    def device_stop(self):
        variables.dev = None
        self.dev.stop()
        del self.dev
