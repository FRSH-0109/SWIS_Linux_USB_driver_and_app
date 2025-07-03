#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define min(a,b) ((a) < (b) ? (a) : (b))  // Declaring min function

// Command list for the USB device
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

// List of accepted command line flags
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

uint32_t period; // Variable to pass measurment cycle period [ms]
int global_fd = -1; // Global variable to handle closing signals

// Protection from data race between handle_signal and while loop in main
volatile sig_atomic_t stop = 0;

void handle_signal(int signum) {
    /*
    Function used to handle cases of forced app closing
    For example closing app in the cyclic measurment mode.
    */
    printf("\nCaught signal %d\n", signum);
    stop = 1;

    if (global_fd >= 0) {
        char msg[64] = {0};
        // Building a command to stop to the device
        snprintf(msg, sizeof(msg), "%s%s", SHTC3_CMD_SET_PERIOD, '0');
        // Sending a command to the driver
        write(global_fd, msg, strlen(msg));

        // Building a command to stop to the device
        snprintf(msg, sizeof(msg), "%s%s", BME280_CMD_SET_PERIOD, '0');
        // Sending a command to the driver
        write(global_fd, msg, strlen(msg));

        printf("Closing USB file...\n");
        close(global_fd);
    }

    exit(0);
}

int main(int argc, char* argv[]) {
    // Signals recogniesed by handle_signal()
    signal(SIGINT, handle_signal);   // Ctrl+C
    signal(SIGTERM, handle_signal);  // kill
    signal(SIGQUIT, handle_signal);

    uint32_t value;
    int single_mes = 0; // Signal to kill the measurment loop after first measurment acquired
    // Opening device
    const char *dev = "/dev/vendor0"; // Name of char device representing our USB device
    int fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    global_fd = fd;

    if (argc > 1) // If there are possible flags in the program calling
    {
        char* param = argv[1];
        if (!strncmp(param, FLAG_SENSOR1_READ_STATE, min(strlen(param), strlen(FLAG_SENSOR1_READ_STATE))))
        {
            // Sending a command to the driver
            if (write(fd, SHTC3_CMD_READ_STATE, strlen(SHTC3_CMD_READ_STATE)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR1_READ_DATA, min(strlen(param), strlen(FLAG_SENSOR1_READ_DATA))))
        {
            // Sending a command to the driver
            if (write(fd, SHTC3_CMD_READ_DATA, strlen(SHTC3_CMD_READ_DATA)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR1_SET_SINGLE, min(strlen(param), strlen(FLAG_SENSOR1_SET_SINGLE))))
        {
            // Sending a command to the driver
            if (write(fd, SHTC3_CMD_SET_SINGLE, strlen(SHTC3_CMD_SET_SINGLE)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR1_SET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR1_SET_PERIOD))))
        {
            if (argc < 3) // Checikng if the cycle period were given
            {
                perror("No cycle period given!");
                close(global_fd);
                return -2;
            }
            else
            {
                char msg[64] = {0};
                // Building a command to send to the device
                snprintf(msg, sizeof(msg), "%s%s", SHTC3_CMD_SET_PERIOD, argv[2]);
                // Sending a command to the driver
                if (write(fd, msg, strlen(msg)) < 0)
                {
                    perror("write");close(fd);return -1;
                }
                printf("Parameter value: %s\n", argv[2]);
                printf("Writing new period value (%s) to driver!\n", argv[2]);
                // Writing a new period value to the usb_vendor struct
                ioctl(fd, WR_PERIOD, (uint32_t*) &period);
                if (*argv[2] == '0')
                {
                    close(global_fd);
                    return 0;
                }
            }
        }
        else if (!strncmp(param, FLAG_SENSOR1_GET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR1_GET_PERIOD))))
        {
            // Sending a command to the driver
            if (write(fd, SHTC3_CMD_GET_PERIOD, strlen(SHTC3_CMD_GET_PERIOD)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR2_READ_STATE, min(strlen(param), strlen(FLAG_SENSOR2_READ_STATE))))
        {
            // Sending a command to the driver
            if (write(fd, BME280_CMD_READ_STATE, strlen(BME280_CMD_READ_STATE)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR2_READ_DATA, min(strlen(param), strlen(FLAG_SENSOR2_READ_DATA))))
        {
            // Sending a command to the driver
            if (write(fd, BME280_CMD_READ_DATA, strlen(BME280_CMD_READ_DATA)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR2_SET_SINGLE, min(strlen(param), strlen(FLAG_SENSOR2_SET_SINGLE))))
        {
            // Sending a command to the driver
            if (write(fd, BME280_CMD_SET_SINGLE, strlen(BME280_CMD_SET_SINGLE)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else if (!strncmp(param, FLAG_SENSOR2_SET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR2_SET_PERIOD))))
        {
            if (argc < 3)
            {
                perror("No cycle period given!");
                close(global_fd);
                return -2;
            }
            else
            {
                char msg[64] = {0};
                // Building a command to send to the device
                snprintf(msg, sizeof(msg), "%s%s", BME280_CMD_SET_PERIOD, argv[2]);
                // Sending a command to the driver
                if (write(fd, msg, strlen(msg)) < 0)
                {
                    perror("write");close(fd);return -1;
                }
                printf("Parameter value: %s\n", argv[2]);
                printf("Writing new period value (%s) to driver!\n", argv[2]);
                // Writing a new period value to the usb_vendor struct
                ioctl(fd, WR_PERIOD, (uint32_t*) &period);
                if (*argv[2] == '0')
                {
                    close(global_fd);
                    return 0;
                }
            }
        }
        else if (!strncmp(param, FLAG_SENSOR2_GET_PERIOD, min(strlen(param), strlen(FLAG_SENSOR2_GET_PERIOD))))
        {
            // Sending a command to the driver
            if (write(fd, BME280_CMD_GET_PERIOD, strlen(BME280_CMD_GET_PERIOD)) < 0)
            {
                perror("write");close(fd);return -1;
            }
            single_mes = 1;
        }
        else // If an unrecogniesed flags were given
        {
            perror("Invalid flag");
            close(global_fd);
            return -4;
        }
    }
    else
    {
        perror("No flags given!");
        close(global_fd);
        return -4;
    }

    while(!stop) { // Universal data receiving

        int n = 0;
        char buffer[64] = {0};

        // Read data from a device and write it to the buffer
        n = read(fd, buffer, sizeof(buffer));
        // printf("N %d\n", n);
        if (n > 0) { // Print response from device
            printf("%s\n\r", buffer);
            if (single_mes) {
                break;
            }
        }
    }

    // Closing app
    close(global_fd);
    return 0;
}