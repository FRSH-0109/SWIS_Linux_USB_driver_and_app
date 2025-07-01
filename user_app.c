#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <sys/ioctl.h>

#define min(a,b) ((a) < (b) ? (a) : (b))

#define WR_PERIOD _IOW('c_usb', 1, uint32_t*)
#define RD_PERIOD _IOR('c_usb', 2, uint32_t*)

#define SHTC3_CMD_READ_DATA					"SHTC3 READ"
#define SHTC3_CMD_READ_STATE				"SHTC3 STATE"
#define SHTC3_CMD_SET_PERIOD				"SHTC3 PERIOD:"
#define SHTC3_CMD_GET_PERIOD				"SHTC3 PERIOD?"
#define SHTC3_CMD_SET_SINGLE				"SHTC3 SINGLE"
#define BME280_CMD_READ_DATA				"BME280 READ"
#define BME280_CMD_READ_STATE				"BME280 STATE"
#define BME280_CMD_SET_PERIOD				"BME280 PERIOD:"
#define BME280_CMD_GET_PERIOD				"BME280 PERIOD?"
#define BME280_CMD_SET_SINGLE				"BME280 SINGLE"

#define FLAG_SENSOR1_READ_STATE              "s1"
#define FLAG_SENSOR1_READ_DATA               "d1"
#define FLAG_SENSOR1_SET_SINGLE              "i1"
#define FLAG_SENSOR1_SET_PERIOD              "cw1"
#define FLAG_SENSOR1_GET_PERIOD              "cr1"
#define FLAG_SENSOR2_READ_STATE              "s2"
#define FLAG_SENSOR2_READ_DATA               "d2"
#define FLAG_SENSOR2_SET_SINGLE              "i2"
#define FLAG_SENSOR2_SET_PERIOD              "cw2"
#define FLAG_SENSOR2_GET_PERIOD              "cr2"

uint32_t period = 1000;
int global_fd = -1;

void handle_signal(int signum) {
    printf("\nCaught signal %d\n", signum);

    if (global_fd >= 0) {
        printf("Closing USB file...\n");
        close(global_fd);
    }

    exit(0);
}

void print_getopt_state(void) {
  printf("optind: %d\t" "opterr: %d\t" "optopt: %c (%d)\n" ,
    optind, opterr, optopt, optopt
  );
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);   // Ctrl+C
    signal(SIGTERM, handle_signal);  // kill
    signal(SIGQUIT, handle_signal);

    uint32_t value;
    const char *dev = "/dev/vendor0";
    int fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    global_fd = fd;

    printf("%d\n",argc);    // print number of arguments
    if (argc > 1)
    {
        char* param = argv[1];
        if (!strncmp(param, FLAG_SENSOR1_READ_STATE, min(strlen(param), strlen(FLAG_SENSOR1_READ_STATE))))
        {
            printf("Here place a device state getter!\n");
            if (write(fd, SHTC3_CMD_READ_STATE, strlen(SHTC3_CMD_READ_STATE)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR1_READ_DATA, min(strlen(param), strlen(FLAG_SENSOR1_READ_DATA))))
        {
            printf("Here place a data getter!\n");
            if (write(fd, SHTC3_CMD_READ_DATA, strlen(SHTC3_CMD_READ_DATA)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR1_SET_SINGLE, min(strlen(param), strlen(FLAG_SENSOR1_SET_SINGLE))))
        {
            printf("Here place a single measure request!\n");
            if (write(fd, SHTC3_CMD_SET_SINGLE, strlen(SHTC3_CMD_SET_SINGLE)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR1_SET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR1_SET_PERIOD))))
        {
            // char *endptr;
            // int cycle_time = strtol(optarg, &endptr, 10); // Base 10
            // printf("Przekazany okres pomiaru: %d\n", cycle_time);
            // printf("Cycle time passed: %s\n", optarg);
            if (argc < 3)
            {
                perror("No cycle period given!");
                close(global_fd);
                return 1;
            }
            else
            {
                char msg[64] = {0};
                snprintf(msg, sizeof(msg), "%s%s", SHTC3_CMD_SET_PERIOD, argv[2]);
                if (write(fd, msg, strlen(msg)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                printf("Parameter value: %s\n", argv[2]);
                printf("Writing new period value (%d) to driver!\n", period);
                ioctl(fd, WR_PERIOD, (uint32_t*) &period);
            }
        }
        else if (!strncmp(param, FLAG_SENSOR1_GET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR1_GET_PERIOD))))
        {
            if (write(fd, SHTC3_CMD_GET_PERIOD, strlen(SHTC3_CMD_GET_PERIOD)) < 0)
            {
                perror("write");close(fd);return 1;
            }
            printf("Current period is: %d\n", value);
        }
        else if (!strncmp(param, FLAG_SENSOR2_READ_STATE, min(strlen(param), strlen(FLAG_SENSOR2_READ_STATE))))
        {
            printf("Here place a device state getter!\n");
            if (write(fd, BME280_CMD_READ_STATE, strlen(BME280_CMD_READ_STATE)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR2_READ_DATA, min(strlen(param), strlen(FLAG_SENSOR2_READ_DATA))))
        {
            printf("Here place a data getter!\n");
            if (write(fd, BME280_CMD_READ_DATA, strlen(BME280_CMD_READ_DATA)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR2_SET_SINGLE, min(strlen(param), strlen(FLAG_SENSOR2_SET_SINGLE))))
        {
            printf("Here place a single measure request!\n");
            if (write(fd, BME280_CMD_SET_SINGLE, strlen(BME280_CMD_SET_SINGLE)) < 0)
            {
                perror("write");close(fd);return 1;
            }
        }
        else if (!strncmp(param, FLAG_SENSOR2_SET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR2_SET_PERIOD))))
        {
            // char *endptr;
            // int cycle_time = strtol(optarg, &endptr, 10); // Base 10
            // printf("Przekazany okres pomiaru: %d\n", cycle_time);
            // printf("Cycle time passed: %s\n", optarg);
            if (argc < 3)
            {
                perror("No cycle period given!");
                close(global_fd);
                return -1;
            }
            else
            {
                char msg[64] = {0};
                snprintf(msg, sizeof(msg), "%s%s", BME280_CMD_SET_PERIOD, argv[2]);
                if (write(fd, msg, strlen(msg)) < 0)
                {
                    perror("write");close(fd);return 1;
                }
                printf("Parameter value: %s\n", argv[2]);
                printf("Writing new period value (%d) to driver!\n", period);
                ioctl(fd, WR_PERIOD, (uint32_t*) &period);
            }
        }
        else if (!strncmp(param, FLAG_SENSOR2_GET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR2_GET_PERIOD))))
        {
            if (write(fd, BME280_CMD_GET_PERIOD, strlen(BME280_CMD_GET_PERIOD)) < 0)
            {
                perror("write");close(fd);return 1;
            }
            printf("Current period is: %d\n", value);
        }
        else
        {
            perror("Invalid flag");
            close(global_fd);
            return -2;
        }
    }
    else
    {
        perror("No flags given!");
        close(global_fd);
        return -3;
    }

    //     switch(param[0])
    //     {
    //         case 's':
    //             printf("Here place a device state getter!\n");
    //             if (write(fd, SHTC3_CMD_READ_STATE, strlen(SHTC3_CMD_READ_STATE)) < 0)
    //             {
    //                 perror("write");close(fd);return 1;
    //             }
    //             break;
    //         case 'd':
    //             printf("Here place a data getter!\n");
    //             if (write(fd, SHTC3_CMD_READ_DATA, strlen(SHTC3_CMD_READ_DATA)) < 0)
    //             {
    //                 perror("write");close(fd);return 1;
    //             }
    //             break;
    //         case 'i':
    //             printf("Here place a single measure request!\n");
    //             if (write(fd, SHTC3_CMD_SET_SINGLE, strlen(SHTC3_CMD_SET_SINGLE)) < 0)
    //             {
    //                 perror("write");close(fd);return 1;
    //             }
    //             break;
    //         case '': // Writing a new period od cyclic messeging
    //             // char *endptr;
    //             // int cycle_time = strtol(optarg, &endptr, 10); // Base 10
    //             // printf("Przekazany okres pomiaru: %d\n", cycle_time);
    //             // printf("Cycle time passed: %s\n", optarg);
    //             const char msg[64] = {0};
    //             snprintf(msg, sizeof(msg), "%s%d", SHTC3_CMD_SET_PERIOD, period);
    //             if (write(fd, msg, strlen(msg)) < 0)
    //             {
    //                 perror("write");close(fd);return 1;
    //             }
    //             printf("Writing new period value (%d) to driver!\n", period);
    //             ioctl(fd, WR_PERIOD, (uint32_t*) &period);

    //             break;
    //         case '': // Reading current period of cyclic messeging
    //             if (write(fd, SHTC3_CMD_GET_PERIOD, strlen(SHTC3_CMD_GET_PERIOD)) < 0)
    //             {
    //                 perror("write");close(fd);return 1;
    //             }
    //             printf("Current period is: %d\n", value);
    //             break;
    //         default:
    //             printf("Unknown option: %s\n", param);
    //         }

    //     argc--;
    // }

    while(1) { // Uniwersalne odbieranie danych
        /*
        if(read) {
            wyświetl dane z buforu // W tej wersji read sprawdza i wyciąga dane z kolejki fifo w sterowniku
        }
        */
        int n = 0;
        char buffer[64] = {0};

        n = read(fd, buffer, sizeof(buffer));
        // printf("N %d\n", n);
        if (n > 0) { // Read response from device
            printf("Read ASCI %d bytes: %s\n", n, buffer);
            float temp = 0.0;
            float hum = 0.0;
            memcpy(&temp, buffer, sizeof(float));
            memcpy(&hum, &buffer[4], sizeof(float));
            printf("Read as measurement: Temp: %.3f, Hum: %.3f\n", temp, hum);
        }
    }

    return 0;
}