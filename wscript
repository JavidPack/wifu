import Options
import subprocess
import os
import os.path
import re

top = '.'
out = 'build'

def set_options(opt):
	# opt.tool_options('compiler_cxx')

	opt.add_option('--32',
					help=('Compile 32-bit version.'),
					action="store_true", default=False,
					dest='bit_32')

	opt.add_option('--debug',
					help=('Compile with debugging symbols.'),
					action="store_true", default=False,
					dest='debug')


def configure(conf):
	print('→ configuring the project')
	# conf.check_tool('compiler_cxx')
	conf.check_tool('gcc g++')
	conf.env['LIBPATH'] += ['../lib/unittest++']
	conf.env['LIB_PTHREAD'] = ['pthread']
	conf.env['STATICLIB'] += ['UnitTest++']
	conf.env['LIB_RT'] = ['rt']
	conf.env.PREFIX = ".."

	if Options.options.bit_32:
		print "Configuring for 32-bit compilation"
		conf.env['LIBPATH'] += ['../lib']
		conf.env['CXXFLAGS'] += [ '-m32']
		conf.env['LINKFLAGS'] += ['-m32']

	if Options.options.debug:
		print "Configuring with debugging symbols"
		conf.env['CXXFLAGS'] += ['-g']

def post(ctx):
	import os
	val = 0
	count = 1
	for i in range(0, count):
		val = os.system("bin/wifu-end-test")
	
	val = (val >> 8)

	# val now contains the number of tests which failed
	if val > 0:
		error = "%d error(s) encountered during wifu-end tests." %(val)
		#raise Exception(error)

	val = os.system("bin/wifu-frontend-test")
	val = (val >> 8)

	# val now contains the number of tests which failed
	if val > 0:
		error = "%d error(s) encountered during wifu-frontend tests." %(val)
		#raise Exception(error)

def build_blaster(bld):
    # udp blaster
	udp_files = bld.glob('preliminary/Timer.cc')
	udp_files += bld.glob('preliminary/UDPBlaster.cc')
	udp_files += bld.glob('src/UDPSocket.cc')
	udp_files += bld.glob('src/Semaphore.cc')

	udp_blaster = bld(features='cxx cprogram',
        source=udp_files,
        includes='headers',
        uselib='PTHREAD RT',
		target='udp-blaster')

def build_sink(bld):
	# udp sink
	udp_sink_files = bld.glob('preliminary/Timer.cc')
	udp_sink_files += bld.glob('preliminary/UDPSink.cc')
	udp_sink_files += bld.glob('src/UDPSocket.cc')
	udp_sink_files += bld.glob('src/Semaphore.cc')

	udp_sink = bld(features='cxx cprogram',
        source=udp_sink_files,
        includes='headers',
        uselib='PTHREAD RT',
		target='udp-sink')

def build_simpletcp(bld):
	# SimpleTCP
	simple_tcp_files = bld.glob('preliminary/SimpleTCPServer.cc')
	simple_tcp_files += bld.glob('preliminary/SimpleTCP.cc')
	simple_tcp_files += bld.glob('src/UDPSocket.cc')
	simple_tcp_files += bld.glob('src/Semaphore.cc')
	simple_tcp_files += bld.glob('src/Identifiable.cc')
	simple_tcp_files += bld.glob('src/IDGenerator.cc')

	udp_sink = bld(features='cxx cprogram',
        source=simple_tcp_files,
        includes='headers',
        uselib='PTHREAD RT',
		target='simple-tcp-server')

#	simple_tcp_files = bld.glob('preliminary/SimpleTCPClient.cc')
#	simple_tcp_files += bld.glob('preliminary/SimpleTCP.cc')
#	simple_tcp_files += bld.glob('src/UDPSocket.cc')
#	simple_tcp_files += bld.glob('src/Semaphore.cc')
#	simple_tcp_files += bld.glob('src/Identifiable.cc')
#	simple_tcp_files += bld.glob('src/IDGenerator.cc')
#	udp_sink = bld(features='cxx cprogram',
#        source=simple_tcp_files,
#        includes='headers',
#        uselib='PTHREAD RT',
#		target='simple-tcp-client')


def build_staticlib(bld):
    # shared files
	src_files = bld.glob('src/*.cc')

	# shared library
	api_files = bld.glob('applib/*.cc')
	api_files += src_files

	lib = bld(features='cxx cstaticlib',
        source=api_files,
        includes='applib headers lib/gc/include',
		ccflags="-c -fPIC",
		export_incdirs='applib lib/gc/include',
		uselib='PTHREAD RT',
		libpath = ['../lib/gc'],
		staticlib = ['gccpp','gc','cord'],
        target='wifu-end-api')

def build_wifu(bld):
        src_files = bld.glob('src/*.cc')
        src_files += bld.glob('src/contexts/*.cc')
        src_files += bld.glob('src/states/*.cc')

	exe = bld(features='cxx cprogram',
        source=src_files,
        includes='headers lib/gc/include headers/contexts headers/states',
        uselib='PTHREAD RT',
		libpath = ['../lib/gc'],
		staticlib = ['gccpp','gc','cord'],
        target='wifu-end')

def build_wifu_end_test(bld):
	test_end_files = bld.glob('test/end/*.cc')
	src_files = bld.glob('src/*.cc')
        src_files += bld.glob('src/contexts/*.cc')
        src_files += bld.glob('src/states/*.cc')

	all_files = src_files
	all_files += test_end_files
	all_files.remove("src/main.cc")

	test_end = bld(features='cxx cprogram',
        source=all_files,
        includes='headers test/end/headers lib/gc/include lib/unittest++/include headers/contexts headers/states',
        uselib='PTHREAD RT',
		libpath = '../lib/gc',
		staticlib = ['gccpp','gc','cord'],
		target='wifu-end-test')
		
def build_wifu_frontend_test(bld):
	test_frontend_files = bld.glob('test/frontend/*.cc')

	test_frontend = bld(features='cxx cprogram',
		source=test_frontend_files,
		includes='test/frontend/headers lib/unittest++/include',
		libpath = '../lib/gc',
		staticlib = ['gccpp', 'gc', 'cord'],
		uselib_local='wifu-end-api',
		target='wifu-frontend-test')

def build(bld):
#	build_blaster(bld)
#	build_sink(bld)
#	build_simpletcp(bld)

	build_staticlib(bld)
	build_wifu(bld)
	build_wifu_end_test(bld)
	build_wifu_frontend_test(bld)

	bld.add_post_fun(post)

def get_files(dir, regex):
        list = []
        for filename in os.listdir (dir):
            # Ignore subfolders
            path = os.path.join (dir, filename)
            if os.path.isdir (path):
                continue

            # Ignore mismatched regex
            if not re.match(regex, filename):
                continue

            list.append(path)

        list.sort()
        return list
