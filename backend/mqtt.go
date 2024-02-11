package main

import (
	"fmt"
	"log"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

type MQTT struct {
	mqttClient mqtt.Client
	mqtt.MessageHandler
	mqtt.ConnectionLostHandler
	mqtt.OnConnectHandler
	mqtt.ConnectionAttemptHandler
	mqtt.ReconnectHandler
	logger *log.Logger
}

func NewMQTTClient(port int, broker string, username, password string) (*MQTT, error) {
	options := mqtt.NewClientOptions()
	options.AddBroker(fmt.Sprintf("tcp://%s:%d", broker, port))
	client := mqtt.NewClient(options)
	token := client.Connect()
	token.Wait()
	err := token.Error()
	if err != nil {
		return nil, err
	}
	return &MQTT{
		mqttClient: client,
	}, nil
}

func (mq *MQTT) Publish(topic string, payload []byte) error {
	token := mq.mqttClient.Publish(topic, 0, false, payload)
	token.Wait()
	err := token.Error()
	if err != nil {
		return err
	}
	return nil
}

func (mq *MQTT) Subscribe(topic string, data chan []byte) {
	mq.mqttClient.Subscribe(topic, 0, func(client mqtt.Client, message mqtt.Message) {
		//fmt.Printf("Received message: %s from topic: %s\n", message.Payload(), message.Topic())
		message.Ack()
		data <- message.Payload()
	})
}
