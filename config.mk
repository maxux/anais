CORE_EXEC = anais

# flags
CFLAGS   = -fpic -W -Wall -O2 -pipe -g -ansi -pedantic -std=gnu99
LDFLAGS  = -ldl -lcrypto -lssl -lm -lsqlite3 -lpthread

CC = gcc
