CFLAGS=-Wall -O3 -Wextra -Wno-unused-parameter
CXXFLAGS=$(CFLAGS)
OBJECTS=emulate.o
BINARIES=emulate

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=rpi-rgb-led-matrix
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread

all : $(BINARIES)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

emulate : emulate.o $(RGB_LIBRARY)
	$(CXX) $< -o $@ $(LDFLAGS)

# All the binaries that have the same name as the object file.q
% : %.o $(RGB_LIBRARY)
	$(CXX) $< -o $@ $(LDFLAGS)

# Since the C example uses the C++ library underneath, which depends on C++
# runtime stuff, you still have to also link -lstdc++
c-example : c-example.o $(RGB_LIBRARY)
	$(CC) $< -o $@ $(LDFLAGS) -lstdc++

emulate.o : game.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BINARIES)

FORCE:
.PHONY: FORCE

# CFLAGS=-Wall -O3 -g -Wextra -Wno-unused-parameter
# CXXFLAGS=$(CFLAGS)
# OBJECTS=main.o
# BINARIES=main

# # Where our library resides. You mostly only need to change the
# # RGB_LIB_DISTRIBUTION, this is where the library is checked out.
# RGB_LIB_DISTRIBUTION=rpi-rgb-led-matrix
# RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
# RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
# RGB_LIBRARY_NAME=rgbmatrix
# RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
# LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread

# all : $(BINARIES)

# $(RGB_LIBRARY): FORCE
# 	$(MAKE) -C $(RGB_LIBDIR)

# main : main.o $(RGB_LIBRARY)
# 	$(CXX) $< -o $@ $(LDFLAGS)

# # All the binaries that have the same name as the object file.q
# % : %.o $(RGB_LIBRARY)
# 	$(CXX) $< -o $@ $(LDFLAGS)

# # Since the C example uses the C++ library underneath, which depends on C++
# # runtime stuff, you still have to also link -lstdc++
# c-example : c-example.o $(RGB_LIBRARY)
# 	$(CC) $< -o $@ $(LDFLAGS) -lstdc++

# main.o : chip8/main.cpp chip8/chip8.h chip8/chip8.cpp
# 	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<

# clean:
# 	rm -f $(OBJECTS) $(BINARIES)

# FORCE:
# .PHONY: FORCE
