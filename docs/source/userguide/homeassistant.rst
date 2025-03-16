OVMS Home Assistant Integration
===============================

Overview
--------

The Open Vehicle Monitoring System (OVMS) integration for Home Assistant connects your electric vehicle to your smart home via MQTT, creating a seamless monitoring and control experience. This integration automatically discovers all vehicle metrics published by your OVMS module and creates appropriate entities in Home Assistant.

Features
--------

* **Real-time monitoring** of vehicle metrics (battery, climate, location, etc.)
* **Remote control** of climate systems, charging, and other vehicle functions
* **Automation integration** with Home Assistant's automation engine
* **Location tracking** with GPS accuracy estimation
* **Advanced battery analytics** including cell-level data with statistical analysis
* **Secure communication** with TLS/SSL support for MQTT connections

How It Works
-------------

The integration operates through these components:

1. Your OVMS module publishes vehicle data to an MQTT broker
2. Home Assistant subscribes to these MQTT topics
3. The integration automatically:

   * Transforms MQTT messages into appropriate Home Assistant entities
   * Creates sensors, binary sensors, device trackers, and switches
   * Categorizes entities by function (battery, climate, etc.)
   * Maintains real-time state based on MQTT updates
   * Provides services for sending commands back to your vehicle

MQTT Overhead and Performance
------------------------------

Data Usage and Bandwidth
~~~~~~~~~~~~~~~~~~~~~~~~

The OVMS integration uses MQTT for communication, which is designed to be lightweight and efficient for IoT devices. Typical data usage depends on:

* Number of metrics being published
* Update frequency of metrics
* Message payload size

For most vehicles, the total MQTT traffic ranges from 0.04 MB - 1 MB per day with default settings, depending on how actively the vehicle is used and how many metrics are enabled. This averages to approximately 10 MB per month when your car is connected to WiFi while at home, and driven to work around 1 hour per day.

Memory and CPU Usage
~~~~~~~~~~~~~~~~~~~~

The integration's resource footprint on Home Assistant server is minimal:

* **Memory**: Typically 10-30MB RAM depending on the number of entities
* **CPU**: Negligible impact during normal operation (1-3% on average systems)
* **Storage**: Minimal state data, primarily in Home Assistant's database

Comparison with OVMS V2 Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------+------------------------------------+----------------------------------+
| Aspect               | MQTT/Home Assistant Integration    | OVMS V2 Server                   |
+======================+====================================+==================================+
| Data Transfer        | 0.04-1MB/day (~10MB/month)         | 60KB/day baseline (~2MB/month)   |
+----------------------+------------------------------------+----------------------------------+
| Protocol Efficiency  | Moderate (MQTT is IoT optimized    | Higher (uses lighter MP protocol)|
|                      | but requires more bandwidth)       |                                  |
+----------------------+------------------------------------+----------------------------------+
| Server Resource Usage| Lower (leverages existing MQTT)    | Higher (dedicated server needed) |
+----------------------+------------------------------------+----------------------------------+
| Scalability          | Excellent (thousands of clients)   | Good (limited by server)         |
+----------------------+------------------------------------+----------------------------------+
| Historical Data      | Home Assistant's native history    | Built-in data retention          |
+----------------------+------------------------------------+----------------------------------+

The V2 server generally provides better efficiency and lower data usage than the MQTT integration, while the MQTT integration offers deeper integration with home automation systems.

Optimization Tips
~~~~~~~~~~~~~~~~~

To reduce data usage:

* **Don't use the V3 (MQTT) protocol if data usage is a concern**. MQTT needs significantly more bandwidth than the MP protocol used by V2.
* Don't activate the App's background service mode (currently only applies to Android).
* Don't activate GPS Streaming Mode (feature #8).
* Reduce the general update intervals (web UI: Config → Server V2/V3).

For MQTT-specific optimization:

* Configure appropriate update intervals in your OVMS module
* Use QoS level 0 for non-critical metrics
* Use QoS level 1 for commands.
* Consider disabling metrics you don't need in your OVMS configuration
* For large deployments, use a dedicated MQTT broker with optimized settings

Prerequisites for using the Home Assistant HACS integration
-----------------------------------------------------------

* Home Assistant (2025.2.5 or newer)
* MQTT integration configured in Home Assistant, or any external MQTT Broker that supports MQTTv3.1 (or newer). EMQX are confirmed to be working well.
* OVMS module with firmware 3.3.001 or newer recommended
* OVMS Server V3 enabled

Installation
------------

Integration Installation
~~~~~~~~~~~~~~~~~~~~~~~~

1. Configure OVMS Module
~~~~~~~~~~~~~~~~~~~~~~~~

In your OVMS web UI:

1. Navigate to **Config → Server V3 (MQTT)**
2. Configure the following settings::

      Server: Your MQTT broker address
      Port: 1883 (mqtt://), 8083 (ws://), 8883 (mqtts://), or 8084 (wss://)
      Username/Password: If required by your broker
      Topic Prefix: ovms (default, can be customized)
      Enable Auto-Start: YES

3. Save your configuration

2. Configure MQTT Broker Permissions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure your MQTT broker permits these operations::

   Subscribe Permissions:
   - ovms/# (For all OVMS topics)
   - homeassistant/# (For testing connection)

   Publish Permissions:
   - ovms/+/+/client/rr/command/# (For sending commands)
   - ovms/+/+/status (For publishing status)

3. Install the HACS Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

HACS Installation (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. In Home Assistant, go to **HACS → Integrations**
2. Click on **+ Explore & Download Repositories**
3. Search for **OVMS Home Assistant**
4. Install the integration
5. Restart Home Assistant

Adding the HACS integration as a Custom Repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the integration is not available in the HACS store:

1. In Home Assistant, go to **HACS → Integrations**
2. Click the three dots in the upper right corner
3. Select **Custom repositories**
4. Enter the following information:
   
   * Repository URL: ``https://github.com/enoch85/ovms-home-assistant``
   * Category: **Integration**
   
5. Click **Add**
6. The OVMS integration will now appear in your HACS Integrations list
7. Click on it and select **Download**
8. Restart Home Assistant after installation

Manual Installation of the HACS integration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Download the repository as a ZIP file
2. Extract it and copy the ``custom_components/ovms`` folder to your Home Assistant's ``custom_components`` directory
3. Restart Home Assistant

4. Set Up the HACS Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. In Home Assistant, go to **Settings → Devices & Services → Integrations**
2. Click on **+ Add integration** and search for **OVMS**
3. Enter MQTT broker details and connection information
4. Configure topic structure to match your OVMS settings
5. Select your vehicle ID when prompted

Available Services
------------------

The integration provides several services to control your vehicle:

* **ovms.send_command**: Send any command to the OVMS module
* **ovms.set_feature**: Set an OVMS configuration feature
* **ovms.control_climate**: Control the vehicle's climate system
* **ovms.control_charging**: Control the vehicle's charging functions

MQTT Topic Structure
--------------------

The integration supports these MQTT topic structures:

* Default: ``ovms/username/vehicle_id/metric/...``
* Alternative: ``ovms/client/vehicle_id/...``
* Simple: ``ovms/vehicle_id/...``
* Custom: Define your own structure with placeholders

Secure MQTT with TLS
--------------------

For secure MQTT connections, you can configure TLS/SSL. This is especially important for remote connections.

Certificate Authority Setup
~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Generate certificates on your MQTT broker
2. Import the CA certificate to OVMS through the web interface:
   * Go to 'Tools' > 'Editor'
   * Create a folder at '/store/trustedca/'
   * Save your CA certificate as a .pem file in this folder
   * Run 'tls trust reload' from the OVMS shell

Configure OVMS for Secure MQTT
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the OVMS web UI:

1. Navigate to 'Config' > 'Server V3 (MQTT)'
2. Enable TLS
3. Set the port to 8883 (or your secure port)
4. Configure authentication if required
5. Save and restart the MQTT service

Troubleshooting
---------------

If no entities are created:

1. Check if your OVMS module is publishing to the MQTT broker
2. Verify the topic structure matches your configuration
3. Enable debug logging by adding to your ``configuration.yaml``::

      logger:
        default: info
        logs:
          custom_components.ovms: debug

   *Warning! The debug output is substantial. It may fill your disk if you are not careful, don't leave it turned on.*

4. Verify ACL permissions in your MQTT broker

Additional Resources
--------------------

For advanced usage, dashboard examples, and technical details, refer to the full documentation at:
https://github.com/enoch85/ovms-home-assistant

Manual Configuration (non HACS)
-------------------------------

As an alternative to using the integration, you can manually configure Home Assistant to work with OVMS using MQTT sensors defined in your configuration.yaml file. This approach gives you more control over which metrics are tracked and how they are displayed.

1. Setup MQTT Broker Connection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This can be either an external broker or the built-in MQTT broker in Home Assistant.

2. Configure OVMS
~~~~~~~~~~~~~~~~~

Follow the same MQTT configuration as above in your OVMS module.

3. Configure Home Assistant YAML
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add MQTT sensors to your configuration.yaml file. Example sensors::

   mqtt:
     binary_sensor:
       - name: "OVMS 12V Battery Alert"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/b/12v/voltage/alert"
         icon: mdi:car-battery
     sensor:
       - name: "OVMS GPS Latitude"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/p/latitude"
         icon: mdi:latitude
       - name: "OVMS GPS Longitude"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/p/longitude"
         icon: mdi:longitude
       - name: "OVMS GPS Signal Strength"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/p/gpssq"
         device_class: signal_strength
         unit_of_measurement: '%'
       - name: "OVMS GPS Time Updated"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/p/gpstime"
         value_template: '{{ value_json | timestamp_local }}'
         device_class: timestamp
       - name: "OVMS 12V Battery"
         state_topic: "ovms/CAR/UNIQUEID/metric/v/b/12v/voltage"
         value_template: '{{ value | round(1) }}'
         icon: mdi:car-battery
         unit_of_measurement: 'V'

Replace "CAR/UNIQUEID" with your actual vehicle identifier. Add additional sensors based on the metrics available from your vehicle.
