// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterWin::CreateAdapter(init_callback);
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapterWin::CreateAdapter(
    const InitCallback& init_callback) {
  BluetoothAdapterWin* adapter = new BluetoothAdapterWin(init_callback);
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

BluetoothAdapterWin::BluetoothAdapterWin(const InitCallback& init_callback)
    : BluetoothAdapter(),
      init_callback_(init_callback),
      initialized_(false),
      powered_(false),
      discovery_status_(NOT_DISCOVERING),
      num_discovery_listeners_(0),
      force_update_device_for_test_(false),
      weak_ptr_factory_(this) {}

BluetoothAdapterWin::~BluetoothAdapterWin() {
  if (task_manager_.get()) {
    task_manager_->RemoveObserver(this);
    task_manager_->Shutdown();
  }
}

std::string BluetoothAdapterWin::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWin::GetName() const {
  return name_;
}

void BluetoothAdapterWin::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// TODO(youngki): Return true when |task_manager_| initializes the adapter
// state.
bool BluetoothAdapterWin::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterWin::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterWin::IsPowered() const {
  return powered_;
}

void BluetoothAdapterWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  task_manager_->PostSetPoweredBluetoothTask(powered, callback, error_callback);
}

bool BluetoothAdapterWin::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsDiscovering() const {
  return discovery_status_ == DISCOVERING ||
      discovery_status_ == DISCOVERY_STOPPING;
}

static void RunDiscoverySessionErrorCallback(
    const base::Callback<void(UMABluetoothDiscoverySessionOutcome)>& callback,
    UMABluetoothDiscoverySessionOutcome outcome) {
  callback.Run(outcome);
}

void BluetoothAdapterWin::DiscoveryStarted(bool success) {
  discovery_status_ = success ? DISCOVERING : NOT_DISCOVERING;
  for (const auto& callbacks : on_start_discovery_callbacks_) {
    if (success)
      ui_task_runner_->PostTask(FROM_HERE, callbacks.first);
    else
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&RunDiscoverySessionErrorCallback, callbacks.second,
                     UMABluetoothDiscoverySessionOutcome::UNKNOWN));
  }
  num_discovery_listeners_ = on_start_discovery_callbacks_.size();
  on_start_discovery_callbacks_.clear();

  if (success) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, true));

    // If there are stop discovery requests, post the stop discovery again.
    MaybePostStopDiscoveryTask();
  } else if (!on_stop_discovery_callbacks_.empty()) {
    // If there are stop discovery requests but start discovery has failed,
    // notify that stop discovery has been complete.
    DiscoveryStopped();
  }
}

void BluetoothAdapterWin::DiscoveryStopped() {
  discovered_devices_.clear();
  bool was_discovering = IsDiscovering();
  discovery_status_ = NOT_DISCOVERING;
  for (std::vector<base::Closure>::const_iterator iter =
           on_stop_discovery_callbacks_.begin();
       iter != on_stop_discovery_callbacks_.end();
       ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, *iter);
  }
  num_discovery_listeners_ = 0;
  on_stop_discovery_callbacks_.clear();
  if (was_discovering)
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, false));

  // If there are start discovery requests, post the start discovery again.
  MaybePostStartDiscoveryTask();
}

BluetoothAdapter::UUIDList BluetoothAdapterWin::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterWin::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket =
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_);
  socket->Listen(this, uuid, options,
                 base::Bind(callback, socket),
                 error_callback);
}

void BluetoothAdapterWin::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  // TODO(keybuk): implement.
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::RegisterAudioSink(
    const BluetoothAudioSink::Options& options,
    const AcquiredCallback& callback,
    const BluetoothAudioSink::ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAudioSink::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterWin::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const CreateAdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterWin::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
}

void BluetoothAdapterWin::AdapterStateChanged(
    const BluetoothTaskManagerWin::AdapterState& state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  name_ = state.name;
  bool was_present = IsPresent();
  bool is_present = !state.address.empty();
  address_ = BluetoothDevice::CanonicalizeAddress(state.address);
  if (was_present != is_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, is_present));
  }
  if (powered_ != state.powered) {
    powered_ = state.powered;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPoweredChanged(this, powered_));
  }
  if (!initialized_) {
    initialized_ = true;
    init_callback_.Run();
  }
}

void BluetoothAdapterWin::DevicesPolled(
    const ScopedVector<BluetoothTaskManagerWin::DeviceState>& devices) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // We are receiving a new list of all devices known to the system. Merge this
  // new list with the list we know of (|devices_|) and raise corresponding
  // DeviceAdded, DeviceRemoved and DeviceChanged events.

  typedef std::set<std::string> DeviceAddressSet;
  DeviceAddressSet known_devices;
  for (DevicesMap::const_iterator iter = devices_.begin();
       iter != devices_.end();
       ++iter) {
    known_devices.insert((*iter).first);
  }

  DeviceAddressSet new_devices;
  for (ScopedVector<BluetoothTaskManagerWin::DeviceState>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    new_devices.insert((*iter)->address);
  }

  // Process device removal first
  DeviceAddressSet removed_devices =
      base::STLSetDifference<DeviceAddressSet>(known_devices, new_devices);
  for (DeviceAddressSet::const_iterator iter = removed_devices.begin();
       iter != removed_devices.end();
       ++iter) {
    std::unique_ptr<BluetoothDevice> device_win =
        devices_.take_and_erase(*iter);
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, device_win.get()));
  }

  // Process added and (maybe) changed devices in one pass
  DeviceAddressSet added_devices =
      base::STLSetDifference<DeviceAddressSet>(new_devices, known_devices);
  DeviceAddressSet changed_devices =
      base::STLSetIntersection<DeviceAddressSet>(known_devices, new_devices);
  for (ScopedVector<BluetoothTaskManagerWin::DeviceState>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    BluetoothTaskManagerWin::DeviceState* device_state = (*iter);
    if (added_devices.find(device_state->address) != added_devices.end()) {
      BluetoothDeviceWin* device_win =
          new BluetoothDeviceWin(this, *device_state, ui_task_runner_,
                                 socket_thread_, NULL, net::NetLog::Source());
      devices_.set(device_state->address,
                   std::unique_ptr<BluetoothDevice>(device_win));
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                        observers_,
                        DeviceAdded(this, device_win));
    } else if (changed_devices.find(device_state->address) !=
               changed_devices.end()) {
      DevicesMap::const_iterator iter = devices_.find(device_state->address);
      DCHECK(iter != devices_.end());
      BluetoothDeviceWin* device_win =
          static_cast<BluetoothDeviceWin*>(iter->second);
      if (!device_win->IsEqual(*device_state)) {
        device_win->Update(*device_state);
        FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                          DeviceChanged(this, device_win));
      }
      // Above IsEqual returns true if device name, address, status and services
      // (primary services of BLE device) are the same. However, in BLE tests,
      // we may simulate characteristic, descriptor and secondary GATT service
      // after device has been initialized.
      if (force_update_device_for_test_)
        device_win->Update(*device_state);
    }
  }
}

// If the method is called when |discovery_status_| is DISCOVERY_STOPPING,
// starting again is handled by BluetoothAdapterWin::DiscoveryStopped().
void BluetoothAdapterWin::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  if (discovery_status_ == DISCOVERING) {
    num_discovery_listeners_++;
    callback.Run();
    return;
  }
  on_start_discovery_callbacks_.push_back(
      std::make_pair(callback, error_callback));
  MaybePostStartDiscoveryTask();
}

void BluetoothAdapterWin::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  if (discovery_status_ == NOT_DISCOVERING) {
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_ACTIVE);
    return;
  }
  on_stop_discovery_callbacks_.push_back(callback);
  MaybePostStopDiscoveryTask();
}

void BluetoothAdapterWin::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

void BluetoothAdapterWin::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  socket_thread_ = BluetoothSocketThread::Get();
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->Initialize();
}

void BluetoothAdapterWin::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  ui_task_runner_ = ui_task_runner;
  if (ui_task_runner_ == nullptr)
    ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->InitializeWithBluetoothTaskRunner(bluetooth_task_runner);
}

void BluetoothAdapterWin::MaybePostStartDiscoveryTask() {
  if (discovery_status_ == NOT_DISCOVERING &&
      !on_start_discovery_callbacks_.empty()) {
    discovery_status_ = DISCOVERY_STARTING;
    task_manager_->PostStartDiscoveryTask();
  }
}

void BluetoothAdapterWin::MaybePostStopDiscoveryTask() {
  if (discovery_status_ != DISCOVERING)
    return;

  if (on_stop_discovery_callbacks_.size() < num_discovery_listeners_) {
    for (std::vector<base::Closure>::const_iterator iter =
             on_stop_discovery_callbacks_.begin();
         iter != on_stop_discovery_callbacks_.end();
         ++iter) {
      ui_task_runner_->PostTask(FROM_HERE, *iter);
    }
    num_discovery_listeners_ -= on_stop_discovery_callbacks_.size();
    on_stop_discovery_callbacks_.clear();
    return;
  }

  discovery_status_ = DISCOVERY_STOPPING;
  task_manager_->PostStopDiscoveryTask();
}

}  // namespace device
