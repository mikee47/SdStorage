// List of test modules to register

#ifdef ARCH_HOST
#define ARCH_TESTS(XX)
#else
#define ARCH_TESTS(XX) XX(command)
#endif

#define TEST_MAP(XX)                                                                                                   \
	XX(basic)                                                                                                          \
	ARCH_TESTS(XX)
