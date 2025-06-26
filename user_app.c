#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    const char *dev = "/dev/vendor0";
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
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
        printf("N %d\n", n);
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
    
    float temp = 0.0;
    float hum = 0.0;
    memcpy(&temp, buffer, sizeof(float));
    memcpy(&hum, &buffer[4], sizeof(float));
    printf("Device response: Temp: %.3f, Hum: %.3f\n", temp, hum);
    close(fd);
    return 0;
}