#include <sttserv/cmdline.h>
#include <sttserv/model.h>
#include <cstdio>


int main(int argc, char* argv[])
{
    CommandLineArguments args;
    BackendContextHandle ctxtHandle;

    if(!parse_arguments(argc, argv, &args)) {
        fprintf(stderr, "Error Parsing Command Line Arguments\n");
        return -1;
    }

    if(!createBackend(&args, &ctxtHandle)) {
        fprintf(stderr, "Error Creating Model Backend\n");
        return -1;
    }
    



    destroyBackend(ctxtHandle);
    fprintf(stdout, "\nAAAAAAAAAAAAAAAAa");
    return 1;
}