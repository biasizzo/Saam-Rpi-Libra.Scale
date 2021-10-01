#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

const char *TZTAB = "/usr/share/zoneinfo/zone.tab";
const char *TZSEARCH = "Timezone=";
const char *LOCATION = "/boot/SAAM/location.id";

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

int main() {
  const char *LOCID = "SI";
  char *p, buff[256];
  int len;
  char *tz;
  FILE *fp;

  if ((fp = fopen(LOCATION, "rt")) == NULL) exit(-1);
  fgets(buff, 255, fp);
  fclose(fp);
  len = strlen(buff) - 1;
  if (buff[len] == '\n') buff[len] = '\0';
  len = strlen(buff);
  if (len < 2) exit(-2);
  for (p=buff+2; *p; p++)
    if (isdigit(*p)) *p = '\0';
    else break;
  if (*p) exit(-3);
  
  if (strcmp((tz = get_timezone(buff)), current_timezone())) {
    execl("/usr/bin/timedatectl", "timedatectl", "--no-ask-password", \
          "set-timezone", tz, (char *)NULL);
  }
}
