
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "sim.hpp"
#include "settings.hpp"

Sim_settings settings;

Simulator *Sim;

extern char *optarg;
extern int optind, optopt;

// When running under address sanitizer, do not tell students about the copius
// leaks plaguing this ancient simulator
extern "C" {
const char *__asan_default_options() { return "detect_leaks=0"; }
}

void usage (void)
{
    fprintf (stderr, "Usage:\n");
    fprintf (stderr, "\t-p <protocol> (choices MI, MSI, MESI, MOSIF)\n");
    fprintf (stderr, "\t-t <trace directory>\n");
    fprintf (stderr, "\t-h prints this useful message>\n");
}

int main (int argc, char *argv[])
{
    int num_nodes = 0;
    char *trace_dir = NULL;
    char *protocol = NULL;
    FILE *config_file = NULL;
    char config_path[1000];

    /** Parse command line arguments.  */
    int c;

    while ((c = getopt(argc, argv, "HhP:p:T:t:")) != -1)
    {
        switch(c)
        {
        case 'h':
        case 'H':
            usage ();
            exit (0);
            break;

        case 'p':
        case 'P':
            protocol = strdup (optarg);
            break;

        case 't':
        case 'T':
            trace_dir = strdup (optarg);
            break;

        default:
            fprintf (stderr, "Invalid command line arguments - %c", c);
            usage();
            return 1;
        }
    }

    if (protocol == NULL) {
        fprintf(stderr, "Error: invalid protocol specified.\n");
        if (trace_dir) free(trace_dir);
        usage();
        return 1;
    }

    if (trace_dir == NULL) {
        fprintf(stderr, "Error: trace file directory not defined!\n");
        if (protocol) free(protocol);
        usage();
        return 1;
    }

    sprintf(config_path,"%s/config",trace_dir);
    config_file = fopen (config_path,"r");
    if (fscanf(config_file,"%d\n",&num_nodes) != 1)
    {
    	fatal_error("Config File should contain number of traces\n");
    }

    if (num_nodes == 0)
        fatal_error ("Error: number of processors is zero.\n");


    /** Init settings.  */
    settings.set_defaults ();
    settings.num_nodes = num_nodes;
    settings.trace_dir = trace_dir;

    if (!strcmp(protocol,"MI"))
    {
    	settings.protocol = MI_PRO;
    }
    else if (!strcmp(protocol,"MSI"))
    {
    	settings.protocol = MSI_PRO;
    }
    else if (!strcmp(protocol,"MESI"))
    {
    	settings.protocol = MESI_PRO;
    }
    else if (!strcmp(protocol,"MOSI"))
    {
    	settings.protocol = MOSI_PRO;
    }
    else if (!strcmp(protocol,"MOESI"))
    {
    	settings.protocol = MOESI_PRO;
    }
    else if (!strcmp(protocol,"MOSIF"))
    {
    	settings.protocol = MOSIF_PRO;
    }
    else
    {
    	fatal_error ("Error: invalid protocol specified.\n");
    }


    /** Build simulator.  */
    Sim = new Simulator ();
    Sim->run ();
    delete Sim;
    free(trace_dir);
    free(protocol);
}
