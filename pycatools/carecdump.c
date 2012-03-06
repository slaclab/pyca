#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/time.h>
#include "cadef.h"
#include "db_access.h"

FILE *fp = NULL;
int nelem = 0;
chtype dbftype;
int dbrtype;
int size = 0;
char *buf;

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: carecdump FILE\n");
        exit(0);
    }

    if (!(fp = fopen(argv[1], "r"))) {
        fprintf(stderr, "Cannot open %s for input!\n", argv[1]);
        exit(0);
    }

    // Read our header!
    if (!fread(&dbftype, sizeof(dbftype), 1, fp) ||
        !fread(&dbrtype, sizeof(dbrtype), 1, fp) ||
        !fread(&nelem, sizeof(nelem), 1, fp)) {
        fprintf(stderr, "Cannot read header.\n");
        exit(0);
    }

    size = dbr_size_n(dbrtype, nelem);
    buf = malloc(size);

    while (fread(buf, size, 1, fp)) {
        switch (dbrtype) {
        case DBR_TIME_DOUBLE: {
            struct dbr_time_double *db = (struct dbr_time_double *) buf;
            printf("%08x.%08x %.1lf\n", db->stamp.secPastEpoch, db->stamp.nsec, db->value);
            break;
        }
        default: {
            struct dbr_time_float *db = (struct dbr_time_float *) buf;
            printf("%08x.%08x ???\n", db->stamp.secPastEpoch, db->stamp.nsec);
            break;
        }
        }
    }

    fclose(fp);

    ca_context_destroy();
    return 0;
}
