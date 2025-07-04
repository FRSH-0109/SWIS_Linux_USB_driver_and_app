obj-m += usb_custom_driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -o user_app user_app.c

driver:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

app:
	gcc -o user_app user_app.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm user_app