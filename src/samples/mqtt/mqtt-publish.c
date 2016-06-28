/*
 * This file is part of the Soletta (TM) Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief MQTT Publish client
 *
 * Sample client that connects a broker at host:port and publishes the
 * provided message once every 1 second.
 */

#include <string.h>

#include <sol-log.h>
#include <sol-mainloop.h>
#include <sol-mqtt.h>

static struct sol_timeout *timeout;
static char *topic;
static struct sol_mqtt_message message;

static bool
publish_callback(void *data)
{
    static int id = 0;
    struct sol_mqtt *mqtt = data;

    SOL_NULL_CHECK(mqtt, false);

    SOL_INF("%d: Sending Message.", id++);

    if (sol_mqtt_publish(mqtt, &message)) {
        SOL_WRN("Unable to publish message");
        return false;
    }

    return true;
}

static bool
try_reconnect(void *data)
{
    return sol_mqtt_reconnect((struct sol_mqtt *)data) != 0;
}

static void
on_connect(void *data, struct sol_mqtt *mqtt)
{
    if (sol_mqtt_get_connection_status(mqtt) != SOL_MQTT_CONNECTED) {
        SOL_WRN("Unable to connect, retrying...");
        sol_timeout_add(1000, try_reconnect, mqtt);
        return;
    }

    if (!publish_callback(mqtt))
        return;

    timeout = sol_timeout_add(1000, publish_callback, mqtt);
    if (!timeout)
        SOL_WRN("Unable to setup callback");
}

static void
on_disconnect(void *data, struct sol_mqtt *mqtt)
{
    SOL_INF("Reconnecting...");
    sol_timeout_add(1000, try_reconnect, mqtt);
}

int
main(int argc, char *argv[])
{
    struct sol_mqtt *mqtt;
    struct sol_buffer payload;
    struct sol_mqtt_config config = {
        SOL_SET_API_VERSION(.api_version = SOL_MQTT_CONFIG_API_VERSION, )
        .clean_session = true,
        .keep_alive = 60,
        .handlers = {
            SOL_SET_API_VERSION(.api_version = SOL_MQTT_HANDLERS_API_VERSION, )
            .connect = on_connect,
            .disconnect = on_disconnect,
        },
    };

    sol_init();

    if (argc < 5) {
        SOL_INF("Usage: %s <ip> <port> <topic> <message>", argv[0]);
        return 0;
    }

    config.port = atoi(argv[2]);
    config.host = argv[1];
    topic = argv[3];

    payload = SOL_BUFFER_INIT_CONST(argv[4], strlen(argv[4]));

    message = (struct sol_mqtt_message){
        SOL_SET_API_VERSION(.api_version = SOL_MQTT_MESSAGE_API_VERSION, )
        .topic = topic,
        .payload = &payload,
        .qos = SOL_MQTT_QOS_EXACTLY_ONCE,
        .retain = false,
    };

    mqtt = sol_mqtt_connect(&config);
    if (!mqtt) {
        SOL_WRN("Unable to create MQTT session");
        return -1;
    }

    sol_run();

    if (timeout)
        sol_timeout_del(timeout);

    sol_mqtt_disconnect(mqtt);

    sol_shutdown();

    return 0;
}
