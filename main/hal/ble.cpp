// BLE GATT server for Tab5 — NimBLE via esp_hosted C6 coprocessor
// Wraps NimBLE C API in extern "C" blocks to avoid C++ issues

#include "hal/hal.h"
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
}

static const char *TAG = "hal:ble";

#define SVC_UUID        0xFF10
#define CHR_CMD_UUID    0xFF11
#define CHR_RESP_UUID   0xFF12
#define CHR_STATUS_UUID 0xFF13

static uint16_t conn_handle_ = 0;
static bool connected_ = false;
static uint16_t resp_chr_handle_ = 0;
static hal::command_handler_t cmd_handler_ = nullptr;

// Static UUID storage (avoids C++ rvalue issues with BLE_UUID16_DECLARE)
static ble_uuid16_t svc_uuid   = BLE_UUID16_INIT(SVC_UUID);
static ble_uuid16_t cmd_uuid   = BLE_UUID16_INIT(CHR_CMD_UUID);
static ble_uuid16_t resp_uuid  = BLE_UUID16_INIT(CHR_RESP_UUID);
static ble_uuid16_t stat_uuid  = BLE_UUID16_INIT(CHR_STATUS_UUID);

// Forward declarations
static int gap_event(struct ble_gap_event *event, void *arg);

extern "C" {

static int cmd_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0 || len > 4096) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char *buf = (char *)malloc(len + 1);
    if (!buf) return BLE_ATT_ERR_INSUFFICIENT_RES;

    ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);
    buf[len] = '\0';

    ESP_LOGI(TAG, "Command received (%d bytes)", len);
    if (cmd_handler_) cmd_handler_(buf, len);

    free(buf);
    return 0;
}

static int resp_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return 0;
}

static int status_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char *s = connected_ ? "connected" : "advertising";
        return os_mbuf_append(ctxt->om, s, strlen(s)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

} // extern "C"

// GATT service — defined as C-compatible structs
static struct ble_gatt_chr_def chrs[] = {
    {   // Command (write)
        .uuid = &cmd_uuid.u,
        .access_cb = cmd_chr_access,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
    {   // Response (notify)
        .uuid = &resp_uuid.u,
        .access_cb = resp_chr_access,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &resp_chr_handle_,
        .cpfd = NULL,
    },
    {   // Status (read)
        .uuid = &stat_uuid.u,
        .access_cb = status_chr_access,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
    { 0 }, // terminator
};

static struct ble_gatt_svc_def svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .includes = NULL,
        .characteristics = chrs,
    },
    { 0 },
};

// --- GAP ---

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = &svc_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ESP_LOGI(TAG, "Advertising as '%s'", name);
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                      &adv_params, gap_event, NULL);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                conn_handle_ = event->connect.conn_handle;
                connected_ = true;
                ESP_LOGI(TAG, "Connected (handle=%d)", conn_handle_);
                ble_att_set_preferred_mtu(512);
            } else {
                start_advertising();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected");
            connected_ = false;
            conn_handle_ = 0;
            start_advertising();
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU: %d", event->mtu.value);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;
        default:
            break;
    }
    return 0;
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, NULL);
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset: %d", reason);
}

// --- Public API ---

void hal::ble_init()
{
    // Power on C6 coprocessor
    bsp_feature_enable(BSP_FEATURE_WIFI, true);
    vTaskDelay(pdMS_TO_TICKS(200));

    int rc = nimble_port_init();
    assert(rc == 0);

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(svcs);
    assert(rc == 0);

    ble_svc_gap_device_name_set("Tab5");

    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "BLE initialized");
}

bool hal::ble_is_connected()
{
    return connected_;
}

void hal::ble_notify(const char *json)
{
    if (!connected_) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(json, strlen(json));
    if (om) {
        int rc = ble_gatts_notify_custom(conn_handle_, resp_chr_handle_, om);
        if (rc != 0) ESP_LOGW(TAG, "Notify failed: %d", rc);
    }
}

void hal::ble_set_command_handler(command_handler_t handler)
{
    cmd_handler_ = handler;
}
