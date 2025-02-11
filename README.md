# Projct_ESP_IDF
The goal of this project is to integrate and configure the I2C driver to communicate with multiple sensors, retrieve various environmental and system parameters, and transmit the collected data to an MQTT server for remote monitoring and processing.
This involves:
Implementing the I2C driver to establish communication between the microcontroller and the sensors.
Reading and processing sensor data such as temperature, humidity, pressure, or motion.
Formatting the data into structured MQTT messages.
Establishing an MQTT connection using an embedded network  (wifi).
Optimizing power consumption and timing to ensure efficient data retrieval and transmission.
Ensuring reliability through error handling, reconnection mechanisms.
