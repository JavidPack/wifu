all:
	./waf configure -j4 -p --prefix='.'
	./waf build -j4 -p
	./waf install -j4 -p

clean:
	./waf uninstall -j4 -p
	./waf clean -j4 -p
	./waf distclean -j4 -p

mesh:
	./waf configure -j4  -p --32 --prefix='.'
	./waf build -j4  -p --32
	./waf install -j4  -p --32

debug:
	./waf configure -j4  -p --debug --prefix='.'
	./waf build -j4  -p --debug
	./waf install -j4  -p --debug

prof:
	./waf configure -j4 --prof --prefix='.'
	./waf build -j4 --prof
	./waf install -j4 --prof

dist:
	./waf configure -j4 --dist --prefix='.'
	./waf build -j4 --dist
	./waf install -j4 --dist