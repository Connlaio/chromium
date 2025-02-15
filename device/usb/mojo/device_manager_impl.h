// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
#define DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_

#include <memory>
#include <queue>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class UsbDevice;
class UsbDeviceFilter;
class UsbDeviceHandle;

namespace usb {

class PermissionProvider;

// Implementation of the public DeviceManager interface. This interface can be
// requested from the devices app located at "mojo:devices", if available.
class DeviceManagerImpl : public DeviceManager, public UsbService::Observer {
 public:
  static void Create(base::WeakPtr<PermissionProvider> permission_provider,
                     mojo::InterfaceRequest<DeviceManager> request);

  DeviceManagerImpl(base::WeakPtr<PermissionProvider> permission_provider,
                    UsbService* usb_service,
                    mojo::InterfaceRequest<DeviceManager> request);
  ~DeviceManagerImpl() override;

  void set_connection_error_handler(const mojo::Closure& error_handler) {
    connection_error_handler_ = error_handler;
  }

 private:
  // DeviceManager implementation:
  void GetDevices(EnumerationOptionsPtr options,
                  const GetDevicesCallback& callback) override;
  void GetDeviceChanges(const GetDeviceChangesCallback& callback) override;
  void GetDevice(const mojo::String& guid,
                 mojo::InterfaceRequest<Device> device_request) override;

  // Callbacks to handle the async responses from the underlying UsbService.
  void OnGetDevices(EnumerationOptionsPtr options,
                    const GetDevicesCallback& callback,
                    const std::vector<scoped_refptr<UsbDevice>>& devices);

  // UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override;
  void WillDestroyUsbService() override;

  void MaybeRunDeviceChangesCallback();

  base::WeakPtr<PermissionProvider> permission_provider_;

  // If there are unfinished calls to GetDeviceChanges their callbacks
  // are stored in |device_change_callbacks_|. Otherwise device changes
  // are collected in |devices_added_| and |devices_removed_| until the
  // next call to GetDeviceChanges.
  std::queue<GetDeviceChangesCallback> device_change_callbacks_;
  std::map<std::string, DeviceInfoPtr> devices_added_;
  std::vector<DeviceInfoPtr> devices_removed_;

  UsbService* usb_service_;
  ScopedObserver<UsbService, UsbService::Observer> observer_;

  mojo::Closure connection_error_handler_;

  mojo::Binding<DeviceManager> binding_;
  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
