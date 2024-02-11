package main

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"
)

func main() {
	var broker = "178.79.181.102"
	var port = 1883
	fmt.Println("Connecting to MQTT Broker")

	_, err := ConnectDB()

	if err != nil {
		panic(err)
	}

	//options.SetClientID("test_client")
	// no username or password is use, as allow allow_anonymous is set to true
	mqttService, err := NewMQTTClient(port, broker, "", "")
	if err != nil {
		panic(err)
	}

	topic := "sensor_data/power_data"
	outbound_topic := "sensor_data/power_data/outbound"

	type publishMessage struct {
		Message string `json:"message"`
	}

	wg := sync.WaitGroup{}
	wg.Add(1)

	go func() {
		defer wg.Done()
		for {
			fmt.Println("sending data")
			message, _ := json.Marshal(&publishMessage{Message: fmt.Sprintf("send_data")})
			err := mqttService.Publish(outbound_topic, message)
			if err != nil {
				fmt.Println(err)
			}
			time.Sleep(time.Second * 6)
		}
	}()

	respDataChan := make(chan []byte)
	quit := make(chan bool)

	go func() {
		mqttService.Subscribe(topic, respDataChan)
	}()

	go func() {
		wg.Wait()
		quit <- true
		close(respDataChan)
	}()

	for {
		select {
		case respData := <-respDataChan:
			fmt.Printf("got response %v \n", string(respData))
		case <-quit:
			fmt.Println("program exited")
			return
		}
	}
}
