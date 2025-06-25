#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void print_getopt_state(void) {
  printf("optind: %d\t" "opterr: %d\t" "optopt: %c (%d)\n" ,
    optind, opterr, optopt, optopt
  );
}

int main(int argc, char* argv[]) {
    const char *dev = "/dev/vendor0";
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int character;
    char *options = ":dt:";

    // character = getopt(argc, argv, options);

    // print_getopt_state();
    // printf("getopt returned: '%c' (%d)\n", character, character);
    // print_getopt_state();

    while ((character = getopt(argc, argv, options)) != -1) {
        switch(character)
        {
            case 'd':
                printf("Here place a measurment on demand!\n");
                break;
            case 't':
                // char *endptr;
                // int cycle_time = strtol(optarg, &endptr, 10); // Base 10
                // printf("Przekazany okres pomiaru: %d\n", cycle_time);
                printf("Cycle time passed: %s\n", optarg);
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
        }
    }

    // Write command to device
    // const char *msg = "HELLO_DEVICE";
    // if (write(fd, msg, strlen(msg)) < 0) {
    //     perror("write");
    //     close(fd);
    //     return 1;
    // }

    // Read response from device
    char buffer[64] = {0};
    int n = 0;
    for (int i = 0; i < 100; i++) {
        n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            perror("read");
            close(fd);
            return 1;
        }
        if(n > 0) {
            break; // Exit loop if we successfully read data
        }
        sleep(10);
    }

    printf("Device response %d: [%.*s]\n", n, n, buffer);
    close(fd);
    return 0;
}