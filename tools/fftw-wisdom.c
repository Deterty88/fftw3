/* Re-use libbench2 and the test program, but override bench_main so that
   we can have different command-line syntax. */
#include "getopt.h"
#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fftw3.h>
#include <string.h>

#define CONCAT(prefix, name) prefix ## name
#if defined(BENCHFFT_SINGLE)
#define FFTW(x) CONCAT(fftwf_, x)
#elif defined(BENCHFFT_LDOUBLE)
#define FFTW(x) CONCAT(fftwl_, x)
#else
#define FFTW(x) CONCAT(fftw_, x)
#endif

/* from bench.c: */
extern unsigned the_flags;
extern int usewisdom;
extern int nthreads;

/* dummy routines to replace those in hook.c */
void install_hook(void) {}
void uninstall_hook(void) {}

int verbose;

static void do_problem(const char *param)
{
     bench_problem *p;
     if (verbose > 0)
	  printf("PLANNING PROBLEM: %s\n", param);
     p = problem_parse(param);
     BENCH_ASSERT(can_do(p));
     problem_alloc(p);
     setup(p);
     done(p);
     problem_destroy(p);
}

static struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"verbose", optional_argument, 0, 'v'},

  {"canonical", no_argument, 0, 'c'},

  {"output-file", required_argument, 0, 'o'},

  {"impatient", no_argument, 0, 'i'},
  {"estimate", no_argument, 0, 'e'},
  {"exhaustive", no_argument, 0, 'x'},

  {"no-system-wisdom", no_argument, 0, 'n'},
  {"wisdom-file", required_argument, 0, 'w'},

  {"threads", required_argument, 0, 't'},

  /* options to restrict configuration to rdft-only, etcetera? */
  
  {0, no_argument, 0, 0}
};

/* powers of two and ten up to 2^20, for now */
static char canonical_sizes[][32] = {
     "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024",
     "2048", "4096", "8192", "16384", "32768", "65536", "131072",
     "262144", "524288", "1048576",

     "10", "100", "1000", "10000", "100000", "1000000",

     "2x2", "4x4", "8x8", "10x10", "16x16", "32x32", "64x64", "100x100",
     "128x128", "512x512", "1000x1000", "1024x1024",

     "2x2x2", "4x4x4", "8x8x8", "10x10x10", "16x16x16", "32x32x32",
     "64x64x64", "100x100x100"
};

#define NELEM(array)(sizeof(array) / sizeof((array)[0]))

static int bench_main1(int argc, char *argv[])
{
     int c;
     int ind;
     unsigned i;
     char *short_options = make_short_options(long_options);
     int impatient = 0;
     int system_wisdom = 1;
     int canonical = 0;
     FILE *output_file;
     char *output_fname = 0;

     verbose = 0;
     usewisdom = 0;

     bench_srand(1);
#ifdef HAVE_THREADS
     BENCH_ASSERT(FFTW(init_threads)());
#endif

     while ((c = getopt_long (argc, argv, short_options,
			      long_options, &ind)) != -1) {
	  switch (c) {
	      case 'h':
		   usage(argv[0], long_options); /* FIXME: better help? */
		   break;

	      case 'v':
		   if (optarg)
			verbose = atoi(optarg);
		   else
			++verbose;
		   break;
		   
	      case 'c':
		   canonical = 1;
		   break;

	      case 'o':
		   if (output_fname)
			bench_free(output_fname);
		   
		   if (!strcmp(optarg, "-"))
			output_fname = 0;
		   else {
			output_fname = bench_malloc(sizeof(char) *
						    (strlen(optarg) + 1));
			strcpy(output_fname, optarg);
		   }
		   break;

	      case 'i':
		   impatient = 1;
		   break;

	      case 'e':
		   the_flags |= FFTW_ESTIMATE;
		   break;

	      case 'x':
		   the_flags |= FFTW_EXHAUSTIVE;
		   break;

	      case 'n':
		   system_wisdom = 0;
		   break;

	      case 'w': {
		   FILE *w = stdin;
		   if (strcmp(optarg, "-") && !(w = fopen(optarg, "r"))) {
			fprintf(stderr,
				"fftw-wisdom: error opening \"%s\"", optarg);
			perror("");
			exit(EXIT_FAILURE);
		   }
		   if (!FFTW(import_wisdom_from_file)(w)) {
			fprintf(stderr, "fftw_wisdom: error reading wisdom"
				"from \"%s\"\n", optarg);
			exit(EXIT_FAILURE);
		   }
		   if (w != stdin)
			fclose(w);
		   break;
	      }

	      case 't':
		   nthreads = atoi(optarg);
#ifndef HAVE_THREADS
		   if (nthreads > 1) {
			fprintf(stderr, "fftw-wisdom: "
				"not compiled with thread support\n");
			exit(EXIT_FAILURE);
		   }
#endif
		   break;

	      case '?':
		   /* `getopt_long' already printed an error message. */
		   break;

	      default:
		   abort ();
	  }
     }

     if (!impatient)
	  the_flags |= FFTW_PATIENT;

     if (system_wisdom)
	  if (!FFTW(import_system_wisdom)() && verbose)
	       fprintf(stderr, "fftw-wisdom: system-wisdom import failed\n");

     if (canonical) {
	  for (i = 0; i < NELEM(canonical_sizes); ++i) {
	       unsigned j;
	       char types[][8] = { 
		    "cof", "cob", "cif", "cib", "rof", "rob", "rif", "rib"
	       };
	       
	       for (j = 0; j < NELEM(types); ++j) {
		    char ps[64];
		    if (!strchr(canonical_sizes[i],'x')
			|| !strchr(types[j],'o')) {
			 sprintf(ps, "%s%s", types[j], canonical_sizes[i]);
			 do_problem(ps);
		    }
	       }
	  }
     }

     while (optind < argc)
	  do_problem(argv[optind++]);
     
     if (!output_fname)
	  output_file = stdout;
     else
	  if (!(output_file = fopen(output_fname, "w"))) {
	       fprintf(stderr,
		       "fftw-wisdom: error creating \"%s\"", output_fname);
	       perror("");
	       exit(EXIT_FAILURE);
	  }

     FFTW(export_wisdom_to_file)(output_file);
     if (output_file != stdout)
	  fclose(output_file);
     if (output_fname)
	  bench_free(output_fname);

     cleanup();
     bench_free(short_options);

     return EXIT_SUCCESS;
}

int bench_main(int argc, char *argv[])
{
#if defined(__GNUC__) && defined(__i386__)
     /*
      * horrible hack to align the stack so that double precision
      * numbers are 8-bytes aligned on x86 processors.  If not,
      * the benchmark is totally useless.
      *
      * We assume a gcc version >= 2.95 so that
      * -mpreferred-stack-boundary works.  Otherwise, all bets are
      * off.  However, -mpreferred-stack-boundary does not create a
      * stack alignment, but it only preserves it.  Unfortunately,
      * many versions of libc on linux call main() with the wrong
      * initial stack alignment, with the result that the code is now
      * pessimally aligned instead of having a 50% chance of being
      * correct.
      *
      * Here, we check the alignment of a double variable and restore
      * the proper alignment if it is wrong.
      */
     {
	  double x;
	  if (((long)&x) & 0x7) {
	       /* wrong alignment. */

	       /* 
		* You would imagine that __builtin_alloca(4) would
		* solve the problem.  However, the overzealous gcc
		* aligns it to __builtin_alloca(8) so that we
		* accomplish nothing.  So here is what we do:
		*/
               /*
		* Use alloca to allocate some memory on the stack.
		* This alerts gcc that something funny is going
		* on, so that it does not omit the frame pointer
		* etc.
		*/
	       (void)__builtin_alloca(16); 

	       /*
		* Now allocate 4 stack bytes using inline asm.
		*/
	       __asm__ __volatile__ ("addl $-4, %esp");
	  }
     }
#endif

#ifdef __ICC /* Intel's compiler for ia32 */
     {
	  /*
	   * Simply calling alloca seems to do the right thing. 
	   * The size of the allocated block seems to be irrelevant.
	   */
	  _alloca(8);
     }
#endif

     return bench_main1(argc, argv);
}
