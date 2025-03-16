OVMS Home Assistant Integration
===========================

Overview
--------

The Open Vehicle Monitoring System (OVMS) integration for Home Assistant connects your electric vehicle to your smart home via MQTT, creating a seamless monitoring and control experience. This integration automatically discovers all vehicle metrics published by your OVMS module and creates appropriate entities in Home Assistant.

What You Can Do
--------------

The OVMS integration enables:

* **Real-time monitoring** of all vehicle metrics (battery, climate, location, etc.)
* **Remote control** of climate systems, charging, and other vehicle functions
* **Automation integration** with Home Assistant's automation engine
* **Location tracking** with GPS accuracy estimation
* **Advanced battery analytics** including cell-level data with statistical analysis
* **Secure communication** with TLS/SSL support for MQTT connections

How It Works
-----------

The integration operates through these components:

1. Your OVMS module publishes vehicle data to an MQTT broker
2. Home Assistant subscribes to these MQTT topics
3. The integration automatically:

   * Transforms MQTT messages into appropriate Home Assistant entities
   * Creates sensors, binary sensors, device trackers, and switches
   * Categorizes entities by function (battery, climate, etc.)
   * Maintains real-time state based on MQTT updates
   * Provides services for sending commands back to your vehicle

MQTT overhead
-----------

Placeholder


Getting Started
--------------

Prerequisites
~~~~~~~~~~~~

* Home Assistant (2025.2.5 or newer)
* MQTT integration configured in Home Assistant, or any external MQTT Broker that supports MQTTv3.1 (or newer). EMQX are confirmed to be working well.
* OVMS module with firmware 3.3.001 or newer recommended
* OVMS Server V3 enabled

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure your MQTT broker permits these operations::

   Subscribe Permissions:
   - ovms/# (For all OVMS topics)
   - homeassistant/# (For testing connection)

   Publish Permissions:
   - ovms/+/+/client/rr/command/# (For sending commands)
   - ovms/+/+/status (For publishing status)

3. Install the Integration
~~~~~~~~~~~~~~~~~~~~~~~~~

HACS Installation (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. In Home Assistant, go to **HACS → Integrations**
2. Click on **+ Explore & Download Repositories**
3. Search for **OVMS Home Assistant**
4. Install the integration
5. Restart Home Assistant

Adding as a Custom Repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Manual Installation
^^^^^^^^^^^^^^^^^

1. Download the repository as a ZIP file
2. Extract it and copy the ``custom_components/ovms`` folder to your Home Assistant's ``custom_components`` directory
3. Restart Home Assistant

4. Set Up the Integration
~~~~~~~~~~~~~~~~~~~~~~~

1. In Home Assistant, go to **Settings → Devices & Services → Integrations**
2. Click on **+ Add integration** and search for **OVMS**
3. Enter MQTT broker details and connection information
4. Configure topic structure to match your OVMS settings
5. Select your vehicle ID when prompted

Available Services
-----------------

The integration provides several services to control your vehicle:

* **ovms.send_command**: Send any command to the OVMS module
* **ovms.set_feature**: Set an OVMS configuration feature
* **ovms.control_climate**: Control the vehicle's climate system
* **ovms.control_charging**: Control the vehicle's charging functions

MQTT Topic Structure
------------------

The integration supports these MQTT topic structures:

* Default: ``ovms/username/vehicle_id/metric/...``
* Alternative: ``ovms/client/vehicle_id/...``
* Simple: ``ovms/vehicle_id/...``
* Custom: Define your own structure with placeholders

Troubleshooting
--------------

If no entities are created:

1. Check if your OVMS module is publishing to the MQTT broker
2. Verify the topic structure matches your configuration
3. Enable debug logging by adding to your ``configuration.yaml``::

      logger:
        default: info
        logs:
          custom_components.ovms: debug

4. Verify ACL permissions in your MQTT broker

Additional Resources
------------------

For advanced usage, dashboard examples, and technical details, refer to the full documentation at:
https://github.com/enoch85/ovms-home-assistant
