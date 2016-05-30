#ifndef VERSION
#define VERSION

#define XBRIDGE_VERSION_MAJOR 0
#define XBRIDGE_VERSION_MINOR 44
#define XBRIDGE_VERSION_DESCR "w-tax-2-fixed-with-1/2-ether"

#define MAKE_VERSION(major,minor) (( major << 16 ) + minor )
#define XBRIDGE_VERSION MAKE_VERSION(XBRIDGE_VERSION_MAJOR, XBRIDGE_VERSION_MINOR)

#endif // VERSION

