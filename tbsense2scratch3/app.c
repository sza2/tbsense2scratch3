/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "em_common.h"
#include "sl_app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_timer.h"
#include "sl_sensor_imu.h"
#include "sl_sensor_rht.h"

#include "brd4166a/board.h"

#define CMD_LED_CONTROL 0x01
#define CMD_LED_CONTROL_LENGTH 3
#define CMD_LED_COLOR 0x02
#define CMD_LED_COLOR_LENGTH 4

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static uint8_t rx_connection = 0;

#define RX_INTERVAL_MS 50
static sl_simple_timer_t rx_timer;

#define CHAR_RX_SIZE 21
uint8_t rx_array[CHAR_RX_SIZE];

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
//  sl_app_log("Scratch TB Sense 2\n");
  rgb_led_init();
  rgb_led_set(0x00, 0, 0, 0);
  sl_sensor_imu_init();
  sl_sensor_imu_enable(true);
  sl_sensor_rht_init();
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

static void rx_notify(void)
{
  sl_status_t sc;
  sc = sl_bt_gatt_server_send_characteristic_notification(
    rx_connection,
    gattdb_rx,
    CHAR_RX_SIZE,
    rx_array,
    NULL);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to send characteristic notification\n",
                (int)sc);
}

bool rx_imu_update(void)
{
  static int16_t orientation_current[3];
  static int16_t acceleration_current[3];
  static int16_t acceleration_previous[3];
  bool value_changed = false;
  sl_status_t sc = sl_sensor_imu_get(orientation_current, acceleration_current);
  if (sc == SL_STATUS_OK) {
    // decrease sensitivity otherwise the update is too fast
    acceleration_current[0] = (acceleration_current[0] + 0x0020) & 0xffc0;
    acceleration_current[1] = (acceleration_current[1] + 0x0020) & 0xffc0;
    rx_array[0] = acceleration_current[1] >> 8;
    rx_array[1] = acceleration_current[1] & 0xff;
    rx_array[2] = acceleration_current[0] >> 8;
    rx_array[3] = acceleration_current[0] & 0xff;
    if (acceleration_current[0] != acceleration_previous[0] ||
        acceleration_current[1] != acceleration_previous[1]) {
        value_changed = true;
    }
    acceleration_previous[0] = acceleration_current[0];
    acceleration_previous[1] = acceleration_current[1];
  }
  return value_changed;
}

void rx_rht_update(void)
{
  uint32_t relative_humidity;
  int32_t temperature;
  sl_sensor_rht_get(&relative_humidity, &temperature);
  // MSB first, to follow micro:bit format
  rx_array[6] = (temperature >> 8) & 0xff;
  rx_array[7] = temperature & 0xff;
  rx_array[8] = (relative_humidity >> 8) & 0xff;
  rx_array[9] = relative_humidity & 0xff;
}

static void rx_timer_cb(sl_simple_timer_t *timer, void *data)
{
  static uint8_t counter_imu = 0;
  static uint8_t counter_rht = 0;
  (void)data;
  (void)timer;
  // counters a separated to make it possible to set different update interval
  if (rx_imu_update() || counter_imu > 20 || counter_rht > 20) {
    rx_rht_update();
    rx_notify();
    counter_imu = 0;
    counter_rht = 0;
  }

  counter_imu++;
  counter_rht++;
}

static void rx_read_cb(sl_bt_evt_gatt_server_user_read_request_t *data)
{
  sl_status_t sc;
  rx_imu_update();
  sc = sl_bt_gatt_server_send_user_read_response(
    data->connection,
    data->characteristic,
    0,
    CHAR_RX_SIZE,
    rx_array,
    NULL);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to send user read response\n",
                (int)sc);
}


static void rx_changed_cb(sl_bt_evt_gatt_server_characteristic_status_t *data)
{
  sl_status_t sc;
  bool enable = gatt_disable != data->client_config_flags;
  rx_connection = data->connection;
  if (enable) {
    rx_notify();
    sc = sl_simple_timer_start(&rx_timer,
                                   RX_INTERVAL_MS,
                                   rx_timer_cb,
                                   NULL,
                                   true);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to send characteristic notification\n",
                  (int)sc);
  }
  else {
    sc = sl_simple_timer_stop(&rx_timer);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to stop periodic timer\n",
                  (int)sc);

  }
}

static void tx_write_cb(sl_bt_evt_gatt_server_user_write_request_t *data)
{
  sl_status_t sc;
  uint8_t att_errorcode = 0;

  static uint8_t led_rgb_state = 0;
  static uint8_t led_rgb_r = 0x10;
  static uint8_t led_rgb_g = 0x10;
  static uint8_t led_rgb_b = 0x10;

  printf("Write, %d ", data->value.len);
  for(int i = 0; i < data->value.len; i++) {
      printf("%02x ", data->value.data[i]);
  }
  printf("\r\n");
  if (data->value.len < 1) {
    att_errorcode = 0x01; // Invalid Attribute Value Length
  }

  switch (data->value.data[0]) {
    case CMD_LED_CONTROL:
      if (data->value.len != CMD_LED_CONTROL_LENGTH) {
        att_errorcode = 0x01;
        break;
      }
      if (data->value.data[2]) {
        if (data->value.data[1] > 3) {
          led_rgb_state = 0x0f;
        } else {
          led_rgb_state = led_rgb_state | ((1 << data->value.data[1]) & 0x0f);
        }
      } else {
        if (data->value.data[1] > 3) {
          led_rgb_state = 0x00;
        } else {
          led_rgb_state = led_rgb_state & ~((1 << data->value.data[1]) & 0x0f);
        }
      }
      break;
    case CMD_LED_COLOR:
      if (data->value.len != CMD_LED_COLOR_LENGTH) {
        att_errorcode = 0x01;
        break;
      }
      led_rgb_r = data->value.data[1];
      led_rgb_g = data->value.data[2];
      led_rgb_b = data->value.data[3];
      break;
    default:
      break;
  }
  sc = sl_bt_gatt_server_send_user_write_response(data->connection,
                                                  data->characteristic,
                                                  att_errorcode);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to send user write response\n",
                (int)sc);

  if (0 == att_errorcode) {
    rgb_led_set(
        led_rgb_state,
        led_rgb_r,
        led_rgb_g,
        led_rgb_b
  );
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to get Bluetooth address\n",
                    (int)sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to write attribute\n",
                    (int)sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to create advertising set\n",
                    (int)sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising timing\n",
                    (int)sc);
      // Start general advertising and enable connections.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      sc = sl_simple_timer_stop(&rx_timer);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to stop periodic timer\n",
                    (int)sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_gatt_server_characteristic_status_id:
      if ((gatt_server_client_config == (gatt_server_characteristic_status_flag_t)evt->data.evt_gatt_server_characteristic_status.status_flags)
          && (gattdb_rx == evt->data.evt_gatt_server_user_read_request.characteristic)) {
        // client characteristic configuration changed by remote GATT client
        rx_changed_cb(&evt->data.evt_gatt_server_characteristic_status);
      }
      break;
    case sl_bt_evt_gatt_server_user_read_request_id:
      if (gattdb_rx == evt->data.evt_gatt_server_user_read_request.characteristic) {
        rx_read_cb(&evt->data.evt_gatt_server_user_read_request);
      }
      break;
    case sl_bt_evt_gatt_server_user_write_request_id:
      if (gattdb_tx == evt->data.evt_gatt_server_user_write_request.characteristic) {
        tx_write_cb(&evt->data.evt_gatt_server_user_write_request);
      }
      break;
    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Push button event handler

void sl_button_on_change(const sl_button_t *handle)
{
  (void)handle;
  if (handle == &sl_button_btn0) {
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      rx_array[4] = 1;
    }
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED) {
      rx_array[4] = 0;
    }
  }
  if (handle == &sl_button_btn1) {
      if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
        rx_array[5] = 1;
      }
      if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED) {
        rx_array[5] = 0;
      }
  }
  rx_notify();
}
