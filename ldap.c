#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>

#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
// Anonymes bind funktioniert nur im technikum
#define BIND_USER "uid=if09b505,ou=People,dc=technikum-wien,dc=at"		/* anonymous bind with user and pw NULL */
#define BIND_PW "kumbeiz123456"

int verify_user(char *bind_user, char *bind_pw)
{
   LDAP *ld;			/* LDAP resource handle */

   int rc=0;

   char *attribs[3];		/* attribute array for search */

   attribs[0]=strdup("uid");		/* return uid and cn of entries */
   attribs[1]=strdup("cn");
   attribs[2]=NULL;		/* array must be NULL terminated */


   /* setup LDAP connection */
   if ((ld=ldap_init(LDAP_HOST, LDAP_PORT)) == NULL)
   {
      perror("ldap_init failed");
      return EXIT_FAILURE;
   }

   printf("connected to LDAP server %s on port %d\n",LDAP_HOST,LDAP_PORT);

   /* anonymous bind */
   rc = ldap_simple_bind_s(ld,BIND_USER,BIND_PW);

   if (rc != LDAP_SUCCESS)
   {
		ldap_unbind(ld);
		return 0;
   }
   else
   {
		ldap_unbind(ld);
		return 1;
   }
}
   
