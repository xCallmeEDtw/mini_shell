# set complier
CC = clang++
CFLAGS = -Wall

# def source c
SRCS := $(wildcard *.cpp)
BINS := $(patsubst %.cpp,%,$(SRCS))

# all  exec gmake exec
all: $(BINS)

# define rule
%: %.cpp
	$(CC) $(CFLAGS) $< -o $@

# clean tmp object
clean:
	rm -f $(BINS)

