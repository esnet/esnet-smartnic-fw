__all__ = ()

from robot.api.deco import keyword, library

from .tests.endian_check import *

#---------------------------------------------------------------------------------------------------
@library
class Library:
    @keyword
    def endian_check_packed_to_unpacked(self, dev, exp_data):
        endian_check_packed_to_unpacked(dev, exp_data)

    @keyword
    def endian_check_unpacked_to_packed(self, dev, exp_data):
        endian_check_unpacked_to_packed(dev, exp_data)
