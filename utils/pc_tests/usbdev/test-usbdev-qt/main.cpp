//#include <QCoreApplication>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>
#include <time.h>

// Device USB vendor ID
#define VENDOR_ID        0x0111

// Device USB product ID
#define PRODUCT_ID       0x0001

//
// Debug modes (higher the level, more messages).
//
typedef enum {
    LUSB_DEBUG_SILENT,        // no debug messages
    LUSB_DEBUG_ERROR,         // only error messages
    LUSB_DEBUG_WARNING,       // error and warning
    LUSB_DEBUG_INFO,          // error, warning and info
    LUSB_DEBUG_MORE_INFO,     // all of above with more info
    LUSB_DEBUG_LIBUSB         // all of above plus turn on libusb debug messages
} lusb_debug_mode_t;

enum {
    PROBE,
    RX,
    TX,
    VENDOR_REQ
};

int device_initialized = 0;
libusb_device_handle *handle;
lusb_debug_mode_t debug_mode = LUSB_DEBUG_INFO;
uint8_t databuf[2][32 * 1024];

unsigned bulk_in_ep = 0;
unsigned bulk_in_size;
unsigned bulk_out_ep = 0;
unsigned int_in_ep = 0;
unsigned int_out_ep = 0;
unsigned iso_in_ep = 0;
unsigned iso_out_ep = 0;

void device_deinit();

void finish (int res, const char *msg)
{
    if (msg)
        fprintf (stderr, "%s\n", msg);
    if (device_initialized)
        device_deinit();
    exit (res);
}


void device_init()
{
    libusb_device **devs;
    libusb_device *dev;
    struct libusb_device_descriptor dev_desc;
    int r;

    if (debug_mode >= LUSB_DEBUG_MORE_INFO)
        fprintf(stderr, "Initialization\n");

    r = libusb_init(NULL);
    if (r < 0)
        finish(-2, "Failed to initialize libusb!");

    if (debug_mode >= LUSB_DEBUG_LIBUSB)
        libusb_set_debug(NULL, 3);

    r = libusb_get_device_list(NULL, &devs);
    if (r < 0)
        finish(-2, "Failed to list USB devices!");

    int i = 0;
    while ((dev = devs[i++]) != NULL) {
        int r = libusb_get_device_descriptor(dev, &dev_desc);
        if (r < 0) {
            if (debug_mode >= LUSB_DEBUG_WARNING)
                fprintf(stderr, "Failed to get USB device descriptor, libusb error code: %d!\n", r);
            continue;
        }

        if (debug_mode >= LUSB_DEBUG_MORE_INFO)
            fprintf(stderr, "Vendor: 0x%04X, product 0x%04X\n", dev_desc.idVendor, dev_desc.idProduct);

        if (dev_desc.idVendor == VENDOR_ID &&
                dev_desc.idProduct == PRODUCT_ID) {
            struct libusb_config_descriptor *pconf_desc;
            r = libusb_get_active_config_descriptor(dev, &pconf_desc);
            if (r < 0) {
                if (debug_mode >= LUSB_DEBUG_WARNING)
                    fprintf(stderr, "Failed to read configuration descriptor, libusb error code: %d!\n", r);
                continue;
            }
            const struct libusb_interface_descriptor *piface_desc;
            piface_desc = &pconf_desc->interface->altsetting[0];
            int i;
            for (i = 0; i < piface_desc->bNumEndpoints; ++i) {
                const struct libusb_endpoint_descriptor *pep_desc = &piface_desc->endpoint[i];
                if ((pep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                    if (pep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                        bulk_in_ep = pep_desc->bEndpointAddress;
                        bulk_in_size = pep_desc->wMaxPacketSize;
                    } else {
                        bulk_out_ep = pep_desc->bEndpointAddress;
                    }
                } else if ((pep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                    if (pep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                        int_in_ep = pep_desc->bEndpointAddress;
                    } else {
                        int_out_ep = pep_desc->bEndpointAddress;
                    }
                } else if ((pep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
                    if (pep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                        iso_in_ep = pep_desc->bEndpointAddress;
                    } else {
                        iso_out_ep = pep_desc->bEndpointAddress;
                    }
                }
            }
            if (debug_mode >= LUSB_DEBUG_INFO) {
                fprintf(stderr, "BULK IN endpoint: ");
                if (bulk_in_ep)
                    fprintf(stderr, "%d\n", bulk_in_ep & 0xF);
                else fprintf(stderr, "not found!\n");
                fprintf(stderr, "BULK OUT endpoint: ");
                if (bulk_out_ep)
                    fprintf(stderr, "%d\n", bulk_out_ep & 0xF);
                else fprintf(stderr, "not found!\n");
                fprintf(stderr, "INTERRUPT IN endpoint: ");
                if (int_in_ep)
                    fprintf(stderr, "%d\n", int_in_ep & 0xF);
                else fprintf(stderr, "not found!\n");
                fprintf(stderr, "INTERRUPT OUT endpoint: ");
                if (int_out_ep)
                    fprintf(stderr, "%d\n", int_out_ep & 0xF);
                else fprintf(stderr, "not found!\n");
                fprintf(stderr, "ISOCHRONUOS IN endpoint: ");
                if (iso_in_ep)
                    fprintf(stderr, "%d\n", iso_in_ep & 0xF);
                else fprintf(stderr, "not found!\n");
                fprintf(stderr, "ISOCHRONUOS OUT endpoint: ");
                if (iso_out_ep)
                    fprintf(stderr, "%d\n", iso_out_ep & 0xF);
                else fprintf(stderr, "not found!\n");
            }
            r = libusb_open(dev, &handle);
            if (r < 0) {
                if (debug_mode >= LUSB_DEBUG_WARNING)
                    fprintf(stderr, "Failed to open USB device, libusb error code: %d!\n", r);
                continue;
            }
#ifndef WIN32
            r = libusb_detach_kernel_driver(handle, 0);
            if (r < 0 && r != LIBUSB_ERROR_NOT_FOUND)
                continue;
#endif
            r = libusb_claim_interface(handle, 0);
            if (r < 0) {
                if (debug_mode >= LUSB_DEBUG_ERROR)
                    fprintf(stderr, "Failed to claim interface, libusb error code: %d!\n", r);
                continue;
            }
        }
    }

    if (handle == 0)
        finish(-3, "Device for test not found!");

    libusb_free_device_list(devs, 1);

    device_initialized = 1;
}

void device_deinit()
{
    if (debug_mode >= LUSB_DEBUG_MORE_INFO)
        fprintf(stderr, "Deinitialization\n");

    libusb_close(handle);
}


struct libusb_transfer *transfer[2];
unsigned char v = 0;
int up_trans;
unsigned packet_size = 4096;
unsigned packet_num = 0xFFFFFFFF;
int transfer_type = 0;
time_t t0, t1;
unsigned trx_size = 0, cum_trx_size = 0;
unsigned bad_data = 0;


void start_transfer(struct libusb_transfer *transfer, long int index);

void LIBUSB_CALL trans_cb(struct libusb_transfer *transfer)
{
    unsigned i;
    double seconds;
    long int index = (long int)transfer->user_data;

    trx_size += packet_size;
    cum_trx_size += packet_size;

    if (!up_trans) {
        for (i = 0; i < packet_size; ++i)
            if (databuf[index][i] != v++) {
                fprintf(stderr, "rec: %02X, exp: %02X, i = %d\n", databuf[index][i], v - 1, i);
                bad_data++;
                v = databuf[index][i] + 1;
            }
    }

    if (packet_num-- > 0)
        start_transfer(transfer, index);

    time(&t1);
    seconds = difftime(t1, t0);
    if (seconds > 1.0) {
        if (up_trans) fprintf(stderr, "Sent: ");
        else fprintf(stderr, "Received: ");
        fprintf(stderr, "%12u bytes, rate: %u bytes/sec",
                cum_trx_size, (unsigned)(trx_size / seconds));
        if (up_trans) fprintf(stderr, "\n");
        else fprintf(stderr, ", bad data: %d\n", bad_data);
        memcpy(&t0, &t1, sizeof(time_t));
        trx_size = 0;
    }
}


void start_transfer(struct libusb_transfer *transfer, long int index)
{
    int res;
    unsigned i;
    if (up_trans) {
        for (i = 0; i < packet_size; ++i)
            databuf[index][i] = v++;
    }

    switch (transfer_type) {
    case LIBUSB_TRANSFER_TYPE_BULK:
        libusb_fill_bulk_transfer(transfer, handle, (up_trans) ? bulk_out_ep : bulk_in_ep,
                                  databuf[index], packet_size, trans_cb, (void*) index, 1000);
        break;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        libusb_fill_interrupt_transfer(transfer, handle, (up_trans) ? int_out_ep : int_in_ep,
                                       databuf[index], packet_size, trans_cb, (void*) index, 1000);
        break;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        libusb_fill_iso_transfer(transfer, handle, (up_trans) ? iso_out_ep : iso_in_ep,
                                       databuf[index], packet_size, 1, trans_cb, (void*) index, 1000);
        libusb_set_iso_packet_lengths(transfer, packet_size);
        break;
        /*
    default:
        r = libusb_control_transfer(handle, ((up) ? LIBUSB_ENDPOINT_OUT : LIBUSB_ENDPOINT_IN) |
                                    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
                                    0, 0, 0, databuf, packet_size, 1000);
                                    */
    }

    res = libusb_submit_transfer(transfer);
    if (res != LIBUSB_SUCCESS) {
        fprintf(stderr, "libusb_submit_transfer returned %d\n", res);
        return;
    }
}

void check_vendor_req()
{
    int r;

    uint32_t data = 0x12345678;
    uint8_t  array[32];

    memset (array, 0, sizeof(array));

    r = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN,
                                6, 0x0100, 0, array, 32, 5000);

    if (r < 0) {
        fprintf(stderr, "libusb_control_transfer returned %d\n", r);
        return;
    }

    fprintf(stderr, "libusb_control_transfer finished successfully!\n");

    fprintf(stderr, "data = %08X\n", data);
    for (unsigned i = 0; i < sizeof(array); ++i)
        fprintf(stderr, "%02X ", array[i]);
    fprintf(stderr, "\n");
}

void usage()
{
    printf ("Probe:\n");
    printf ("       test-usbdev\n");
    printf ("\nTest rx speed:\n");
    printf ("       test-usbdev [-b][-i] -r <packet_size> [-n <number_of_packets>]\n");
    printf ("\nTest tx speed:\n");
    printf ("       test-usbdev [-b][-i] -t <packet_size> [-n <number_of_packets>]\n");
    printf ("\n -b   -   test BULK transmission.");
    printf ("\n -i   -   test INTERRUPT transmission.");
    printf ("\n -s   -   test ISOCHRONUOS transmission.");
    printf ("\n If no BULK and no INTERRUPT set then use endpoint 0.");
    printf ("\n\nNote:");
    printf ("\nFor BULK IN transmission (rx) <packet_size> will be");
    printf ("\ntruncated to be multiply of endpoint maximum size.");
    printf ("\n");
}

int do_exit = 0;

static void sighandler(int signum)
{
    signum = signum;
    do_exit = 1;
}

int main(int argc, char **argv)
{
    //QCoreApplication app(argc, argv);

    struct sigaction sigact;
    int mode = PROBE;
    int res;

    static const struct option long_options[] = {
        { "help",        0, 0, 'h' },
        { NULL,          0, 0, 0 },
    };

    printf ("\nTest of USB device performance through libusb\n\n");

    char ch;
    while ((ch = getopt_long (argc, argv, "bisr:t:n:vh",
      long_options, 0)) != -1) {
        switch (ch) {
        case 'b':
            transfer_type = LIBUSB_TRANSFER_TYPE_BULK;
            continue;
        case 'i':
            transfer_type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
            continue;
        case 's':
            transfer_type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
            continue;
        case 'r':
            mode = RX;
            packet_size = strtoul (optarg, 0, 0);
            continue;
        case 't':
            mode = TX;
            packet_size = strtoul (optarg, 0, 0);
            continue;
        case 'n':
            packet_num = strtoul (optarg, 0, 0);
            continue;
        case 'v':
            mode = VENDOR_REQ;
            continue;
        case 'h':
            usage();
            finish (0, 0);
        }
    }

    device_init();

    if (mode == PROBE) finish (0, "Device for test detected successfully!");

    switch (mode) {
    case RX:
        up_trans = 0;
        break;
    case TX:
        up_trans = 1;
        break;
    case VENDOR_REQ:
        check_vendor_req();
        finish (0, 0);
        break;
    default:
        usage();
        finish (-11, 0);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    transfer[0] = libusb_alloc_transfer(1);
    transfer[1] = libusb_alloc_transfer(1);

    time(&t0);

    start_transfer(transfer[0], 0);
    start_transfer(transfer[1], 1);

    while (!do_exit) {
        res = libusb_handle_events(NULL);
        if (res != LIBUSB_SUCCESS) {
            fprintf(stderr, "libusb_handle_events returned %d\n", res);
            break;
        }
    }

    device_deinit();
}

