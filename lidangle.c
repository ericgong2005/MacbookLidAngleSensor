// lidangle.c
// Build: clang -framework IOKit -framework CoreFoundation -o lidangle lidangle.c
// Run:   ./lidangle

#include <stdio.h>
#include <unistd.h>
#include <IOKit/hid/IOHIDManager.h>

IOHIDDeviceRef findLidAngleSensor() {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) return NULL;

    if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        CFRelease(manager);
        return NULL;
    }

    CFMutableDictionaryRef matchingDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    int vendor = 0x05AC;   // Apple
    int product = 0x8104;  // Known lid sensor product
    int usagePage = 0x20;  // Sensor page
    int usage = 0x8A;      // Orientation / lid angle usage

    CFNumberRef vendorRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor);
    CFNumberRef productRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &product);
    CFNumberRef usagePageRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
    CFNumberRef usageRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);

    CFDictionarySetValue(matchingDict, CFSTR("VendorID"), vendorRef);
    CFDictionarySetValue(matchingDict, CFSTR("ProductID"), productRef);
    CFDictionarySetValue(matchingDict, CFSTR("UsagePage"), usagePageRef);
    CFDictionarySetValue(matchingDict, CFSTR("Usage"), usageRef);

    IOHIDManagerSetDeviceMatching(manager, matchingDict);

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    IOHIDDeviceRef device = NULL;

    if (devices && CFSetGetCount(devices) > 0) {
        const void **deviceArray = malloc(sizeof(void*) * CFSetGetCount(devices));
        CFSetGetValues(devices, deviceArray);

        for (CFIndex i = 0; i < CFSetGetCount(devices); i++) {
            IOHIDDeviceRef testDevice = (IOHIDDeviceRef)deviceArray[i];
            if (IOHIDDeviceOpen(testDevice, kIOHIDOptionsTypeNone) == kIOReturnSuccess) {
                device = (IOHIDDeviceRef)CFRetain(testDevice);
                IOHIDDeviceClose(testDevice, kIOHIDOptionsTypeNone);
                break;
            }
        }
        free(deviceArray);
    }

    if (devices) CFRelease(devices);
    CFRelease(matchingDict);
    CFRelease(vendorRef);
    CFRelease(productRef);
    CFRelease(usagePageRef);
    CFRelease(usageRef);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);

    return device;
}

double readLidAngle(IOHIDDeviceRef device) {
    uint8_t report[8] = {0};
    CFIndex reportLength = sizeof(report);

    IOReturn result = IOHIDDeviceGetReport(device,
                                           kIOHIDReportTypeFeature,
                                           1,  // Report ID = 1
                                           report,
                                           &reportLength);

    if (result == kIOReturnSuccess && reportLength >= 3) {
        uint16_t rawValue = (report[2] << 8) | report[1];
        return (double)rawValue;
    }
    return -1.0;
}

int main() {
    IOHIDDeviceRef device = findLidAngleSensor();
    if (!device) {
        fprintf(stderr, "Failed to find lid angle sensor\n");
        return 1;
    }

    if (IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        fprintf(stderr, "Failed to open lid angle sensor\n");
        CFRelease(device);
        return 1;
    }

    while (1) {
        double angle = readLidAngle(device);
        printf("Lid Angle: %.1f°\n", angle);
        usleep(500000); // 0.5 seconds
    }

    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    CFRelease(device);
    return 0;
}
