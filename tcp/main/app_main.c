/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>


#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "ahtxx.h" 



static const char *TAG = "Mqtt_AHT20";
#define PUSH_BUTTON_PIN  34


#define CONFIG_APP_TAG    "AHT20 [APP]"

static esp_mqtt_client_handle_t client;  // Handle global du client MQTT

// configuration de I2C

#define CONFIG_I2C_0_PORT               I2C_NUM_0   
#define CONFIG_I2C_0_SDA_IO             (gpio_num_t)(21) // blue 21
#define CONFIG_I2C_0_SCL_IO             (gpio_num_t)(22) // yellow 22
//
#define CONFIG_I2C_0_TASK_NAME          "i2c_0_tsk"   //Nom de la tâche qui gère l'I2C.
#define CONFIG_I2C_0_TASK_STACK_SIZE    (configMINIMAL_STACK_SIZE * 4)  //Taille de la pile allouée à la tâche. Ici, la pile est multipliée par 4 par rapport à la taille minimale.
//Priorité de la tâche I2C dans le système. Plus le nombre est bas, plus la priorité est élevée. Ici, c'est un peu plus élevé que la priorité de la tâche la plus basse.
#define CONFIG_I2C_0_TASK_PRIORITY      (tskIDLE_PRIORITY + 2)  

// Global variables to store sensor and button state

float ta = NAN, rh = NAN;  // Temperature and Humidity values
int button_state = 0;  // Button state (0 or 1)

// configure le bus I2C maître avec certains paramètres par défaut
#define CONFIG_I2C_0_MASTER_DEFAULT {                               \
        .clk_source                     = I2C_CLK_SRC_DEFAULT,      \
        .i2c_port                       = CONFIG_I2C_0_PORT,        \
        .scl_io_num                     = CONFIG_I2C_0_SCL_IO,      \
        .sda_io_num                     = CONFIG_I2C_0_SDA_IO,      \
        .glitch_ignore_cnt              = 7,                        \
        .flags.enable_internal_pullup   = true, }

// fonction permet de créer un délai en millisecondes pour la tâche. 
static inline void vTaskDelayMs(const uint ms) {
    const TickType_t xDelay = (ms / portTICK_PERIOD_MS);
    vTaskDelay( xDelay );
}


//Cette fonction crée un délai jusqu'à une période définie (en secondes) depuis le dernier réveil de la tâche.
static inline void vTaskDelaySecUntil(TickType_t *previousWakeTime, const uint sec) {
    const TickType_t xFrequency = ((sec * 1000) / portTICK_PERIOD_MS);
    vTaskDelayUntil( previousWakeTime, xFrequency );  
}

//fonction principale de capteur
static void i2c_0_task( void *pvParameters ) {
    TickType_t                  xLastWakeTime;
    float                       ta_max = NAN, ta_min = NAN;
    char                        deg_char = 176;
    float ta_temp, td, rh_temp;
    // initialize the xLastWakeTime variable with the current time
    xLastWakeTime               = xTaskGetTickCount ();
    // initialize master i2c 0 bus configuration
    i2c_master_bus_config_t     i2c0_master_cfg = CONFIG_I2C_0_MASTER_DEFAULT;
    i2c_master_bus_handle_t     i2c0_bus_hdl;
    // initialize ahtxx i2c device configuration
    i2c_ahtxx_config_t          ahtxx_dev_cfg = I2C_AHT2X_CONFIG_DEFAULT;
    i2c_ahtxx_handle_t          ahtxx_dev_hdl;
    // instantiate i2c 0 master bus
    i2c_new_master_bus(&i2c0_master_cfg, &i2c0_bus_hdl);
    if (i2c0_bus_hdl == NULL) ESP_LOGE(CONFIG_APP_TAG, "i2c0 i2c_bus_create handle init failed");
    // ahtxx init device
    i2c_ahtxx_init(i2c0_bus_hdl, &ahtxx_dev_cfg, &ahtxx_dev_hdl);
    if (ahtxx_dev_hdl == NULL) ESP_LOGE(CONFIG_APP_TAG, "i2c0 i2c_bus_device_create ahtxx handle init failed");
    //
    ESP_LOGI(CONFIG_APP_TAG, "Status Register: 0x%02x (0b%s)", ahtxx_dev_hdl->status_reg.reg, uint8_to_binary(ahtxx_dev_hdl->status_reg.reg));

    // task loop entry point
    /*for ( ;; ) {
        ESP_LOGI(CONFIG_APP_TAG, "######################## AHTXX - START #########################");  

        float ta; float td; float rh;
        if(i2c_ahtxx_get_measurements(ahtxx_dev_hdl, &ta, &rh, &td) != 0) {
            ESP_LOGI(CONFIG_APP_TAG, "i2c_ahtxx_get_measurements failed");
        } else {
            // process min and max ta since reboot

            char message[100];  // A buffer to hold the formatted st
            snprintf(message, sizeof(message), "{\"temp\": \"%.2f\", \"humidity\": \"%.2f\"}", ta, rh);
            esp_mqtt_client_publish(client, "test_asma/Temp_Hum", message, 0, 1, 0);

            if(isnan(ta_max) && isnan(ta_min)) {
                // initialize
                ta_max = ta;
                ta_min = ta;
            }         

            ESP_LOGI(CONFIG_APP_TAG, "temp:         %.2f%cC", ta, deg_char);
            ESP_LOGI(CONFIG_APP_TAG, "humidity:    %.2f %%", rh);
        }       
        ESP_LOGI(CONFIG_APP_TAG, "######################## AHTXX - END ###########################");
        // pause the task per defined wait period
        vTaskDelaySecUntil( &xLastWakeTime, 1);*/

           for (;;) {
        if (i2c_ahtxx_get_measurements(ahtxx_dev_hdl, &ta_temp, &rh_temp, &td) != 0) {
            ESP_LOGI(CONFIG_APP_TAG, "i2c_ahtxx_get_measurements failed");
        } else {
            ESP_LOGI(TAG, "Measurement successful: Temp = %.2f, Humidity = %.2f", ta_temp, rh_temp);
            ta = ta_temp;  // Update global temperature
            rh = rh_temp;  // Update global humidity
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay to prevent excessive sensor reads
    }
}
    
    
/*
    // free up task resources and remove task from stack
    i2c_ahtxx_rm( ahtxx_dev_hdl );      //remove aht2x device from master i2c bus
    i2c_del_master_bus( i2c0_bus_hdl ); //delete master i2c bus
    vTaskDelete( NULL );
}*/


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:



         char message[100];
        snprintf(message, sizeof(message), "{\"temp\": \"%.2f\", \"humidity\": \"%.2f\"}", ta, rh);
        msg_id = esp_mqtt_client_publish(client, "test_asma/Temp_Hum", message, 0, 1, 0);
        ESP_LOGI(TAG, "Temperature and humidity published, msg_id=%d", msg_id);
        
        snprintf(message, sizeof(message), "{\"position\": \"%d\"}", button_state);
        msg_id = esp_mqtt_client_publish(client, "test_asma/Position", message, 0, 1, 0);
        ESP_LOGI(TAG, "Button state published, msg_id=%d", msg_id);

        //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        //msg_id = esp_mqtt_client_publish(client, "test_asma/Temp_Hum", "data1", 0, 1, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_subscribe (client, "test_asma/Temp_Hum", 0);
        //ESP_LOGI(TAG, "sent subscribe successfull, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe (client, "test_asma/Temp_Hum", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "test_asma/Temp_Hum");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        //.broker.address.uri = CONFIG_BROKER_URL,
        .broker.address.uri = "mqtt://IP adress:1883",
        .credentials.username = "", // Nom'utilisateur
        .credentials.authentication.password = "", // Mot de passe
        
        };
     

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client); 
}



void pos_button (){
    
    gpio_set_direction(PUSH_BUTTON_PIN, GPIO_MODE_INPUT);
    volatile int pos ; 

     for (;;) {    
        if (gpio_get_level(PUSH_BUTTON_PIN) == 1)
        {  
            ESP_LOGI(TAG, "Active....");   
            pos = 1 ;
            char message[100];  // A buffer to hold the formatted st
            snprintf(message, sizeof(message), "{\"position \": \"%d\"}",pos );
            esp_mqtt_client_publish(client, "test_asma/Position", message, 0, 1, 0);
            
        } 
        else
        {
            ESP_LOGI(TAG, "Relached.....");    
            pos = 0;
            char message[100];  // A buffer to hold the formatted st
            snprintf(message, sizeof(message), "{\"position \": \"%d\"}",pos );
            esp_mqtt_client_publish(client, "Test_asma/Position", message, 0, 1, 0);      
        }

        vTaskDelay(1000);
    
     }
}




static void read_button_state(void *pvParameters) {
    
    gpio_set_direction(PUSH_BUTTON_PIN, GPIO_MODE_INPUT);
    
    for (;;) {
        ESP_LOGI(TAG, "read_button_state...... ");
        button_state = gpio_get_level(PUSH_BUTTON_PIN);  // Update global button state
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay to avoid excessive reads
    }
}



void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);

    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());



    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // Initialiser le capteur AHT20

    //mqtt_app_start();
 

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(CONFIG_APP_TAG, ESP_LOG_VERBOSE);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
     {
      ESP_ERROR_CHECK( nvs_flash_erase() );
      ret = nvs_flash_init();
     }
    ESP_ERROR_CHECK( ret );

    mqtt_app_start();
    //pos_button();
    
      
    // Create tasks for reading sensor data and button state
    //xTaskCreate(i2c_0_task, "read_sensor_data", 2048, NULL, 5, NULL);
    read_button_state() ;
    i2c_0_task();
    

    /*xTaskCreatePinnedToCore( 
        i2c_0_task, 
        CONFIG_I2C_0_TASK_NAME, 
        CONFIG_I2C_0_TASK_STACK_SIZE, 
        NULL, 
        CONFIG_I2C_0_TASK_PRIORITY, 
        NULL, 
        APP_CPU_NUM );*/
        
    
  
}
