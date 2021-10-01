/* replace using posix regular expressions */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <time.h>

#define FILEBUF 65536

const char *HOSTS_FILE = "/etc/hosts";
const char *LOCATION   = "/boot/SAAM/location.id";
const char *TZTAB = "/usr/share/zoneinfo/zone.tab";
const char *TZSEARCH = "Timezone=";
const char *REGEX      = "(^[[:space:]]*127\\.0\\.1\\.1[[:space:]]*)[[:alpha:]].*$";

// Regular expression search and replace with max 9 match variables
int rreplace (char *buf, int size, regex_t *re, char *rp) {
   char *pos;
   int sub, so, n;
   regmatch_t pmatch [10]; /* regoff_t is int so size is int */

   if (regexec (re, buf, 10, pmatch, 0)) return 0;
   for (pos = rp; *pos; pos++)
      if (*pos == '\\' && *(pos + 1) > '0' && *(pos + 1) <= '9') {
         so = pmatch [*(pos + 1) - 48].rm_so;
         n = pmatch [*(pos + 1) - 48].rm_eo - so;
         if (so < 0 || strlen (rp) + n - 1 > size) return 1;
         memmove (pos + n, pos + 2, strlen (pos) - 1);
         memmove (pos, buf + so, n);
         pos = pos + n - 2;
      }
   sub = pmatch [1].rm_so; /* no repeated replace when sub >= 0 */
   for (pos = buf; !regexec (re, pos, 1, pmatch, 0); ) {
      n = pmatch [0].rm_eo - pmatch [0].rm_so;
      pos += pmatch [0].rm_so;
      if (strlen (buf) - n + strlen (rp) + 1 > size) return 1;
      memmove (pos + strlen (rp), pos + n, strlen (pos) - n + 1);
      memmove (pos, rp, strlen (rp));
      pos += strlen (rp);
      if (sub >= 0) break;
   }
   return 0;
}

int modify_file(const char *filename, regex_t *re, char *rstr) {
   char buf[FILENAME_MAX], rp[FILENAME_MAX], tmp[FILENAME_MAX];
   char *buffer;
   size_t count;
   FILE *in, *out;
   struct stat fst;
   time_t t = time(NULL);
   struct tm time_info = *localtime(&t);
   int err;
 
   err = 1;
   if (stat(filename, &fst) == -1) goto err_last;       // cannot read hosts status
   if ((in = fopen(filename, "r")) == NULL) goto err_last; // cannot open hosts file
   strftime(buf, FILENAME_MAX, "%Y.%m.%d", &time_info);
   sprintf(tmp, "%s-%s", filename, buf);
   if ((out = fopen(tmp, "w")) == NULL) goto err_in;      // cannot open new host file
   if (!(buffer = (char *)malloc(FILEBUF))) goto err_out; // cannot allocate buffer
   while ((count = fread( buffer, 1, FILEBUF, in )) > 0)  // Backup original file
      if (fwrite(buffer, 1, count, out) != count) break;  // Error, could not write the full length.
   free(buffer);
   if (count > 0) goto err_out; // Error, could not write the full length.
   fclose(in);
   fclose(out);
   chmod(tmp, fst.st_mode);            // Restore file permissions from original
   chown(tmp, fst.st_uid, fst.st_gid); // Restore file ownership from original
   if ((in = fopen(tmp, "r")) == NULL) goto err_last;       // cannot open new host file
   if ((out = fopen(filename, "w")) == NULL) goto err_in; // cannot open hosts file
   for (; buffer=fgets(buf, FILENAME_MAX, in); ) {
      strcpy(rp, rstr);
      if (rreplace(buf, FILENAME_MAX, re, rp)) break;
      fprintf(out, "%s", buf);
   }
   if (!buffer) err = 0;  // All ok
err_out:
   fclose(out);
err_in:
   fclose(in);
err_last:
   return err;
}

char *get_timezone(char *Country) {
  int clen, i;
  char *p, *q, *tz, buf[256];
  FILE *fp;

  for (p=Country; *p; p++)
    *p = toupper(*p);
  if ((fp=fopen(TZTAB, "rt")) == NULL) return NULL;
  clen = strlen(Country);
  tz = NULL;
  while (fgets(buf, 255, fp)) {
    for (p=buf; isspace(*p) && *p; p++);
    if (strncmp(p, Country, clen) == 0) {
      for (p+=clen; *p; p++) if (! isspace(*p)) break;
      if (! *p) continue;
      for (; *p; p++) if (isspace(*p)) break;
      if (! *p) continue;
      for (; *p; p++) if (! isspace(*p)) break;
      if (! *p) continue;
      for (q=p; *q; q++) if (isspace(*q)) break;
      *q = '\0';
      tz = strdup(p);
    }
  }
  return tz;
}

char *current_timezone() {
  int hnd[2], blen;
  pid_t pid;
  char buff[4096], *tz;
  FILE *pfd;

  if (pipe(hnd) == -1) return NULL;
  if ((pid = fork()) == -1) return NULL;
  if (pid == 0) {
    dup2(hnd[1], STDOUT_FILENO);
    close(hnd[0]);
    close(hnd[1]);
    execl("/usr/bin/timedatectl", "timedatectl", "show", (char *)NULL);
    exit(0);
  } else {
    close(hnd[1]);
    pfd = fdopen(hnd[0], "r");
    tz = NULL;
    buff[4095] = '\0';
    while (fgets(buff, 4095, pfd)) {
      blen = strlen(buff)-1;
      if (strncmp(buff, TZSEARCH, strlen(TZSEARCH))) 
        continue;
      if (buff[blen] == '\n') buff[blen] = '\0';
      tz = strdup(buff+strlen(TZSEARCH));
    }
    return tz;
  }
}

// when location == NULL inquire the current location_id
char *update_location(const char *filename, char *location) { 
   char buf[FILENAME_MAX];
   char *buffer;
   size_t count;
   FILE *fp;

   buf[0] = 0;
   if (fp = fopen(filename, "r")) {
      fgets(buf, FILENAME_MAX, fp);
      fclose(fp);
      strtok(buf, "\n");
   }
   if (! location) return buf[0] ? strdup(buf) : NULL;   // return current location_id
   // if location != NULL record new location_id
   if (strcmp(buf, location) == 0) return NULL;          // no change in location_id
   if ((fp = fopen(filename, "w")) == NULL) return NULL; // cannot write to location file
   fprintf(fp, "%s\n", location);
   fclose(fp);
   return location;
}

int main (int argc, char **argv)  {
   char buf[FILENAME_MAX];
   char *hostname, *p, *tz;
   regex_t re;
   char *replace;
   int len, err;

   // Set/get location_id from file
   p = NULL;
   if (argc == 2) p = argv[1];
   p = update_location(LOCATION, p);
   if (p == NULL) return 0;
   if (! *p) return 0;
   if (!(hostname = (char *) malloc(strlen(p) + 10))) 
     return -1;  // cannot allocate buffer
   // Set hostname generated from location_id
   sprintf(hostname, "%s-amb", p);  // from location_id determine hostname
   for (p=hostname; p=strchr(p, '_'); *p++='-');
   if (gethostname(buf, FILENAME_MAX) == 0)
     if (strcmp(hostname, buf) == 0) return 0; // hostname is already set
   sprintf(buf, "hostnamectl set-hostname %s", hostname);
   system(buf);
   // modify hosts file
   sprintf(buf, "\\1%s", hostname);
   if (regcomp (&re, REGEX, REG_EXTENDED|REG_NEWLINE)) return -1;
   err = modify_file(HOSTS_FILE, &re, buf);
   regfree (&re);
   // Adjust timezone if necessary
   p = hostname;
   len = strlen(p) - 4;
   p[len] = '\0'; // Eliminate ending '-amb' from the hostname
   if (len < 2) return err;
   for (p=hostname+2; *p; p++)
      if (isdigit(*p)) *p = '\0';
      else break;
   if (*p) return err;
  
   if (strcmp((tz = get_timezone(hostname)), current_timezone())) {
      execl("/usr/bin/timedatectl", "timedatectl", "--no-ask-password", \
            "set-timezone", tz, (char *)NULL);
   }
   return err;
}

