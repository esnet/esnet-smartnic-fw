# Definitions from /usr/include/asm-generic/ioctl.h for linux
_IOC_NRBITS   =  8
_IOC_TYPEBITS =  8
_IOC_SIZEBITS = 14
_IOC_DIRBITS  =  2

_IOC_NRMASK   = ((1 << _IOC_NRBITS)-1)
_IOC_TYPEMASK = ((1 << _IOC_TYPEBITS)-1)
_IOC_SIZEMASK = ((1 << _IOC_SIZEBITS)-1)
_IOC_DIRMASK  = ((1 << _IOC_DIRBITS)-1)

_IOC_NRSHIFT   = 0
_IOC_TYPESHIFT = (_IOC_NRSHIFT+_IOC_NRBITS)
_IOC_SIZESHIFT = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
_IOC_DIRSHIFT  = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

IOC_NONE  = 0
IOC_WRITE = 1
IOC_READ  = 2

def IOC(d, t, nr, size):
    if type(t) != int:
        t = ord(t)

    return (d << _IOC_DIRSHIFT) | (t << _IOC_TYPESHIFT) | (nr << _IOC_NRSHIFT) | (size << _IOC_SIZESHIFT)

def IOC_TYPECHECK(t):
    return (ctypes.sizeof(t))

def IO(type, nr):
    return IOC(IOC_NONE, type, nr, 0)

def IOR(type, nr, size):
    return IOC(IOC_READ, type, nr, IOC_TYPECHECK(size))

def IOW(type, nr, size):
    return IOC(IOC_WRITE, type, nr, IOC_TYPECHECK(size))

def IOWR(type, nr, size):
    return IOC(IOC_READ | IOC_WRITE, type, nr, IOC_TYPECHECK(size))

def IOC_DIR(nr):
    return (nr >> _IOC_DIRSHIFT) & _IOC_DIRMASK

def IOC_TYPE(nr):
    return (nr >> _IOC_TYPESHIFT) & _IOC_TYPEMASK

def IOC_NR(nr):
    return (nr >> _IOC_NRSHIFT) & _IOC_NRMASK

def IOC_SIZE(nr):
    return (nr >> _IOC_SIZESHIFT) & _IOC_SIZEMASK

IOC_IN        = (IOC_WRITE << _IOC_DIRSHIFT)
IOC_OUT       = (IOC_READ << _IOC_DIRSHIFT)
IOC_INOUT     = ((IOC_WRITE|IOC_READ) << _IOC_DIRSHIFT)
IOC_SIZEMASK  = (_IOC_SIZEMASK << _IOC_SIZESHIFT)
IOC_SIZESHIFT = (_IOC_SIZESHIFT)


