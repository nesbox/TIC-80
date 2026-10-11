/* A virtual gamepad for the Linux backend's smoke test, built on /dev/uinput:
 * the layout the kernel's own drivers use, so the header's claim test accepts
 * it — and with --vendor one it must refuse. It presses the pad's buttons in
 * turn and holds the device open long enough to watch the app.
 *
 * Not built by cmake: a bench tool, run by hand, on the machine with the app.
 *
 *     cc -o /tmp/tic80-pad tests/linux-uinput-pad.c
 *     sudo /tmp/tic80-pad            # a pad the layer must read
 *     sudo /tmp/tic80-pad --vendor   # a device it must leave alone
 */
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

// The positional names the kernel uses since 4.16; older headers spell the
// same codes BTN_A and friends.
#ifndef BTN_SOUTH
#define BTN_SOUTH BTN_A
#endif
#ifndef BTN_EAST
#define BTN_EAST BTN_B
#endif
#ifndef BTN_NORTH
#define BTN_NORTH BTN_X
#endif
#ifndef BTN_WEST
#define BTN_WEST BTN_Y
#endif

// The buttons a canonical pad carries, and the four a vendor-layout device
// does not: the claim test is exactly this difference.
static const int Canonical[] = { BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_SELECT, BTN_START };
static const int Vendor[] = { BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_BASE };

static void emit(int fd, int type, int code, int value)
{
    struct input_event event = { 0 };

    event.type = type;
    event.code = code;
    event.value = value;

    if (write(fd, &event, sizeof event) != sizeof event)
        perror("write");
}

static void report(int fd)
{
    emit(fd, EV_SYN, SYN_REPORT, 0);
}

static void press(int fd, int code, int value)
{
    emit(fd, EV_KEY, code, value);
    report(fd);
}

static void stick(int fd, int code, int value)
{
    emit(fd, EV_ABS, code, value);
    report(fd);
}

int main(int argc, char** argv)
{
    const int vendor = argc > 1 && strcmp(argv[1], "--vendor") == 0;
    const int* buttons = vendor ? Vendor : Canonical;
    const int count = vendor ? (int)(sizeof Vendor / sizeof Vendor[0])
                             : (int)(sizeof Canonical / sizeof Canonical[0]);

    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0)
    {
        perror("open /dev/uinput");
        return 1;
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    for (int i = 0; i < count; i++)
        ioctl(fd, UI_SET_KEYBIT, buttons[i]);

    // A canonical pad wears the four face buttons by position and a hat for
    // the direction pad; the vendor one has neither, which is the claim test.
    int axes[2];
    int axisCount = 0;

    if (!vendor)
    {
        const int Wanted[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y };

        for (int i = 0; i < (int)(sizeof Wanted / sizeof Wanted[0]); i++)
        {
            struct uinput_abs_setup setup = { 0 };

            setup.code = Wanted[i];
            setup.absinfo.minimum = -32768;
            setup.absinfo.maximum = 32767;

            if (ioctl(fd, UI_SET_ABSBIT, Wanted[i]) < 0
                || ioctl(fd, UI_ABS_SETUP, &setup) < 0)
                perror("abs setup");
        }

        axes[axisCount++] = ABS_HAT0Y;
        axes[axisCount++] = ABS_RX;
    }

    struct uinput_setup device = { 0 };

    device.id.bustype = BUS_USB;
    device.id.vendor = vendor ? 0xdead : 0x045e;
    device.id.product = 0xbeef;

    snprintf(device.name, sizeof device.name, "TIC-80 test pad%s", vendor ? " (vendor layout)" : "");

    if (ioctl(fd, UI_DEV_SETUP, &device) < 0 || ioctl(fd, UI_DEV_CREATE) < 0)
    {
        perror("uinput device");
        return 1;
    }

    if (vendor)
        puts("a device with no face buttons and no hat: the layer must not claim it");
    else
        puts("a canonical pad: the layer must claim it, and follow what follows");

    puts("connect it, wait for the app to take it, then watch the presses");

    sleep(5);

    for (int i = 0; i < count; i++)
    {
        printf("pressing %s\n", vendor ? "a vendor button" : (const char*[]){ "A", "B", "X", "Y", "back", "start" }[i]);
        fflush(stdout);

        press(fd, buttons[i], 1);
        sleep(1);
        press(fd, buttons[i], 0);
        sleep(1);
    }

    if (axisCount > 0)
    {
        puts("direction pad down, then up");
        fflush(stdout);

        stick(fd, ABS_HAT0Y, 1);
        sleep(1);
        stick(fd, ABS_HAT0Y, -1);
        sleep(1);
        stick(fd, ABS_HAT0Y, 0);

        puts("right stick pushed right");
        fflush(stdout);

        stick(fd, ABS_RX, 32767);
        sleep(2);
        stick(fd, ABS_RX, 0);
    }

    puts("done: the device goes away now");

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);

    return 0;
}
