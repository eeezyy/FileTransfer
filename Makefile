# Makefile for 'ftp-server/client'
VERSION=1.0.0
CFLAGS=-g -lpthread -Wall -O
PROGRAM_SERVER=server
PROGRAM_CLIENT=client
PROGRAM_LDAP=ldap

all: ${PROGRAM_SERVER} ${PROGRAM_CLIENT} ${PROGRAM_LDAP}

${PROGRAM_SERVER}: ${PROGRAM_SERVER}.o
	gcc ${CFLAGS} -o ${PROGRAM_SERVER} ${PROGRAM_SERVER}.o

${PROGRAM_SERVER}.o: ${PROGRAM_SERVER}.c
	gcc ${CFLAGS} -c ${PROGRAM_SERVER}.c
	
${PROGRAM_CLIENT}: ${PROGRAM_CLIENT}.o
	gcc ${CFLAGS} -o ${PROGRAM_CLIENT} ${PROGRAM_CLIENT}.o

${PROGRAM_CLIENT}.o: ${PROGRAM_CLIENT}.c
	gcc ${CFLAGS} -c ${PROGRAM_CLIENT}.c
	
${PROGRAM_LDAP}: ${PROGRAM_LDAP}.o
	gcc ${CFLAGS} -o ${PROGRAM_LDAP} ${PROGRAM_LDAP}.o -lldap -DLDAP_DEPRECATED

${PROGRAM_LDAP}.o: ${PROGRAM_LDAP}.c
	gcc ${CFLAGS} -c ${PROGRAM_LDAP}.c -lldap -DLDAP_DEPRECATED
	
clean: 
	rm server server.o client client.o ldap ldap.o
	
	# gcc -Wall -lldap -o ldap ldap.c -DLDAP_DEPRECATED
