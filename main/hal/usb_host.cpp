#include "hal/hal.h"
#include "bsp/m5stack_tab5.h"
#include "usb/usb_host.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char *TAG = "hal:usb";

static usb_host_client_handle_t client_ = nullptr;
static bool initialized_ = false;
static SemaphoreHandle_t transfer_done_ = nullptr;
static usb_transfer_status_t last_xfer_status_;
static int last_xfer_actual_ = 0;

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New device connected (addr=%d)", msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "Device disconnected (addr=%d)", msg->dev_gone.dev_hdl ? 1 : 0);
            break;
        default:
            break;
    }
}

static void client_task(void *arg)
{
    while (initialized_) {
        usb_host_client_handle_events(client_, pdMS_TO_TICKS(500));
    }
    vTaskDelete(NULL);
}

void hal::usb_init()
{
    if (initialized_) return;

    // Start USB host via BSP (powers USB port + installs host library)
    esp_err_t ret = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_usb_host_start failed: %d", ret);
        return;
    }

    // Register a client
    usb_host_client_config_t client_cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = NULL,
        },
    };

    ret = usb_host_client_register(&client_cfg, &client_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb_host_client_register failed: %d", ret);
        return;
    }

    transfer_done_ = xSemaphoreCreateBinary();
    initialized_ = true;

    xTaskCreate(client_task, "usb_client", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "USB host initialized");
}

int hal::usb_list_json(char *buf, size_t len)
{
    if (!initialized_) {
        snprintf(buf, len, "{\"error\":\"USB host not initialized\"}");
        return -1;
    }

    uint8_t addr_list[10];
    int num_devs = 0;
    usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs);

    int pos = snprintf(buf, len, "{\"ok\":true,\"count\":%d,\"devices\":[", num_devs);

    for (int i = 0; i < num_devs && pos < (int)len - 200; i++) {
        usb_device_handle_t dev;
        if (usb_host_device_open(client_, addr_list[i], &dev) != ESP_OK) continue;

        usb_device_info_t info;
        if (usb_host_device_info(dev, &info) == ESP_OK) {
            const usb_device_desc_t *desc = NULL;
            usb_host_get_device_descriptor(dev, &desc);

            if (i > 0) pos += snprintf(buf + pos, len - pos, ",");
            pos += snprintf(buf + pos, len - pos,
                "{\"addr\":%d,\"speed\":%d,\"vid\":\"0x%04x\",\"pid\":\"0x%04x\","
                "\"class\":%d,\"subclass\":%d,\"protocol\":%d,"
                "\"manufacturer\":%d,\"product\":%d,\"serial\":%d}",
                addr_list[i], info.speed,
                desc ? desc->idVendor : 0, desc ? desc->idProduct : 0,
                desc ? desc->bDeviceClass : 0, desc ? desc->bDeviceSubClass : 0,
                desc ? desc->bDeviceProtocol : 0,
                desc ? desc->iManufacturer : 0, desc ? desc->iProduct : 0,
                desc ? desc->iSerialNumber : 0);
        }

        usb_host_device_close(client_, dev);
    }

    snprintf(buf + pos, len - pos, "]}");
    return num_devs;
}

int hal::usb_info_json(uint8_t addr, char *buf, size_t len)
{
    if (!initialized_) {
        snprintf(buf, len, "{\"error\":\"USB host not initialized\"}");
        return -1;
    }

    usb_device_handle_t dev;
    if (usb_host_device_open(client_, addr, &dev) != ESP_OK) {
        snprintf(buf, len, "{\"error\":\"device not found at addr %d\"}", addr);
        return -1;
    }

    const usb_device_desc_t *desc = NULL;
    usb_host_get_device_descriptor(dev, &desc);

    const usb_config_desc_t *cfg_desc = NULL;
    usb_host_get_active_config_descriptor(dev, &cfg_desc);

    int pos = snprintf(buf, len,
        "{\"ok\":true,\"addr\":%d", addr);

    if (desc) {
        pos += snprintf(buf + pos, len - pos,
            ",\"device\":{\"usb\":\"%d.%d\",\"vid\":\"0x%04x\",\"pid\":\"0x%04x\","
            "\"class\":%d,\"subclass\":%d,\"protocol\":%d,"
            "\"max_packet\":%d,\"num_configs\":%d}",
            desc->bcdUSB >> 8, (desc->bcdUSB >> 4) & 0xF,
            desc->idVendor, desc->idProduct,
            desc->bDeviceClass, desc->bDeviceSubClass, desc->bDeviceProtocol,
            desc->bMaxPacketSize0, desc->bNumConfigurations);
    }

    if (cfg_desc) {
        pos += snprintf(buf + pos, len - pos,
            ",\"config\":{\"num_interfaces\":%d,\"max_power_ma\":%d,\"total_length\":%d}",
            cfg_desc->bNumInterfaces, cfg_desc->bMaxPower * 2, cfg_desc->wTotalLength);

        // Walk interfaces
        pos += snprintf(buf + pos, len - pos, ",\"interfaces\":[");
        int offset = 0;
        int iface_count = 0;
        const uint8_t *p = (const uint8_t *)cfg_desc;
        int total = cfg_desc->wTotalLength;

        while (offset < total && pos < (int)len - 150) {
            uint8_t bLength = p[offset];
            uint8_t bDescType = p[offset + 1];
            if (bLength == 0) break;

            if (bDescType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
                const usb_intf_desc_t *intf = (const usb_intf_desc_t *)&p[offset];
                if (iface_count > 0) pos += snprintf(buf + pos, len - pos, ",");
                pos += snprintf(buf + pos, len - pos,
                    "{\"num\":%d,\"alt\":%d,\"class\":%d,\"subclass\":%d,"
                    "\"protocol\":%d,\"endpoints\":%d}",
                    intf->bInterfaceNumber, intf->bAlternateSetting,
                    intf->bInterfaceClass, intf->bInterfaceSubClass,
                    intf->bInterfaceProtocol, intf->bNumEndpoints);
                iface_count++;
            }
            offset += bLength;
        }
        pos += snprintf(buf + pos, len - pos, "]");
    }

    snprintf(buf + pos, len - pos, "}");

    usb_host_device_close(client_, dev);
    return 0;
}

static void xfer_cb(usb_transfer_t *xfer)
{
    last_xfer_status_ = xfer->status;
    last_xfer_actual_ = xfer->actual_num_bytes;
    xSemaphoreGive(transfer_done_);
}

int hal::usb_control_transfer(uint8_t addr, uint8_t bmRequestType, uint8_t bRequest,
                              uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength,
                              char *resp_buf, size_t resp_len)
{
    if (!initialized_) {
        snprintf(resp_buf, resp_len, "{\"error\":\"USB host not initialized\"}");
        return -1;
    }

    usb_device_handle_t dev;
    if (usb_host_device_open(client_, addr, &dev) != ESP_OK) {
        snprintf(resp_buf, resp_len, "{\"error\":\"device not found\"}");
        return -1;
    }

    usb_transfer_t *xfer;
    size_t xfer_size = USB_SETUP_PACKET_SIZE + wLength;
    if (usb_host_transfer_alloc(xfer_size, 0, &xfer) != ESP_OK) {
        usb_host_device_close(client_, dev);
        snprintf(resp_buf, resp_len, "{\"error\":\"transfer alloc failed\"}");
        return -1;
    }

    // Fill setup packet
    usb_setup_packet_t *setup = (usb_setup_packet_t *)xfer->data_buffer;
    setup->bmRequestType = bmRequestType;
    setup->bRequest = bRequest;
    setup->wValue = wValue;
    setup->wIndex = wIndex;
    setup->wLength = wLength;

    // If OUT transfer, copy data after setup packet
    if (!(bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN) && data && wLength > 0) {
        memcpy(xfer->data_buffer + USB_SETUP_PACKET_SIZE, data, wLength);
    }

    xfer->device_handle = dev;
    xfer->num_bytes = xfer_size;
    xfer->callback = xfer_cb;
    xfer->bEndpointAddress = 0;

    esp_err_t ret = usb_host_transfer_submit_control(client_, xfer);
    if (ret != ESP_OK) {
        usb_host_transfer_free(xfer);
        usb_host_device_close(client_, dev);
        snprintf(resp_buf, resp_len, "{\"error\":\"transfer submit failed: %d\"}", ret);
        return -1;
    }

    // Wait for completion
    if (xSemaphoreTake(transfer_done_, pdMS_TO_TICKS(3000)) != pdTRUE) {
        usb_host_transfer_free(xfer);
        usb_host_device_close(client_, dev);
        snprintf(resp_buf, resp_len, "{\"error\":\"transfer timeout\"}");
        return -1;
    }

    if (last_xfer_status_ != USB_TRANSFER_STATUS_COMPLETED) {
        usb_host_transfer_free(xfer);
        usb_host_device_close(client_, dev);
        snprintf(resp_buf, resp_len, "{\"error\":\"transfer failed: %d\"}", last_xfer_status_);
        return -1;
    }

    // Build response
    int actual = last_xfer_actual_;
    int data_len = actual > USB_SETUP_PACKET_SIZE ? actual - USB_SETUP_PACKET_SIZE : 0;

    int pos = snprintf(resp_buf, resp_len, "{\"ok\":true,\"bytes\":%d", data_len);

    if ((bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN) && data_len > 0) {
        pos += snprintf(resp_buf + pos, resp_len - pos, ",\"data\":[");
        uint8_t *resp_data = xfer->data_buffer + USB_SETUP_PACKET_SIZE;
        for (int i = 0; i < data_len && pos < (int)resp_len - 10; i++) {
            if (i > 0) pos += snprintf(resp_buf + pos, resp_len - pos, ",");
            pos += snprintf(resp_buf + pos, resp_len - pos, "%d", resp_data[i]);
        }
        pos += snprintf(resp_buf + pos, resp_len - pos, "]");
    }

    snprintf(resp_buf + pos, resp_len - pos, "}");

    usb_host_transfer_free(xfer);
    usb_host_device_close(client_, dev);
    return data_len;
}
