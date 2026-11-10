#include "kernel.h"
#include <circle/startup.h>
int main(){CRecoveryKernel kernel;if(!kernel.Initialize())halt();kernel.Run();halt();return EXIT_HALT;}
