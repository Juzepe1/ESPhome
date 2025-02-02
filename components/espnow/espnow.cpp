#include "espnow.h"

#if defined(USE_ESP32)

#include <cstring>

#include "esp_mac.h"
#include "esp_random.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"

#include <memory>

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow";

static const size_t SEND_BUFFER_SIZE = 200;

ESPNowComponent *ESPNowComponent::static_{nullptr};  // NOLINT

void show_packet(const std::string &title, const ESPNowPacket &packet) {
  ESP_LOGV(TAG, "%s packet. Peer: '%s', Protocol:%c%c%c-%02x, Sequents: %d.%d, Size: %d, Valid: %s", title.c_str(),
           packet.get_peer_code().c_str(), packet.at(3), packet.at(4), packet.at(5), packet.at(6), packet.at(7),
           packet.attempts, packet.content_size(), packet.is_valid() ? "Yes" : "No");
}

std::string peer_str(const uint64_t peer) {
  char mac[24];
  if (peer == 0)
    return "[Not Set]";
  if (peer == ESPNOW_BROADCAST_ADDR)
    return "[Broadcast]";
  uint8_t *peer_ = (uint8_t *) &peer;
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", peer_[0], peer_[1], peer_[2], peer_[3], peer_[4],
           peer_[5]);
  return mac;
}

/* ESPNowComponent ********************************************************************** */

ESPNowComponent::ESPNowComponent() { ESPNowComponent::static_ = this; }  // NOLINT

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "esp_now:");

  ESP_LOGCONFIG(TAG, "  Own Peer code: %s.", peer_str(this->own_peer_address_).c_str());
  ESP_LOGCONFIG(TAG, "  Wifi channel: %d.", this->wifi_channel_);
  ESP_LOGCONFIG(TAG, "  Auto add new peers: %s.", this->auto_add_peer_ ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Use sent status: %s.", this->use_sent_check_ ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Convermation timeout: %" PRIx32 "ms.", this->conformation_timeout_);
  ESP_LOGCONFIG(TAG, "  Send retries: %d.", this->retries_);
}

bool ESPNowComponent::validate_channel_(uint8_t channel) {
  wifi_country_t g_self_country;
  esp_wifi_get_country(&g_self_country);
  if (channel >= g_self_country.schan + g_self_country.nchan) {
    ESP_LOGE(TAG, "Can't set channel %d, not allowed in country %c%c%c.", channel, g_self_country.cc[0],
             g_self_country.cc[1], g_self_country.cc[2]);
    return false;
  }
  return true;
}

void ESPNowComponent::setup() {
#ifndef USE_WIFI
  esp_event_loop_create_default();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_disconnect());

#ifdef CONFIG_ESPNOW_ENABLE_LONG_RANGE
  esp_wifi_get_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(this->wifi_channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
#else
  // this->wifi_channel_ =  wifi::global_wifi_component->
#endif

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_now_register_recv_cb(ESPNowComponent::on_data_received);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  if (this->use_sent_check_) {
    ESP_LOGI(TAG, "send check enabled");

    err = esp_now_register_send_cb(ESPNowComponent::on_data_sent);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_now_register_send_cb failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
  }

  esp_wifi_get_mac(WIFI_IF_STA, (uint8_t *) &this->own_peer_address_);

  for (auto id : this->peers_) {
    this->add_peer(id);
  }

  this->send_queue_ = xQueueCreate(SEND_BUFFER_SIZE, sizeof(ESPNowPacket));
  if (this->send_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create send queue");
    this->mark_failed();
    return;
  }

  this->receive_queue_ = xQueueCreate(SEND_BUFFER_SIZE, sizeof(ESPNowPacket));
  if (this->receive_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create receive queue");
    this->mark_failed();
    return;
  }
}

void ESPNowComponent::loop() {
  if (!this->task_running_) {
    xTaskCreate(espnow_task, "espnow_task", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
    this->task_running_ = true;
  }
}

void ESPNowComponent::espnow_task(void *param) {
  ESPNowComponent *this_espnow = (ESPNowComponent *) param;
  ESPNowPacket packet;  // NOLINT
  for (;;) {
    if (xQueueReceive(this_espnow->receive_queue_, (void *) &packet, (TickType_t) 1) == pdTRUE) {
      if (packet.is_broadcast) {
        this_espnow->call_on_broadcast_(packet);
        continue;
      } else if (!this_espnow->is_paired(packet.peer)) {
        this_espnow->call_on_new_peer_(packet);
      }
      if (this_espnow->is_paired(packet.peer)) {
        this_espnow->call_on_receive_(packet);
      } else {
        ESP_LOGI(TAG, "message skipt, not paired.");
      }
    }
    if (xQueueReceive(this_espnow->send_queue_, (void *) &packet, (TickType_t) 1) == pdTRUE) {
      if (packet.attempts > this_espnow->retries_) {
        ESP_LOGE(TAG, "Dropped '%s' (%d.%d). To many retries.", packet.get_peer_code().c_str(), packet.get_sequents(),
                 packet.attempts);
        this_espnow->unlock();
        continue;
      } else if (this_espnow->is_locked()) {
        if (packet.timestamp + this_espnow->conformation_timeout_ < millis()) {
          ESP_LOGW(TAG, "TimeOut '%s' (%d.%d).", packet.get_peer_code().c_str(), packet.get_sequents(),
                   packet.attempts);
          this_espnow->unlock();
        }
      } else {
        this_espnow->lock();
        packet.retry();
        packet.timestamp = millis();

        esp_err_t err = esp_now_send(packet.get_peer(), packet.get_content(), packet.content_size());

        if (err == ESP_OK) {
          ESP_LOGD(TAG, "Sended '%s' (%d.%d) from buffer. Wait for conformation.", packet.get_peer_code().c_str(),
                   packet.get_sequents(), packet.attempts);
        } else {
          ESP_LOGE(TAG, "Sending '%s' (%d.%d) FAILED. B: %d.", packet.get_peer_code().c_str(), packet.get_sequents(),
                   packet.attempts, this_espnow->send_queue_used());
          this_espnow->unlock();
        }
      }
      xQueueSendToFront(this_espnow->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS);
    }
  }
}

esp_err_t ESPNowComponent::add_peer(uint64_t peer) {
  esp_err_t result = ESP_OK;
  if (!this->is_ready()) {
    this->peers_.push_back(peer);
    return result;
  } else {
    if (esp_now_is_peer_exist((uint8_t *) &peer)) {
      result = esp_now_del_peer((uint8_t *) &peer);
      if (result != ESP_OK)
        return result;
    }

    esp_now_peer_info_t peer_info = {};
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    peer_info.channel = this->wifi_channel_;
    peer_info.encrypt = false;
    memcpy((void *) peer_info.peer_addr, (void *) &peer, 6);
    esp_err_t result = esp_now_add_peer(&peer_info);
    if (result == ESP_OK) {
      this->call_on_add_peer_(peer);
    }
    return result;
  }
}

esp_err_t ESPNowComponent::del_peer(uint64_t peer) {
  esp_err_t result = ESP_OK;
  if (esp_now_is_peer_exist((uint8_t *) &peer)) {
    esp_err_t result = esp_now_del_peer((uint8_t *) &peer);
    if (result == ESP_OK) {
      this->call_on_del_peer_(peer);
    }
  }
  return result;
}

ESPNowDefaultProtocol *ESPNowComponent::get_default_protocol() {
  if (this->protocols_[ESPNOW_MAIN_PROTOCOL_ID] == nullptr) {
    this->register_protocol(new ESPNowDefaultProtocol());  // NOLINT
  }
  return (ESPNowDefaultProtocol *) this->protocols_[ESPNOW_MAIN_PROTOCOL_ID];
}

ESPNowProtocol *ESPNowComponent::get_protocol_(uint32_t protocol) {
  if (this->protocols_[protocol] == nullptr) {
    ESP_LOGE(TAG, "Protocol for '%06" PRIx32 "' is not registered", protocol);
    return nullptr;
  }
  return this->protocols_[protocol];
}

void ESPNowComponent::call_on_receive_(ESPNowPacket &packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet.get_protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet]() { protocol->on_receive(packet); });
  }
}

void ESPNowComponent::call_on_broadcast_(ESPNowPacket &packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet.get_protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet]() { protocol->on_broadcast(packet); });
  }
}

void ESPNowComponent::call_on_sent_(ESPNowPacket &packet, bool status) {
  ESPNowProtocol *protocol = this->get_protocol_(packet.get_protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet, status]() { protocol->on_sent(packet, status); });
  }
}

void ESPNowComponent::call_on_new_peer_(ESPNowPacket &packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet.get_protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet]() { protocol->on_new_peer(packet); });
  }
}

void ESPNowComponent::call_on_add_peer_(uint64_t peer) {
  this->defer([this, peer]() {
    for (const auto &kv : this->protocols_) {
      kv.second->on_add_peer(peer);
    }
  });
}

void ESPNowComponent::call_on_del_peer_(uint64_t peer) {
  this->defer([this, peer]() {
    for (const auto &kv : this->protocols_) {
      kv.second->on_del_peer(peer);
    }
  });
}

bool ESPNowComponent::is_paired(uint64_t peer) {
  bool result = false;
  if (this->pairing_protocol_ != nullptr) {
    result = this->pairing_protocol_->is_paired(peer);
  } else {
    result = (esp_now_is_peer_exist((uint8_t *) &peer));
  }
  if (!result && this->auto_add_peer_) {
    result = (this->add_peer(peer) == ESP_OK);
  }
  return result;
}

/**< callback function of receiving ESPNOW data */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
void ESPNowComponent::on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size)
#else
void ESPNowComponent::on_data_received(const uint8_t *addr, const uint8_t *data, int size)
#endif
{
  if (ESPNowComponent::static_ == nullptr) {
    return;
  }

  bool broadcast = false;
  wifi_pkt_rx_ctrl_t *rx_ctrl = nullptr;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  uint8_t *addr = recv_info->src_addr;
  broadcast = ((uint64_t) *recv_info->des_addr & ESPNOW_BROADCAST_ADDR) == ESPNOW_BROADCAST_ADDR;
  rx_ctrl = recv_info->rx_ctrl;
#else
  wifi_promiscuous_pkt_t *promiscuous_pkt =
      (wifi_promiscuous_pkt_t *) (data - sizeof(wifi_pkt_rx_ctrl_t) - 39);  // = sizeof (espnow_frame_format_t)
  rx_ctrl = &promiscuous_pkt->rx_ctrl;
#endif

  ESPNowPacket packet(addr, data, (uint8_t) size);  // NOLINT
  packet.is_broadcast = broadcast;
  if (rx_ctrl != nullptr) {
    packet.rssi = rx_ctrl->rssi;
    packet.timestamp = rx_ctrl->timestamp;
  } else {
    packet.timestamp = millis();
  }

  show_packet("Receive", packet);

  if (packet.is_valid()) {
    xQueueSendToBack(ESPNowComponent::static_->receive_queue_, (void *) &packet, 10);
  } else {
    ESP_LOGE(TAG, "Invalid ESP-NOW packet received.");
  }
}

bool ESPNowComponent::send(ESPNowPacket packet) {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow is not setup yet.");
  } else if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow failed to setup");
  } else if (this->send_queue_full()) {
    ESP_LOGE(TAG, "Send Buffer Out of Memory.");
  } else if (!this->is_paired(packet.peer)) {
    ESP_LOGE(TAG, "Peer not registered: %s.", packet.get_peer_code().c_str());
  } else if (!packet.is_valid()) {
    ESP_LOGE(TAG, "This Packet is invalid: %s (%d.%d)", packet.get_peer_code().c_str(), packet.get_sequents(),
             packet.attempts);
  } else if (this->use_sent_check_) {
    ESP_LOGV(TAG, "Placing %s (%d.%d) into send buffer. Used: %d of %d", packet.get_peer_code().c_str(),
             packet.get_sequents(), packet.attempts, this->send_queue_used(), SEND_BUFFER_SIZE);
    xQueueSendToBack(this->send_queue_, (void *) &packet, 10);
    return true;
  } else {
    esp_err_t err = esp_now_send(packet.get_peer(), packet.get_content(), packet.content_size());
    ESP_LOGV(TAG, "Sending to %s (%d.%d) directly%s.", packet.get_peer_code().c_str(), packet.get_sequents(),
             packet.attempts, (err == ESP_OK) ? "" : " FAILED");

    this->call_on_sent_(packet, err == ESP_OK);
    return true;
  }

  return false;
}

void ESPNowComponent::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESPNowPacket packet;  // NOLINT
  uint64_t peer = 0;
  memcpy((void *) &peer, mac_addr, 6);
  if (xQueuePeek(ESPNowComponent::static_->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    if (packet.peer != peer) {
      ESP_LOGE(TAG, " Invalid mac address. Expected: %s (%d.%d); got: %s", packet.get_peer_code().c_str(),
               packet.get_sequents(), packet.attempts, peer_str(peer).c_str());
      return;
    } else if (status != ESP_OK) {
      ESP_LOGE(TAG, "Sent packet failed for %s (%d.%d)", packet.get_peer_code().c_str(), packet.get_sequents(),
               packet.attempts);
    } else {
      ESP_LOGV(TAG, "Confirm packet sent %s (%d.%d)", packet.get_peer_code().c_str(), packet.get_sequents(),
               packet.attempts);
      xQueueReceive(ESPNowComponent::static_->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS);
    }
    ESPNowComponent::static_->call_on_sent_(packet, status == ESP_OK);
    ESPNowComponent::static_->unlock();
  }
}

/* ESPNowProtocol ********************************************************************** */

bool ESPNowProtocol::send(uint64_t peer, const uint8_t *data, uint8_t len, uint8_t command) {
  if (peer == 0ULL)
    return false;
  ESPNowPacket packet(peer, data, len, this->get_protocol_id(), command);  // NOLINT
  packet.set_sequents(this->get_next_sequents(peer));
  return this->parent_->send(packet);
}

}  // namespace espnow
}  // namespace esphome

#endif
