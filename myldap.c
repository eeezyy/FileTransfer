#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>

#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
#define SEARCHBASE "dc=technikum-wien,dc=at"
#define SCOPE LDAP_SCOPE_SUBTREE
#define FILTER "(uid=if09b*)"
// Anonymes bind funktioniert nur im technikum
#define BIND_USER NULL		/* anonymous bind with user and pw NULL */
#define BIND_PW NULL

int main()
{
   LDAP *ld;			/* LDAP resource handle */
   LDAPMessage *result, *e;	/* LDAP result handle */
   BerElement *ber;		/* array of attributes */
   char *attribute;
   char **vals;

   int i,rc=0;

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
      fprintf(stderr,"LDAP error: %s\n",ldap_err2string(rc));
      return EXIT_FAILURE;
   }
   else
   {
      printf("bind successful\n");
   }

   /* perform ldap search */
   rc = ldap_search_s(ld, SEARCHBASE, SCOPE, FILTER, attribs, 0, &result);

   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr,"LDAP search error: %s\n",ldap_err2string(rc));
      return EXIT_FAILURE;
   }

   printf("Total results: %d\n", ldap_count_entries(ld, result));

   for (e = ldap_first_entry(ld, result); e != NULL; e = ldap_next_entry(ld,e))
   {
      printf("DN: %s\n", ldap_get_dn(ld,e));
      
      /* Now print the attributes and values of each found entry */

      for (attribute = ldap_first_attribute(ld,e,&ber); attribute!=NULL; attribute = ldap_next_attribute(ld,e,ber))
      {
         if ((vals=ldap_get_values(ld,e,attribute)) != NULL)
         {
            for (i=0;vals[i]!=NULL;i++)
            {
               printf("\t%s: %s\n",attribute,vals[i]);
            }
            /* free memory used to store the values of the attribute */
            ldap_value_free(vals);
         }
         /* free memory used to store the attribute */
         ldap_memfree(attribute);
      }
      /* free memory used to store the value structure */
      if (ber != NULL) ber_free(ber,0);

      printf("\n");
   }
  
   /* free memory used for result */
   ldap_msgfree(result);
   free(attribs[0]);
   free(attribs[1]);
   printf("LDAP search suceeded\n");
   
   ldap_unbind(ld);
   return EXIT_SUCCESS;
}
   
