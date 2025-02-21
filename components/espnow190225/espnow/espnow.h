#pragma once

#if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_mac.h"
#include <esp_now.h>

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>
#include <random>
#include <string>

namespace esphome {
namespace espnow {

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;
static const uint64_t ESPNOW_MULTICAST_ADDR = 0xFFFFFFFFFFFE;

static const uint8_t MAX_ESPNOW_DATA_SIZE = 240;

static const uint8_t TRANSPORT_HEADER[3] = {'N', '0', 'w'};
static const uint32_t ESPNOW_MAIN_PROTOCOL_ID = 0x447453;  // = StD

static const uint8_t ESPNOW_COMMAND_ACK = 0x06;
static const uint8_t ESPNOW_COMMAND_NAK = 0x15;
static const uint8_t ESPNOW_COMMAND_RESEND = 0x05;

static const uint64_t FAILED = 0;

struct ESPNowPacket;

class ESPNowTAG {
 public:
  // could be made inline with C++17
  static const char *const TAG;
};

template<typename T> std::string espnow_i2h(T i) { return sprintf("%04x", i); }

std::string espnow_rdm(std::string::size_type length);

std::string peer_str(uint64_t peer);

void show_packet(const std::string &title, const ESPNowPacket &packet);

struct ESPNowPeer {
  int8_t channel{-1};
};

struct ESPNowPacket {
  uint64_t peer{0};
  uint8_t rssi{0};
  int8_t attempts{0};
  bool is_broadcast{false};
  bool is_multicast{false};
  uint8_t scan_start{0};
  uint32_t timestamp{0};
  uint8_t size{0};
  struct {
    struct {
      uint8_t header[3]{'N', '0', 'w'};
      uint32_t protocol{0};
      uint8_t sequents{0};
    } __attribute__((packed)) prefix;
    uint8_t payload[MAX_ESPNOW_DATA_SIZE + 1]{0};
  } __attribute__((packed)) content;

  inline ESPNowPacket() ESPHOME_ALWAYS_INLINE {}
  // Create packet to be send.

  inline ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size, uint32_t protocol,
                      uint8_t command = 0) ESPHOME_ALWAYS_INLINE;
  inline ESPNowPacket(const uint8_t *peer, const uint8_t *data, uint8_t size) ESPHOME_ALWAYS_INLINE;

  uint8_t prefix_size() const { return sizeof(this->content.prefix); }

  uint8_t content_size() const { return (this->prefix_size() + this->size); }

  inline void set_peer(const uint8_t *peer) ESPHOME_ALWAYS_INLINE {
    memcpy((void *) this->get_peer(), (const void *) peer, 6);
  };
  inline bool is_peer(const uint8_t *peer) const { return memcmp(peer, this->get_peer(), 6) == 0; }

  inline uint8_t *get_peer() const { return (uint8_t *) &(this->peer); }
  inline std::string get_peer_code() const { return peer_str(this->peer); }

  inline uint32_t get_protocol() const { return this->content.prefix.protocol & 0x00FFFFFF; }
  inline void set_protocol(uint32_t protocol) {
    this->content.prefix.protocol = (this->content.prefix.protocol & 0xFF000000) + (protocol & 0x00FFFFFF);
  }

  inline uint8_t get_command() const { return this->content.prefix.protocol >> 24; }
  inline void set_command(uint32_t command) ESPHOME_ALWAYS_INLINE {
    this->content.prefix.protocol = (this->content.prefix.protocol & 0x00FFFFFF) + (command << 24);
  }

  uint8_t get_sequents() const { return this->content.prefix.sequents; }
  inline void set_sequents(uint8_t sequents) ESPHOME_ALWAYS_INLINE { this->content.prefix.sequents = sequents; }

  inline uint8_t *get_content() const { return (uint8_t *) &(this->content); }
  inline uint8_t *get_payload() const { return (uint8_t *) &(this->content.payload); }
  inline uint8_t at(uint8_t pos) const {
    if (pos >= this->content_size()) {
      esph_log_e(ESPNowTAG::TAG, "Trying to read out of space (%d of %d).", pos, this->content_size());
      return 0;
    }
    return *(((uint8_t *) &this->content) + pos);
  }

  inline void retry() ESPHOME_ALWAYS_INLINE { attempts++; }

  inline bool is_valid() const {
    bool valid = (memcmp((const void *) this->get_content(), (const void *) &TRANSPORT_HEADER, 3) == 0);
    valid &= (this->get_protocol() != 0);
    return valid;
  }
};

class ESPNowComponent;

enum ESPNowProtocolMode { PM_UNIVERSAL, PM_KEEPER, PM_DRUDGE };

class ESPNowProtocol : public Parented<ESPNowComponent> {
 public:
  void set_protocol_mode(ESPNowProtocolMode mode) { this->protocol_mode_ = mode; }
  ESPNowProtocolMode get_protocol_mode() { return this->protocol_mode_; }

 protected:
  ESPNowProtocolMode protocol_mode_{PM_UNIVERSAL};

 public:
  virtual uint32_t get_protocol_id() = 0;
  virtual std::string get_protocol_name() = 0;
  virtual void init_protocol() {}

  virtual void on_receive(const ESPNowPacket &packet){};
  virtual void on_broadcast(const ESPNowPacket &packet) { this->on_receive(packet); };

  virtual void on_sent(const ESPNowPacket &packet, bool status){};
  virtual void on_new_peer(const ESPNowPacket &packet){};

  virtual void on_add_peer(uint64_t peer){};
  virtual void on_del_peer(uint64_t peer){};

  virtual bool is_paired(uint64_t to_peer) { return true; }

  uint8_t get_next_sequents(uint64_t peer) {
    if (this->next_sequents_[peer] == 255) {
      this->next_sequents_[peer] = 0;
    } else {
      this->next_sequents_[peer]++;
    }
    return this->next_sequents_[peer];
  }

  bool is_valid_squence(uint64_t peer, uint8_t received_sequence) {
    bool valid = this->next_sequents_[peer] + 1 == received_sequence;
    if (valid) {
      this->next_sequents_[peer] = received_sequence;
    }
    return valid;
  }

  bool send(uint64_t peer, const uint8_t *data, uint8_t len, uint8_t command = 0);

 protected:
  std::map<uint64_t, uint8_t> next_sequents_{};
  std::string get_mode_name_() {
    switch (this->protocol_mode_) {
      case PM_UNIVERSAL:
        return "Universal";
      case PM_KEEPER:
        return "Keeper";
      case PM_DRUDGE:
        return "Drudge";
    }
  }
};

class ESPNowDefaultProtocol : public ESPNowProtocol {
 public:
  uint32_t get_protocol_id() override { return ESPNOW_MAIN_PROTOCOL_ID; };
  std::string get_protocol_name() override { return "Default"; }

  void add_on_receive_callback(std::function<void(const ESPNowPacket)> &&callback) {
    this->on_receive_.add(std::move(callback));
  }
  void on_receive(const ESPNowPacket &packet) override { this->on_receive_.call(packet); };

  void add_on_broadcast_callback(std::function<void(const ESPNowPacket)> &&callback) {
    this->on_broadcast_.add(std::move(callback));
  }
  void on_broadcast(const ESPNowPacket &packet) override {
    if (this->on_broadcast_.size() > 0) {
      this->on_broadcast_.call(packet);
    } else {
      this->on_receive(packet);
    }
  };

  void add_on_sent_callback(std::function<void(const ESPNowPacket, bool status)> &&callback) {
    this->on_sent_.add(std::move(callback));
  }
  void on_sent(const ESPNowPacket &packet, bool status) override { this->on_sent_.call(packet, status); };

  void add_on_new_peer_callback(std::function<void(const ESPNowPacket)> &&callback) {
    this->on_new_peer_.add(std::move(callback));
  }
  void on_new_peer(const ESPNowPacket &packet) override { this->on_new_peer_.call(packet); };

 protected:
  CallbackManager<void(const ESPNowPacket, bool)> on_sent_;
  CallbackManager<void(const ESPNowPacket)> on_receive_;
  CallbackManager<void(const ESPNowPacket)> on_new_peer_;
  CallbackManager<void(const ESPNowPacket)> on_broadcast_;
};

class ESPNowComponent : public Component {
 public:
  ESPNowComponent();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  static void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size);
#else
  static void on_data_received(const uint8_t *addr, const uint8_t *data, int size);
#endif

  static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

  void dump_config() override;

  float get_setup_priority() const override { return -100; }

  void set_wifi_channel(uint8_t channel);
  void set_auto_add_peer(bool value) { this->auto_add_peer_ = value; }
  void set_use_sent_check(bool value) { this->use_sent_check_ = value; }
  void set_auto_channel_scan(bool value) { this->auto_channel_scan_ = value; }

  void set_conformation_timeout(uint32_t timeout) { this->conformation_timeout_ = timeout; }
  void set_retries(uint8_t value) { this->retries_ = value; }
  void set_pairing_protocol(ESPNowProtocol *pairing_protocol) { this->pairing_protocol_ = pairing_protocol; }

  uint64_t get_own_peer_address() { return this->own_peer_address_; }

  void setup() override;
  void loop() override;
  bool can_proceed() override;

  bool is_paired(uint64_t to_peer);

  bool send(ESPNowPacket packet);

  void register_protocol(ESPNowProtocol *protocol) {
    protocol->set_parent(this);
    this->protocols_[protocol->get_protocol_id()] = protocol;
    protocol->init_protocol();
  }

  esp_err_t add_peer(uint64_t peer, int8_t channel = -1);
  esp_err_t del_peer(uint64_t peer);

  bool send_queue_empty() { return uxQueueMessagesWaiting(this->send_queue_) == 0; }
  bool send_queue_full() { return uxQueueSpacesAvailable(this->send_queue_) == 0; }
  size_t send_queue_used() { return uxQueueMessagesWaiting(this->send_queue_); }
  size_t send_queue_free() { return uxQueueSpacesAvailable(this->send_queue_); }

  void lock() { this->lock_ = true; }
  bool is_locked() { return this->lock_; }
  void unlock() { this->lock_ = false; }

  ESPNowDefaultProtocol *get_default_protocol();

  static void espnow_task(void *params);

  void handle_internal_commands(ESPNowPacket packet);
  void handle_internal_sent(ESPNowPacket packet, bool status);

  uint8_t get_channel_for(uint64_t peer);

 protected:
  bool validate_channel_(uint8_t channel);

  ESPNowProtocol *get_protocol_(uint32_t protocol);
  ESPNowProtocol *pairing_protocol_{nullptr};

  uint64_t own_peer_address_{0};
  uint8_t wifi_channel_{0};
  uint32_t conformation_timeout_{5000};
  uint8_t retries_{5};

  bool auto_add_peer_{false};
  bool use_sent_check_{true};
  bool auto_channel_scan_{false};

  bool lock_{false};

  void call_on_receive_(ESPNowPacket &packet);
  void call_on_broadcast_(ESPNowPacket &packet);
  void call_on_sent_(ESPNowPacket &packet, bool status);
  void call_on_new_peer_(ESPNowPacket &packet);

  void call_on_add_peer_(uint64_t peer);
  void call_on_del_peer_(uint64_t peer);

  void update_channel_scan_(ESPNowPacket &packet);
  void start_multi_cast_();

  QueueHandle_t receive_queue_{};
  QueueHandle_t send_queue_{};

  std::map<uint32_t, ESPNowProtocol *> protocols_{};
  std::map<uint64_t, ESPNowPeer> peers_{};
  std::vector<uint64_t> multicast_{};

  bool task_running_{false};
  static ESPNowComponent *static_;  // NOLINT
};

/*********************************  Actions **************************************/
template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  TEMPLATABLE_VALUE(uint8_t, command);
  TEMPLATABLE_VALUE(std::vector<uint8_t>, payload);

 public:
  void play(Ts... x) override {
    uint64_t peer = this->mac_address_.value(x...);
    uint8_t command = this->command_.value(x...);
    std::vector<uint8_t> payload = this->payload_.value(x...);
    this->parent_->get_default_protocol()->send(peer, payload.data(), payload.size(), command);
  }
};

template<typename... Ts> class NewPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  TEMPLATABLE_VALUE(int8_t, wifi_channel);
  void play(Ts... x) override {
    uint64_t mac_address = this->mac_address_.value(x...);
    if (this->wifi_channel_.has_value()) {
      parent_->add_peer(mac_address);
    } else {
      parent_->add_peer(mac_address, this->wifi_channel_.value(x...));
    }
  }
};

template<typename... Ts> class DelPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  void play(Ts... x) override {
    uint64_t mac_address = this->mac_address_.value(x...);
    parent_->del_peer(mac_address);
  }
};

template<typename... Ts> class SetChannelAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(int8_t, channel);
  void play(Ts... x) override {
#ifdef USE_WIFI
    esph_log_e(ESPNowTAG::TAG, "Manual changing the channel is not possible with WIFI enabled.");
#else
    int8_t value = this->channel_.value(x...);
    parent_->set_wifi_channel(value);
#endif
  }
};

/********************************* triggers **************************************/
class ESPNowSentTrigger : public Trigger<const ESPNowPacket, bool> {
 public:
  explicit ESPNowSentTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_sent_callback([this](const ESPNowPacket packet, bool status) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet, status);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

class ESPNowReceiveTrigger : public Trigger<const ESPNowPacket> {
 public:
  explicit ESPNowReceiveTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_receive_callback([this](const ESPNowPacket packet) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

class ESPNowBroadcaseTrigger : public Trigger<const ESPNowPacket> {
 public:
  explicit ESPNowBroadcaseTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_broadcast_callback([this](const ESPNowPacket packet) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

class ESPNowNewPeerTrigger : public Trigger<const ESPNowPacket> {
 public:
  explicit ESPNowNewPeerTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_new_peer_callback([this](const ESPNowPacket packet) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

}  // namespace espnow
}  // namespace esphome

#endif
