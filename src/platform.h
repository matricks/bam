/* platforms */

/* windows family */
#if defined(WIN64) || defined(_WIN64)
	/* hmm, is this IA64 or x86-64? */
	#define BAM_FAMILY_WINDOWS
	#define BAM_FAMILY_STRING "windows"
	#define BAM_PLATFORM_WIN64
	#define BAM_PLATFORM_STRING "win64"
#elif defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
	#define BAM_FAMILY_WINDOWS
	#define BAM_FAMILY_STRING "windows"
	#define BAM_PLATFORM_WIN32
	#define BAM_PLATFORM_STRING "win32"
#elif defined(CYGWIN) || defined(__CYGWIN__) || defined(__CYGWIN32__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_CYGWIN
	#define BAM_PLATFORM_STRING "cygwin"
#endif

/* unix family */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_FREEBSD
	#define BAM_PLATFORM_STRING "freebsd"
#endif

#if defined(__OpenBSD__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_OPENBSD
	#define BAM_PLATFORM_STRING "openbsd"
#endif

#if defined(__LINUX__) || defined(__linux__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_LINUX
	#define BAM_PLATFORM_STRING "linux"
#endif

#if defined(__GNU__) || defined(__gnu__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_HURD
	#define BAM_PLATFORM_STRING "gnu"
#endif

#if defined(MACOSX) || defined(__APPLE__) || defined(__DARWIN__)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFORM_MACOSX
	#define BAM_PLATFORM_STRING "macosx"
#endif

#if defined(__sun)
	#define BAM_FAMILY_UNIX
	#define BAM_FAMILY_STRING "unix"
	#define BAM_PLATFROM_SOLARIS
	#define BAM_PLATFORM_STRING "solaris"
#endif

/* beos family */
#if defined(__BeOS) || defined(__BEOS__)
	#define BAM_FAMILY_BEOS
	#define BAM_FAMILY_STRING "beos"
	#define BAM_PLATFORM_BEOS
	#define BAM_PLATFORM_STRING "beos"
#endif

#if defined(__HAIKU) || defined(__HAIKU__)
	#define BAM_FAMILY_BEOS
	#define BAM_FAMILY_STRING "beos"
	#define BAM_PLATFORM_BEOS
	#define BAM_PLATFORM_STRING "haiku"
#endif


/* architectures */
#if defined(i386) || defined(__i386__) || defined(__x86__) || defined(i386_AT386) || defined(BAM_PLATFORM_WIN32)
	#define BAM_ARCH_IA32
	#define BAM_ARCH_STRING "ia32"
#endif

#if defined(__ia64__)
	#define BAM_ARCH_IA64
	#define BAM_ARCH_STRING "ia64"
#endif

#if defined(__amd64__) || defined(__x86_64__)
	#define BAM_ARCH_AMD64
	#define BAM_ARCH_STRING "amd64"
#endif

#if defined(__powerpc__) || defined(__ppc__)
	#define BAM_ARCH_PPC
	#define BAM_ARCH_STRING "ppc"
#endif

#if defined(__sparc__)
	#define BAM_ARCH_SPARC
	#define BAM_ARCH_STRING "sparc"
#endif


#ifndef BAM_FAMILY_STRING
#define BAM_FAMILY_STRING "unknown"
#endif

#ifndef BAM_PLATFORM_STRING
#define BAM_PLATFORM_STRING "unknown"
#endif

#ifndef BAM_ARCH_STRING
#define BAM_ARCH_STRING "unknown"
#endif
