#include <gtest/gtest.h>
#include <sttserv/cmdline.hpp>


CommandLineArguments g_args;


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // fprintf(stderr, "--------------------------------\n");
    // for(i32 i = 0; i < argc; ++i) {
    //     fprintf(stdout, "argv[%d] -> %s\n", i, argv[i]);
    // }
    // fprintf(stderr, "--------------------------------\n");

    if(!parse_commandline_args(argc, argv, g_args)) {
        fprintf(stderr, "Incorrect Values Passed in command-line\n");
        return 1;
    }

    print_arguments(g_args);
    return RUN_ALL_TESTS();
}