CC = mipsel-unknown-linux-gnu-gcc
STRIP = mipsel-unknown-linux-gnu-strip
CFLAGS = -Wall -O2
TARGET = ups_reader
SRC = ups_reader.c

# Точный путь к линкеру вашего роутера
ROUTER_LINKER = /lib/ld-musl-mipsel-sf.so.1

all: static

static: CFLAGS += -static
static: $(TARGET)

# Динамическая сборка с подменой линкера
shared: CFLAGS += -Wl,-dynamic-linker=$(ROUTER_LINKER)
shared: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	$(STRIP) $(TARGET)
	@echo "--- Информация о файле: ---"
	@ls -lh $(TARGET)
	file $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all static shared clean

