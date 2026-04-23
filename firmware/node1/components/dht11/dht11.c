/**
 * @file dht11.c
 * @author Anthony Yalong
 * @brief DHT11 temperature and humidity sensor driver using bit-banged GPIO timing.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "esp_log.h"
#include "dht11.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "dht11";

// ── Private Function Prototypes ───────────────────────────────────────────────

/**
 * @brief Set GPIO pin as output.
 *
 * @param pin   GPIO number
 */
static inline void dht11_set_output(gpio_num_t pin);

/**
 * @brief Set GPIO pin as input.
 *
 * @param pin   GPIO number
 */
static inline void dht11_set_input(gpio_num_t pin);

/**
 * @brief Send DHT11 start signal to initiate communication.
 *
 * @param pin   GPIO number
 * @return      ESP_OK on success, error code on failure
 */
static esp_err_t dht11_send_start_signal(gpio_num_t pin);

/**
 * @brief Wait for GPIO to reach a specified logic level within a timeout.
 *
 * @param pin           GPIO number
 * @param level         Target logic level (0 or 1)
 * @param timeout_us    Timeout in microseconds
 * @return              ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
static esp_err_t dht11_wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us);

/**
 * @brief Wait for DHT11 response sequence after start signal.
 *
 * @param pin   GPIO number
 * @return      ESP_OK on success, error code on failure
 */
static esp_err_t dht11_wait_for_response(gpio_num_t pin);

/**
 * @brief Read a single data bit from DHT11.
 *
 * @param pin   GPIO number
 * @param bit   Pointer to variable where the bit will be stored (0 or 1)
 * @return      ESP_OK on success, error code on failure
 */
static esp_err_t dht11_read_bit(gpio_num_t pin, uint8_t *bit);

/**
 * @brief Read a single byte (8 bits) from DHT11.
 *
 * @param pin   GPIO number
 * @param byte  Pointer to variable where the byte will be stored
 * @return      ESP_OK on success, error code on failure
 */
static esp_err_t dht11_read_byte(gpio_num_t pin, uint8_t *byte);

/**
 * @brief Read full 5-byte data frame from DHT11.
 *
 * @param pin   GPIO number
 * @param data  Buffer to store 5 bytes of sensor data
 * @return      ESP_OK on success, error code on failure
 */
static esp_err_t dht11_read_data(gpio_num_t pin, uint8_t data[5]);

/**
 * @brief Validate DHT11 checksum.
 *
 * @param data  5-byte data buffer (humidity, temperature, checksum)
 * @return      true if checksum is valid, false otherwise
 */
static bool dht11_validate_checksum(uint8_t data[5]);

// ── Private API ───────────────────────────────────────────────────────────────

static inline void dht11_set_output(gpio_num_t pin) {
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

static inline void dht11_set_input(gpio_num_t pin) {
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

static esp_err_t dht11_send_start_signal(gpio_num_t pin) {
    esp_err_t ret;

    // set output mode
    dht11_set_output(pin);

    // pull line low
    ret = gpio_set_level(pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set level on GPIO %d", (int)pin);
        return ret;
    }

    // hold for at least 18ms
    esp_rom_delay_us(DHT11_START_SIGNAL_LOW_US);

    // release line (go HIGH briefly)
    ret = gpio_set_level(pin, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set level on GPIO %d", (int)pin);
        return ret;
    }

    // switch to input to let sensor drive line
    dht11_set_input(pin);

    return ESP_OK;
}

static esp_err_t dht11_wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us) {
    int64_t start_time = esp_timer_get_time();

    // wait until pin matches target level or timeout occurs
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start_time) > timeout_us) {
            ESP_LOGE(TAG, "timeout waiting for level %d on GPIO %d", level, (int)pin);
            return ESP_ERR_TIMEOUT;
        }
    }

    return ESP_OK;
}

static esp_err_t dht11_wait_for_response(gpio_num_t pin) {
    esp_err_t ret;

    // wait for sensor LOW response
    ret = dht11_wait_for_level(pin, 0, DHT11_TIMEOUT_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "timeout waiting for sensor LOW response");
        return ret;
    }

    // wait for sensor HIGH response
    ret = dht11_wait_for_level(pin, 1, DHT11_TIMEOUT_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "timeout waiting for sensor HIGH response");
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dht11_read_bit(gpio_num_t pin, uint8_t *bit) {
    esp_err_t ret;

    // sanity check
    if (bit == NULL) {
        ESP_LOGE(TAG, "null bit pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // wait for start of bit (LOW ~50us)
    ret = dht11_wait_for_level(pin, 0, DHT11_BIT_TIMEOUT_US);
    if (ret != ESP_OK) return ret;

    // wait for HIGH (bit signal start)
    ret = dht11_wait_for_level(pin, 1, DHT11_BIT_TIMEOUT_US);
    if (ret != ESP_OK) return ret;

    // measure HIGH duration
    int64_t start_time = esp_timer_get_time();

    ret = dht11_wait_for_level(pin, 0, DHT11_BIT_TIMEOUT_US);
    if (ret != ESP_OK) return ret;

    int64_t duration = esp_timer_get_time() - start_time;

    // interpret bit
    *bit = (duration > DHT11_BIT_THRESHOLD_US) ? 1 : 0;

    return ESP_OK;
}

static esp_err_t dht11_read_byte(gpio_num_t pin, uint8_t *byte) {
    esp_err_t ret;
    uint8_t value = 0;

    // sanity check
    if (byte == NULL) {
        ESP_LOGE(TAG, "null byte pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // read 8 bits MSB first
    for (int i = 0; i < 8; i++) {
        uint8_t bit;

        ret = dht11_read_bit(pin, &bit);
        if (ret != ESP_OK) {
            return ret;
        }

        value <<= 1;
        value |= bit;
    }

    *byte = value;
    return ESP_OK;
}

static esp_err_t dht11_read_data(gpio_num_t pin, uint8_t data[5]) {
    esp_err_t ret;

    if (data == NULL) {
        ESP_LOGE(TAG, "null data pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // read 5 bytes: humidity int, humidity dec, temp int, temp dec, checksum
    for (int i = 0; i < 5; i++) {
        ret = dht11_read_byte(pin, &data[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed reading byte %d", i);
            return ret;
        }
    }

    return ESP_OK;
}

static bool dht11_validate_checksum(uint8_t data[5]) {
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    return (sum == data[4]);
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t dht11_init(gpio_num_t pin, dht11_t *dht11) {
    esp_err_t ret;

    // sanity check
    if (dht11 == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // gpio config (idle HIGH via pull-up)
    gpio_config_t config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << pin),
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };

    ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure GPIO %d", (int)pin);
        return ret;
    }

    // initialize dht11 structure
    dht11->pin = pin;
    dht11->temperature = -1.0f;
    dht11->humidity = -1.0f;
    dht11->last_update = 0;

    ESP_LOGI(TAG, "initialized on GPIO %d", (int)pin);
    return ESP_OK;
}

esp_err_t dht11_update(dht11_t *dht11) {
    // initialization
    esp_err_t ret;
    uint8_t data[5];
    int64_t start_time;

    // sanity check
    if (dht11 == NULL) {
        ESP_LOGE(TAG, "null pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // check last read
    if ((esp_timer_get_time() - dht11->last_update) < DHT11_MIN_INTERVAL_US) {
        ESP_LOGW(TAG, "DHT11 read too soon (rate limited)");
        return ESP_ERR_INVALID_STATE;
    }

    // ── CRITICAL TIMING SECTION START ─────────────────────
    portDISABLE_INTERRUPTS();

    // timeout
    start_time = esp_timer_get_time();

    // start sensor
    ret = dht11_send_start_signal(dht11->pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to send start signal");
        portENABLE_INTERRUPTS();
        return ret;
    }

    // total timeout check
    if ((esp_timer_get_time() - start_time) > DHT11_TOTAL_TIMEOUT_US) {
        ESP_LOGE(TAG, "total timeout after start signal");
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }

    // wait for ACK
    ret = dht11_wait_for_response(dht11->pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to receive sensor response");
        portENABLE_INTERRUPTS();
        return ret;
    }

    // total timeout check
    if ((esp_timer_get_time() - start_time) > DHT11_TOTAL_TIMEOUT_US) {
        ESP_LOGE(TAG, "total timeout after ACK");
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }

    // read data
    ret = dht11_read_data(dht11->pin, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to read sensor data");
        portENABLE_INTERRUPTS();
        return ret;
    }

    // total timeout check
    if ((esp_timer_get_time() - start_time) > DHT11_TOTAL_TIMEOUT_US) {
        ESP_LOGE(TAG, "total timeout after data read");
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }

    portENABLE_INTERRUPTS();
    // ── CRITICAL TIMING SECTION END ───────────────────────

    // validation
    if (!dht11_validate_checksum(data)) {
        ESP_LOGE(TAG, "checksum validation failed");
        return ESP_ERR_INVALID_CRC;
    }

    // set values
    dht11->humidity = data[0];
    dht11->temperature = data[2];
    dht11->last_update = esp_timer_get_time();

    return ESP_OK;
}

esp_err_t dht11_read(dht11_t *dht11, float *temperature, float *humidity) {
    // sanity check
    if (dht11 == NULL || temperature == NULL || humidity == NULL) {
        ESP_LOGE(TAG, "null pointer");
        return ESP_ERR_INVALID_ARG;
    }

    *temperature = dht11->temperature;
    *humidity = dht11->humidity;

    return ESP_OK;
}