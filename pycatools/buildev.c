#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/time.h>
#include<string.h>
#include<strings.h>
#include "cadef.h"

//#define DEBUG
//#define DEBUG2
//#define DEBUG3

#define OUTPUT_DELAY          180 /* 0.5 sec, in fiducials! */

FILE *outfp = NULL;
int haveint = 0;
struct timeval start, stop;

#define MAXPV     100
#define MAXEVENT  65536
#define EVTMASK   (MAXEVENT-1)
#define IDX(n)    (((n) + 65536) & EVTMASK)

union value {
    double d;
    long   l;
};

struct pv {
    char *name;
    int   flags;
#define FLAG_TRACE      1
#define FLAG_CONTINUOUS 2
#define FLAG_CVAL       4
    chid chan;
    evid event;
    int nelem;
    int dbrtype;
    int size;
    //    epicsTimeStamp last;
    union value    lval;
    int            lidx;
    union value    vals[MAXEVENT];
    long           ts[MAXEVENT];      // secPastEpoch, for event validation!
    char           haveval[MAXEVENT]; // This CPV value has been set!
} pvlist[MAXPV];
int pvcnt = 0;             // Total number of PVs.
int nccnt = 0;             // Total number of non-continuous PVs that need to be
                           // timestamp matched.
int ccnt = 0;              // Total number of continuous PVs that need to have a value.

int conncnt = 0;           // Total number of PVs connected.
int cvals = 0;             // How many continuous PVs have values.

epicsTimeStamp laststamp;
int lastev = MAXEVENT;            // The last event number we actually output.

struct event {
    epicsTimeStamp stamp;
    int            vcnt;
} elist[MAXEVENT];

void int_handler(int signal)
{
    haveint = 1;
}

int find_event(epicsTimeStamp *t)
{
    int i = t->nsec & EVTMASK, j;
    if (t->secPastEpoch == elist[i].stamp.secPastEpoch &&
        t->nsec == elist[i].stamp.nsec)
        return i;
#ifdef DEBUG
    printf("Throwing away ev%d with %d\n", i, elist[i].vcnt);
#endif
    elist[i].stamp = *t;
    elist[i].vcnt  = 0;
    for (j = 0; j < pvcnt; j++)
        pvlist[j].haveval[i] = 0;
    return i;
}

void output_event(int idx)
{
    int i;
#ifdef DEBUG
    printf("event %d, ccnt=%d, cvals=%d\n", idx, ccnt, cvals);
    fflush(stdout);
#endif
    fprintf(outfp, "%9d.%09d %5d ", elist[idx].stamp.secPastEpoch, elist[idx].stamp.nsec, 
            elist[idx].stamp.nsec & 0xffff);
    for (i = 0; i < pvcnt; i++) {
        if ((pvlist[i].flags & FLAG_CONTINUOUS) && !pvlist[i].haveval[idx]) {
            pvlist[i].vals[idx] = pvlist[i].lval;
            pvlist[i].lidx = idx;
            pvlist[i].haveval[idx] = 1;
        }
        switch (pvlist[i].dbrtype) {
        case DBR_TIME_INT:
        case DBR_TIME_LONG:
            fprintf(outfp, "%9d ", pvlist[i].vals[idx].l);
            break;
        case DBR_TIME_DOUBLE:
            fprintf(outfp, "%13.6e ", pvlist[i].vals[idx].d);
            break;
        }
    }
    fprintf(outfp, "\n");
    fflush(outfp);
    elist[idx].vcnt = 0; // Don't output this again!
}

void event_handler(struct event_handler_args args)
{
    struct pv *pv = (struct pv *)args.usr;
    int idx, i;
    union value v;
    epicsTimeStamp now;

    if (args.status != ECA_NORMAL) {
        printf("Bad status: %d\n", args.status);
        fflush(stdout);
        return;
    }
    if (args.type != pv->dbrtype || args.count != pv->nelem) {
        printf("%s: type = %d, count = %d -> expected type = %d, count = %d\n",
               pv->name, args.type, args.count, pv->dbrtype, pv->nelem);
        fflush(stdout);
        return;
    }
    switch (pv->dbrtype) {
        case DBR_TIME_INT: {
            struct dbr_time_short *d = (struct dbr_time_short *)args.dbr;
            if ((d->stamp.nsec & EVTMASK) == EVTMASK) // Skip bad fiducials!
                return;
            now = d->stamp;
            v.l = d->value;
#ifndef DEBUG
            if (pv->flags & FLAG_TRACE)
#endif
            {
                printf("%d.%08x %s %d ev%d\n", d->stamp.secPastEpoch, 
                       d->stamp.nsec, pv->name, d->value, now.nsec & EVTMASK);
                fflush(stdout);
            }
            break;
        }
        case DBR_TIME_LONG: {
            struct dbr_time_long *d = (struct dbr_time_long *)args.dbr;
            if ((d->stamp.nsec & EVTMASK) == EVTMASK) // Skip bad fiducials!
                return;
            now = d->stamp;
            v.l = d->value;
#ifndef DEBUG
            if (pv->flags & FLAG_TRACE)
#endif
            {
                printf("%d.%08x %s %d ev%d\n", d->stamp.secPastEpoch, 
                       d->stamp.nsec, pv->name, d->value, now.nsec & EVTMASK);
                fflush(stdout);
            }
            break;
        }
        case DBR_TIME_DOUBLE: {
            struct dbr_time_double *d = (struct dbr_time_double *)args.dbr;
            if ((d->stamp.nsec & EVTMASK) == EVTMASK) // Skip bad fiducials!
                return;
            now = d->stamp;
            v.d = d->value;
#ifndef DEBUG
            if (pv->flags & FLAG_TRACE)
#endif
            {
                printf("%d.%08x %s %.5lf ev%d\n", d->stamp.secPastEpoch, 
                       d->stamp.nsec, pv->name, d->value, now.nsec & EVTMASK);
                fflush(stdout);
            }
            break;
        }
    }

    idx = find_event(&now);
    if (pv->ts[idx] == now.secPastEpoch) {  // A duplicate timestamp!  Just flush it!
        elist[idx].vcnt = 0;
#ifdef DEBUG3
        printf("Duplicate time stamp %d.%08x for %s!\n",
               now.secPastEpoch, now.nsec, pv->name);
        fflush(stdout);
        fprintf(stderr, "Duplicate time stamp %d.%08x for %s!\n",
                now.secPastEpoch, now.nsec, pv->name);
        fflush(stderr);
#endif
        return;
    }

    // Save the value and timestamp associated with the event.
    pv->ts[idx] = now.secPastEpoch;
    pv->vals[idx] = v;
    pv->haveval[idx] = 1;

    if (pv->flags & FLAG_CONTINUOUS) {
        // Continuous PV

        // If this is the first value, count this PV as initialized.
        if (!(pv->flags & FLAG_CVAL)) {
            pv->flags |= FLAG_CVAL;
            cvals++;
#ifdef DEBUG
            printf("%s: cvals = %d\n", pv->name, cvals);
            fflush(stdout);
#endif
        }

        // If we have a prior value, store it into every event after the
        // last but prior to this one.
        if (pv->lidx != MAXEVENT) {
            for (i = IDX(pv->lidx + 1); i != idx; i = IDX(i + 1)) {
                pv->vals[i] = pv->lval;
                pv->haveval[i] = 1;
            }
        }
        // Save the latest event.
        pv->lval = v;
        pv->lidx = idx;
    } else {
        // Non-continuous PV

        // If all of the continuous PVs have values already, count this
        // as a non-continuous PV with a value for this event.
        if (cvals == ccnt) {
            elist[idx].vcnt++;
#ifdef DEBUG
            printf("elist[%d].vcnt = %d\n", idx, elist[idx].vcnt);
            fflush(stdout);
#endif
        }
    }
    if (lastev == MAXEVENT) {
        // If this is our first event, just save the timestamp and
        // initialize our fiducial counter.
        lastev = IDX(idx - OUTPUT_DELAY);
    } else if (epicsTimeGreaterThan(&now, &laststamp)) {
        // If time has advanced, check for completed events.
        while (lastev < IDX(idx - OUTPUT_DELAY)) {
            if (cvals == ccnt && elist[lastev].vcnt == nccnt)
                output_event(lastev);
            lastev = IDX(lastev + 1);
        }
    }
    laststamp = now;
}

void connection_handler(struct connection_handler_args args)
{
    chtype dbftype;
    int status;
    struct pv *pv = (struct pv *)ca_puser(args.chid);

    if (args.op == CA_OP_CONN_UP) {
        pv->nelem = ca_element_count(args.chid);
        dbftype = ca_field_type(args.chid);
        pv->dbrtype = dbf_type_to_DBR_TIME(dbftype);
        if (dbr_type_is_ENUM(pv->dbrtype))
            pv->dbrtype = DBR_TIME_INT;
        pv->size = dbr_size_n(pv->dbrtype, pv->nelem);
        conncnt++;
        if (pv->nelem != 1) {
            fprintf(stderr, "%s is not a scalar!\n", pv->name);
            exit(0);
        }
        if (pv->dbrtype != DBR_TIME_INT && 
            pv->dbrtype != DBR_TIME_LONG && 
            pv->dbrtype != DBR_TIME_DOUBLE) {
            fprintf(stderr, "%s is not a double or int (dbrtype=%d)!\n", pv->name,
                    pv->dbrtype);
            exit(0);
        }

        status = ca_create_subscription(pv->dbrtype, pv->nelem, args.chid, DBE_VALUE | DBE_ALARM,
                                        event_handler, pv, &pv->event);
        if (status != ECA_NORMAL) {
            fprintf(stderr, "Failed to create subscription for %s! error %d!\n", pv->name, status);
            exit(0);
        } else {
#ifdef DEBUG
            printf("connection_handler(%s)\n", pv->name);
            if (conncnt == pvcnt) {
                printf("All PVs are connected!\n");
                fflush(stdout);
            }
#endif
        }
    } else {
        conncnt--;
        if ((pv->flags & (FLAG_CONTINUOUS|FLAG_CVAL)) == (FLAG_CONTINUOUS|FLAG_CVAL)) {
            pv->flags &= ~FLAG_CVAL;
            cvals--;
        }
    }
    fflush(stdout);
}

void add_pv(char *name, char *flags)
{
    int result;
    char *s;

    int idx = pvcnt++;
    pvlist[idx].name = strdup(name);
    pvlist[idx].flags = 0;
    for (s = flags; *s; s++)
        switch (*s) {
        case 'T':
        case 't':
            pvlist[idx].flags |= FLAG_TRACE;
            break;
        case 'C':
        case 'c':
            pvlist[idx].flags |= FLAG_CONTINUOUS;
            break;
    }
    if (pvlist[idx].flags & FLAG_CONTINUOUS)
        ccnt++;
    else
        nccnt++;
    pvlist[idx].lidx = MAXEVENT;
    pvlist[idx].event = 0;

#ifdef DEBUG
    printf("Adding PV %s with flags \"%s\"\n", name, flags);
#endif
    result = ca_create_channel (name,
                                connection_handler,
                                &pvlist[idx],
                                50, /* priority?!? */
                                &pvlist[idx].chan);
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s while creating channel to %s!\n", ca_message(result), name);
        exit(0);
    }
    fprintf(outfp, "%s ", name);
#ifdef DEBUG
    printf("add_pv(%s)\n", name);
    fflush(stdout);
#endif
}

int main(int argc, char **argv)
{
    double runtime;
    FILE *fp;
    char buf[1024], *s;
    int i;
    int alrm = 0;

    bzero(&elist, sizeof(elist));

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: buildev PV_LIST_FILE OUTPUT_FILE [ TIMEOUT ]\n");
        exit(0);
    }
    if (argc == 4)
        alrm = atoi(argv[3]);

    if (!(outfp = fopen(argv[2], "w"))) {
        fprintf(stderr, "Cannot open %s for output!\n", argv[2]);
        exit(0);
    }

    if (!(fp = fopen(argv[1], "r"))) {
        fprintf(stderr, "Cannot open %s for input!\n", argv[1]);
        exit(0);
    }

    ca_context_create(ca_disable_preemptive_callback);

    fprintf(outfp, "#timestamp pulse ");
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (buf[0] == '#' || buf[0] == '\n')
            continue;
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';
        s = index(buf, ' ');
        if (s) {
            *s++ = '\0';
            add_pv(buf, s);
        } else
            add_pv(buf, "");
    }
    fclose(fp);
    fprintf(outfp, "\n");
    fflush(outfp);
#ifdef DEBUG
    printf("Found %d PVs in file: %d continuous, %d non-continuous.\n", pvcnt, ccnt, nccnt);
    fflush(stdout);
#endif

    signal(SIGINT, int_handler);
    signal(SIGALRM, int_handler);

    if (alrm)
        alarm(alrm);
    gettimeofday(&start, NULL);

    while (!haveint)
        ca_pend_event(2.0);

    for (i = 0; i < pvcnt; i++)
        if (pvlist[i].event)
            ca_clear_subscription(pvlist[i].event);
    ca_pend_io(0.0);

    gettimeofday(&stop, NULL);
    runtime = (1000000LL * stop.tv_sec + stop.tv_usec) - 
              ( 1000000LL * start.tv_sec + start.tv_usec);
    runtime /= 1000000.;
    printf("Runtime: %.4lf seconds\n", runtime);
    fclose(outfp);

    ca_context_destroy();
    return 0;
}
