#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <PubSubClient.h>
#include <hal/adc_types.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "driver/adc.h"
#include <driver/i2s.h>
#include "freertos/semphr.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"


#define mqtt_port 1883
#define MQTT_SERIAL_PUBLISH_CH "sensor_data/power_data"
#define RED_LED GPIO_NUM_4
#define GREEN_LED GPIO_NUM_7
#define BLUE_LED GPIO_NUM_6
#define CURRENT_SENSOR_PIN GPIO_NUM_2
#define LONG_PRESS_DURATION 1000
#define MIDPOINT_VOLTAGE 2047
#define ADC_INPUT ADC1_CHANNEL_2 
#define BUTTON_PIN GPIO_NUM_12


//I2S
#define ADC_BUFFER_SIZE 1024
#define ADC_FRAME_SIZE  256

#define SEM_WAIT_TIME 50  // Ticks

// Use two buffers so we don't have to stop the ADC while printing out a frame:
uint8_t sample_data_uint8_a[ADC_FRAME_SIZE] = {0};
uint8_t sample_data_uint8_b[ADC_FRAME_SIZE] = {0};
uint8_t *p_store_buffer = sample_data_uint8_a;
uint8_t *p_print_buffer = sample_data_uint8_b;
static SemaphoreHandle_t l_buffer_mutex;
static TaskHandle_t l_store_task_handle;
static TaskHandle_t l_main_task_handle;
static adc_continuous_handle_t l_adc_handle = NULL;

#define ENSURE_TRUE(ACTION)           \
    do                                \
    {                                 \
        BaseType_t __res = (ACTION);  \
        assert(__res == pdTRUE);      \
        (void)__res;                  \
    } while (0)

static void adc_store_task(void *arg);


// =============== Conversion Done Callback: ========================================================
static bool IRAM_ATTR adc_conversion_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t must_yield = pdFALSE;

    // Notify "store" task that a frame has finished.
    vTaskNotifyGiveFromISR(l_store_task_handle, &must_yield);

    return (must_yield == pdTRUE);
}


// ================ Ring buffer overflow callback: ==================================================
static bool IRAM_ATTR adc_pool_overflow_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    // OVERFLOW! We don't expect this to happen, so generate exception.
    assert(false);

    return false; // Ssouldn't get to here.
}


// ========== Configure the ADC in continuous mode: ============
void adc_init(void)
{
    adc_continuous_handle_cfg_t continuous_handle_config =
    {
        .max_store_buf_size = ADC_BUFFER_SIZE,
        .conv_frame_size = ADC_FRAME_SIZE,
    };

    ESP_ERROR_CHECK( adc_continuous_new_handle(&continuous_handle_config, &l_adc_handle) );

    adc_digi_pattern_config_t pattern_config =
    {
        .unit = ADC_UNIT_1,
        .channel = ADC_CHANNEL_0,
        .bit_width = 12,
        .atten = ADC_ATTEN_DB_11,
    };

    adc_continuous_config_t adc_config =
    {
        .pattern_num = 1,
        .adc_pattern = &pattern_config,

        .sample_freq_hz = 60 * 1000,

        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    ESP_ERROR_CHECK( adc_continuous_config(l_adc_handle, &adc_config) );

    adc_continuous_evt_cbs_t adc_callbacks =
    {
        .on_conv_done = adc_conversion_done_cb,
        .on_pool_ovf = adc_pool_overflow_cb,
    };

    ESP_ERROR_CHECK( adc_continuous_register_event_callbacks(l_adc_handle, &adc_callbacks, NULL) );

    ESP_ERROR_CHECK( adc_continuous_start(l_adc_handle) );
}


// ADC store task:
// Woken up by the callback when a frame is finished.
// Read the frame data from the ADC and store to static buffer.
// This must be done frequently to prevent ring buffer overflow.
static void adc_store_task(void *arg)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        esp_err_t read_result;

        do
        {
            // Read all available frames from the ring buffer (probably only 1):
            ENSURE_TRUE( xSemaphoreTake(l_buffer_mutex, SEM_WAIT_TIME) );

            uint32_t bytes_read = 0;
            read_result = adc_continuous_read(l_adc_handle, p_store_buffer, ADC_FRAME_SIZE, &bytes_read, 0);

            ENSURE_TRUE( xSemaphoreGive(l_buffer_mutex) );

        } while (read_result == ESP_OK); // ESP_OK means more frames may be available

        // Result should be a timeout (no more frames available):
        assert(read_result == ESP_ERR_TIMEOUT);

        // Wake the main task to print out the latest frame:
        xTaskNotifyGive(l_main_task_handle);
    }
}



char ssid[] = "HUAWEI-5GCPE-025C";
const char *password = "YBABM6ARH3A";
const char *mqtt_server = "178.79.181.102";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const bool wifi_connected = false;
unsigned long lastPressTime = 0;

long readingsSum = 0;
float rms = 0;
float rmsAve = 0;
uint16_t rmsBatches = 0;
float rmsSum = 0;



float average(int *array, int len)
{
  int sum = 0;
  for (int i = 0; i < len; i++)
    sum += array[i];
  return int(((float)sum) / len);
}

// void calculateCurrent(void *pvParameters)
// {

//   while (1)
//   {
//     size_t bytesRead = 0;
//     i2s_read(I2S_PORT,
//              (void *)samples,
//              sizeof(samples),
//              &bytesRead,
//              portMAX_DELAY);

//     if (bytesRead != sizeof(samples))
//     {
//       continue;
//     }
//     for (uint16_t i = 0; i < ARRAYSIZE(bytesRead); i++)
//     {
//       readingsSum += samples[i] * samples[i];
//     }
//     float rms = sqrt(readingsSum / sizeof(samples));
//     rmsSum += rms;
//     rmsBatches++;
//     vTaskDelay(100 / portTICK_PERIOD_MS);
//   }
// }

// void sendCurrentDraw(double rms)
// {
//   String message = String(rms, 2);
//   client.publish(MQTT_SERIAL_PUBLISH_CH, message.c_str());

// }

void setup_wifi()
{
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32But"))
    {
      client.subscribe("sensor_data/power_data/outbound");
    }
    else
    {
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  rmsAve = rmsSum / rmsBatches;
  // sendCurrentDraw(rmsAve);
  rmsSum = 0.0;
  readingsSum = 0.0;
  rmsBatches = 0;
}

void mqtt_loop(void *pvParameters)
{
  while (1)
  {
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void led_blink(void *pvParams)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      gpio_set_level(RED_LED, 1);
      vTaskDelay(500 / portTICK_RATE_MS);
      gpio_set_level(RED_LED, 0);
      vTaskDelay(500 / portTICK_RATE_MS);
    }
    else if (!client.connected())
    {
      gpio_set_level(BLUE_LED, 1);
      vTaskDelay(2000 / portTICK_RATE_MS);
      gpio_set_level(BLUE_LED, 0);
      vTaskDelay(2000 / portTICK_RATE_MS);
    }
    else
    {
      gpio_set_level(GREEN_LED, 1);
    }
    // if (longPressDetected)
    // {
    //     // Flash between green and red LED
    //     gpio_set_level(GREEN_LED, 1);
    //     gpio_set_level(RED_LED, 0);
    //     vTaskDelay(500 / portTICK_RATE_MS);
    //     gpio_set_level(GREEN_LED, 0);
    //     gpio_set_level(RED_LED, 1);
    //     vTaskDelay(500 / portTICK_RATE_MS);
    // }
  }
}

// void buttonTask(void *pvParameters)
// {
//   TickType_t pressStartTime = 0;

//   while (1)
//   {
//     // Read the button state
//     bool buttonState = gpio_get_level(BUTTON_PIN) == 0;

//     if (buttonState && !buttonPressed)
//     {
//       // Button is pressed for the first time
//       buttonPressed = true;
//       pressStartTime = xTaskGetTickCount();
//     }
//     else if (!buttonState && buttonPressed)
//     {
//       // Button is released after being pressed

//       // int val = adc1_get_raw(ADC1_CHANNEL_2);
//       // String message = String(val, 2);
//       // client.publish(MQTT_SERIAL_PUBLISH_CH, message.c_str());

//       buttonPressed = false;
//       longPressDetected = (xTaskGetTickCount() - pressStartTime >= pdMS_TO_TICKS(LONG_PRESS_DURATION));
//       pressStartTime = 0;
//     }

//     vTaskDelay(pdMS_TO_TICKS(50)); // Adjust delay as needed
//   }
// }

void setup()
{
  gpio_pad_select_gpio(RED_LED);
  gpio_pad_select_gpio(GREEN_LED);
  gpio_pad_select_gpio(BLUE_LED);
  gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
  gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
  gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);
  gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
  // setupI2S();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  xTaskCreatePinnedToCore(&led_blink, "LED_BLINK", 2048, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(&mqtt_loop, "MQTT_LOOP", 2048, NULL, 5, NULL, 1);
  // xTaskCreatePinnedToCore(&calculateCurrent, "Calculate_Current", 2048, NULL, 5, NULL, 1);
}

void loop()
{

  // if (doublePress)
  // {
  //     // Perform action for double press (calculate midpoint value and store in NVS)
  //     // Calculate midpoint value
  //     float midpoint = average(sample_buff, num_samples);
  //     nvs_handle_t nvs_handle;
  //     nvs_open("storage", NVS_READWRITE, &nvs_handle);
  //     nvs_set_i32(nvs_handle, "midpoint", midpoint);
  //     nvs_commit(nvs_handle);
  //     nvs_close(nvs_handle);
  //     doublePress = false;
  // }
}
