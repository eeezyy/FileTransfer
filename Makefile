# Makefile for 'ftp-server/client'
VERSION=1.0.0
CFLAGS=-g -Wall -O
PROGRAM_SERVER=server
PROGRAM_CLIENT=client

all: ${PROGRAM_SERVER} ${PROGRAM_CLIENT}

${PROGRAM_SERVER}: ${PROGRAM_SERVER}.o
	gcc ${CFLAGS} -o ${PROGRAM_SERVER} ${PROGRAM_SERVER}.o

${PROGRAM_SERVER}.o: ${PROGRAM_SERVER}.c ${PROGRAM_SERVER}.h heap.h types.h prioritysearch.h
	gcc ${CFLAGS} -c ${PROGRAM_SERVER}.c
	
${PROGRAM_CLIENT}: ${PROGRAM_CLIENT}.o
	gcc ${CFLAGS} -o ${PROGRAM_CLIENT} ${PROGRAM_CLIENT}.o

${PROGRAM_CLIENT}.o: ${PROGRAM_CLIENT}.c ${PROGRAM_CLIENT}.h heap.h types.h prioritysearch.h
	gcc ${CFLAGS} -c ${PROGRAM_CLIENT}.c