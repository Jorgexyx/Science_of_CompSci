// LD_PRELOAD code courtesy of https://gist.github.com/apsun/1e144bf7639b22ff0097171fa0f8c6b1
// setitimer code courtesy of https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_71/apis/setitime.htm
// rdmsr and wrmsr code courtesty of https://01.org/msr-tools/downloads/msr-tools-source-code

#define _GNU_SOURCE		// Required for non-portable GNU dlsym extensions.
				// According to lore, this must be defined before any #includes.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE  	// Required for sigaction.
#endif 

#include <stdio.h>		// printf() and friends.
#include <sys/time.h>		// get/setitimer().
#include <errno.h>		// errno.
#include <stdlib.h>		// exit().
#include <signal.h>		// sigemptyset(), sigaction().
#include <string.h>		// strerror().
#include <dlfcn.h>		// dlsym().
#include <sys/types.h>		// .
#include <sys/stat.h>		//  > open() and friends
#include <fcntl.h>		// .
#include <unistd.h>		// pread/pwrite().
#include <stdint.h>		// uint64_t
#include <assert.h>		// assert macros.
#include <inttypes.h>		// PRId64 macro for printing uint64_t values

#define typeof __typeof__	// Not sure while this is needed on rhetoric but not feyerabend.

#define IA32_MPERF		((off_t)0x0E7)
#define IA32_APERF		((off_t)0x0E8)
#define IA32_PERFEVTSEL		((off_t)0x186)
#define IA32_PWRLIMIT		((off_t)0x610)
#define IA32_PCM0		((off_t)0xC1)

static struct my_msrs{
	uint64_t mperf;
	uint64_t aperf;
	uint64_t perfevtsel;
	uint64_t pcm0;
	uint64_t pwrLimit;
	
} my_msrs;

static int msr0_fd = -1;

static void read_msrs(){
	int rc;
	assert( msr0_fd > 0 );
	//rc = pread( msr0_fd, &my_msrs.mperf, sizeof(my_msrs.mperf), IA32_MPERF );	assert( 8 == rc );
	// read from the program counter
	rc = pread( msr0_fd, &my_msrs.pcm0, sizeof(my_msrs.pcm0), IA32_PCM0);	assert( 8 == rc );
}

static void write_msrs(const void *buf, int type){
	int rc;
	assert(msr0_fd > 0);
	if (type == 0) 		//Selected to write to perf evt sel
		rc = pwrite(msr0_fd, &buf, sizeof(my_msrs.perfevtsel), IA32_PERFEVTSEL);
	else if (type == 2)
		rc = pwrite(msr0_fd, &buf, sizeof(my_msrs.pwrLimit), IA32_PWRLIMIT);
	else			//selected to write to pmc0
		rc = pwrite(msr0_fd, &buf, sizeof(my_msrs.pcm0), IA32_PCM0);
	assert( 8 == rc);
}

static void dump_msrs(int power){
	static int initialized = 0;
	if( !initialized ){
		fprintf(stdout, "PCM0 POWER\n");
		initialized = 1;
	}
	fprintf(stdout, "%" PRId64 " % " PRId64"\n" , my_msrs.pcm0, power);
}

static void set_up_msr_file_descriptors(){
	// Getting virtual processors mapped to physical hyperthreads, cores and sockets
	// is a nightmare.  To do this correctly, see https://github.com/LLNL/libmsr/blob/master/src/msr_core.c	
	// For now, we're going to assume single-socket and only work off of one core.

	// Also note:  the stock msr kernel module tests for capability CAP_SYS_RAWIO 
	// if the program is not running as root.  There's a way of bestowing that 
	// capability onto a binary, but I can't be arsed.  If you'd rather not run as
	// root, use msr-safe (https://github.com/LLNL/msr-safe).  Setting file permissions
	// in /dev/cpu/x/msr is not sufficient for accessing the stock msr kernel module.

	// See https://lwn.net/Articles/542327/ for a bit of the politics.

	msr0_fd = open("/dev/cpu/0/msr_safe", O_RDWR);
	if(msr0_fd == -1){
		fprintf(stderr, "QQQ %s:%d ERROR open() returned (%d) (%s).\n", 
			__FILE__, __LINE__, msr0_fd, strerror(errno) );
		exit(-1);	// No need to continue.  Bail out now.
	}	
}

int incrPwr = 0;
static void mytimerhandler( int sig ) {
	read_msrs();
	dump_msrs(incrPwr);
	uint64_t pwrLimit = 0x4203e800148000 ;//init value
	uint64_t pmcVal = 0x0;		//set pmc to 0
	pwrLimit = ((pwrLimit >> 14) << 14) | incrPwr; //set first 0 - 14 bytes 0
	const void *pwrBuf  = (void *) pwrLimit;
	const void *pmcBuf  = (void *) pmcVal;
	incrPwr += 20;
	write_msrs(pwrBuf, 2);
	write_msrs(pmcBuf, 1);
}

static void set_up_itimer(){
	// Note that there are three timers and their associated signals.
	// 	ITIMER_REAL    -> SIGALRM 	wall-clock time
	//	ITIMER_VIRTUAL -> SIGVTALRM	process time
	//	ITIMER_PROF    -> SIGPROF	profiling time
	//					(includes syscalls made on behalf of the process)
	struct itimerval itv;
	struct sigaction sact;
	int rc;
	sigemptyset( &sact.sa_mask );		// macro to initialize sact.
	sact.sa_flags = 0;			// see man page for sigaction.
	sact.sa_handler = mytimerhandler;	// execute this function when the timer expires.
	sigaction( SIGPROF, &sact, NULL );	// set up our signal handler.

	itv.it_interval.tv_sec  =    10; 	// Timer interval in seconds        
	itv.it_interval.tv_usec =     0;  	//   and microseconds.
	itv.it_value.tv_sec     =    10;      	// Time to first signal,
	itv.it_value.tv_usec    =     0;     	//   same units.

	rc = setitimer( ITIMER_PROF, &itv, NULL );
	if(rc){
		fprintf(stderr, "QQQ %s:%d ERROR setitimer returned (%d) (%s).\n", 
			__FILE__, __LINE__, rc, strerror(errno) );
		exit(-1);	// No need to continue.  Bail out now.
	}
}

// Declare a pointer to hold the original value for main().
// Being static makes sure nothing outside this file can see it.
static int (*main_orig)(int, char **, char **);

int main_hook(int argc, char **argv, char **envp)
{
	// Dump out the command line arguments.
	fprintf(stdout, "QQQ %s:%d Printing out command line arguments.\n",
		__FILE__, __LINE__ );
	if(argc == 0){
		fprintf(stdout, "QQQ %s:%d argc = 0 (really?)\n",
			__FILE__, __LINE__);
	}else{
		for (int i = 0; i < argc; ++i) {
			fprintf(stdout, "QQQ %s:%d argv[%d] = %s\n", 
				__FILE__, __LINE__, i, argv[i]);
		}
	}
	
	// Open up the msr file(s)
	fprintf(stdout, "QQQ %s:%d Setting up the msr file descriptors.\n",
		__FILE__, __LINE__ );
	set_up_msr_file_descriptors();

	// Set up our itimer.
	fprintf(stdout, "QQQ %s:%d Setting up itimer.\n",
		__FILE__, __LINE__ );
	set_up_itimer();

	// Set up MSR for perfeventsel, program counter, and power limit
	uint64_t perfValue = 0x4300c0;
	uint64_t pmcVal = 0x0;		//set pmc to 0
	uint64_t pwrLimit = 0x4203e800148320 ;//init value

	pwrLimit = (pwrLimit >> 14) << 14; //set first 0 - 14 bytes 0
	
	const void *perfBuf = (void *) perfValue;
	const void *pmcBuf  = (void *) pmcVal;
	const void *pwrBuf  = (void *) pwrLimit;

	write_msrs(pmcBuf, 1);
	write_msrs(perfBuf, 0);
	write_msrs(pwrBuf, 2);

	// Pass control to the original program.
	fprintf(stdout, "QQQ %s:%d Calling the original main().\n",
		__FILE__, __LINE__ );
	int ret = main_orig(argc, argv, envp);

	// Dump out main's return value.
	fprintf(stdout, "QQQ %s:%d main() returned %d\n", 
		__FILE__, __LINE__, ret);

	return ret;
}

// We can't trap _start(), but we can trap __libc_start_main().
// See the definition at http://refspecs.linuxbase.org/LSB_3.1.0/LSB-generic/LSB-generic/baselib---libc-start-main-.html
int __libc_start_main(
    int (*main)(int, char **, char **),		// function pointer for main()
    int argc,					// number of arguments in argv
    char **argv,				// list of the arguments passed to the program
    int (*init)(int, char **, char **),		// function pointer for init()
    void (*fini)(void),				// function pointer for fini()
    void (*rtld_fini)(void),			// function pointer for rtld_fini
    void *stack_end)				// Haven't been able to track this down in the standards.
{
    main_orig = main;

    // Find the real __libc_start_main() and call it.
    // Note that typeof is used instead of typedef.
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");
    return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}
