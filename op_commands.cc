#include "op_commands.h"

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

#include <cstring>

#include <base/logging.h>

// These are constants used to describe the exported device. They are used to
// populate the OpRepDevice message used when responding to OpReqDevlist and
// OpReqImport requests.
const char kUsbPath[] = "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1";
const char kBusId[] = "1-1";
constexpr int kBusnum = 1;
constexpr int kDevnum = 2;
constexpr int kSpeed = 3;  // Represents a high-speed USB device.
constexpr uint16_t kUsbipVersion = 0x0111;  // usbip version in BCD.

void SetOpHeader(uint16_t command, int status, OpHeader* header) {
  header->version = kUsbipVersion;
  header->command = command;
  header->status = status;
}

void SetOpRepDevlistHeader(uint16_t command, int status, int numExportedDevices,
                           OpRepDevlistHeader* devlist_header) {
  SetOpHeader(command, status, &devlist_header->header);
  devlist_header->numExportedDevices = numExportedDevices;
}

void SetOpRepDevice(const UsbDeviceDescriptor& dev_dsc,
                    const UsbConfigurationDescriptor& config,
                    OpRepDevice* device) {
  // Set constants.
  memset(device->usbPath, 0, 256);
  strcpy(device->usbPath, kUsbPath);
  memset(device->busID, 0, 32);
  strcpy(device->busID, kBusId);

  device->busnum = kBusnum;
  device->devnum = kDevnum;
  device->speed = kSpeed;

  // Set values using |dev_dsc|.
  device->idVendor = dev_dsc.idVendor;
  device->idProduct = dev_dsc.idProduct;
  device->bcdDevice = dev_dsc.bcdDevice;
  device->bDeviceClass = dev_dsc.bDeviceClass;
  device->bDeviceSubClass = dev_dsc.bDeviceSubClass;
  device->bDeviceProtocol = dev_dsc.bDeviceProtocol;
  device->bNumConfigurations = dev_dsc.bNumConfigurations;

  // Set values using |config|.
  device->bConfigurationValue = config.bConfigurationValue;
  device->bNumInterfaces = config.bNumInterfaces;
}

void SetOpRepDevlistInterfaces(
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    OpRepDevlistInterface** rep_interfaces) {
  // TODO(daviev): Change this to use a smart pointer at some point.
  *rep_interfaces = (OpRepDevlistInterface*)malloc(
      interfaces.size() * sizeof(OpRepDevlistInterface));
  for (size_t i = 0; i < interfaces.size(); ++i) {
    const auto& interface = interfaces[i];
    (*rep_interfaces)[i].bInterfaceClass = interface.bInterfaceClass;
    (*rep_interfaces)[i].bInterfaceSubClass = interface.bInterfaceSubClass;
    (*rep_interfaces)[i].bInterfaceProtocol = interface.bInterfaceProtocol;
    (*rep_interfaces)[i].padding = 0;
  }
}

// Creates the OpRepDevlist message used to respond to a request to list the
// host's exported USB devices.
void CreateOpRepDevlist(const UsbDeviceDescriptor& device,
                        const UsbConfigurationDescriptor& config,
                        const std::vector<UsbInterfaceDescriptor>& interfaces,
                        OpRepDevlist* list) {
  SetOpRepDevlistHeader(OP_REP_DEVLIST_CMD, 0, 1, &list->header);
  SetOpRepDevice(device, config, &list->device);
  SetOpRepDevlistInterfaces(interfaces, &list->interfaces);
}

void CreateOpRepImport(const UsbDeviceDescriptor& dev_dsc,
                       const UsbConfigurationDescriptor& config,
                       OpRepImport* rep) {
  SetOpHeader(OP_REP_IMPORT_CMD, 0, &rep->header);
  SetOpRepDevice(dev_dsc, config, &rep->device);
}

SmartBuffer<uint8_t> PackOpHeader(OpHeader header) {
  header.version = htons(header.version);
  header.command = htons(header.command);
  header.status = htonl(header.status);
  SmartBuffer<uint8_t> packed_header(sizeof(header));
  packed_header.Add(&header, sizeof(header));
  return packed_header;
}

SmartBuffer<uint8_t> PackOpRepDevice(OpRepDevice device) {
  device.busnum = htonl(device.busnum);
  device.devnum = htonl(device.devnum);
  device.speed = htonl(device.speed);
  device.idVendor = htons(device.idVendor);
  device.idProduct = htons(device.idProduct);
  device.bcdDevice = htons(device.bcdDevice);
  SmartBuffer<uint8_t> packed_device(sizeof(device));
  packed_device.Add(&device, sizeof(device));
  return packed_device;
}

SmartBuffer<uint8_t> PackOpRepDevlistHeader(OpRepDevlistHeader devlist_header) {
  SmartBuffer<uint8_t> packed_op_header = PackOpHeader(devlist_header.header);
  devlist_header.numExportedDevices = htonl(devlist_header.numExportedDevices);
  SmartBuffer<uint8_t> packed_header(sizeof(OpRepDevlistHeader));
  packed_header.Add(packed_op_header);
  packed_header.Add(&devlist_header.numExportedDevices,
                    sizeof(devlist_header.numExportedDevices));
  return packed_header;
}

SmartBuffer<uint8_t> PackOpRepDevlist(OpRepDevlist devlist) {
  SmartBuffer<uint8_t> packed_header = PackOpRepDevlistHeader(devlist.header);
  SmartBuffer<uint8_t> packed_device = PackOpRepDevice(devlist.device);
  size_t interfaces_size =
      sizeof(*devlist.interfaces) * devlist.device.bNumInterfaces;
  size_t buffer_size =
      sizeof(devlist.header) + sizeof(devlist.device) + interfaces_size;
  SmartBuffer<uint8_t> packed_devlist(buffer_size);
  packed_devlist.Add(packed_header);
  packed_devlist.Add(packed_device);
  packed_devlist.Add(devlist.interfaces, interfaces_size);
  return packed_devlist;
}

SmartBuffer<uint8_t> PackOpRepImport(OpRepImport import) {
  SmartBuffer<uint8_t> packed_header = PackOpHeader(import.header);
  SmartBuffer<uint8_t> packed_device = PackOpRepDevice(import.device);
  SmartBuffer<uint8_t> packed_import(sizeof(import));
  packed_import.Add(packed_header);
  packed_import.Add(packed_device);
  return packed_import;
}

void UnpackOpHeader(OpHeader* header) {
  header->version = ntohs(header->version);
  header->command = ntohs(header->command);
  header->status = ntohl(header->status);
}
