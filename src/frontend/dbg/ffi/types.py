from ctypes import Structure, c_char, c_int, c_uint8, c_uint32, c_uint64


class DbgInstructionView(Structure):
    _fields_ = [
        ("address", c_uint64),
        ("size", c_uint8),
        ("text", c_char * 256),
        ("bytes", c_char * 128),
        ("is_current", c_int),
        ("kind", c_uint32),
    ]


class DbgMemoryRegionView(Structure):
    _fields_ = [
        ("base", c_uint64),
        ("size", c_uint64),
        ("state", c_uint32),
        ("protect", c_uint32),
        ("type", c_uint32),
        ("info", c_char * 512),
    ]


class DbgBreakpointView(Structure):
    _fields_ = [
        ("address", c_uint64),
        ("hit_count", c_uint64),
        ("kind", c_uint32),
        ("enabled", c_int),
    ]


class DbgRegisterView(Structure):
    _fields_ = [
        ("name", c_char * 32),
        ("value", c_uint64),
    ]


class DbgThreadInfoView(Structure):
    _fields_ = [
        ("tid", c_uint32),
        ("teb", c_uint64),
        ("alive", c_int),
        ("suspended", c_int),
        ("is_current", c_int),
        ("frame_index", c_uint32),
        ("callstack_address", c_uint64),
    ]
