include config.mk

# Core files
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: options $(CORE_EXEC)

options: config.mk
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "OBJ     = $(OBJ)"


$(CORE_EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ):
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) $*.c

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(CORE_EXEC)

