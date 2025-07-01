#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <sys/ioctl.h>

#define WR_PERIOD _IOW('c_usb', 1, int*)
#define RD_PERIOD _IOR('c_usb', 2, int*)

#define SHTC3_CMD_READ_DATA					"SHTC3 READ"
#define SHTC3_CMD_READ_STATE				"SHTC3 STATE"
#define SHTC3_CMD_SET_PERIOD				"SHTC3 PERIOD:"
#define SHTC3_CMD_SET_SINGLE				"SHTC3 SINGLE"
uint32_t period = 1000;

void print_getopt_state(void) {
  printf("optind: %d\t" "opterr: %d\t" "optopt: %c (%d)\n" ,
    optind, opterr, optopt, optopt
  );
}

int main(int argc, char* argv[]) {
    uint32_t value;
    const char *dev = "/dev/vendor0";
    int fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("%d\n",argc);    // print number of arguments
    argc--; //reduce argc to match array index
    while (argc >= 1)
    {
        char* param = argv[argc];
        switch(param[0])
        {
            case 's':
                printf("Here place a device state getter!\n");
                if (write(fd, SHTC3_CMD_READ_STATE, strlen(SHTC3_CMD_READ_STATE)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                break;
            case 'd':
                printf("Here place a data getter!\n");
                if (write(fd, SHTC3_CMD_READ_DATA, strlen(SHTC3_CMD_READ_DATA)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                break;
            case 'i':
                printf("Here place a single measure request!\n");
                if (write(fd, SHTC3_CMD_SET_SINGLE, strlen(SHTC3_CMD_SET_SINGLE)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                break;
            case 'c': // Writing a new period od cyclic messeging
                // char *endptr;
                // int cycle_time = strtol(optarg, &endptr, 10); // Base 10
                // printf("Przekazany okres pomiaru: %d\n", cycle_time);
                // printf("Cycle time passed: %s\n", optarg);
                const char msg[64] = {0};
                snprintf(msg, sizeof(msg), "%s%d", SHTC3_CMD_SET_PERIOD, period);
                if (write(fd, msg, strlen(msg)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                printf("Writing new period value (%d) to driver!\n", period);
                ioctl(fd, WR_PERIOD, (uint32_t*) &period);

                break;
            case 'v': // Reading current period of cyclic messeging
                ioctl(fd, RD_PERIOD, (uint32_t*) &value);
                printf("Current period is: %d\n", value);
                break;
            default:
                printf("Unknown option: %s\n", param);
            }

        argc--;
    }

    // Read response from device
    char buffer[64] = {0};
    int n = 0;
    for (int i = 0; i < 1000; i++) {
        n = read(fd, buffer, sizeof(buffer));
        // printf("N %d\n", n);
        if (n < 0) {
            perror("read");
            close(fd);
            return 1;
        }
        if(n > 0) {
            break; // Exit loop if we successfully read data
        }
        sleep(0.1);
    }
    printf("Read ASCI %d bytes: %s\n", n, buffer);
    float temp = 0.0;
    float hum = 0.0;
    memcpy(&temp, buffer, sizeof(float));
    memcpy(&hum, &buffer[4], sizeof(float));
    printf("Read as measurement: Temp: %.3f, Hum: %.3f\n", temp, hum);


    close(fd);
    return 0;
}