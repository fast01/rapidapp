import platform
import os

env = Environment(CCFLAGS='-Werror -g -pg',
                 LIBPATH=['/usr/local/libevent/lib', '/usr/local/protoc/lib', './'],
                 CPPPATH=['/usr/local/libevent/include/', '/usr/local/protoc/include/', './'])
Export('env')

SConscript('SConscript')
