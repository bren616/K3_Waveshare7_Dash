/*
 * BLE Manager - RaceChrono DIY BLE Device Implementation
 *
 * Implements the RaceChrono DIY BLE protocol as a GATT server using
 * ESP-Hosted Bluedroid. The ESP32-P4 acts as a peripheral that
 * RaceChrono connects to and sends lap/delta data via BLE writes.
 *
 * RaceChrono DIY Protocol:
 *   Service UUID: 0x1FF8
 *   Characteristics:
 *     - 0x0005 (Config):  WRITE | INDICATE  -> monitor config
 *     - 0x0006 (Notify):  WRITE_NO_RESPONSE -> monitor data packets
 *
 *   Data format per monitor: 1 byte ID + 4 bytes int32 big-endian
 *   Monitor 0: delta_lap_time (milliseconds)
 *   Monitor 1: delta_speed (tenths)
 */

#include "ble_manager.h"

#include <inttypes.h>
#include <string.h>

#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_hosted.h"
#include "esp_hosted_bluedroid.h"
#include "esp_hosted_misc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "BLE_MGR";

/* ---------- RaceChrono Protocol Constants ---------- */

#define RC_SERVICE_UUID 0x1FF8
#define RC_CHAR_CANBUS_UUID 0x0001
#define RC_CHAR_CANBUS_FILTER_UUID 0x0002
#define RC_CHAR_CONFIG_UUID 0x0005
#define RC_CHAR_NOTIFY_UUID 0x0006

#define RC_MONITOR_COUNT 2
#define RC_MONITOR_LAP_TIME 0
#define RC_MONITOR_DELTA 1

/* ---------- BLE Constants ---------- */

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x55
#define SVC_INST_ID 0
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

/* ---------- GATT Database Indices ---------- */

enum {
  RC_IDX_SVC,

  RC_IDX_CHAR_CANBUS,
  RC_IDX_CHAR_VAL_CANBUS,
  RC_IDX_CHAR_CCC_CANBUS, /* CCC for CAN-Bus notifications */

  RC_IDX_CHAR_CANBUS_FILTER,
  RC_IDX_CHAR_VAL_CANBUS_FILTER,

  RC_IDX_CHAR_CONFIG,
  RC_IDX_CHAR_VAL_CONFIG,
  RC_IDX_CHAR_CCC_CONFIG, /* Client Characteristic Config (for indications) */

  RC_IDX_CHAR_NOTIFY,
  RC_IDX_CHAR_VAL_NOTIFY,

  RC_IDX_NB,
};

/* ---------- State ---------- */

static uint16_t g_gatt_handle_table[RC_IDX_NB];
static uint8_t g_adv_config_done = 0;
static bool g_connected = false;
static uint16_t g_conn_id = 0;
static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;

/* Monitor data protected by mutex */
static SemaphoreHandle_t g_data_mutex = NULL;
static int32_t g_lap_time_ms = 0;
static bool g_lap_time_valid = false;
static int32_t g_delta_ms = 0;
static bool g_delta_valid = false;

/* Device name buffer */
static char g_device_name[16] = "RC DIY #0000";

/* ---------- Advertising ---------- */

static uint8_t rc_service_uuid[16] = {
    /* 128-bit UUID in LSB order, with 16-bit UUID at bytes [12],[13] */
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00,
    0x80, 0x00, 0x10, 0x00, 0x00, 0xF8, 0x1F, /* 0x1FF8 little-endian */
    0x00, 0x00,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0, /* UUID goes in scan_rsp to fit 31-byte limit */
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = false, /* Name already in adv_data */
    .include_txpower = false,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(rc_service_uuid),
    .p_service_uuid = rc_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20, /* 20ms */
    .adv_int_max = 0x40, /* 40ms */
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ---------- GATT Database ---------- */

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid =
    ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t rc_service_uuid_val = RC_SERVICE_UUID;
static const uint16_t rc_canbus_char_uuid = RC_CHAR_CANBUS_UUID;
static const uint16_t rc_canbus_filter_char_uuid = RC_CHAR_CANBUS_FILTER_UUID;
static const uint16_t rc_config_char_uuid = RC_CHAR_CONFIG_UUID;
static const uint16_t rc_notify_char_uuid = RC_CHAR_NOTIFY_UUID;

static const uint8_t char_prop_read_notify =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_write_indicate =
    ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t char_prop_write_nr = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t char_ccc_default[2] = {0x00, 0x00};
static const uint8_t char_val_default[1] = {0x00};

static const esp_gatts_attr_db_t rc_gatt_db[RC_IDX_NB] = {
    /* Service Declaration */
    [RC_IDX_SVC] = {{ESP_GATT_AUTO_RSP},
                    {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
                     ESP_GATT_PERM_READ, sizeof(uint16_t),
                     sizeof(rc_service_uuid_val),
                     (uint8_t *)&rc_service_uuid_val}},

    /* ---- CAN-Bus Main Characteristic (UUID 0x0001) ---- */
    [RC_IDX_CHAR_CANBUS] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16,
                             (uint8_t *)&character_declaration_uuid,
                             ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE,
                             CHAR_DECLARATION_SIZE,
                             (uint8_t *)&char_prop_read_notify}},

    [RC_IDX_CHAR_VAL_CANBUS] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&rc_canbus_char_uuid, ESP_GATT_PERM_READ,
          20, sizeof(char_val_default), (uint8_t *)char_val_default}},

    [RC_IDX_CHAR_CCC_CANBUS] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16,
                                 (uint8_t *)&character_client_config_uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                 sizeof(uint16_t), sizeof(char_ccc_default),
                                 (uint8_t *)char_ccc_default}},

    /* ---- CAN-Bus Filter Characteristic (UUID 0x0002) ---- */
    [RC_IDX_CHAR_CANBUS_FILTER] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16,
                                    (uint8_t *)&character_declaration_uuid,
                                    ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE,
                                    CHAR_DECLARATION_SIZE,
                                    (uint8_t *)&char_prop_write}},

    [RC_IDX_CHAR_VAL_CANBUS_FILTER] = {{ESP_GATT_AUTO_RSP},
                                       {ESP_UUID_LEN_16,
                                        (uint8_t *)&rc_canbus_filter_char_uuid,
                                        ESP_GATT_PERM_WRITE, 20,
                                        sizeof(char_val_default),
                                        (uint8_t *)char_val_default}},

    /* Config Characteristic Declaration */
    [RC_IDX_CHAR_CONFIG] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16,
                             (uint8_t *)&character_declaration_uuid,
                             ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE,
                             CHAR_DECLARATION_SIZE,
                             (uint8_t *)&char_prop_write_indicate}},

    /* Config Characteristic Value */
    [RC_IDX_CHAR_VAL_CONFIG] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16,
                                 (uint8_t *)&rc_config_char_uuid,
                                 ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ, 512,
                                 sizeof(char_val_default),
                                 (uint8_t *)char_val_default}},

    /* Config Characteristic - Client Characteristic Configuration Descriptor */
    [RC_IDX_CHAR_CCC_CONFIG] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16,
                                 (uint8_t *)&character_client_config_uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                 sizeof(uint16_t), sizeof(char_ccc_default),
                                 (uint8_t *)char_ccc_default}},

    /* Notify Characteristic Declaration */
    [RC_IDX_CHAR_NOTIFY] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16,
                             (uint8_t *)&character_declaration_uuid,
                             ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE,
                             CHAR_DECLARATION_SIZE,
                             (uint8_t *)&char_prop_write_nr}},

    /* Notify Characteristic Value */
    [RC_IDX_CHAR_VAL_NOTIFY] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&rc_notify_char_uuid, ESP_GATT_PERM_WRITE,
          512, sizeof(char_val_default), (uint8_t *)char_val_default}},
};

/* ---------- Profile ---------- */

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static struct {
  esp_gatts_cb_t gatts_cb;
  uint16_t gatts_if;
  uint16_t app_id;
  uint16_t conn_id;
} gl_profile = {
    .gatts_cb = gatts_profile_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
};

/* ---------- Data Parsing ---------- */

/**
 * Parse a RaceChrono notification write.
 * Format: N x 5-byte blocks, each = [monitor_id(1)] [value_be32(4)]
 */
static void rc_parse_notification(const uint8_t *data, uint16_t len) {
  if (len < 5)
    return;

  for (uint16_t offset = 0; offset + 5 <= len; offset += 5) {
    uint8_t monitor_id = data[offset];
    int32_t value = (int32_t)(((uint32_t)data[offset + 1] << 24) |
                              ((uint32_t)data[offset + 2] << 16) |
                              ((uint32_t)data[offset + 3] << 8) |
                              ((uint32_t)data[offset + 4]));

    ESP_LOGI(TAG, "Monitor %d value: %" PRId32, monitor_id, value);

    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      switch (monitor_id) {
      case RC_MONITOR_LAP_TIME:
        g_lap_time_ms = value;
        g_lap_time_valid = true;
        break;
      case RC_MONITOR_DELTA:
        g_delta_ms = value;
        g_delta_valid = true;
        break;
      default:
        ESP_LOGW(TAG, "Unknown monitor ID: %d", monitor_id);
        break;
      }
      xSemaphoreGive(g_data_mutex);
    }
  }
}

/* ---------- GAP Event Handler ---------- */

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    g_adv_config_done &= (~ADV_CONFIG_FLAG);
    if (g_adv_config_done == 0) {
      esp_ble_gap_start_advertising(&adv_params);
    }
    break;

  case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    g_adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
    if (g_adv_config_done == 0) {
      esp_ble_gap_start_advertising(&adv_params);
    }
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Advertising start failed, status=%d",
               param->adv_start_cmpl.status);
    } else {
      ESP_LOGI(TAG, "Advertising started successfully");
    }
    break;

  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Advertising stop failed");
    } else {
      ESP_LOGI(TAG, "Advertising stopped");
    }
    break;

  case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    ESP_LOGI(TAG, "Conn params updated: int=%d, latency=%d, timeout=%d",
             param->update_conn_params.conn_int,
             param->update_conn_params.latency,
             param->update_conn_params.timeout);
    break;

  case ESP_GAP_BLE_SEC_REQ_EVT:
    /* Accept the security request (pairing) from the peer */
    ESP_LOGI(TAG, "Security request from peer");
    esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    break;

  case ESP_GAP_BLE_NC_REQ_EVT:
    /* Numeric Comparison: auto-confirm (Just Works) */
    ESP_LOGI(TAG, "NC request, passkey: %" PRIu32,
             param->ble_security.key_notif.passkey);
    esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
    break;

  case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
    ESP_LOGI(TAG, "Passkey notify: %" PRIu32,
             param->ble_security.key_notif.passkey);
    break;

  case ESP_GAP_BLE_AUTH_CMPL_EVT: {
    esp_bd_addr_t bd_addr;
    memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr,
           sizeof(esp_bd_addr_t));
    ESP_LOGI(TAG,
             "Auth complete: addr=%02x:%02x:%02x:%02x:%02x:%02x, success=%d",
             bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4],
             bd_addr[5], param->ble_security.auth_cmpl.success);
    if (!param->ble_security.auth_cmpl.success) {
      ESP_LOGW(TAG, "Auth failed, reason=0x%x",
               param->ble_security.auth_cmpl.fail_reason);
    }
    break;
  }

  default:
    ESP_LOGD(TAG, "GAP event: %d", event);
    break;
  }
}

/* ---------- GATTS Profile Event Handler ---------- */

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT: {
    ESP_LOGI(TAG, "GATTS_REG_EVT, status=%d, app_id=%d", param->reg.status,
             param->reg.app_id);

    /* Set device name based on MAC */
    const uint8_t *mac = esp_bt_dev_get_address();
    if (mac) {
      snprintf(g_device_name, sizeof(g_device_name), "RC DIY #%02X%02X", mac[4],
               mac[5]);
    }
    esp_ble_gap_set_device_name(g_device_name);
    ESP_LOGI(TAG, "Device name: %s", g_device_name);

    /* Config advertising data */
    esp_ble_gap_config_adv_data(&adv_data);
    g_adv_config_done |= ADV_CONFIG_FLAG;

    esp_ble_gap_config_adv_data(&scan_rsp_data);
    g_adv_config_done |= SCAN_RSP_CONFIG_FLAG;

    /* Create attribute table */
    esp_err_t ret = esp_ble_gatts_create_attr_tab(rc_gatt_db, gatts_if,
                                                  RC_IDX_NB, SVC_INST_ID);
    if (ret) {
      ESP_LOGE(TAG, "Create attr table failed: 0x%x", ret);
    }
    break;
  }

  case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
    if (param->add_attr_tab.status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "Create attr table failed, error=0x%x",
               param->add_attr_tab.status);
    } else if (param->add_attr_tab.num_handle != RC_IDX_NB) {
      ESP_LOGE(TAG, "Attr table handle count mismatch: %d vs %d",
               param->add_attr_tab.num_handle, RC_IDX_NB);
    } else {
      ESP_LOGI(TAG, "Attr table created, %d handles",
               param->add_attr_tab.num_handle);
      memcpy(g_gatt_handle_table, param->add_attr_tab.handles,
             sizeof(g_gatt_handle_table));
      esp_ble_gatts_start_service(g_gatt_handle_table[RC_IDX_SVC]);
    }
    break;
  }

  case ESP_GATTS_CONNECT_EVT: {
    ESP_LOGI(TAG, "Client connected, conn_id=%d", param->connect.conn_id);
    g_conn_id = param->connect.conn_id;
    g_connected = true;
    g_gatts_if = gatts_if;

    /* Reset data on new connection */
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      g_lap_time_ms = 0;
      g_lap_time_valid = false;
      g_delta_ms = 0;
      g_delta_valid = false;
      xSemaphoreGive(g_data_mutex);
    }
    break;
  }

  case ESP_GATTS_DISCONNECT_EVT:
    ESP_LOGI(TAG, "Client disconnected, reason=0x%x", param->disconnect.reason);
    g_connected = false;

    /* Reset data */
    if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      g_lap_time_valid = false;
      g_delta_valid = false;
      xSemaphoreGive(g_data_mutex);
    }

    /* Restart advertising */
    esp_ble_gap_start_advertising(&adv_params);
    break;

  case ESP_GATTS_WRITE_EVT: {
    ESP_LOGI(TAG, "WRITE_EVT: handle=0x%x, len=%d, is_prep=%d",
             param->write.handle, param->write.len, param->write.is_prep);

    if (!param->write.is_prep) {
      /* Check which characteristic was written to */
      if (param->write.handle == g_gatt_handle_table[RC_IDX_CHAR_VAL_NOTIFY]) {
        /* Data notification from RaceChrono */
        rc_parse_notification(param->write.value, param->write.len);
      } else if (param->write.handle ==
                 g_gatt_handle_table[RC_IDX_CHAR_VAL_CANBUS_FILTER]) {
        /* CAN-Bus filter command from RaceChrono */
        ESP_LOGI(TAG, "CAN filter write received, len=%d", param->write.len);
        ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
      } else if (param->write.handle ==
                 g_gatt_handle_table[RC_IDX_CHAR_VAL_CONFIG]) {
        /* Monitor configuration from RaceChrono */
        ESP_LOGI(TAG, "Config write received, len=%d", param->write.len);
        ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
      } else if (param->write.handle ==
                 g_gatt_handle_table[RC_IDX_CHAR_CCC_CONFIG]) {
        /* CCC descriptor write (enable/disable indications) */
        uint16_t descr_value =
            param->write.value[1] << 8 | param->write.value[0];
        if (descr_value == 0x0002) {
          ESP_LOGI(TAG, "Client enabled indications on config char");
        } else if (descr_value == 0x0000) {
          ESP_LOGI(TAG, "Client disabled indications on config char");
        }
      }

      /* Send response if needed */
      if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                    param->write.trans_id, ESP_GATT_OK, NULL);
      }
    }
    break;
  }

  case ESP_GATTS_READ_EVT:
    ESP_LOGI(TAG, "READ_EVT: handle=0x%x", param->read.handle);
    /* Send an empty response for reads */
    {
      esp_gatt_rsp_t rsp;
      memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
      rsp.attr_value.handle = param->read.handle;
      rsp.attr_value.len = 0;
      esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                  param->read.trans_id, ESP_GATT_OK, &rsp);
    }
    break;

  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(TAG, "MTU set to %d", param->mtu.mtu);
    break;

  case ESP_GATTS_START_EVT:
    ESP_LOGI(TAG, "Service started, status=%d, handle=0x%x",
             param->start.status, param->start.service_handle);
    break;

  default:
    ESP_LOGI(TAG, "GATTS event: %d", event);
    break;
  }
}

/* ---------- GATTS Event Dispatcher ---------- */

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
  ESP_LOGD(TAG, "GATTS event: %d, gatts_if=%d", event, gatts_if);

  if (event == ESP_GATTS_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      gl_profile.gatts_if = gatts_if;
    } else {
      ESP_LOGE(TAG, "Reg app failed, app_id=%04x, status=%d", param->reg.app_id,
               param->reg.status);
      return;
    }
  }

  if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile.gatts_if) {
    if (gl_profile.gatts_cb) {
      gl_profile.gatts_cb(event, gatts_if, param);
    }
  }
}

/* ---------- Public API ---------- */

esp_err_t ble_manager_init(void) {
  esp_err_t ret;

  /* Create data mutex */
  g_data_mutex = xSemaphoreCreateMutex();
  if (!g_data_mutex) {
    ESP_LOGE(TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  /* Initialize NVS (required by BT) */
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /* Connect to the ESP-Hosted co-processor (resets C6 slave via GPIO 54,
   * initializes SDIO card, and performs the transport handshake). */
  ESP_LOGI(TAG, "Connecting to ESP-Hosted slave...");
  esp_hosted_connect_to_slave();

  /* Verify connection by fetching co-processor firmware version */
  esp_hosted_coprocessor_fwver_t fwver;
  if (ESP_OK == esp_hosted_get_coprocessor_fwversion(&fwver)) {
    ESP_LOGI(TAG, "Co-processor FW Version: %" PRIu32 ".%" PRIu32 ".%" PRIu32,
             fwver.major1, fwver.minor1, fwver.patch1);
  } else {
    ESP_LOGW(TAG, "Failed to get co-processor FW version");
  }

  /* Initialize BT controller on co-processor */
  ret = esp_hosted_bt_controller_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
    return ESP_FAIL;
  }

  ret = esp_hosted_bt_controller_enable();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable BT controller: %s", esp_err_to_name(ret));
    return ESP_FAIL;
  }

  /* Open Bluedroid HCI channel */
  hosted_hci_bluedroid_open();

  /* Attach HCI driver operations to Bluedroid */
  esp_bluedroid_hci_driver_operations_t operations = {
      .send = hosted_hci_bluedroid_send,
      .check_send_available = hosted_hci_bluedroid_check_send_available,
      .register_host_callback = hosted_hci_bluedroid_register_host_callback,
  };
  esp_bluedroid_attach_hci_driver(&operations);

  /* Initialize Bluedroid */
  ret = esp_bluedroid_init();
  if (ret) {
    ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_bluedroid_enable();
  if (ret) {
    ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Register callbacks */
  ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret) {
    ESP_LOGE(TAG, "GATTS register callback failed: 0x%x", ret);
    return ret;
  }

  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret) {
    ESP_LOGE(TAG, "GAP register callback failed: 0x%x", ret);
    return ret;
  }

  /* Register application profile */
  ret = esp_ble_gatts_app_register(ESP_APP_ID);
  if (ret) {
    ESP_LOGE(TAG, "GATTS app register failed: 0x%x", ret);
    return ret;
  }

  /* Set MTU - RaceChrono typically uses small packets */
  esp_ble_gatt_set_local_mtu(64);

  /* No BLE security/pairing — RaceChrono DIY devices don't require it */

  ESP_LOGI(TAG, "BLE Manager initialized successfully");
  return ESP_OK;
}

bool ble_manager_is_connected(void) { return g_connected; }

void ble_manager_get_lap_time(int32_t *val_ms, bool *valid) {
  if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    *val_ms = g_lap_time_ms;
    *valid = g_lap_time_valid;
    xSemaphoreGive(g_data_mutex);
  } else {
    *val_ms = 0;
    *valid = false;
  }
}

void ble_manager_get_delta(int32_t *val_ms, bool *valid) {
  if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    *val_ms = g_delta_ms;
    *valid = g_delta_valid;
    xSemaphoreGive(g_data_mutex);
  } else {
    *val_ms = 0;
    *valid = false;
  }
}
