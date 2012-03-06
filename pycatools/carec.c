#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/time.h>
#include "cadef.h"

FILE *fp = NULL;
int nelem = 0;
int dbrtype;
int records = 0;
int haveint = 0;
int size = 0;
evid event = 0;
int alrm = 0;
struct timeval start, stop;

void int_handler(int signal)
{
    haveint = 1;
}

void event_handler(struct event_handler_args args)
{
    if (args.status != ECA_NORMAL) {
        printf("Bad status: %d\n", args.status);
    } else if (args.type == dbrtype && args.count == nelem) {
        if (!fwrite(args.dbr, size, 1, fp)) {
            fprintf(stderr, "Cannot write to file!\n");
            exit(0);
        }
        fflush(fp);
        records++;
    } else {
        printf("type = %d, count = %d -> expected type = %d, count = %d\n",
               args.type, args.count, dbrtype, nelem);
    }
    fflush(stdout);
}

void connection_handler(struct connection_handler_args args)
{
    chtype dbftype;
    int status;

    if (args.op == CA_OP_CONN_UP) {
        nelem = ca_element_count(args.chid);
        dbftype = ca_field_type(args.chid);
        dbrtype = dbf_type_to_DBR_TIME(dbftype);
        if (dbr_type_is_ENUM(dbrtype))
            dbrtype = DBR_TIME_INT;
        size = dbr_size_n(dbrtype, nelem);

        /* Save the type and element information into the file! */
        if (!fwrite(&dbftype, sizeof(dbftype), 1, fp) ||
            !fwrite(&dbrtype, sizeof(dbrtype), 1, fp) ||
            !fwrite(&nelem, sizeof(nelem), 1, fp)) {
            fprintf(stderr, "Failed to write to output file!\n");
            exit(0);
        }
        fflush(fp);

        if (alrm)
            alarm(alrm);
        gettimeofday(&start, NULL);
        status = ca_create_subscription(dbrtype, nelem, args.chid, DBE_VALUE | DBE_ALARM,
                                        event_handler, NULL, &event);
        if (status != ECA_NORMAL) {
            fprintf(stderr, "Failed to create subscription! error %d!\n", status);
            exit(0);
        }
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    chid chan;
    char *pvname;
    int result;
    double runtime;

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: carec PV OUTPUT_FILE [ TIMEOUT ]\n");
        exit(0);
    }
    if (argc == 4)
        alrm = atoi(argv[3]);

    pvname = argv[1];
    if (!(fp = fopen(argv[2], "w"))) {
        fprintf(stderr, "Cannot open %s for output!\n", argv[2]);
        exit(0);
    }

    ca_context_create(ca_disable_preemptive_callback);

    result = ca_create_channel (pvname,
                                connection_handler,
                                pvname,
                                50, /* priority?!? */
                                &chan);
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s while creating channel to %s!\n", ca_message(result), pvname);
        exit(0);
    }

    signal(SIGINT, int_handler);
    signal(SIGALRM, int_handler);

    while (!haveint)
        ca_pend_event(2.0);

    if (event)
        ca_clear_subscription(event);
    ca_pend_io(0.0);

    gettimeofday(&stop, NULL);
    runtime = (1000000LL * stop.tv_sec + stop.tv_usec) - 
              ( 1000000LL * start.tv_sec + start.tv_usec);
    runtime /= 1000000.;
    printf("Wrote %d records of %d bytes each (total file size = %d bytes)\n",
           records, size,
           records * size + 2 * sizeof(int) + sizeof(chtype));
    printf("Runtime: %.4lf seconds, Rate: %.2lf Hz\n", runtime, ((double) records) / runtime);
    fclose(fp);

    ca_context_destroy();
    return 0;
}
