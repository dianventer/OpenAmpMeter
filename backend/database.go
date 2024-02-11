package main

import (
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

type Database struct {
	db *gorm.DB
}

func ConnectDB() (*Database, error) {
	dsn := "host=178.79.181.102 user=postgres password=mootbrood dbname=postgres port=5432 sslmode=disable TimeZone=Africa/Cairo"
	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{})
	if err != nil {
		panic(err)
	}
	db.AutoMigrate(&Sensor{})
	db.AutoMigrate(&SensorData{})
	db.AutoMigrate(&Units{})
	db.AutoMigrate(&SensorUnit{})
	db.AutoMigrate(&User{})
	db.AutoMigrate(&UserSensor{})
	db.AutoMigrate(&SensorRelation{})
	db.AutoMigrate(&SensorRules{})
	db.AutoMigrate(&SensorEvents{})
	return &Database{db: db}, nil
}
