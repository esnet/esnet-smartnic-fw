from robot.api.deco import keyword, library

import os
import variables # Module used for passing global variables to test cases.

#---------------------------------------------------------------------------------------------------
@library
class Library:
    def __init__(self, pci_id=None):
        if pci_id is None:
            pci_id = os.environ['REGIO_SELECT']
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
