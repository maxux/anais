CORE_EXEC = anais

# flags
CFLAGS   = -W -Wall -O2 -pipe -g -ansi -pedantic -std=gnu99
LDFLAGS  = -lcrypto -lssl -lm -lsqlite3 -lpthread

CC = gcc
