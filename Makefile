# The compiler of choice because I really like `clangd` and whould not like to
# get surprised by the differences between `clang` and `g++`.
CC = clang++

# According to to the [documentation](https://libcxx.llvm.org/UsingLibcxx.html)
# you need to use the following flags to use a custom build of `libc++` for
# `clang`.
CUSTOM_LIBCXX =                                       \
	-nostdinc++                                         \
	-nostdlib++                                         \
	-isystem $(shell brew --prefix llvm)/include/c++/v1 \
	-L $(shell brew --prefix llvm)/lib                  \
	-Wl,-rpath,$(shell brew --prefix llvm)/lib          \
	-lc++

# Compiler flags.
CFLAGS =                                  \
	-std=c++23                              \
	-stdlib=libc++                          \
	$(CUSTOM_LIBCXX)                        \
	-isystem $(shell brew --prefix)/include \
	-Wall                                   \
	-Wextra                                 \
	-Wpedantic                              \
	-Werror                                 \
	-g                                      \
	-DDEBUG

# Target executable.
TARGET = hibiscus

# Source files.
SOURCES = main.cpp freelist.cpp hibiscus.cpp chi/page.cpp chi/panic.cpp

STYLE = '{                                       \
        AllowShortFunctionsOnASingleLine: Empty, \
        AlwaysBreakTemplateDeclarations: Yes,    \
        IndentWidth: 8,                          \
        IndentAccessModifiers: false,            \
        AccessModifierOffset: -8,                \
        ColumnLimit: 120                         \
}'

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

format:
	clang-format --style=$(STYLE) -i *.cpp *.h

clean:
	rm -f $(TARGET)
